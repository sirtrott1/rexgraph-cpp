#!/usr/bin/env python3
"""Independent integer/Fraction checks of lossless-integrity fixtures.

This imports only the standalone Python codec, not C++ bindings. Existing nested
source/temporal codecs are intentionally not independently implemented here.
"""
from __future__ import annotations
import argparse
import csv
from fractions import Fraction
import json
from pathlib import Path
import sys
from verify_boolean_layout_reference import decode_archive, encode_archive


def verify(model: Path, io: Path) -> dict:
    count = 0
    with (model / "multiplication.csv").open(newline="") as file:
        for row in csv.DictReader(file):
            result = int(row["left"]) * int(row["right"])
            overflow = not -(1 << 63) <= result < (1 << 63)
            assert int(row["overflow"]) == int(overflow)
            assert row["value"] == ("" if overflow else str(result))
            count += 1
    assert count == 4177
    big = (1 << 257) + 7
    q = Fraction(big, 17)
    expected = [False, ("symbol", 0), -big, q, ("optional", None),
                ("repeated", [("symbol", i) for i in range(256)]),
                ("tensor", [0, 2, 5], [(0, q), (5, Fraction(-2))]),
                ("alternative", 402, ("optional", Fraction(0)))]
    archives = []
    for name in ("all-types.rfcxv", "recoded-types.rfcxv"):
        data = (io / name).read_bytes()
        root, graph, values = decode_archive(data)
        assert encode_archive(root, graph, values) == data
        assert len(values) == 1 and values[0][0] == "product"
        fields = dict(zip((f[0] for f in graph[root][1]), values[0][1], strict=True))
        for index, value in enumerate(expected, start=101):
            if index != 107:
                assert fields[index] == value, (name, index)
        # Convert tensor slots to their retained axis and coordinate identities.
        axes = graph[(992, 7, root[2])][1]
        _, support, nonzeros = fields[107]
        def named(flat: int):
            out = []
            for axis, extent, coordinates in reversed(axes):
                out.append((axis, coordinates[flat % extent])); flat //= extent
            assert flat == 0
            return tuple(sorted(out))
        states = {named(i): Fraction(0) for i in support}
        for i, value in nonzeros:
            states[named(i)] = value
        assert states == {
            ((101, 1001), (102, 2001)): q,
            ((101, 1001), (102, 2003)): Fraction(0),
            ((101, 1002), (102, 2003)): Fraction(-2),
        }
        assert ((101, 1001), (102, 2002)) not in states
        archives.append({"file": name, "bytes": len(data), "declarations": len(graph),
                         "values": len(values)})
    raw = bytes((i * 17) % 256 for i in range((1 << 20) + 9))
    assert (io / "bytes.bin").read_bytes() == raw
    assert (io / "conflict.bin").read_bytes() == raw
    assert (io / "fault.bin").read_bytes() == bytes(range(256)) * 2
    return {"status": "passed", "integer_cases": count, "all_octets": 256,
            "exact_declaration_archives": archives,
            "support_zero_and_absence": "distinct", "raw_fault_preservation": True,
            "limitations": "not an independent implementation of embedded source/temporal codecs"}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model", type=Path)
    parser.add_argument("io", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    text = json.dumps(verify(args.model, args.io), indent=2) + "\n"
    if args.report:
        args.report.write_text(text, encoding="utf-8")
    print(text, end="")


if __name__ == "__main__":
    main()
