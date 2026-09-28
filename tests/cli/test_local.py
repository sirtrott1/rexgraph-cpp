#!/usr/bin/env python3
"""End-to-end local preview checks. Uses only Python's standard library."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import tempfile


def run_checks(binary: Path, root: Path) -> dict:
    checks = 0
    def check(condition, message):
        nonlocal checks
        checks += 1
        if not condition:
            raise AssertionError(message)
    def run(*args, ok=True):
        result = subprocess.run([str(binary), *map(str, args)], capture_output=True, text=True, timeout=120)
        check((result.returncode == 0) == ok, f"{args}: rc={result.returncode}, {result.stderr}")
        if ok:
            return json.loads(result.stdout)
        check(not result.stdout, "failed command emitted a partial JSON publication")
        return result.stderr
    info = run("doctor")
    check(info["network"] is False, "unexpected network requirement")
    check("rfc_local" in subprocess.check_output([str(binary), "--help"], text=True), "missing help")
    run("unknown-command", ok=False)
    base, target, delta, replay = [root / name for name in ("base.rfcxi", "target.rfcxi", "delta.rfcxiu", "replay.rfcxi")]
    b = run("sample", base)
    t = run("sample", target, "changed")
    check(b["retained_occurrences"] == "4" and t["retained_occurrences"] == "6", "sample source counts")
    check(run("sum", base, 10, 1)["exact_sum"] == "3", "sample exact sum")
    check(run("sum", target, 10, 1)["exact_sum"] == "3/2", "sample changed sum")
    rows = run("show", target, 10)
    check(len(rows["assertions"]) == 3, "retained duplicate/missing assertions")
    check(run("select", target, 10, 1, "null")["native_keys"] == ["3"], "null is not zero")
    check(run("select", target, 10, 1, "eq", "0")["native_keys"] == ["1"], "zero equality")
    check(run("select", target, 10, 1, "gt", "1")["native_keys"] == ["2"], "exact predicate")
    run("diff", base, target, delta)
    run("replay", base, delta, replay)
    check(replay.read_bytes() == target.read_bytes(), "disk-only replay not byte exact")
    saved = base.read_bytes()
    for args in [("sample", base), ("replay", base, delta, base), ("diff", base, target, base)]:
        run(*args, ok=False)
        check(base.read_bytes() == saved, "existing input changed")
    for args in [("select", base, 10, 1, "eq", "1/0"), ("select", base, 10, 1, "eq", "1/2/3"),
                 ("select", base, 10, 1, "eq", "+1"), ("select", base, 10, "1/", "eq", "1"),
                 ("select", base, 10, 1, "null", "0"), ("show", base, 10, 1001),
                 ("sum", base, 10, "999"), ("sum", base, "-1", 1), ("sum", base, "18446744073709551616", 1),
                 ("sample", root / "bad.rfcxi", "typo"), ("replay", target, delta, root / "bad.rfcxi")]:
        run(*args, ok=False)
    check(not (root / "bad.rfcxi").exists(), "failed computation published output")
    symlink = root / "symlink.rfcxi"
    if hasattr(os, "symlink"):
        symlink.symlink_to(base)
        run("sample", symlink, ok=False)
        check(base.read_bytes() == saved, "symlink overwrite")
    # True O_EXCL race, not two sequential existence checks.
    collision = root / "race.rfcxi"
    commands = [[str(binary), "sample", str(collision)] for _ in range(2)]
    procs = [subprocess.Popen(c, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) for c in commands]
    results = [p.communicate(timeout=120) for p in procs]
    check(sorted(p.returncode for p in procs) == [0, 1], "concurrent create did not pick exactly one writer")
    check(collision.read_bytes() == base.read_bytes(), "concurrent writer corrupted snapshot")
    broken = root / "broken.rfcxi"
    for n in (0, 1, 7, 31, len(saved) - 1):
        broken.write_bytes(saved[:n])
        run("inspect", broken, ok=False)
    if os.name == "posix":
        check(base.stat().st_mode & 0o077 == 0, "new archive has group/world permissions")
    if os.name == "posix":
        import resource
        def constrain_output():
            resource.setrlimit(resource.RLIMIT_FSIZE, (64, 64))
        interrupted = root / "incomplete.rfcxi"
        failure = subprocess.run([str(binary), "sample", str(interrupted)], capture_output=True,
                                 timeout=120, preexec_fn=constrain_output)
        check(failure.returncode != 0 and not failure.stdout, "limited write reported success")
        check(interrupted.exists() and interrupted.stat().st_size <= 64, "physical failure fixture")
        run("inspect", interrupted, ok=False)
        check(base.read_bytes() == saved, "physical write failure changed input")
    constructors = Path(__file__).resolve().parents[2] / "data/fixtures/local_constructors.rfcxi"
    constructor_bytes = constructors.read_bytes()
    shape = run("show", constructors, 10)["assertions"][0]
    contents = shape["payload"]["product"]
    check(contents[0] == {"boolean": True}, "Boolean projection")
    check(contents[1] == {"symbol": "9"}, "finite identity projection")
    check(contents[2]["integer"] == "-999999999999999999999999999999999999999", "large integer rounded")
    check(contents[3] == {"rational": "17/23"}, "rational projection")
    check(contents[4] == {"optional": None}, "optional absence")
    check(contents[5]["repeated"] == [{"optional": None}, {"optional": {"rational": "0"}}], "repeated unknown versus zero")
    check(contents[6]["alternative"] == "20" and contents[6]["value"] == contents[5], "alternative identity/order")
    check(contents[7] == {"tensor": {"shape": ["65536"], "support": ["7"], "values": [["7", "1/7"]]}}, "tensor shape/support/value conflation")
    check(len(shape["roles"][0]["occurrences"]) == 2 and len(shape["roles"][1]["occurrences"]) == 3, "collective branch expanded")
    check(shape["roles"][0]["occurrences"][1]["active"] is False, "inactive occurrence lost")
    check(constructors.read_bytes() == constructor_bytes, "frozen constructor archive changed")
    sqlite_comparisons = 0
    if info["sqlite"]:
        path = root / "input.sqlite"
        db = sqlite3.connect(path)
        db.executescript("CREATE TABLE people(id INTEGER PRIMARY KEY, name TEXT); CREATE TABLE events(id INTEGER PRIMARY KEY, sender INTEGER NOT NULL, recipient INTEGER, amount INTEGER, note TEXT, bytes BLOB);")
        db.executemany("INSERT INTO people VALUES(?,?)", [(1,"alice"),(2,"bob")])
        db.executemany("INSERT INTO events VALUES(?,?,?,?,?,?)", [(1,1,2,7,"same",b"\x00\xff"),(2,1,2,7,"same",b"\x00\xff"),(3,2,None,0,"\x00",b""),(4,1,2,None,None,None)])
        db.commit(); db.close()
        original = path.read_bytes()
        args = ["--namespace", "900", "--revision", "revision/0", "--subject", "1", "people", "id", "--assertion", "10", "events", "id", "--reference", "10", "11", "1", "sender", "--reference", "10", "12", "1", "recipient", "--assertion", "20", "people", "id"]
        archive = root / "imported.rfcxi"
        metadata = run("import-sqlite", path, archive, *args)
        check(path.read_bytes() == original, "SQLite input changed")
        check(run("sum", archive, 10, 1)["exact_sum"] == "14", "SQL import sum")
        imported = run("show", archive, 10)
        check(len(imported["assertions"]) == 4, "duplicate assertions merged")
        values = [x["payload"]["product"] for x in imported["assertions"]]
        check(values[0][1] == {"optional":{"repeated":[{"symbol":"0"},{"symbol":"255"}]}}, "blob octets lost")
        check(values[2][1] == {"optional":{"repeated":[]}}, "empty blob differs from null")
        check(values[3][1] == {"optional":None}, "null blob")
        predicates = [("eq","=",0),("eq","=",7),("lt","<",7),("le","<=",7),("ge",">=",0),("ne","!=",0)]
        db = sqlite3.connect(path)
        for native, sql, x in predicates:
            expected = db.execute(f"SELECT count(*) FROM events WHERE amount {sql} ?", (x,)).fetchone()[0]
            got = run("select", archive, 10, 1, native, x)
            check(len(got["native_keys"]) == expected, "SQLite predicate mismatch"); sqlite_comparisons += 1
        db.execute("DELETE FROM events WHERE id=1")
        db.execute("UPDATE events SET amount=20 WHERE id=2")
        db.execute("INSERT INTO events VALUES(5,2,1,9,'new',X'')")
        db.commit(); db.close()
        args[3] = "revision/1"
        refreshed = root / "refreshed.rfcxi"
        run("refresh-sqlite", archive, path, refreshed, *args)
        check(run("sum", refreshed, 10, 1)["exact_sum"] == "29", "refresh scalar value")
        changed = run("show", refreshed, 10)
        check([a["native_key"] for a in changed["assertions"]] == ["2","3","4","7"], "lineage/tombstone reuse")
        sql_delta, sql_replay = root / "sqlite.rfcxiu", root / "sqlite-replay.rfcxi"
        run("diff", archive, refreshed, sql_delta)
        run("replay", archive, sql_delta, sql_replay)
        check(sql_replay.read_bytes() == refreshed.read_bytes(), "SQL temporal replay differs")
        db = sqlite3.connect(path);db.execute("ALTER TABLE events ADD COLUMN extra INTEGER");db.commit();db.close()
        run("refresh-sqlite", refreshed, path, root/"no-schema-guess.rfcxi", *args, ok=False)
        check(not (root/"no-schema-guess.rfcxi").exists(), "implicit schema migration")
        run("import-sqlite", path, root/"missing-plan.rfcxi", "--namespace", 900, ok=False)
    else:
        run("import-sqlite", root/"absent.sqlite", root/"no.rfcxi", ok=False)
    fixtures = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in root.iterdir() if p.suffix in (".rfcxi", ".rfcxiu") and p.name not in ("broken.rfcxi","symlink.rfcxi","incomplete.rfcxi")}
    return dict(status="passed", checks=checks, sqlite=info["sqlite"], sqlite_comparisons=sqlite_comparisons, fixtures=fixtures)


def main():
    p=argparse.ArgumentParser();p.add_argument("binary",type=Path);p.add_argument("--work",type=Path);p.add_argument("--report",type=Path);a=p.parse_args()
    if a.work:
        a.work.mkdir(parents=True,exist_ok=False)
        result=run_checks(a.binary.resolve(),a.work.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix="rfc-local-check-") as d:result=run_checks(a.binary.resolve(),Path(d))
    text=json.dumps(result,indent=2)+"\n"
    if a.report:a.report.write_text(text)
    print(text,end="")

if __name__=="__main__":main()
