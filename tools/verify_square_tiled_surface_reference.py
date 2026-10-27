#!/usr/bin/env python3
"""Independent square-gluing topology and full integral symplectic reference.

Consumes test_square_tiled_surface --dump and --hard. Uses a perimeter-point
quotient different from the native per-face corner quotient, exact Fraction
linear algebra and exhaustive integral minors. No native Smith actions are used.
"""
import argparse
from copy import deepcopy
from fractions import Fraction as F
from functools import reduce
from itertools import combinations
from math import gcd
import json
from pathlib import Path
import verify_period_construction_reference as ref

matrix, transpose, product = ref.matrix, ref.transpose, ref.product
identity, determinant, pivots = ref.identity, ref.determinant, ref.pivots


def primitive_index(a):
    m = len(a)
    assert all(x.denominator == 1 for row in a for x in row)
    minors = [determinant([[row[j] for j in columns] for row in a])
              for columns in combinations(range(len(a[0]) if a else 0), m)]
    assert all(d.denominator == 1 for d in minors)
    return reduce(gcd, (abs(d.numerator) for d in minors), 0), len(minors)


def canonical(h):
    return [[F(int(j == i+h//2)-int(i == j+h//2))
             for j in range(h)] for i in range(h)]


def geometry(case):
    r, u, n = case['right'], case['up'], case['n']
    s = len(r)
    assert n > 0 and s and len(u) == s
    assert sorted(r) == sorted(u) == list(range(s))
    visited, queue = {0}, [0]
    for a in queue:
        for b in (r[a], u[a]):
            if b not in visited:
                visited.add(b)
                queue.append(b)
    assert len(visited) == s
    # Independent quotient on the unrefined squares' (n+1)^2 grid points.
    points = [(a, x, y) for a in range(s) for x in range(n+1) for y in range(n+1)]
    adjacency = {p: [] for p in points}
    def join(a, b):
        adjacency[a].append(b)
        adjacency[b].append(a)
    for a in range(s):
        for t in range(n+1):
            join((a, n, t), (r[a], 0, t))
            join((a, t, n), (u[a], t, 0))
    component = {}
    count = 0
    for first in points:
        if first in component:
            continue
        todo = [first]
        component[first] = count
        for a in todo:
            for b in adjacency[a]:
                if b not in component:
                    component[b] = count
                    todo.append(b)
        count += 1
    faces = s*n*n
    def face(a, x, y):
        return (a*n+x)*n+y
    def local(cell):
        return cell//(n*n), (cell//n) % n, cell % n
    expected = {}
    reverse = {}
    for cell, declared in enumerate(case['corner_vertices']):
        a, x, y = local(cell)
        corners = [(a, x, y), (a, x+1, y), (a, x+1, y+1), (a, x, y+1)]
        assert len(declared) == 4
        for point, actual in zip(corners, declared):
            c = component[point]
            assert expected.setdefault(c, actual) == actual
            assert reverse.setdefault(actual, c) == c
    assert len(case['corner_vertices']) == faces and len(expected) == count
    assert sorted(reverse) == list(range(count))
    def vertex(a, x, y):
        return expected[component[(a, x, y)]]
    b1 = [[F(0)]*(2*faces) for _ in range(count)]
    b2 = [[F(0)]*faces for _ in range(2*faces)]
    flags = [[] for _ in range(2*faces)]
    for cell in range(faces):
        a, x, y = local(cell)
        bottom = cell
        right = faces+(face(a, x+1, y) if x+1 < n else face(r[a], 0, y))
        top = face(a, x, y+1) if y+1 < n else face(u[a], x, 0)
        left = faces+cell
        assert case['face_edges'][cell] == [bottom, right, top, left]
        for edge, start, end in [(bottom, vertex(a, x, y), vertex(a, x+1, y)),
                                 (left, vertex(a, x, y), vertex(a, x, y+1))]:
            b1[start][edge] -= 1
            b1[end][edge] += 1
        for side, (edge, sign) in enumerate(zip([bottom, right, top, left], [1, 1, -1, -1])):
            b2[edge][cell] += sign
            flags[edge].append((4*cell+side, sign))
    assert b1 == matrix(case['b1']) and b2 == matrix(case['b2'])
    # Links are cycles on quarter-corners, not a rank-only manifold test.
    links = [[] for _ in range(4*faces)]
    def connect(a, b):
        links[a].append(b)
        links[b].append(a)
    for pair in flags:
        assert len(pair) == 2 and pair[0][1] == -pair[1][1]
        a, b = pair[0][0], pair[1][0]
        an, bn = 4*(a//4)+(a % 4+1) % 4, 4*(b//4)+(b % 4+1) % 4
        connect(a, bn)
        connect(an, b)
    assert len(case['corner_links']) == len(links)
    assert all(len(link) == 2 and sorted(link) == sorted(actual)
               for link, actual in zip(links, case['corner_links']))
    seen = set()
    orders = []
    for v in range(count):
        carrier = {4*i+j for i, corners in enumerate(case['corner_vertices'])
                   for j in range(4) if corners[j] == v}
        assert carrier and len(carrier) % 4 == 0
        first = next(iter(carrier))
        todo, reachable = [first], {first}
        for a in todo:
            assert a in carrier
            for b in links[a]:
                assert b in carrier
                if b not in reachable:
                    reachable.add(b)
                    todo.append(b)
        assert reachable == carrier
        seen.update(carrier)
        orders.append(len(carrier)//4)
    assert len(seen) == 4*faces
    genus = (2-count+faces)//2
    assert 2-count+faces == 2*genus and genus == case['genus']
    assert sum(x-1 for x in orders) == 2*genus-2
    assert case['orientation'] in [-1, 1]
    terms = []
    assert len(case['cup_terms']) == 2*faces
    for f in range(faces):
        declared = case['cup_terms'][2*f:2*f+2]
        wanted = [[f, 4*f, 4*f+1, '1', str(case['orientation'])],
                  [f, 4*f+3, 4*f+2, '-1', str(case['orientation'])]]
        assert declared == wanted
        for f, first, second, coefficient, fundamental in declared:
            signs = [1, 1, -1, -1]
            terms.append([f, case['face_edges'][f][first % 4],
                          case['face_edges'][f][second % 4], coefficient,
                          str(signs[first % 4]), str(signs[second % 4]), fundamental])
    return terms, orders


def verify(case):
    terms, cone_orders = geometry(case)
    e = len(case['cycles'])
    # The already independent period reference checks ranks, exact harmonic
    # projection, primitive kernel generation, cup descent and all vertex paths.
    period_case = deepcopy(case)
    period_case['metric'] = [[str(x) for x in row] for row in identity(e)]
    period_case['cup_terms'] = terms
    base = ref.verify(period_case)
    c, z, omega = [matrix(case[k]) for k in ['cochains', 'cycles', 'cup']]
    t, inv, zs, cs = [matrix(case[k]) for k in ['symplectic', 'inverse', 'symplectic_cycles', 'symplectic_cochains']]
    h = len(omega)
    assert all(x.denominator == 1 for a in (t, inv, zs) for row in a for x in row)
    assert abs(determinant(t)) == 1
    assert product(t, inv) == product(inv, t) == identity(h)
    assert product(transpose(t), product(omega, t)) == canonical(h)
    assert cs == product(c, t) and zs == product(z, transpose(inv))
    assert product(transpose(zs), cs) == identity(h)
    assert product(transpose(cs), cs) == product(transpose(t), product(matrix(case['gram']), t))
    gluing, coordinates, cylinders, b1 = [matrix(case[k]) for k in ['gluing', 'gluing_coordinates', 'cylinders', 'b1']]
    assert len(gluing[0]) == len(case['right'])+1
    assert all(x.denominator == 1 for a in (gluing, coordinates, cylinders) for row in a for x in row)
    assert not any(x for row in product(b1, gluing) for x in row)
    assert not any(x for row in product(b1, cylinders) for x in row)
    assert coordinates == product(transpose(c), gluing)
    index, minors = primitive_index(coordinates)
    assert index == 1 and len(pivots(coordinates)) == h
    cylinder_rank = len(pivots(product(transpose(c), cylinders)))
    if case['name'].startswith('four-square-row'):
        assert cylinder_rank == 3 and h == 4
    base.update(surface_genus=h//2, cone_orders=sorted(cone_orders),
                symplectic_index=abs(int(determinant(t))),
                gluing_integral_index=index, gluing_minors_checked=minors,
                cylinder_rank=cylinder_rank)
    return base


def verify_hard(case):
    omega, t, inv, original, c, z = [matrix(case[k]) for k in
                                    ['form', 'basis', 'inverse', 'original_cochains', 'cochains', 'cycles']]
    assert all(abs(x) != 1 for row in omega for x in row)
    assert abs(determinant(omega)) == 1
    assert product(transpose(original), product(canonical(4), original)) == omega
    assert abs(determinant(t)) == 1 and product(t, inv) == identity(4)
    assert product(transpose(t), product(omega, t)) == canonical(4)
    assert c == product(original, t) and product(transpose(z), c) == identity(4)
    return dict(status='passed', dimension=4, no_unit_entries=True, symplectic_index=1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('--hard-input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    cases = json.loads(args.input.read_text())
    results = [verify(c) for c in cases]
    hard = verify_hard(json.loads(args.hard_input.read_text()))
    controls = []
    for kind in ['nonbijective', 'disconnected', 'corner-pinch', 'wrong-link', 'incoherent-orientation', 'incomplete-cup', 'nonunimodular-basis', 'incomplete-gluing', 'wrong-cycle-transport', 'wrong-path']:
        bad = deepcopy(cases[1])
        if kind == 'nonbijective':
            bad = deepcopy(cases[3])
            bad['right'][0] = bad['right'][1]
        elif kind == 'disconnected':
            bad = deepcopy(cases[3])
            bad['right'] = bad['up'] = [0, 1, 2]
        elif kind == 'corner-pinch':
            bad['corner_vertices'][0][0] = bad['corner_vertices'][0][1]
        elif kind == 'wrong-link':
            bad['corner_links'][0][0] = 0
        elif kind == 'incoherent-orientation':
            bad['cup_terms'][0][4] = '-1'
        elif kind == 'incomplete-cup':
            bad['cup_terms'].pop()
        elif kind == 'nonunimodular-basis':
            for row in bad['symplectic']:
                row[0] = str(2*F(row[0]))
        elif kind == 'incomplete-gluing':
            for key in ['gluing', 'gluing_coordinates']:
                bad[key] = [row[:-1] for row in bad[key]]
        elif kind == 'wrong-cycle-transport':
            bad['symplectic_cycles'][0][0] = str(F(bad['symplectic_cycles'][0][0])+1)
        else:
            bad['points'][1]['path'][0][0] = str(F(bad['points'][1]['path'][0][0])+1)
        try:
            verify(bad)
        except (AssertionError, ValueError, IndexError, StopIteration, KeyError):
            controls.append(kind)
        else:
            raise AssertionError('reference admitted '+kind)
    # Orientation reversal changes the cup sign; relabelling preserves invariants.
    positive = next(c for c in cases if c['name'] == 'symmetric-L-n1')
    negative = next(c for c in cases if c['name'] == 'reversed-L-n1')
    assert positive['cycles'] == negative['cycles'] and positive['cochains'] == negative['cochains']
    assert matrix(negative['cup']) == [[-x for x in row] for row in matrix(positive['cup'])]
    a, b = [next(r for r in results if r['name'] == n) for n in ['asymmetric-L-n1', 'relabelled-asymmetric-L-n1']]
    assert [a[k] for k in ['vertices', 'edges', 'surface_genus', 'cone_orders']] == [b[k] for k in ['vertices', 'edges', 'surface_genus', 'cone_orders']]
    report = dict(status='passed', case_count=len(results), cases=results,
                  hard_symplectic=hard, oracle_refusal_controls=controls,
                  arithmetic='Python Fraction; independent perimeter quotient and vertex links; exhaustive primitive integral minors')
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print('Independent surface, cup, full gluing and symplectic references passed:', len(results), 'cases and', len(controls), 'tamper controls')


if __name__ == '__main__':
    main()
