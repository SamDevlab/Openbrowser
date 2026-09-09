#!/usr/bin/env python3
"""Report whether Openbrowser's pinned CEF/Chromium engine needs review.

The monitor deliberately separates two signals:
- a newer stable CEF build is available: actionable CEF upgrade review;
- Chrome Stable is ahead of the Chromium embedded by the current stable CEF:
  upstream security/currency signal, but not permission to substitute Chrome for CEF.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import urllib.request
from pathlib import Path
from typing import Any

CEF_INDEX_URL = "https://cef-builds.spotifycdn.com/index.json"
CHROME_STABLE_URL = (
    "https://googlechromelabs.github.io/chrome-for-testing/"
    "last-known-good-versions.json"
)
DEFAULT_PLATFORM = "windows64"
PIN_RE = re.compile(
    r'"(?P<cef>\d+\.\d+\.\d+\+g[0-9a-f]+\+chromium-'
    r'(?P<chromium>\d+\.\d+\.\d+\.\d+))"'
)
CHROMIUM_IN_CEF_RE = re.compile(r"\+chromium-(\d+\.\d+\.\d+\.\d+)$")


def version_tuple(value: str) -> tuple[int, ...]:
    numeric = value.split("+", 1)[0]
    return tuple(int(part) for part in numeric.split("."))


def read_json(source: str) -> Any:
    if source.startswith(("https://", "http://")):
        request = urllib.request.Request(
            source,
            headers={"User-Agent": "Openbrowser-engine-security-monitor/1"},
        )
        with urllib.request.urlopen(request, timeout=30) as response:
            return json.load(response)
    with Path(source).open("r", encoding="utf-8") as handle:
        return json.load(handle)


def read_pinned_engine(cmake_path: Path) -> tuple[str, str]:
    text = cmake_path.read_text(encoding="utf-8")
    match = PIN_RE.search(text)
    if match is None:
        raise ValueError(f"could not resolve pinned CEF version from {cmake_path}")
    return match.group("cef"), match.group("chromium")


def chromium_from_cef(cef_version: str) -> str:
    match = CHROMIUM_IN_CEF_RE.search(cef_version)
    if match is None:
        raise ValueError(f"CEF version does not contain Chromium version: {cef_version}")
    return match.group(1)


def latest_stable_cef(index: dict[str, Any], platform: str) -> tuple[str, str]:
    platform_data = index.get(platform)
    if not isinstance(platform_data, dict):
        raise ValueError(f"CEF index does not contain platform {platform!r}")
    versions = platform_data.get("versions")
    if not isinstance(versions, list):
        raise ValueError(f"CEF index platform {platform!r} has no versions list")

    stable = [
        entry
        for entry in versions
        if isinstance(entry, dict)
        and entry.get("channel") == "stable"
        and isinstance(entry.get("cef_version"), str)
        and "+" in entry["cef_version"]
    ]
    if not stable:
        raise ValueError(f"CEF index has no stable builds for {platform!r}")

    newest = max(stable, key=lambda entry: version_tuple(entry["cef_version"]))
    cef_version = newest["cef_version"]
    chromium_version = newest.get("chromium_version")
    if not isinstance(chromium_version, str) or not chromium_version:
        chromium_version = chromium_from_cef(cef_version)
    return cef_version, chromium_version


def latest_chrome_stable(payload: dict[str, Any]) -> str:
    channels = payload.get("channels")
    if not isinstance(channels, dict):
        raise ValueError("Chrome for Testing payload has no channels object")
    stable = channels.get("Stable")
    if not isinstance(stable, dict) or not isinstance(stable.get("version"), str):
        raise ValueError("Chrome for Testing payload has no Stable version")
    return stable["version"]


def build_report(
    pinned_cef: str,
    pinned_chromium: str,
    stable_cef: str,
    stable_cef_chromium: str,
    chrome_stable: str,
    platform: str,
) -> dict[str, Any]:
    cef_update_available = version_tuple(stable_cef) > version_tuple(pinned_cef)
    cef_chromium_update_available = (
        version_tuple(stable_cef_chromium) > version_tuple(pinned_chromium)
    )
    chrome_ahead = version_tuple(chrome_stable) > version_tuple(pinned_chromium)

    if cef_update_available or cef_chromium_update_available:
        recommendation = "review-and-upgrade-cef"
    elif chrome_ahead:
        recommendation = "watch-upstream-cef"
    else:
        recommendation = "current"

    return {
        "schema_version": 1,
        "platform": platform,
        "pinned": {
            "cef": pinned_cef,
            "chromium": pinned_chromium,
        },
        "upstream": {
            "stable_cef": stable_cef,
            "stable_cef_chromium": stable_cef_chromium,
            "chrome_stable": chrome_stable,
        },
        "signals": {
            "cef_update_available": cef_update_available,
            "stable_cef_chromium_update_available": cef_chromium_update_available,
            "chrome_stable_ahead_of_embedded_chromium": chrome_ahead,
        },
        "recommendation": recommendation,
    }


def markdown_summary(report: dict[str, Any]) -> str:
    pinned = report["pinned"]
    upstream = report["upstream"]
    signals = report["signals"]
    return "\n".join(
        [
            "## Openbrowser engine security/currency monitor",
            "",
            f"- Pinned CEF: `{pinned['cef']}`",
            f"- Embedded Chromium: `{pinned['chromium']}`",
            f"- Latest stable CEF ({report['platform']}): `{upstream['stable_cef']}`",
            f"- Chromium in latest stable CEF: `{upstream['stable_cef_chromium']}`",
            f"- Chrome Stable: `{upstream['chrome_stable']}`",
            f"- New stable CEF available: `{signals['cef_update_available']}`",
            f"- Chrome Stable ahead of embedded Chromium: "
            f"`{signals['chrome_stable_ahead_of_embedded_chromium']}`",
            f"- Recommendation: **{report['recommendation']}**",
            "",
            "Chrome moving ahead is an upstream risk signal, not permission to replace "
            "the supported CEF distribution with a Chrome binary.",
        ]
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cmake", default="cmake/OpenbrowserCEF.cmake")
    parser.add_argument("--platform", default=DEFAULT_PLATFORM)
    parser.add_argument("--cef-index", default=CEF_INDEX_URL)
    parser.add_argument("--chrome-json", default=CHROME_STABLE_URL)
    parser.add_argument("--output", default="engine-security-report.json")
    parser.add_argument("--fail-on-cef-update", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        pinned_cef, pinned_chromium = read_pinned_engine(Path(args.cmake))
        cef_index = read_json(args.cef_index)
        chrome_payload = read_json(args.chrome_json)
        stable_cef, stable_cef_chromium = latest_stable_cef(
            cef_index, args.platform
        )
        chrome_stable = latest_chrome_stable(chrome_payload)
        report = build_report(
            pinned_cef,
            pinned_chromium,
            stable_cef,
            stable_cef_chromium,
            chrome_stable,
            args.platform,
        )
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        print(f"engine security monitor failed: {exc}", file=sys.stderr)
        return 1

    Path(args.output).write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    summary = markdown_summary(report)
    print(summary)

    step_summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if step_summary:
        with Path(step_summary).open("a", encoding="utf-8") as handle:
            handle.write(summary + "\n")

    signals = report["signals"]
    if signals["cef_update_available"]:
        print(
            "::warning title=CEF upgrade review required::A newer stable CEF build "
            "is available. Review compatibility, security notes, checksums and the "
            "full browser test matrix before changing the pin."
        )
    elif signals["chrome_stable_ahead_of_embedded_chromium"]:
        print(
            "::warning title=Embedded Chromium trails Chrome Stable::Chrome Stable "
            "is newer than Openbrowser's embedded Chromium. Track upstream CEF; do "
            "not substitute Chrome for the supported CEF build."
        )

    if args.fail_on_cef_update and signals["cef_update_available"]:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
