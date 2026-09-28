#!/usr/bin/env python3
"""Independent SQLite oracle for the deliberately bounded F5 adapter.

Creates producer-identified fixtures. sqlite3 evaluates reference queries before
its connection closes. The C++ executable imports and answers native queries
without sending SQL during native execution. This does not certify a SQL parser.
"""
from __future__ import annotations
import argparse
import json
import pathlib
import random
import sqlite3
import subprocess
import tempfile


def run(executable: pathlib.Path, cases: int) -> dict:
    randomizer = random.Random(20260928)
    comparisons = 0
    details = []
    with tempfile.TemporaryDirectory(prefix="rfc-f5-sql-oracle-") as tmp:
        for case in range(cases):
            path = pathlib.Path(tmp) / f"case-{case}.sqlite"
            rows = []
            for identity in range(1, randomizer.randrange(0, 25) + 1):
                rows.append((identity, randomizer.choice([None, 1, 2, 3]),
                             randomizer.choice([None, 1, 2, 3]),
                             randomizer.choice([None, 0, -7, -1, 1, 5, 20]),
                             randomizer.choice([None, "", "x", "a\0b", "λ雪", "same"]),
                             randomizer.choice([None, b"", b"\0", b"\0\xff", b"same"])))
            # Equal data is not producer identity: keep every distinct assertion.
            if case % 3 == 0 and rows:
                rows.append((len(rows) + 1,) + rows[0][1:])
            randomizer.shuffle(rows)
            with sqlite3.connect(path) as db:
                db.executescript("CREATE TABLE people(id INTEGER PRIMARY KEY);"
                                 "INSERT INTO people VALUES(1),(2),(3);"
                                 "CREATE TABLE events(event_id INTEGER NOT NULL,buyer INTEGER,"
                                 "seller INTEGER,amount INTEGER,note TEXT,raw BLOB);")
                db.executemany("INSERT INTO events VALUES(?,?,?,?,?,?)", rows)
                sql_queries = {
                    "rows": "SELECT count(*) FROM events",
                    "positive": "SELECT count(*) FROM events WHERE amount > 0",
                    "zero": "SELECT count(*) FROM events WHERE amount = 0",
                    "nulls": "SELECT count(*) FROM events WHERE amount IS NULL",
                    "sum": "SELECT sum(amount) FROM events",
                    "buyer_null": "SELECT count(*) FROM events WHERE buyer IS NULL",
                    "same_role": "SELECT count(*) FROM events e JOIN people b ON b.id=e.buyer JOIN people s ON s.id=e.seller WHERE b.id=s.id",
                    "legs": "SELECT count(buyer)+count(seller) FROM events",
                }
                reference = {k: db.execute(q).fetchone()[0] for k, q in sql_queries.items()}
                reference["values"] = [
                    [("I" + str(identity)).encode().hex(), amount,
                     "null" if text is None else text.encode("utf-8").hex(),
                     "null" if blob is None else blob.hex()]
                    for identity, amount, text, blob in db.execute(
                        "SELECT event_id,amount,note,raw FROM events")]
                reference["values"].sort(key=lambda r: bytes.fromhex(r[0]))
            # The oracle connection has closed before invoking the leaf reader.
            process = subprocess.run([str(executable), "--oracle", str(path)], text=True,
                                     capture_output=True, check=True)
            actual = json.loads(process.stdout)
            for key, expected in reference.items():
                comparisons += 1
                if actual.get(key) != expected:
                    raise AssertionError(f"case {case}, {key}: {actual.get(key)!r} != {expected!r}")
            details.append({"case": case, "assertions": reference["rows"]})
    return {"status": "passed", "cases": cases, "query_and_export_comparisons": comparisons,
            "python_sqlite_version": sqlite3.sqlite_version, "details": details}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=pathlib.Path)
    parser.add_argument("--cases", type=int, default=101)
    parser.add_argument("--report", type=pathlib.Path)
    args = parser.parse_args()
    if not 1 <= args.cases <= 10000:
        parser.error("cases must be between 1 and 10000")
    result = run(args.executable.resolve(strict=True), args.cases)
    text = json.dumps(result, indent=2) + "\n"
    if args.report:
        args.report.write_text(text)
    print(f"{result['cases']} independent datasets; {result['query_and_export_comparisons']} exact SQL query/export comparisons passed")


if __name__ == "__main__":
    main()
