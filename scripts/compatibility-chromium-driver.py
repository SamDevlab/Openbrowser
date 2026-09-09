#!/usr/bin/env python3
"""Drive a pinned Chromium/Chrome-for-Testing binary for the compatibility harness."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import urllib.parse
from pathlib import Path
from typing import Sequence

from compatibility_observer import ObservationServer


RESULT_SCHEMA_VERSION = 1


def _required_environment(name: str) -> str:
    value = os.environ.get(name, "")
    if not value:
        raise RuntimeError(f"Missing compatibility harness environment variable: {name}")
    return value


def _instrumented_url(url: str, observer_url: str) -> str:
    parsed = urllib.parse.urlsplit(url)
    query = urllib.parse.parse_qsl(parsed.query, keep_blank_values=True)
    query.append(("__ob_observer", observer_url))
    return urllib.parse.urlunsplit(
        (parsed.scheme, parsed.netloc, parsed.path, urllib.parse.urlencode(query), parsed.fragment)
    )


def _browser_version(executable: Path) -> str:
    completed = subprocess.run(
        [str(executable), "--version"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=10,
        check=False,
    )
    return completed.stdout.strip()


def _parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = _parse_args(argv if argv is not None else sys.argv[1:])
    executable = args.executable.resolve()
    if not executable.is_file():
        print(f"Chromium reference executable does not exist: {executable}", file=sys.stderr)
        return 2

    scenario = _required_environment("OPENBROWSER_COMPAT_SCENARIO")
    url = _required_environment("OPENBROWSER_COMPAT_URL")
    result_path = Path(_required_environment("OPENBROWSER_COMPAT_RESULT_FILE"))
    storage_dir = Path(_required_environment("OPENBROWSER_COMPAT_STORAGE_DIR"))
    storage_dir.mkdir(parents=True, exist_ok=True)
    profile_dir = storage_dir / "chromium-profile"
    profile_dir.mkdir(parents=True, exist_ok=True)

    with ObservationServer(scenario) as observer:
        target_url = _instrumented_url(url, observer.url)
        command = [
            str(executable),
            "--headless",
            "--disable-background-networking",
            "--disable-component-update",
            "--disable-default-apps",
            "--disable-extensions",
            "--disable-gpu",
            "--disable-sync",
            "--metrics-recording-only",
            "--no-default-browser-check",
            "--no-first-run",
            "--virtual-time-budget=3000",
            f"--user-data-dir={profile_dir}",
            "--dump-dom",
            target_url,
        ]
        completed = subprocess.run(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        if completed.returncode != 0:
            if completed.stdout:
                print(completed.stdout, file=sys.stdout)
            if completed.stderr:
                print(completed.stderr, file=sys.stderr)
            return completed.returncode or 20

        observation = observer.wait(5.0)
        if observation is None:
            print("Chromium reference exited without publishing a fixture observation.", file=sys.stderr)
            return 21

    result = {
        "schema_version": RESULT_SCHEMA_VERSION,
        "scenario_id": scenario,
        "final_url": observation.get("final_url"),
        "title": observation.get("title"),
        "dom_markers": observation.get("dom_markers", {}),
        "events": observation.get("events", []),
        "storage": observation.get("storage", {}),
        "adapter": {
            "kind": "chromium-reference",
            "version": _browser_version(executable),
        },
    }
    result_path.parent.mkdir(parents=True, exist_ok=True)
    result_path.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
