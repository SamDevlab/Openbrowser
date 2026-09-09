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


TITLES = {
    "/navigation-basic.html": "Openbrowser compatibility navigation",
    "/redirect.html": "Openbrowser compatibility redirect target",
    "/javascript-storage.html": "Openbrowser compatibility JavaScript",
    "/hash-navigation.html": "Openbrowser compatibility hash navigation",
    "/history-state.html": "Openbrowser compatibility history state",
    "/fetch-json.html": "Openbrowser compatibility fetch JSON",
    "/external-script.html": "Openbrowser compatibility external script",
    "/cookie-basic.html": "Openbrowser compatibility cookie",
    "/storage-lifecycle.html": "Openbrowser compatibility storage lifecycle",
    "/dom-apis.html": "Openbrowser compatibility DOM APIs",
}

EXTRA_REQUESTS = {
    "/fetch-json.html": ("/compatibility-data.json",),
    "/external-script.html": ("/compatibility-external.js",),
}

CUSTOM_EVENTS = {
    "/hash-navigation.html": ["hash_changed"],
    "/history-state.html": ["history_replaced"],
    "/fetch-json.html": ["xhr_complete"],
    "/external-script.html": ["external_script_loaded"],
    "/cookie-basic.html": ["cookie_written"],
    "/storage-lifecycle.html": ["storage_mutated"],
    "/dom-apis.html": ["dom_mutated"],
}


def main() -> int:
    mode = sys.argv[1]
    if mode == "crash":
        return 17
    if mode == "timeout":
        time.sleep(10)
        return 0

    url = os.environ["OPENBROWSER_COMPAT_URL"]
    parsed = urlsplit(url)
    with urllib.request.urlopen(url, timeout=5) as response:
        response.read()

    origin = f"{parsed.scheme}://{parsed.netloc}"
    for path in EXTRA_REQUESTS.get(parsed.path, ()):
        with urllib.request.urlopen(origin + path, timeout=5) as response:
            response.read()

    path = parsed.path
    final_path = "/redirect-final.html" if path == "/redirect.html" else path
    events = ["navigation_committed", *CUSTOM_EVENTS.get(path, []), "fixture_ready"]
    result = {
        "schema_version": 1,
        "scenario_id": os.environ["OPENBROWSER_COMPAT_SCENARIO"],
        "final_url": f"http://127.0.0.1:1{final_path}?__ob_token=ignored",
        "title": TITLES[path],
        "dom_markers": {"fixture": path.removeprefix("/").removesuffix(".html")},
        "events": events,
        "storage": (
            {"openbrowser-compatibility": "stable-value"}
            if path == "/javascript-storage.html"
            else ({"beta": "2"} if path == "/storage-lifecycle.html" else {})
        ),
        "session_storage": {"gamma": "3"} if path == "/storage-lifecycle.html" else {},
        "cookies": ["openbrowser_compatibility=stable-cookie"] if path == "/cookie-basic.html" else [],
    }
    if mode == "different":
        result["title"] = "Reference-only title"
    Path(os.environ["OPENBROWSER_COMPAT_RESULT_FILE"]).write_text(
        json.dumps(result), encoding="utf-8"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
