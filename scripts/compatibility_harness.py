#!/usr/bin/env python3
"""Run deterministic local-fixture differential compatibility scenarios.

The harness owns fixture serving, process lifecycle, result normalization and
comparison. Browser-specific adapters only need to launch a browser and write
the versioned result document described in docs/web-compatibility.md.
"""

from __future__ import annotations

import argparse
import http.server
import json
import os
import shlex
import signal
import subprocess
import sys
import tempfile
import threading
import urllib.parse
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable, Mapping, Sequence


RESULT_SCHEMA_VERSION = 1
CONTROL_PREFIX = "/__compatibility__/"
IGNORED_RESULT_KEYS = frozenset(
    {
        "duration_ms",
        "generated_at",
        "pid",
        "process_id",
        "request_id",
        "run_id",
        "session_dir",
        "timestamp",
    }
)


class HarnessError(RuntimeError):
    """Raised for invalid harness configuration or fixture manifests."""


@dataclass(frozen=True)
class Scenario:
    scenario_id: str
    path: str
    description: str
    expected_request_paths: tuple[str, ...]
    compare_fields: tuple[str, ...]
    redirects: Mapping[str, str]


def _require_string(value: Any, field: str) -> str:
    if not isinstance(value, str) or not value:
        raise HarnessError(f"{field} must be a non-empty string")
    return value


def _fixture_path(value: str, field: str) -> str:
    normalized = "/" + value.replace("\\", "/").lstrip("/")
    parts = [part for part in normalized.split("/") if part]
    if not parts or any(part in {".", ".."} for part in parts):
        raise HarnessError(f"{field} must stay inside the fixture root: {value!r}")
    return "/" + "/".join(parts)


def load_manifest(manifest_path: Path) -> tuple[Path, list[Scenario]]:
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise HarnessError(f"Could not read manifest {manifest_path}: {exc}") from exc

    if not isinstance(manifest, dict) or manifest.get("schema_version") != RESULT_SCHEMA_VERSION:
        raise HarnessError(
            f"Manifest schema_version must be {RESULT_SCHEMA_VERSION}: {manifest_path}"
        )

    fixture_root = (manifest_path.parent / manifest.get("fixture_root", ".")).resolve()
    if not fixture_root.is_dir():
        raise HarnessError(f"Fixture root does not exist: {fixture_root}")

    raw_scenarios = manifest.get("scenarios")
    if not isinstance(raw_scenarios, list) or not raw_scenarios:
        raise HarnessError("Manifest must contain a non-empty scenarios array")

    scenarios: list[Scenario] = []
    seen_ids: set[str] = set()
    for raw in raw_scenarios:
        if not isinstance(raw, dict):
            raise HarnessError("Every scenario must be an object")
        scenario_id = _require_string(raw.get("id"), "scenario.id")
        if scenario_id in seen_ids:
            raise HarnessError(f"Duplicate scenario id: {scenario_id}")
        seen_ids.add(scenario_id)

        path = _fixture_path(_require_string(raw.get("path"), f"{scenario_id}.path"), "scenario.path")
        raw_expected = raw.get("expected_request_paths", [path])
        if not isinstance(raw_expected, list) or not all(isinstance(item, str) for item in raw_expected):
            raise HarnessError(f"{scenario_id}.expected_request_paths must be an array of strings")
        expected_paths = tuple(_fixture_path(item, f"{scenario_id}.expected_request_paths") for item in raw_expected)

        raw_fields = raw.get(
            "compare_fields",
            ["final_url", "title", "dom_markers", "events", "storage"],
        )
        if not isinstance(raw_fields, list) or not all(isinstance(item, str) and item for item in raw_fields):
            raise HarnessError(f"{scenario_id}.compare_fields must be an array of strings")

        raw_redirects = raw.get("redirects", {})
        if not isinstance(raw_redirects, dict):
            raise HarnessError(f"{scenario_id}.redirects must be an object")
        redirects = {
            _fixture_path(_require_string(source, "redirect source"), "redirect source"): _fixture_path(
                _require_string(target, "redirect target"), "redirect target"
            )
            for source, target in raw_redirects.items()
        }

        scenarios.append(
            Scenario(
                scenario_id=scenario_id,
                path=path,
                description=str(raw.get("description", "")),
                expected_request_paths=expected_paths,
                compare_fields=tuple(raw_fields),
                redirects=redirects,
            )
        )

    return fixture_root, scenarios


