import importlib.util
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "engine_security_monitor.py"
SPEC = importlib.util.spec_from_file_location("engine_security_monitor", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
monitor = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(monitor)


class EngineSecurityMonitorTests(unittest.TestCase):
    def test_reads_pinned_cef_and_chromium(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "OpenbrowserCEF.cmake"
            path.write_text(
                'set(OPENBROWSER_CEF_VERSION "151.3.17+gf059e67+chromium-151.0.7922.138")\n',
                encoding="utf-8",
            )
            self.assertEqual(
                monitor.read_pinned_engine(path),
                (
                    "151.3.17+gf059e67+chromium-151.0.7922.138",
                    "151.0.7922.138",
                ),
            )

    def test_latest_stable_cef_ignores_newer_beta(self):
        index = {
            "windows64": {
                "versions": [
                    {
                        "cef_version": "151.3.17+gf059e67+chromium-151.0.7922.138",
                        "channel": "stable",
                        "chromium_version": "151.0.7922.138",
                    },
                    {
                        "cef_version": "152.0.4+g6d60deb+chromium-152.0.7977.42",
                        "channel": "beta",
                        "chromium_version": "152.0.7977.42",
                    },
                ]
            }
        }
        self.assertEqual(
            monitor.latest_stable_cef(index, "windows64"),
            (
                "151.3.17+gf059e67+chromium-151.0.7922.138",
                "151.0.7922.138",
            ),
        )

    def test_chrome_ahead_without_new_cef_is_watch_signal(self):
        report = monitor.build_report(
            "151.3.17+gf059e67+chromium-151.0.7922.138",
            "151.0.7922.138",
            "151.3.17+gf059e67+chromium-151.0.7922.138",
            "151.0.7922.138",
            "153.0.8010.36",
            "windows64",
        )
        self.assertFalse(report["signals"]["cef_update_available"])
        self.assertTrue(
            report["signals"]["chrome_stable_ahead_of_embedded_chromium"]
        )
        self.assertEqual(report["recommendation"], "watch-upstream-cef")

    def test_new_stable_cef_requires_upgrade_review(self):
        report = monitor.build_report(
            "151.3.17+gf059e67+chromium-151.0.7922.138",
            "151.0.7922.138",
            "152.0.8+g1234567+chromium-152.0.7977.82",
            "152.0.7977.82",
            "153.0.8010.36",
            "windows64",
        )
        self.assertTrue(report["signals"]["cef_update_available"])
        self.assertEqual(report["recommendation"], "review-and-upgrade-cef")


if __name__ == "__main__":
    unittest.main()
