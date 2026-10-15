#!/usr/bin/env python3
"""Independent three-valued and Fraction oracle over declared tensor coordinates."""
from fractions import Fraction
import itertools
import json
import math
import random
import subprocess
import sys


def main():
    rng = random.Random(6401)
    inputs, expected = [], []
    queries = windows = coordinates = 0
    for case in range(120):
        n = (0, 1, 2, 17, 65, 131)[case % 6]
        workers = (1, 2, 4)[case % 3]
        tile = (1, 7, 64, 256)[case % 4]
        aggregate = case % 2
        sum_integer = case % 3 == 0
        inputs.append(f"q {n} {workers} {tile} {aggregate} {int(sum_integer)}")
        selected, unknown, total = [], 0, Fraction(0)
        for row in range(n):
            value = None if rng.randrange(5) == 0 else Fraction(rng.randrange(-100, 101), rng.randrange(1, 31))
            if case % 19 == 0 and value is not None:
                value *= 1 << 1001
            flag, category = rng.randrange(2), rng.choice((7, 9))
            integer = (1 << 200) + rng.randrange(10)
            inputs.append(f"{'null' if value is None else value} {flag} {category} {integer}")
            truth = None if value is None else value > 0
            if not flag or category == 9:
                truth = False
            if truth is None:
                unknown += 1
            elif truth:
                selected.append(row + 1)
                total += integer if sum_integer else value
        answer = ["q", str(len(selected)), str(unknown), str(len(selected)), str(total) if selected else "null"]
        if not aggregate:
            answer += list(map(str, selected))
        expected.append(answer)
        queries += 1
    for case in range(210):
        rank = case % 5
        shape = [rng.randrange(1, 8) for _ in range(rank)]
        if rank == 1:
            shape = [65 if case % 2 else 129]
        if rank and case % 17 == 0:
            shape[-1] = 0
        size = math.prod(shape)
        lo = [rng.randrange(n) if n else 0 for n in shape]
        hi = [rng.randrange(a + 1, n + 1) if n else 0 for a, n in zip(lo, shape)]
        if case % 13 == 0:
            lo, hi = [0] * rank, shape[:]
        if rank and case % 11 == 0:
            hi[-1] = lo[-1]
        encoding = case % 3
        support = [i for i in range(size) if rng.randrange(3)]
        if encoding == 0:
            support = list(range(size))
        entries, values = [], {}
        for index in support:
            if rng.randrange(3):
                q = Fraction(rng.randrange(-5, 6), rng.randrange(1, 13))
                entries.append((index, q))
                values[index] = values.get(index, Fraction(0)) + q
                if case % 7 == 0:
                    entries.append((index, -q))
                    values[index] -= q
        inputs.append(" ".join(map(str, ["w", encoding, rank, *shape, *lo, *hi, len(support), *support, len(entries)])))
        inputs += [f"{i} {q}" for i, q in entries]
        output, count, nonzero, total = [], 0, 0, Fraction(0)
        active = set(support)
        # Independent lexicographic Cartesian axes, not C++ flatten/unflatten.
        for coordinate in itertools.product(*(range(a, b) for a, b in zip(lo, hi))):
            index = 0
            for x, extent in zip(coordinate, shape):
                index = index * extent + x
            if index in active:
                count += 1
                q = values.get(index, Fraction(0))
                total += q
                nonzero += q != 0
                value = str(q)
            else:
                value = "null"
            output.append(f"{index}:{value}")
        expected.append(["w", str(count), str(nonzero), str(total) if count else "null", *output])
        windows += 1
        coordinates += len(output)
    result = subprocess.run([sys.argv[1], "--oracle"], input="\n".join(inputs) + "\n", text=True, capture_output=True, check=True)
    actual = [line.split() for line in result.stdout.splitlines()]
    assert len(actual) == len(expected)
    for case, (got, want) in enumerate(zip(actual, expected)):
        assert got == want, (case, got, want)
    report = dict(query_cases=queries, tensor_windows=windows, tensor_coordinates=coordinates,
                  support_encodings=3, exact=True)
    if len(sys.argv) > 2:
        with open(sys.argv[2], "w") as out:
            out.write(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
