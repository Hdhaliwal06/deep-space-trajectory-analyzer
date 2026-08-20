#!/usr/bin/env python3
"""Local web UI for the C trajectory analyzer; standard library only."""
from __future__ import annotations
import json
import os
import subprocess
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build" / "voyager"
WEB_ROOT = ROOT / "web"


def server_port() -> int:
    """Return a valid local port, optionally supplied as PORT."""
    raw_port = os.environ.get("PORT", "8000")
    try:
        port = int(raw_port)
    except ValueError as exc:
        raise SystemExit(f"PORT must be a number, not {raw_port!r}.") from exc

    if not 1 <= port <= 65535:
        raise SystemExit("PORT must be between 1 and 65535.")
    return port

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
            return self.send_json(
                {"error": "Choose Voyager 1 or 2 and provide a date."},
                HTTPStatus.BAD_REQUEST,
            )

        if not BINARY.is_file():
            return self.send_json(
                {"error": "Analyzer is not built. Run `make` and restart the server."},
                HTTPStatus.SERVICE_UNAVAILABLE,
            )

        try:
            result = subprocess.run(
                [str(BINARY), spacecraft, date, "--json"],
                cwd=ROOT,
                text=True,
                capture_output=True,
                check=False,
                timeout=10,
            )
        except subprocess.TimeoutExpired:
            return self.send_json(
                {"error": "Analysis timed out. Please try again."},
                HTTPStatus.GATEWAY_TIMEOUT,
            )

        if result.returncode:
            return self.send_json({"error": result.stderr.strip() or "Analysis failed."}, HTTPStatus.BAD_REQUEST)

        try:
            payload = json.loads(result.stdout)
        except json.JSONDecodeError:
            return self.send_json(
                {"error": "Analyzer returned an invalid response."},
                HTTPStatus.INTERNAL_SERVER_ERROR,
            )
        return self.send_json(payload, HTTPStatus.OK)

    def send_json(self, payload, status):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

if __name__ == "__main__":
    port = server_port()
    try:
        server = ThreadingHTTPServer(("127.0.0.1", port), AppHandler)
    except OSError as exc:
        raise SystemExit(f"Could not start http://127.0.0.1:{port}: {exc}") from exc

    print(f"Open http://127.0.0.1:{port}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nServer stopped.")
    finally:
        server.server_close()
