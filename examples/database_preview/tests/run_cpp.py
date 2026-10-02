#!/usr/bin/env python3
import subprocess, tempfile, sys
from pathlib import Path
with tempfile.TemporaryDirectory(prefix="rfcdb-cpp-") as root:
    result = subprocess.run([sys.argv[1], str(Path(root)/"fixtures")], timeout=240)
    raise SystemExit(result.returncode)
