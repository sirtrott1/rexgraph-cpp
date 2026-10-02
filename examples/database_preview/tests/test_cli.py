#!/usr/bin/env python3
"""Fresh-process CLI round trips, non-overwrite, and real native query checks."""
from __future__ import annotations
import json, subprocess, sys, tempfile, struct, hashlib
from pathlib import Path
from fractions import Fraction

exe = str(Path(sys.argv[1]).resolve())
checks = 0

def run(*args: object, ok: bool = True) -> str:
    global checks
    p = subprocess.run([exe, *map(str, args)], capture_output=True, text=True, timeout=180)
    checks += 1
    assert (p.returncode == 0) == ok, (args, p.stdout, p.stderr)
    return p.stdout

def same(a: Path, b: Path) -> None:
    global checks
    checks += 1
    assert a.read_bytes() == b.read_bytes()

with tempfile.TemporaryDirectory(prefix="rfcdb-cli-") as root:
    p = Path(root)
    run("sample", p/"original.db", 9)
    run("pack", p/"original.db", p/"one.rfcxdb")
    run("verify", p/"one.rfcxdb")
    run("unpack", p/"one.rfcxdb", p/"again.db")
    run("pack", p/"again.db", p/"two.rfcxdb")
    same(p/"one.rfcxdb", p/"two.rfcxdb")
    run("export-series", p/"one.rfcxdb", p/"source.rfcxs")
    run("import-series", p/"source.rfcxs", p/"from-series.db")
    run("pack", p/"from-series.db", p/"three.rfcxdb")
    same(p/"one.rfcxdb", p/"three.rfcxdb")
    v = Fraction(0)
    for t in range(10):
        if t: v = (2*v+(1 if t <= 5 else -1))/3
        result = json.loads(run("field", p/"original.db", t, 10, 10))
        assert Fraction(result["value"]) == v
        checks += 1
    info = json.loads(run("inspect", p/"original.db"))
    assert info["last"] == 9 and info["aspects"] == 131
    checks += 1
    catalogue = run("sample-catalogue", p/"original.db").splitlines()
    assert len(catalogue) == 138 and sum(row.startswith("102,") and row.split(",")[2] == "10" for row in catalogue) == 2
    checks += 1
    branch = run("branch", p/"original.db", 9, 150).splitlines()
    assert len(branch) == 6 and all(len(row.split(",")[-1]) == 131 for row in branch[1:])
    checks += 1
    run("branch", p/"original.db", 9, 999, ok=False)
    place = run("placement", p/"original.db").splitlines()
    assert len(place) == info["components"]
    checks += 1
    run("pack", p/"original.db", p/"one.rfcxdb", ok=False)
    run("unpack", p/"one.rfcxdb", p/"original.db", ok=False)
    run("field", p/"original.db", 99, 10, 10, ok=False)
    run("field", p/"original.db", 0, 999, 10, ok=False)
    run("field", p/"original.db", "-1", 10, 10, ok=False)
    run("sample", p/"bad.db", 65, ok=False)
    (p/"link.rfcxdb").symlink_to(p/"one.rfcxdb")
    run("inspect", p/"link.rfcxdb", ok=False)
    run("pack", p/"original.db", p/"link.rfcxdb", ok=False)
    # Extract exact first frame for fresh-process append.
    packed = (p/"one.rfcxdb").read_bytes()
    msize = struct.unpack_from("<Q", packed, 24)[0]
    off, length = struct.unpack_from("<QQ", packed, 64+48+64*2+16)
    frame = packed[64+msize+off:64+msize+off+length]
    (p/"one.rfcxt").write_bytes(frame)
    run("sample", p/"zero.db", 0)
    run("append", p/"zero.db", p/"one.rfcxt", p/"appended.db")
    assert Fraction(json.loads(run("field", p/"appended.db", 1, 10, 10))["value"]) == Fraction(1, 3)
    checks += 1
    run("append", p/"appended.db", p/"one.rfcxt", p/"wrong.db", ok=False)
    (p/"truncated.rfcxt").write_bytes(frame[:-1])
    run("append", p/"zero.db", p/"truncated.rfcxt", p/"bad-append.db", ok=False)
    assert not (p/"bad-append.db").exists()
    checks += 1
    # Publication arbitration: exactly one process wins exclusive file creation.
    procs = [subprocess.Popen([exe, "pack", str(p/"zero.db"), str(p/"race.rfcxdb")], stdout=subprocess.PIPE, stderr=subprocess.PIPE) for _ in range(2)]
    codes=[]
    for proc in procs:
        proc.communicate(timeout=180);codes.append(proc.returncode)
    assert sorted(codes) == [0,1]
    checks += 1
    run("verify", p/"race.rfcxdb")
    # The owning field contribution consumes declarations retained in native
    # history. Its separate-process result never changes the packed publication.
    run("sample-action", p/"action.db", 1)
    run("pack", p/"action.db", p/"action-before.rfcxdb")
    first = json.loads(run("execute", p/"action.db", 0, 1, 9000001))
    later = json.loads(run("execute", p/"action-before.rfcxdb", 1, 1, 9000001))
    assert first["published"] is False and first["backend"] == "exact-cpu"
    assert {d["kind"] for d in first["decisions"]} == {"Declared", "Derived", "Profile"}
    assert len(first["inputs"]) == 6 and len(first["occurrences"]) == 145
    values = {row["cell"]: Fraction(row["values"][0]) for row in later["output"]}
    assert values[10] == Fraction(3, 8) and values[20] == -Fraction(3, 8)
    assert all(q == 0 for cell, q in values.items() if cell not in (10, 20))
    checks += 5
    run("execute", p/"action.db", 0, 1, 9000002, ok=False)
    run("execute", p/"action.db", 0, 1, 9000001, "required-gpu", ok=False)
    run("execute", p/"action.db", 0, 1, 9000001, "unknown", ok=False)
    fallback = json.loads(run("execute", p/"action.db", 0, 1, 9000001, "allow-cpu-fallback"))
    assert fallback["fallback"] and fallback["output"] == first["output"]
    checks += 1
    run("execute", p/"original.db", 0, 1, 9000001, ok=False)
    run("pack", p/"action.db", p/"action-after.rfcxdb")
    same(p/"action-before.rfcxdb", p/"action-after.rfcxdb")
    run("unpack", p/"action-before.rfcxdb", p/"action-copy.db")
    assert json.loads(run("execute", p/"action-copy.db", 1, 1, 9000001)) == later
    checks += 1
    run("export-series", p/"action.db", p/"action.rfcxs")
    run("import-series", p/"action.rfcxs", p/"action-import.db")
    assert json.loads(run("execute", p/"action-import.db", 0, 1, 9000001)) == first
    checks += 1
print(json.dumps({"cli_checks": checks, "separate_process_roundtrips": True, "exclusive_creator_race": True}))
