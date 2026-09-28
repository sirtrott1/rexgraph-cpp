#!/usr/bin/env python3
"""Independent Fraction model and descriptor golden bytes for the F3 fixture.

Does not import production C++ algorithms. Timings are recorded, not interpreted
as constant-time execution or a speedup claim.
"""
from __future__ import annotations
import argparse
from fractions import Fraction
import json
from pathlib import Path
import struct


def verify(root: Path) -> dict:
    rows = [line.split("\t") for line in (root / "pairing.tsv").read_text().splitlines()]
    if len(rows) != 128:
        raise ValueError("expected 128 independent chain/cochain cases")
    for row in rows:
        if len(row) != 12:
            raise ValueError("unexpected pairing fixture dimensions")
        observed = Fraction(row[0])
        chain = [Fraction(x) for x in row[1:9]]
        cochain = [Fraction(x) for x in row[9:12]]
        # Each retained branch has +1 at participant 10, -1 at 11,
        # and an inactive repeated occurrence at 10. Participant 12 is isolated.
        expected = sum(chain) * (cochain[0] - cochain[1])
        if observed != expected:
            raise ValueError(f"exact dual pairing mismatch: {observed} != {expected}")
    # This is only a directory-local key: the C++ descriptor additionally owns
    # exact source/endpoint/query witnesses and a source namespace.
    golden = struct.pack("<BBQQHBBBBQQQB", 1, 0, 101, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0)
    actual = (root / "chain.rfca").read_bytes()
    if actual != golden or len(actual) != 49:
        raise ValueError("canonical local descriptor differs from independent golden bytes")
    counts = []
    for line in (root / "lookup_counts.tsv").read_text().splitlines():
        cells, lookups, elapsed, binding_bytes, accumulator = map(int, line.split("\t"))
        if lookups != 10000 or accumulator != 0 or cells not in (8, 128, 2048):
            raise ValueError("invalid lookup work fixture")
        counts.append(dict(source_relations=cells, handle_lookups=lookups,
                           elapsed_ns=elapsed, canonical_context_bytes=binding_bytes))
    if len(counts) != 3:
        raise ValueError("missing lookup population")
    return dict(status="passed", fraction_pairing_cases=len(rows),
                independent_descriptor_bytes=len(golden), lookup_work_cases=counts,
                scope="bounded independent arithmetic/golden-byte model; not a complete RFCX decoder")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("fixtures", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    result = verify(args.fixtures)
    text = json.dumps(result, indent=2) + "\n"
    if args.report:
        args.report.write_text(text)
    print(text, end="")


if __name__ == "__main__":
    main()
