#!/usr/bin/env python3
"""Local observation collector for deterministic compatibility fixtures."""

from __future__ import annotations

import argparse
import json
import secrets
import threading
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Sequence


RESULT_SCHEMA_VERSION = 1
MAX_OBSERVATION_BYTES = 64 * 1024


class _ObservationHandler(BaseHTTPRequestHandler):
    server: "_ObservationHttpServer"

    def _cors_headers(self) -> None:
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cache-Control", "no-store")

    def do_OPTIONS(self) -> None:  # noqa: N802 - stdlib handler API
        self.send_response(204)
        self._cors_headers()
        self.send_header("Access-Control-Allow-Methods", "POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def do_POST(self) -> None:  # noqa: N802 - stdlib handler API
        parsed = urllib.parse.urlsplit(self.path)
        if parsed.path != "/observe":
            self.send_error(404, "Observation endpoint not found")
            return

        token = urllib.parse.parse_qs(parsed.query).get("token", [""])[0]
        if token != self.server.token:
            self.send_response(403)
            self._cors_headers()
            self.end_headers()
            return

        try:
            content_length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            content_length = 0
        if content_length <= 0 or content_length > MAX_OBSERVATION_BYTES:
            self.send_response(413)
            self._cors_headers()
            self.end_headers()
            return

        try:
            raw = self.rfile.read(content_length).decode("utf-8")
            observation = json.loads(raw)
        except (UnicodeDecodeError, json.JSONDecodeError):
            self.send_response(400)
            self._cors_headers()
            self.end_headers()
            return

        if (
            not isinstance(observation, dict)
            or observation.get("schema_version") != RESULT_SCHEMA_VERSION
            or observation.get("scenario_id") != self.server.expected_scenario
        ):
            self.send_response(422)
            self._cors_headers()
            self.end_headers()
            return

        if not self.server.accept_observation(observation):
            self.send_response(409)
            self._cors_headers()
            self.end_headers()
            return

        self.send_response(204)
        self._cors_headers()
        self.end_headers()

    def log_message(self, _format: str, *_args: object) -> None:
        return


class _ObservationHttpServer(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, expected_scenario: str, token: str) -> None:
        super().__init__(("127.0.0.1", 0), _ObservationHandler)
        self.expected_scenario = expected_scenario
        self.token = token
        self._lock = threading.Lock()
        self._event = threading.Event()
        self._observation: dict[str, Any] | None = None

    def accept_observation(self, observation: dict[str, Any]) -> bool:
        with self._lock:
            if self._observation is not None:
                return False
            self._observation = json.loads(json.dumps(observation))
            self._event.set()
            return True

    def wait(self, timeout_seconds: float) -> dict[str, Any] | None:
        if not self._event.wait(timeout_seconds):
            return None
        with self._lock:
            if self._observation is None:
                return None
            return json.loads(json.dumps(self._observation))


class ObservationServer:
    """Single-observation localhost server with an unguessable per-run endpoint."""

    def __init__(self, scenario_id: str) -> None:
        if not scenario_id:
            raise ValueError("scenario_id must not be empty")
        self._token = secrets.token_urlsafe(24)
        self._server = _ObservationHttpServer(scenario_id, self._token)
        self._thread = threading.Thread(
            target=self._server.serve_forever,
            name="compatibility-observer",
            daemon=True,
        )
        self._thread.start()

    @property
    def url(self) -> str:
        host, port = self._server.server_address
        token = urllib.parse.urlencode({"token": self._token})
        return f"http://{host}:{port}/observe?{token}"

    def wait(self, timeout_seconds: float = 30.0) -> dict[str, Any] | None:
        return self._server.wait(timeout_seconds)

    def close(self) -> None:
        self._server.shutdown()
        self._server.server_close()
        self._thread.join(timeout=5)

    def __enter__(self) -> "ObservationServer":
        return self

    def __exit__(self, *_args: object) -> None:
        self.close()


def _parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario-id", required=True)
    parser.add_argument("--url-file", type=Path, required=True)
    parser.add_argument("--result-file", type=Path, required=True)
    parser.add_argument("--timeout-seconds", type=float, default=30.0)
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = _parse_args(argv if argv is not None else __import__("sys").argv[1:])
    args.url_file.parent.mkdir(parents=True, exist_ok=True)
    args.result_file.parent.mkdir(parents=True, exist_ok=True)

    with ObservationServer(args.scenario_id) as observer:
        args.url_file.write_text(observer.url + "\n", encoding="utf-8")
        observation = observer.wait(args.timeout_seconds)
        if observation is None:
            print(
                "compatibility observer timed out waiting for page observation",
                file=__import__("sys").stderr,
            )
            return 3
        args.result_file.write_text(
            json.dumps(observation, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
