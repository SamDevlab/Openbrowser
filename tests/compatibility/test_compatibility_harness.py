#!/usr/bin/env python3

from __future__ import annotations

import json
import sys
import tempfile
import unittest
import urllib.request
from pathlib import Path


sys.dont_write_bytecode = True
REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT / "scripts"))

from compatibility_harness import run_harness  # noqa: E402
from compatibility_observer import ObservationServer  # noqa: E402


class CompatibilityHarnessTests(unittest.TestCase):
    manifest = REPOSITORY_ROOT / "tests" / "compatibility" / "fixtures" / "manifest.json"
    driver = REPOSITORY_ROOT / "tests" / "compatibility" / "fake_compatibility_runner.py"

    def command(self, mode: str) -> list[str]:
        return [sys.executable, str(self.driver), mode]

    def run_case(self, reference_mode: str = "same", timeout_seconds: float = 5.0) -> dict:
        with tempfile.TemporaryDirectory(prefix="openbrowser-harness-test-") as directory:
            output = Path(directory) / "report.json"
            report = run_harness(
                self.manifest,
                self.command("same"),
                self.command(reference_mode),
                output,
                timeout_seconds,
            )
            self.assertTrue(output.is_file())
            self.assertEqual(report, json.loads(output.read_text(encoding="utf-8")))
            return report

    def test_local_fixture_differential_pass(self) -> None:
        report = self.run_case()
        self.assertEqual(report["summary"], {"total": 3, "passed": 3, "incompatible": 0, "runner_failures": 0})
        self.assertTrue(all(item["verdict"] == "passed" for item in report["scenarios"]))
        self.assertTrue(
            all("_token" not in runner for item in report["scenarios"] for runner in (item["openbrowser"], item["reference"]))
        )

    def test_observation_difference_is_not_a_runner_failure(self) -> None:
        report = self.run_case(reference_mode="different")
        self.assertEqual(report["summary"]["passed"], 0)
        self.assertEqual(report["summary"]["incompatible"], 3)
        self.assertEqual(report["summary"]["runner_failures"], 0)
        self.assertTrue(all(item["verdict"] == "incompatible" for item in report["scenarios"]))

    def test_runner_crash_is_classified_separately(self) -> None:
        report = self.run_case(reference_mode="crash")
        self.assertEqual(report["summary"]["runner_failures"], 3)
        self.assertTrue(all(item["verdict"] == "runner-failure" for item in report["scenarios"]))
        self.assertTrue(all(item["reference"]["status"] == "runner-crash" for item in report["scenarios"]))

    def test_runner_timeout_is_classified_and_cleaned_up(self) -> None:
        report = self.run_case(reference_mode="timeout", timeout_seconds=0.25)
        self.assertEqual(report["summary"]["runner_failures"], 3)
        self.assertTrue(all(item["reference"]["status"] == "timeout" for item in report["scenarios"]))

    def test_page_observation_collector_round_trip(self) -> None:
        payload = {
            "schema_version": 1,
            "scenario_id": "navigation-basic",
            "final_url": "http://127.0.0.1/navigation-basic.html",
            "title": "Openbrowser compatibility navigation",
            "dom_markers": {
                "compatibility-navigation": {
                    "marker": "navigation-basic",
                    "text": "Navigation fixture loaded.",
                }
            },
            "events": ["navigation_committed", "fixture_ready"],
            "storage": {},
        }
        with ObservationServer("navigation-basic") as observer:
            request = urllib.request.Request(
                observer.url,
                data=json.dumps(payload).encode("utf-8"),
                method="POST",
                headers={"Content-Type": "text/plain;charset=UTF-8"},
            )
            with urllib.request.urlopen(request, timeout=2) as response:
                self.assertEqual(response.status, 204)
            self.assertEqual(observer.wait(1.0), payload)


if __name__ == "__main__":
    raise SystemExit(unittest.main())