class _FixtureRequestHandler(http.server.BaseHTTPRequestHandler):
    server: "_FixtureServer"

    def do_GET(self) -> None:  # noqa: N802 - stdlib handler API
        owner = self.server
        parsed = urllib.parse.urlsplit(self.path)
        path = urllib.parse.unquote(parsed.path)
        query = urllib.parse.parse_qs(parsed.query)

        if path == f"{CONTROL_PREFIX}ready":
            token = query.get("token", [""])[0]
            owner.mark_ready(token)
            self.send_response(204)
            self.end_headers()
            owner.record_request("GET", path, 204, control=True)
            return

        if path == f"{CONTROL_PREFIX}status":
            token = query.get("token", [""])[0]
            status = 200 if owner.is_ready(token) else 202
            self.send_response(status)
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            owner.record_request("GET", path, status, control=True)
            return

        redirect_target = owner.redirects.get(path)
        if redirect_target is not None:
            self.send_response(302)
            location = redirect_target + (("?" + parsed.query) if parsed.query else "")
            self.send_header("Location", location)
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            owner.record_request("GET", path, 302)
            return

        try:
            candidate = (owner.fixture_root / path.lstrip("/")).resolve()
            candidate.relative_to(owner.fixture_root)
        except ValueError:
            self.send_error(403, "Fixture path escapes fixture root")
            owner.record_request("GET", path, 403)
            return

        if not candidate.is_file():
            self.send_error(404, "Fixture not found")
            owner.record_request("GET", path, 404)
            return

        body = candidate.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", _content_type(candidate))
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)
        owner.record_request("GET", path, 200)

    def log_message(self, _format: str, *_args: object) -> None:
        return


def _content_type(path: Path) -> str:
    return {
        ".css": "text/css; charset=utf-8",
        ".html": "text/html; charset=utf-8",
        ".js": "text/javascript; charset=utf-8",
        ".json": "application/json; charset=utf-8",
    }.get(path.suffix.lower(), "application/octet-stream")


class _FixtureServer(http.server.ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, fixture_root: Path, redirects: Mapping[str, str]) -> None:
        super().__init__(("127.0.0.1", 0), _FixtureRequestHandler)
        self.fixture_root = fixture_root
        self.redirects = dict(redirects)
        self._lock = threading.Lock()
        self._requests: list[dict[str, Any]] = []
        self._ready_tokens: set[str] = set()
        self._thread = threading.Thread(target=self.serve_forever, name="compatibility-fixtures", daemon=True)
        self._thread.start()

    @property
    def origin(self) -> str:
        host, port = self.server_address
        return f"http://{host}:{port}"

    def begin_run(self, token: str, redirects: Mapping[str, str]) -> None:
        with self._lock:
            self._requests.clear()
            self._ready_tokens.clear()
            self._active_token = token
            self.redirects = dict(redirects)

    def mark_ready(self, token: str) -> None:
        with self._lock:
            if token == getattr(self, "_active_token", None):
                self._ready_tokens.add(token)

    def is_ready(self, token: str) -> bool:
        with self._lock:
            return token in self._ready_tokens

    def record_request(self, method: str, path: str, status: int, control: bool = False) -> None:
        with self._lock:
            self._requests.append(
                {"method": method, "path": path, "status": status, "control": control}
            )

    def requests(self) -> list[dict[str, Any]]:
        with self._lock:
            result = [
                {"method": item["method"], "path": item["path"], "status": item["status"]}
                for item in self._requests
                if not item["control"]
            ]
        return sorted(result, key=lambda item: (item["method"], item["path"], item["status"]))

    def url_for(self, path: str, token: str) -> str:
        query = urllib.parse.urlencode({"__ob_token": token})
        return f"{self.origin}{path}?{query}"

    def status_url(self, token: str) -> str:
        query = urllib.parse.urlencode({"token": token})
        return f"{self.origin}{CONTROL_PREFIX}status?{query}"

    def close(self) -> None:
        self.shutdown()
        self.server_close()
        self._thread.join(timeout=5)


