#!/usr/bin/env python3
"""Independent Fraction checks for native fiber regions and exact local actions."""
from fractions import Fraction as Q
from pathlib import Path
import json
import random
import subprocess
import sys


def run(binary, mode, data=None):
    return subprocess.run([binary, mode], input=data, text=True, capture_output=True, check=True).stdout.splitlines()


def solve(a, b):
    rows = [list(row) + [value] for row, value in zip(a, b)]
    n = len(b)
    for j in range(n):
        pivot = next(i for i in range(j, n) if rows[i][j])
        rows[j], rows[pivot] = rows[pivot], rows[j]
        divisor = rows[j][j]
        rows[j] = [q / divisor for q in rows[j]]
        for i in range(n):
            if i != j:
                factor = rows[i][j]
                rows[i] = [x - factor * y for x, y in zip(rows[i], rows[j])]
    return [row[-1] for row in rows]


def main():
    binary = sys.argv[1]
    cases, field_checks, case = [], 0, None
    for line in run(binary, "--oracle"):
        tag, *p = line.split()
        if tag == "case":
            case = dict(name=p[0], upper={}, lower={}, x={}, y={}, z={}, occ={}, m={})
            cases.append(case)
        elif tag in ("upper", "lower"):
            case[tag][int(p[0])] = int(p[1])
        elif tag in ("x", "y", "z"):
            case[tag][int(p[0]), int(p[1])] = Q(p[2])
        elif tag == "occ":
            key, upper, lower, active = map(int, p[:4])
            case["occ"][key] = upper, lower, active, Q(p[4]), p[5]
        elif tag == "m":
            case["m"][tuple(map(int, p[:3]))] = Q(p[3])
        elif tag == "end":
            expected = {(key, j): Q(0) for key, n in case["lower"].items() for j in range(n)}
            assert len(case["occ"]) == 4 and sum(not v[2] for v in case["occ"].values()) == 1
            for key, (upper, lower, active, coefficient, role) in case["occ"].items():
                if active:
                    for i in range(case["lower"][lower]):
                        for j in range(case["upper"][upper]):
                            q = Q(i == j) if role == "constant" else case["m"].get((key, i, j), Q(0))
                            expected[lower, i] += (q if role == "realized" else coefficient*q) * case["x"][upper, j]
            assert expected == case["y"] == case["z"], case["name"]
            field_checks += 2*len(expected)
        else:
            raise AssertionError(tag)
    assert len(cases) == 40
    assert any(q.numerator.bit_length() > 1000 for case in cases for q in case["y"].values())
    rng = random.Random(6301)
    systems = []
    for n in range(9):
        for repetition in range(12):
            a = [[Q(rng.randrange(-5, 6), rng.randrange(1, 9)) if i != j else Q(0) for j in range(n)] for i in range(n)]
            for i in range(n):
                a[i][i] = sum(abs(q) for q in a[i]) + 1
            indices = list(range(n)); rng.shuffle(indices)
            interior = indices[:repetition % (n+1)]
            b = [Q(rng.randrange(-10, 11), rng.randrange(1, 17)) for _ in range(n)]
            systems.append((a, b, interior))
    systems.append(([[Q(0), Q(1), Q(0)], [Q(1), Q(0), Q(1)], [Q(0), Q(1), Q(1)]], [Q(1), Q(2), Q(3)], [0, 1, 2]))
    data = "".join(" ".join(map(str, [len(b), len(interior), *interior, *(q for row in a for q in row), *b]))+"\n" for a, b, interior in systems)
    outputs = run(binary, "--solve-probe", data)
    assert len(outputs) == len(systems)
    solve_checks = 0
    for line, (a, b, _) in zip(outputs, systems):
        n, *values = line.split()
        assert int(n) == len(b) and [Q(q) for q in values] == solve(a, b)
        solve_checks += len(b)
    inputs = [(Q(rng.randrange(-30, 31), rng.randrange(1, 25)), Q(rng.randrange(-30, 31), rng.randrange(1, 25))) for _ in range(96)]
    outputs = run(binary, "--semantic-probe", "".join(f"{a} {b}\n" for a, b in inputs))
    assert len(outputs) == len(inputs)
    for line, (a, b) in zip(outputs, inputs):
        assert [Q(q) for q in line.split()] == [a, Q(0), b-a, -a, a-b, b]
    result = dict(arithmetic="independent fractions.Fraction", native_cases=len(cases), native_field_region_checks=field_checks,
                  solve_cases=len(systems), solve_checks=solve_checks, semantic_cases=len(inputs), semantic_checks=6*len(inputs))
    if len(sys.argv) > 2:
        Path(sys.argv[2]).write_text(json.dumps(result, indent=2)+"\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
