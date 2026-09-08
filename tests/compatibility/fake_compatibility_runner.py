#!/usr/bin/env python3
"""Small deterministic runner used to test the harness protocol itself."""

from __future__ import annotations

import json
import os
import sys
import time
import urllib.request
from pathlib import Path
from urllib.parse import urlsplit


def main() -> int:
    mode = sys.argv[1]
    if mode == "crash":
        return 17
    if mode == "timeout":
        time.sleep(10)
        return 0

    url = os.environ["OPENBROWSER_COMPAT_URL"]
    with urllib.request.urlopen(url, timeout=5) as response:
        response.read()

    path = urlsplit(url).path
    final_path = "/redirect-final.html" if path == "/redirect.html" else path
    result = {
        "schema_version": 1,
        "scenario_id": os.environ["OPENBROWSER_COMPAT_SCENARIO"],
        "final_url": f"http://127.0.0.1:1{final_path}?__ob_token=ignored",
        "title": {
            "/navigation-basic.html": "Openbrowser compatibility navigation",
            "/redirect.html": "Openbrowser compatibility redirect target",
            "/javascript-storage.html": "Openbrowser compatibility JavaScript",
        }[path],
        "dom_markers": {"fixture": path.removeprefix("/").removesuffix(".html")},
        "events": ["navigation_committed", "fixture_ready"],
        "storage": {"openbrowser-compatibility": "stable-value"} if path == "/javascript-storage.html" else {},
    }
    if mode == "different":
        result["title"] = "Reference-only title"
    Path(os.environ["OPENBROWSER_COMPAT_RESULT_FILE"]).write_text(
        json.dumps(result), encoding="utf-8"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
