#!/usr/bin/env python3
"""Real local series CLI workflow, exact data and no-overwrite/failure checks."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def verify(binary: Path, demo: Path, root: Path):
    checks = 0

    def check(ok, message):
        nonlocal checks
        checks += 1
        if not ok:
            raise AssertionError(message)

    def run(*args, ok=True):
        p = subprocess.run([str(binary), *map(str, args)], text=True, capture_output=True)
        check((p.returncode == 0) == ok, f"{args}: {p.stderr}")
        if ok:
            return json.loads(p.stdout)
        check(not p.stdout.strip(), "error emitted successful output")
        return None

    fixture = root / "demo"
    demonstration = subprocess.run([str(demo), str(fixture)], capture_output=True, text=True)
    check(demonstration.returncode == 0, f"demonstration failed: {demonstration.stderr}")
    base, history, checkpoint, compact = [root / (name+".rfcxs") for name in ("base", "history", "checkpoint", "compact")]
    run("series-create", fixture/"base.rfcx", fixture/"base.rfcxe", base, 700)
    run("series-append", base, fixture/"history.rfcxt", history)
    check(run("series-inspect", history)["segments"] == "2", "frame count")
    check(run("series-at", history, 42)["replayed_segments"] == "2", "base replay count")
    result = run("series-field", history, 40, 42, 10)
    check(result == {"carrier": "1", "transported_initial": ["4", "-1"],
                     "propagated_innovations": [["5", "-4"], ["4", "-2"]], "terminal": ["13", "-7"]}, "exact channels")
    run("series-checkpoint", history, 41, checkpoint)
    check(run("series-at", checkpoint, 42)["replayed_segments"] == "1", "checkpoint not used")
    check(run("series-at", checkpoint, 41)["used_checkpoint"], "checkpoint endpoint")
    run("series-compact", checkpoint, compact)
    check(compact.read_bytes() == history.read_bytes(), "canonical compaction rewrote history")
    check(run("series-field", compact, 40, 42, 10) == result, "compaction changed math")
    saved = history.read_bytes()
    run("series-append", base, fixture/"history.rfcxt", history, ok=False)
    check(history.read_bytes() == saved, "overwrote input")
    run("series-checkpoint", history, 43, root/"bad.rfcxs", ok=False)
    check(not (root/"bad.rfcxs").exists(), "failed query created output")
    run("series-at", history, 39, ok=False)
    run("series-at", history, -1, ok=False)
    run("series-field", history, 42, 40, 10, ok=False)
    run("series-field", history, 40, 42, 20, ok=False)
    run("series-compact", history, root/"dup.rfcxs", 41, 41, ok=False)
    short = root/"short.rfcxt"
    short.write_bytes((fixture/"history.rfcxt").read_bytes()[:-1])
    run("series-append", base, short, root/"prefix.rfcxs", ok=False)
    check(not (root/"prefix.rfcxs").exists(), "truncated history published prefix")
    run("series-append", history, fixture/"history.rfcxt", root/"wrong.rfcxs", ok=False)
    check(not (root/"wrong.rfcxs").exists(), "wrong predecessor wrote output")
    run("series-create", fixture/"base.rfcx", fixture/"base.rfcxe", root/"ns.rfcxs", 0, ok=False)
    target = root/"symlink.rfcxs"
    target.symlink_to(history)
    run("series-checkpoint", history, 41, target, ok=False)
    check(history.read_bytes() == saved, "symlink changed existing file")
    check(run("doctor")["cpp"] == 20, "tool C++ version")
    return {"status": "passed", "checks": checks, "scope": "series commands, exact paper example, replay/compaction and logical failure/no-overwrite"}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("binary", type=Path)
    p.add_argument("demo", type=Path)
    p.add_argument("--work", type=Path)
    p.add_argument("--report", type=Path)
    a = p.parse_args()
    if a.work:
        a.work.mkdir(parents=True, exist_ok=False)
        result = verify(a.binary.resolve(), a.demo.resolve(), a.work)
    else:
        with tempfile.TemporaryDirectory(prefix="rfc-series-cli-") as d:
            result = verify(a.binary.resolve(), a.demo.resolve(), Path(d))
    text = json.dumps(result, indent=2)
    if a.report:
        a.report.write_text(text+"\n")
    print(text)


if __name__ == "__main__":
    main()
