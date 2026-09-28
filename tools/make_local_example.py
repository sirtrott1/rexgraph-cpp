#!/usr/bin/env python3
"""Create a new, noncritical SQLite trial fixture; never overwrite a file."""
import argparse
from pathlib import Path
import sqlite3

p=argparse.ArgumentParser(description=__doc__)
p.add_argument("output",type=Path)
a=p.parse_args()
with a.output.open("xb"):
    pass
try:
    with sqlite3.connect(a.output) as db:
        db.executescript("CREATE TABLE people(id INTEGER PRIMARY KEY, name TEXT); CREATE TABLE events(id INTEGER PRIMARY KEY, sender INTEGER NOT NULL, recipient INTEGER, amount INTEGER, note TEXT, bytes BLOB);")
        db.executemany("INSERT INTO people VALUES(?,?)",[(1,"alice"),(2,"bob")])
        db.executemany("INSERT INTO events VALUES(?,?,?,?,?,?)",[(1,1,2,7,"same",b"\x00\xff"),(2,1,2,7,"same",b"\x00\xff"),(3,2,None,0,"\x00",b""),(4,1,2,None,None,None)])
except Exception:
    # Leave the exclusively created file for inspection rather than unlinking
    # a path that another process could have replaced.
    raise
print(a.output)
