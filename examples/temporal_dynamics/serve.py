#!/usr/bin/env python3
"""Serve one generated presentation and bounded native queries on loopback."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import re
import secrets
import subprocess
import threading
from urllib.parse import unquote, urlsplit


def serve(directory, exe, port=0):
    root, exe = Path(directory).resolve(), Path(exe).resolve()
    if not (root/"manifest.json").is_file() or not exe.is_file():
        raise ValueError("Serve requires a completed demonstration and its native consumer")
    token = secrets.token_hex(24)
    slots = threading.BoundedSemaphore(2)
    tail = max(x["sequence"] for x in json.loads((root/"observations.json").read_text()) if x["chapter"] == "database")

    class Handler(SimpleHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def reply(self, status, value):
            data = json.dumps(value).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        def safe_path(self):
            try:
                path = root/unquote(urlsplit(self.path).path).lstrip("/")
                return path.resolve().is_relative_to(root) and not any(p.is_symlink() for p in [path, *path.parents] if p.is_relative_to(root))
            except (OSError, ValueError):
                return False

        def end_headers(self):
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'")
            super().end_headers()

        def do_GET(self):
            if self.path == "/api/status":
                self.reply(200, {"native_queries": True, "token": token})
            elif self.safe_path():
                super().do_GET()
            else:
                self.reply(403, {"error": "Outside presentation root"})

        def do_HEAD(self):
            if self.safe_path():
                super().do_HEAD()
            else:
                self.reply(403, {"error": "Outside presentation root"})

        def do_POST(self):
            origin = f"http://127.0.0.1:{self.server.server_port}"
            if self.path != "/api/query" or self.headers.get("Origin") != origin or self.headers.get("X-RFC-Demo-Token") != token:
                self.reply(403, {"error": "Query requires the local presentation origin"})
                return
            acquired = False
            try:
                if self.headers.get_content_type() != "application/json":
                    raise ValueError("Query requires application/json")
                length = int(self.headers.get("Content-Length", "0"))
                if length < 1 or length > 4096:
                    raise ValueError("Query request size")
                query = json.loads(self.rfile.read(length))
                cutoff = query["cutoff"]
                if type(cutoff) is not int or cutoff < 0 or cutoff > tail:
                    raise ValueError("Publication cutoff is outside this presentation")
                operands = [query["start"], query["end"]]
                if not all(isinstance(x, str) and len(x) <= 256 and re.fullmatch(r"-?\d+(?:/\d+)?", x) for x in operands):
                    raise ValueError("Use exact rational interval endpoints")
                snapshot = root/"database"/f"publication-{cutoff}.rfcxi"
                if snapshot.is_symlink() or not snapshot.resolve().is_relative_to(root):
                    raise ValueError("Snapshot is outside the presentation")
                acquired = slots.acquire(blocking=False)
                if not acquired:
                    self.reply(429, {"error": "Two native queries are already running"})
                    return
                result = subprocess.run([str(exe), "query", str(snapshot), str(cutoff), *operands, "--exact"],
                                        text=True, capture_output=True, timeout=15)
                if result.returncode:
                    raise ValueError(result.stderr.strip())
                self.reply(200, json.loads(result.stdout))
            except (KeyError, ValueError, TypeError, OSError, subprocess.TimeoutExpired) as error:
                self.reply(400, {"error": str(error)})
            finally:
                if acquired:
                    slots.release()

    server = ThreadingHTTPServer(("127.0.0.1", port), partial(Handler, directory=str(root)))
    print(f"Presentation with native queries: http://127.0.0.1:{server.server_port}/", flush=True)
    print("Press Ctrl+C to close the local presentation server.", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("directory", type=Path)
    p.add_argument("--exe", type=Path, required=True)
    p.add_argument("--port", type=int, default=0)
    a = p.parse_args()
    serve(a.directory, a.exe, a.port)