def _command_argv(command: str | Sequence[str], substitutions: Mapping[str, str]) -> list[str]:
    if isinstance(command, str):
        parts: Iterable[str] = shlex.split(command, posix=True)
    else:
        parts = command
    argv = []
    for part in parts:
        value = str(part)
        for key, replacement in substitutions.items():
            value = value.replace("{" + key + "}", replacement)
        argv.append(value)
    if not argv:
        raise HarnessError("Runner command cannot be empty")
    return argv


def _terminate_process(process: subprocess.Popen[str]) -> None:
    if process.poll() is not None:
        return
    if os.name == "nt":
        subprocess.run(
            ["taskkill.exe", "/PID", str(process.pid), "/T", "/F"],
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        return
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        return


def _tail(value: str, limit: int = 4000) -> str:
    if len(value) <= limit:
        return value
    return value[-limit:]


def _read_result(path: Path, scenario_id: str) -> tuple[dict[str, Any] | None, str | None]:
    if not path.is_file():
        return None, "runner did not write a result document"
    try:
        result = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        return None, f"invalid runner result JSON: {exc}"
    if not isinstance(result, dict):
        return None, "runner result must be a JSON object"
    if result.get("schema_version") != RESULT_SCHEMA_VERSION:
        return None, f"runner result schema_version must be {RESULT_SCHEMA_VERSION}"
    if result.get("scenario_id") != scenario_id:
        return None, "runner result scenario_id does not match the requested scenario"
    return result, None


def run_runner(
    command: str | Sequence[str],
    runner_name: str,
    scenario: Scenario,
    server: _FixtureServer,
    run_root: Path,
    timeout_seconds: float,
) -> dict[str, Any]:
    token = f"{scenario.scenario_id}-{runner_name}-{os.urandom(8).hex()}"
    server.begin_run(token, scenario.redirects)
    result_path = run_root / f"{runner_name}.json"
    storage_path = run_root / runner_name
    storage_path.mkdir(parents=True, exist_ok=True)
    substitutions = {
        "url": server.url_for(scenario.path, token),
        "result_file": str(result_path),
        "storage_dir": str(storage_path),
        "scenario_id": scenario.scenario_id,
        "status_url": server.status_url(token),
    }
    argv = _command_argv(command, substitutions)
    environment = os.environ.copy()
    environment.update(
        {
            "OPENBROWSER_COMPAT_SCENARIO": scenario.scenario_id,
            "OPENBROWSER_COMPAT_URL": substitutions["url"],
            "OPENBROWSER_COMPAT_RESULT_FILE": str(result_path),
            "OPENBROWSER_COMPAT_STORAGE_DIR": str(storage_path),
            "OPENBROWSER_COMPAT_STATUS_URL": substitutions["status_url"],
        }
    )

    creationflags = getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0) if os.name == "nt" else 0
    process = subprocess.Popen(
        argv,
        cwd=str(Path.cwd()),
        env=environment,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        start_new_session=os.name != "nt",
        creationflags=creationflags,
    )
    timed_out = False
    try:
        stdout, stderr = process.communicate(timeout=timeout_seconds)
    except subprocess.TimeoutExpired as exc:
        timed_out = True
        _terminate_process(process)
        stdout, stderr = process.communicate(timeout=5)
        stdout = (exc.stdout or "") + stdout
        stderr = (exc.stderr or "") + stderr

    requests = server.requests()
    result, result_error = _read_result(result_path, scenario.scenario_id)
    observed_paths = [item["path"] for item in requests]
    missing_paths = [path for path in scenario.expected_request_paths if path not in observed_paths]

    if timed_out:
        runner_status = "timeout"
    elif process.returncode != 0:
        runner_status = "runner-crash"
    elif result_error is not None:
        runner_status = "observation-error"
    elif missing_paths:
        runner_status = "fixture-not-observed"
    else:
        runner_status = "passed"

    return {
        "_token": token,
        "status": runner_status,
        "exit_code": process.returncode,
        "result": result,
        "result_error": result_error,
        "requests": requests,
        "missing_request_paths": missing_paths,
        "stdout": _tail(stdout),
        "stderr": _tail(stderr),
    }


