#!/usr/bin/env python3
"""Local web UI for the C trajectory analyzer; standard library only."""
from __future__ import annotations
import json
import subprocess
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build" / "voyager"
WEB_ROOT = ROOT / "web"

class AppHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(WEB_ROOT), **kwargs)
    def do_GET(self):
        request = urlparse(self.path)
        if request.path != "/api/analyze":
            return super().do_GET()
        query = parse_qs(request.query)
        spacecraft = query.get("spacecraft", [""])[0]
        date = query.get("date", [""])[0]
        if spacecraft not in {"voyager1", "voyager2"} or len(date) > 20:
            return self.send_json({"error": "Choose Voyager 1 or 2 and provide a date."}, HTTPStatus.BAD_REQUEST)
        result = subprocess.run([str(BINARY), spacecraft, date, "--json"], cwd=ROOT, text=True, capture_output=True, check=False, timeout=10)
        if result.returncode:
            return self.send_json({"error": result.stderr.strip() or "Analysis failed."}, HTTPStatus.BAD_REQUEST)
        return self.send_json(json.loads(result.stdout), HTTPStatus.OK)
    def send_json(self, payload, status):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

if __name__ == "__main__":
    server = ThreadingHTTPServer(("127.0.0.1", 8000), AppHandler)
    print("Open http://127.0.0.1:8000")
    server.serve_forever()
