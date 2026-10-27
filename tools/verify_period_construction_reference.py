#!/usr/bin/env python3
"""Independent Fraction/forest/minor reference for test_period_construction --dump.

Uses original boundary, metric and cup declarations; no native Smith action or
native elimination code is used. Exhaustive maximal minors certify primitive
integral generation in a primitive graph-kernel frame, not merely rational rank.
"""
import argparse
from copy import deepcopy
from fractions import Fraction as F
from functools import reduce
from itertools import combinations
from math import gcd
import json
from pathlib import Path


def matrix(values):
    return [[F(x) for x in row] for row in values]


def transpose(a):
    return [list(x) for x in zip(*a)] if a else []


def product(a, b):
    if not a:
        return []
    n = len(b[0]) if b else 0
    return [[sum((x*y for x, y in zip(row, col)), F(0))
             for col in transpose(b)] for row in a] if n else [[] for _ in a]


def subtract(a, b):
    return [[x-y for x, y in zip(row, other)] for row, other in zip(a, b)]


def identity(n):
    return [[F(i == j) for j in range(n)] for i in range(n)]


def pivots(a):
    a = deepcopy(a)
    row, result = 0, []
    for col in range(len(a[0]) if a else 0):
        pivot = next((i for i in range(row, len(a)) if a[i][col]), None)
        if pivot is None:
            continue
        a[row], a[pivot] = a[pivot], a[row]
        divisor = a[row][col]
        a[row] = [x/divisor for x in a[row]]
        for i in range(len(a)):
            if i != row:
                factor = a[i][col]
                a[i] = [x-factor*y for x, y in zip(a[i], a[row])]
        result.append(col)
        row += 1
        if row == len(a):
            break
    return result


def determinant(a):
    a = deepcopy(a)
    result = F(1)
    for j in range(len(a)):
        pivot = next((i for i in range(j, len(a)) if a[i][j]), None)
        if pivot is None:
            return F(0)
        if pivot != j:
            a[j], a[pivot] = a[pivot], a[j]
            result = -result
        divisor = a[j][j]
        result *= divisor
        for i in range(j+1, len(a)):
            ratio = a[i][j]/divisor
            a[i] = [x-ratio*y for x, y in zip(a[i], a[j])]
    return result


def inverse(a):
    n = len(a)
    a = [row + unit for row, unit in zip(deepcopy(a), identity(n))]
    for j in range(n):
        pivot = next(i for i in range(j, n) if a[i][j])
        a[j], a[pivot] = a[pivot], a[j]
        divisor = a[j][j]
        a[j] = [x/divisor for x in a[j]]
        for i in range(n):
            if i != j:
                factor = a[i][j]
                a[i] = [x-factor*y for x, y in zip(a[i], a[j])]
    return [row[n:] for row in a]