def _normalize_string(value: str, origin: str, token: str = "") -> str:
    if token:
        value = value.replace(token, "<fixture-token>")
    if value.startswith(origin):
        parsed = urllib.parse.urlsplit(value)
        query = [item for item in urllib.parse.parse_qsl(parsed.query, keep_blank_values=True) if item[0] != "__ob_token"]
        suffix = ("?" + urllib.parse.urlencode(query)) if query else ""
        return f"<fixture-origin>{parsed.path}{suffix}{('#' + parsed.fragment) if parsed.fragment else ''}"
    return value


def normalize_json(value: Any, origin: str, token: str = "") -> Any:
    if isinstance(value, dict):
        return {
            key: normalize_json(item, origin, token)
            for key, item in sorted(value.items())
            if key.lower() not in IGNORED_RESULT_KEYS
        }
    if isinstance(value, list):
        return [normalize_json(item, origin, token) for item in value]
    if isinstance(value, str):
        return _normalize_string(value, origin, token)
    return value


def _value_at(value: Mapping[str, Any], field: str) -> Any:
    current: Any = value
    for part in field.split("."):
        if not isinstance(current, Mapping) or part not in current:
            return None
        current = current[part]
    return current


def _diff(left: Any, right: Any, path: str = "$") -> list[dict[str, Any]]:
    if type(left) is not type(right):
        return [{"path": path, "expected": left, "actual": right}]
    if isinstance(left, dict):
        differences: list[dict[str, Any]] = []
        for key in sorted(set(left) | set(right)):
            if key not in left:
                differences.append({"path": f"{path}.{key}", "expected": None, "actual": right[key]})
            elif key not in right:
                differences.append({"path": f"{path}.{key}", "expected": left[key], "actual": None})
            else:
                differences.extend(_diff(left[key], right[key], f"{path}.{key}"))
        return differences
    if isinstance(left, list):
        differences = []
        for index in range(max(len(left), len(right))):
            if index >= len(left):
                differences.append({"path": f"{path}[{index}]", "expected": None, "actual": right[index]})
            elif index >= len(right):
                differences.append({"path": f"{path}[{index}]", "expected": left[index], "actual": None})
            else:
                differences.extend(_diff(left[index], right[index], f"{path}[{index}]"))
        return differences
    if left != right:
        return [{"path": path, "expected": left, "actual": right}]
    return []


def compare_runner_results(
    scenario: Scenario,
    openbrowser: Mapping[str, Any],
    reference: Mapping[str, Any],
    origin: str,
) -> list[dict[str, Any]]:
    left_result = openbrowser.get("result") or {}
    right_result = reference.get("result") or {}
    left_normalized = normalize_json(left_result, origin, str(openbrowser.get("_token", "")))
    right_normalized = normalize_json(right_result, origin, str(reference.get("_token", "")))
    differences = []
    for field in scenario.compare_fields:
        differences.extend(
            _diff(
                _value_at(left_normalized, field),
                _value_at(right_normalized, field),
                f"$.{field}",
            )
        )
    differences.extend(_diff(openbrowser.get("requests"), reference.get("requests"), "$.requests"))
    return differences


