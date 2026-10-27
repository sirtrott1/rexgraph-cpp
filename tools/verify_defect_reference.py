#!/usr/bin/env python3
"""Independent exact Fraction checks of exported defect matrices and work counts.

This checker does not import RFC code, use GMP, or rely on floating point. Native
wire reconstruction is tested by the C++ suites; this script checks the exported
mathematical data, not the RFCX grammar.
"""
from __future__ import annotations
import argparse
import csv
import json
from collections import defaultdict
from fractions import Fraction
from pathlib import Path
from typing import Iterator

Matrix = tuple[tuple[Fraction, ...], ...]


def matrix(line: str) -> Matrix:
    words = line.split()
    if len(words) < 2:
        raise ValueError("matrix dimensions missing")
    rows, cols = int(words[0]), int(words[1])
    if rows <= 0 or cols <= 0 or rows * cols > 1000000:
        raise ValueError("unsupported reference dimensions")
    if len(words) != 2 + rows * cols:
        raise ValueError("wrong matrix component count")
    values = tuple(map(Fraction, words[2:]))
    return tuple(values[i * cols : (i + 1) * cols] for i in range(rows))


def zero(rows: int, cols: int) -> Matrix:
    return tuple(tuple(Fraction(0) for _ in range(cols)) for _ in range(rows))


def mul(a: Matrix, b: Matrix) -> Matrix:
    if len(a[0]) != len(b):
        raise ValueError("matrix composition mismatch")
    return tuple(tuple(sum((a[i][k] * b[k][j] for k in range(len(b))), Fraction(0))
                       for j in range(len(b[0]))) for i in range(len(a)))


def add(a: Matrix, b: Matrix, sign: int = 1) -> Matrix:
    if (len(a), len(a[0])) != (len(b), len(b[0])):
        raise ValueError("matrix sum mismatch")
    return tuple(tuple(x + sign * y for x, y in zip(ar, br)) for ar, br in zip(a, b))


def lines(path: Path) -> Iterator[str]:
    return iter(path.read_text(encoding="utf-8").splitlines())


def verify(root: Path) -> dict[str, object]:
    checks = 0
    routes = 0
    sums: dict[tuple[int, ...], Matrix] = {}
    frames: dict[int, int] = defaultdict(int)
    src = lines(root / "routes.txt")
    for header in src:
        frame, active, lg, lk, ug, uk = map(int, header.split())
        if active not in (0, 1) or ug != lg + 2:
            raise AssertionError("invalid route role/grade header")
        a, b, expected = (matrix(next(src)) for _ in range(3))
        actual = mul(b, a) if active else zero(len(b), len(a[0]))
        assert expected == actual, (frame, lg, lk, ug, uk, "route product")
        key = (frame, lg, lk, ug, uk)
        sums[key] = add(sums.get(key, zero(len(actual), len(actual[0]))), actual)
        routes += 1
        frames[frame] += 1
        checks += 1
    found: set[tuple[int, ...]] = set()
    src = lines(root / "route-sums.txt")
    for header in src:
        key = tuple(map(int, header.split()))
        assert len(key) == 5 and key not in found
        assert matrix(next(src)) == sums[key], (key, "assembled endpoint block")
        found.add(key)
        checks += 1
    assert found == sums.keys()
    assert len(frames) == 80 and all(n == 30 for n in frames.values())
    a, b, ap, bp, delta = (matrix(s) for s in lines(root / "noncommuting.txt"))
    da, db = add(ap, a, -1), add(bp, b, -1)
    cross = mul(db, da)
    assert cross != zero(len(cross), len(cross[0]))
    assert delta == add(mul(bp, ap), mul(b, a), -1)
    assert delta == add(add(mul(b, da), mul(db, a)), cross)
    checks += 3
    a, b, ap, bp, ju, jm, jl, expected = (matrix(s) for s in lines(root / "rectangular.txt"))
    ea, eb = add(mul(ap, ju), mul(jm, a), -1), add(mul(bp, jm), mul(jl, b), -1)
    direct = add(mul(mul(bp, ap), ju), mul(jl, mul(b, a)), -1)
    assert direct == expected == add(mul(bp, ea), mul(eb, a))
    checks += 1
    with (root / "work.csv").open(newline="", encoding="utf-8") as stream:
        work = list(csv.DictReader(stream))
    assert len(work) == 3
    # The native fixture has 15 routes and 18 retained occurrences per triangle.
    for row in work:
        n = int(row["triangles"])
        assert int(row["routes"]) == 15 * n
        assert int(row["candidate_routes"]) == int(row["products"]) == int(row["changed_legs"]) == 1
        assert int(row["reused"]) == 15 * n - 1
        assert int(row["scanned"]) == 18 * n
        checks += 4
    return {"passed": True, "independent_fraction_checks": checks,
            "route_products": routes, "assembled_endpoint_blocks": len(sums),
            "temporal_frames": len(frames), "noncommuting_cross_term": True,
            "rectangular_word_identity": True, "work_scaling_fixtures": len(work),
            "scope": "Exported exact mathematical fixtures, not a wire-codec audit or universal proof"}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixtures", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    result = verify(args.fixtures)
    text = json.dumps(result, indent=2) + "\n"
    if args.report:
        args.report.write_text(text, encoding="utf-8")
    print(text, end="")


if __name__ == "__main__":
    main()