def verify(case):
    b1, b2, z, c, m, gram, cup = [matrix(case[k]) for k in
                                  ('b1', 'b2', 'cycles', 'cochains', 'metric', 'gram', 'cup')]
    nv, ne = len(b1), len(b2)
    h = len(z[0])
    assert len(z) == len(c) == len(m) == ne
    assert all(len(row) == h for row in z+c)
    assert all(len(row) == ne for row in m)
    assert all(not x for row in product(b1, b2) for x in row)
    assert all(x.denominator == 1 for row in b1+b2+z for x in row)
    assert m == transpose(m)
    assert all(determinant([row[:n] for row in m[:n]]) > 0 for n in range(1, ne+1))
    # A spanning forest supplies a primitive Z-kernel frame. Its chord
    # coordinates are precisely the original coordinates on non-tree edges.
    parent = list(range(nv))
    def root(i):
        while parent[i] != i:
            i = parent[i]
        return i
    chords = []
    for e in range(ne):
        support = [(v, b1[v][e]) for v in range(nv) if b1[v][e]]
        if not support:
            chords.append(e)
            continue
        assert len(support) == 2 and sorted(x for _, x in support) == [-1, 1]
        a, b = (v for v, _ in support)
        if root(a) == root(b):
            chords.append(e)
        else:
            parent[root(a)] = root(b)
    k = len(chords)
    assert k == ne-len(pivots(b1))
    assert h == k-len(pivots(b2)) and h % 2 == 0
    assert all(not x for row in product(b1, z) for x in row)
    generators = [b2[i]+z[i] for i in chords]
    ncols = len(generators[0]) if k else 0
    # Smith index equals gcd of all maximal minors in this primitive frame.
    minors = [determinant([[row[j] for j in cols] for row in generators])
              for cols in combinations(range(ncols), k)] if k else [F(1)]
    assert all(x.denominator == 1 for x in minors)
    index = reduce(gcd, (abs(x.numerator) for x in minors), 0)
    assert index == 1, 'proper sublattice / omitted integral classes'
    assert product(transpose(b2), c) == [[F(0)]*h for _ in transpose(b2)]
    assert product(transpose(z), c) == identity(h)
    minv = inverse(m)
    assert all(not x for row in product(b1, product(minv, c)) for x in row)
    assert product(transpose(c), product(minv, c)) == gram
    # Reconstruct the unique harmonic duals via an independently selected
    # boundary frame, its exact Gram projection, and the full supplied Z basis.
    boundary_cols = pivots(b2)
    a = [[row[j] for j in boundary_cols] for row in b2]
    if boundary_cols:
        coefficients = product(inverse(product(transpose(a), product(m, a))),
                               product(transpose(a), product(m, z)))
        harmonic = subtract(z, product(a, coefficients))
    else:
        harmonic = z
    if h:
        expected = product(product(m, harmonic), inverse(product(transpose(harmonic), product(m, harmonic))))
        assert c == expected
    rawcup = [[F(0)]*h for _ in range(h)]
    for face, first, second, coefficient, x, y, fundamental in case['cup_terms']:
        assert face < len(b2[0]) and first < ne and second < ne
        scale = F(coefficient)*F(x)*F(y)*F(fundamental)
        for i in range(h):
            for j in range(h):
                rawcup[i][j] += scale*c[first][i]*c[second][j]
    assert rawcup == cup
    assert all(cup[i][j] == -cup[j][i] and cup[i][j].denominator == 1
               for i in range(h) for j in range(h))
    assert abs(determinant(cup)) == 1
    for point in case['points']:
        chain = matrix(point['path'])
        target = [[F(int(i == point['vertex'])-int(i == 0))] for i in range(nv)]
        assert product(b1, chain) == target
        periods = product(transpose(c), chain) if h else []
        coordinates = [q[0] % 1 for q in periods]
        assert coordinates == [F(q) for q in point['coordinates']]
    cylinder_rank = None
    if case['name'] == 'four-square-rank-four':
        cores = [[F(0)]*4 for _ in range(8)]
        for i in range(4):
            cores[i][0] = F(1)
        cores[4][1] = cores[5][1] = cores[6][2] = cores[7][3] = F(1)
        cylinder_rank = len(pivots(product(transpose(c), cores)))
        assert cylinder_rank == 3 and h == 4
    return dict(name=case['name'], cylinder_rank=cylinder_rank, vertices=nv, edges=ne, integral_kernel_rank=k,
                complete_period_rank=h, primitive_index=index,
                maximal_minors_checked=len(minors), points=len(case['points']))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    cases = json.loads(args.input.read_text())
    results = [verify(case) for case in cases]
    controls = []
    for kind in ('nonprimitive', 'incomplete', 'nonclosed', 'wrong-metric', 'wrong-cup', 'wrong-path'):
        bad = deepcopy(cases[0])
        if kind == 'nonprimitive':
            for row in bad['cycles']:
                row[0] = str(2*F(row[0]))
            for row in bad['cochains']:
                row[0] = str(F(row[0])/2)
        elif kind == 'incomplete':
            for key in ('cycles', 'cochains'):
                bad[key] = [row[:-1] for row in bad[key]]
        elif kind == 'nonclosed':
            bad['cycles'][0][0] = str(F(bad['cycles'][0][0])+1)
        elif kind == 'wrong-metric':
            bad['metric'][0][0] = '-1'
        elif kind == 'wrong-cup':
            bad['cup_terms'][0][3] = '2'
        else:
            bad['points'][1]['path'][0][0] = str(F(bad['points'][1]['path'][0][0])+1)
        try:
            verify(bad)
        except (AssertionError, ValueError, StopIteration, IndexError):
            controls.append(kind)
        else:
            raise AssertionError('reference admitted '+kind)
    report = dict(status='passed', case_count=len(results), cases=results,
                  oracle_refusal_controls=controls, arithmetic='Python Fraction, forest primitive kernel, exhaustive integral maximal minors')
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