def _public_runner_report(outcome: Mapping[str, Any], origin: str) -> dict[str, Any]:
    token = str(outcome.get("_token", ""))
    public = {key: value for key, value in outcome.items() if key != "_token"}
    if isinstance(public.get("result"), dict):
        public["result"] = normalize_json(public["result"], origin, token)
    for key in ("stdout", "stderr"):
        if isinstance(public.get(key), str) and token:
            public[key] = public[key].replace(token, "<fixture-token>")
    return public


def run_harness(
    manifest_path: Path,
    openbrowser_command: str | Sequence[str],
    reference_command: str | Sequence[str],
    output_path: Path,
    timeout_seconds: float = 30.0,
) -> dict[str, Any]:
    fixture_root, scenarios = load_manifest(manifest_path)
    all_redirects: dict[str, str] = {}
    for scenario in scenarios:
        all_redirects.update(scenario.redirects)

    server = _FixtureServer(fixture_root, all_redirects)
    try:
        report: dict[str, Any] = {
            "schema_version": RESULT_SCHEMA_VERSION,
            "harness": "openbrowser-web-compatibility-differential",
            "manifest": manifest_path.name,
            "scenarios": [],
            "summary": {
                "total": len(scenarios),
                "passed": 0,
                "incompatible": 0,
                "runner_failures": 0,
            },
        }
        with tempfile.TemporaryDirectory(prefix="openbrowser-compatibility-") as temp_dir:
            run_root = Path(temp_dir)
            for scenario in scenarios:
                scenario_root = run_root / scenario.scenario_id
                scenario_root.mkdir()
                openbrowser = run_runner(
                    openbrowser_command,
                    "openbrowser",
                    scenario,
                    server,
                    scenario_root,
                    timeout_seconds,
                )
                reference = run_runner(
                    reference_command,
                    "reference",
                    scenario,
                    server,
                    scenario_root,
                    timeout_seconds,
                )

                runner_statuses = {openbrowser["status"], reference["status"]}
                if runner_statuses != {"passed"}:
                    verdict = "runner-failure"
                    differences: list[dict[str, Any]] = []
                    report["summary"]["runner_failures"] += 1
                else:
                    differences = compare_runner_results(scenario, openbrowser, reference, server.origin)
                    verdict = "incompatible" if differences else "passed"
                    report["summary"]["incompatible" if differences else "passed"] += 1

                report["scenarios"].append(
                    {
                        "id": scenario.scenario_id,
                        "description": scenario.description,
                        "verdict": verdict,
                        "openbrowser": _public_runner_report(openbrowser, server.origin),
                        "reference": _public_runner_report(reference, server.origin),
                        "differences": differences,
                    }
                )
    finally:
        server.close()

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return report


def _parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--openbrowser-command", required=True)
    parser.add_argument("--reference-command", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--timeout-seconds", type=float, default=30.0)
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    try:
        args = _parse_args(argv if argv is not None else sys.argv[1:])
        report = run_harness(
            args.manifest,
            args.openbrowser_command,
            args.reference_command,
            args.output,
            args.timeout_seconds,
        )
    except HarnessError as exc:
        print(f"compatibility harness configuration error: {exc}", file=sys.stderr)
        return 2
    except OSError as exc:
        print(f"compatibility harness execution error: {exc}", file=sys.stderr)
        return 2

    summary = report["summary"]
    print(
        "Compatibility differential: "
        f"{summary['passed']} passed, "
        f"{summary['incompatible']} incompatible, "
        f"{summary['runner_failures']} runner failures"
    )
    return 0 if summary["incompatible"] == 0 and summary["runner_failures"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
