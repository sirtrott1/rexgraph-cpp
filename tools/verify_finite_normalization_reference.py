#!/usr/bin/env python3
"""Exact independent normalization reference, optionally aligning old period tables.

Uses Fraction quotient arithmetic, rational regular-representation inverses and
exact real quadratic comparisons. Native test fixtures use rational/real quadratic
fields; native construction accepts any supported declared real algebraic field.
"""
import argparse
from copy import deepcopy
from fractions import Fraction as Q
from math import isqrt
from pathlib import Path
import json
import re
import verify_period_construction_reference as rational
from verify_branch_chain_reference import Field


class ExactField(Field):
    def __init__(self, polynomial, interval):
        super().__init__(polynomial)
        self.zero, self.one = tuple(self.scalar(0)), tuple(self.scalar(1))
        lower, upper = map(Q, interval)
        assert lower < upper
        if self.d == 1:
            assert lower < -self.p[0] < upper
        else:
            assert self.d == 2 and self.p[1] == 0 and self.p[0] < 0
            self.radicand = -self.p[0]
            assert lower >= 0 and lower*lower < self.radicand < upper*upper
            root = isqrt(self.radicand.numerator*self.radicand.denominator)
            assert root*root != self.radicand.numerator*self.radicand.denominator
    def element(self, x):
        if isinstance(x, (list, tuple)):
            assert len(x) == self.d
            return tuple(self.reduce(x))
        return tuple(self.scalar(Q(x)))
    def plus(self, a, b):
        return tuple(self.add(a, b))
    def times(self, a, b):
        return tuple(self.mul(a, b))
    def negative(self, a):
        return tuple(-x for x in a)
    def minus(self, a, b):
        return self.plus(a, self.negative(b))
    def inverse(self, a):
        assert a != self.zero
        columns = [self.mul(a, [Q(i == j) for i in range(self.d)]) for j in range(self.d)]
        regular = rational.transpose(columns)
        inverse = rational.inverse(regular)
        return tuple(row[0] for row in inverse)
    def sign(self, a):
        if self.d == 1:
            return (a[0] > 0)-(a[0] < 0)
        x, y = a
        if not y:
            return (x > 0)-(x < 0)
        if not x or x*y > 0:
            return (y > 0)-(y < 0)
        delta = x*x-y*y*self.radicand
        return ((delta > 0)-(delta < 0)) * (1 if x > 0 else -1)
    def matrix(self, a):
        assert not a or all(len(row) == len(a[0]) for row in a)
        return [[self.element(x) for x in row] for row in a]
    def identity(self, n):
        return [[self.one if i == j else self.zero for j in range(n)] for i in range(n)]
    def product(self, a, b):
        cols = len(b[0]) if b else 0
        assert all(len(row) == len(b) for row in a)
        out = [[self.zero]*cols for _ in a]
        for i, row in enumerate(a):
            for k, x in enumerate(row):
                for j in range(cols):
                    out[i][j] = self.plus(out[i][j], self.times(x, b[k][j]))
        return out
    def sum(self, a, b):
        assert len(a) == len(b)
        return [[self.plus(x, y) for x, y in zip(row, other)] for row, other in zip(a, b)]
    def scale(self, a, value):
        return [[self.times(x, value) for x in row] for row in a]
    def eliminate(self, a):
        a = deepcopy(a)
        row, pivots = 0, []
        for col in range(len(a[0]) if a else 0):
            found = next((i for i in range(row, len(a)) if a[i][col] != self.zero), None)
            if found is None:
                continue
            a[row], a[found] = a[found], a[row]
            inv = self.inverse(a[row][col])
            a[row] = [self.times(x, inv) for x in a[row]]
            for i in range(len(a)):
                if i != row:
                    coefficient = a[i][col]
                    a[i] = [self.minus(x, self.times(coefficient, y)) for x, y in zip(a[i], a[row])]
            pivots.append(col)
            row += 1
            if row == len(a):
                break
        return a, pivots
    def matrix_inverse(self, a):
        n = len(a)
        reduced, pivots = self.eliminate([row+unit for row, unit in zip(a, self.identity(n))])
        assert pivots[:n] == list(range(n))
        return [row[n:] for row in reduced]
    def determinant(self, a):
        a = deepcopy(a)
        result = self.one
        for j in range(len(a)):
            pivot = next((i for i in range(j, len(a)) if a[i][j] != self.zero), None)
            if pivot is None:
                return self.zero
            if pivot != j:
                a[j], a[pivot] = a[pivot], a[j]
                result = self.negative(result)
            value = a[j][j]
            result = self.times(result, value)
            inv = self.inverse(value)
            for i in range(j+1, len(a)):
                ratio = self.times(a[i][j], inv)
                a[i] = [self.minus(x, self.times(ratio, y)) for x, y in zip(a[i], a[j])]
        return result
    def positive(self, a):
        assert a == rational.transpose(a)
        assert all(self.sign(self.determinant([row[:i] for row in a[:i]])) > 0 for i in range(1, len(a)+1))


def periods(k, omega, structure, basis):
    form = k.product(omega, structure)
    k.positive(form)
    h, g = len(form), len(form)//2
    transported = k.product(rational.transpose(basis), k.product(form, basis))
    ab = [row[g:] for row in transported[:g]]
    bb = [row[g:] for row in transported[g:]]
    y = k.matrix_inverse(bb)
    x = k.scale(k.product(ab, y), k.element(-1))
    assert x == rational.transpose(x)
    k.positive(y)
    return x, y


def verify(case):
    k = ExactField(case['polynomial'], case['embedding'])
    omega, gram, star, square, basis, normalization, structure, real, imaginary = [k.matrix(case[key]) for key in ['cup', 'gram', 'star', 'square', 'basis', 'normalization', 'J', 'real', 'imaginary']]
    h = len(gram)
    assert h % 2 == 0 and all(len(a) == h and all(len(row) == h for row in a) for a in [omega, gram, star, square, basis, normalization, structure])
    k.positive(gram)
    canonical = k.matrix([[int(j == i+h//2)-int(i == j+h//2) for j in range(h)] for i in range(h)])
    qbasis = rational.matrix(case['basis'])
    assert all(x.denominator == 1 for row in qbasis for x in row) and abs(rational.determinant(qbasis)) == 1
    assert k.product(rational.transpose(basis), k.product(omega, basis)) == canonical
    assert star == k.product(k.matrix_inverse(omega), gram)
    assert square == k.product(star, star)
    identity = k.identity(h)
    total = k.matrix([[0]*h for _ in range(h)])
    weighted, expected_normalizer = deepcopy(total), deepcopy(total)
    projectors, dimensions = [], []
    mus = [k.element(s['mu']) for s in case['sectors']]
    assert len(set(mus)) == len(mus)
    for i, sector in enumerate(case['sectors']):
        mu, root = mus[i], k.element(sector['root'])
        assert k.times(root, root) == mu and k.sign(root) > 0 and k.sign(mu) > 0
        projector = identity
        for j, other in enumerate(mus):
            if i != j:
                factor = k.scale(k.sum(square, k.scale(identity, other)), k.inverse(k.minus(other, mu)))
                projector = k.product(projector, factor)
        assert projector == k.matrix(sector['projector'])
        assert k.product(projector, projector) == projector
        assert k.product(k.sum(square, k.scale(identity, mu)), projector) == k.matrix([[0]*h for _ in range(h)])
        assert k.product(star, projector) == k.product(projector, star)
        for prior in projectors:
            assert k.product(prior, projector) == k.product(projector, prior) == k.matrix([[0]*h for _ in range(h)])
        rank = len(k.eliminate(projector)[1])
        assert rank == sector['dimension'] and rank > 0
        projectors.append(projector)
        dimensions.append(rank)
        total = k.sum(total, projector)
        weighted = k.sum(weighted, k.scale(projector, mu))
        expected_normalizer = k.sum(expected_normalizer, k.scale(projector, k.inverse(root)))
    assert total == identity and sum(dimensions) == h
    assert k.sum(square, weighted) == k.matrix([[0]*h for _ in range(h)])
    assert normalization == expected_normalizer
    assert k.product(star, normalization) == structure
    assert k.product(structure, structure) == k.scale(identity, k.element(-1))
    assert (real, imaginary) == periods(k, omega, structure, basis)
    jb = k.product(k.matrix_inverse(basis), k.product(structure, basis))
    g = h//2
    real_forms = k.identity(g)+deepcopy(real)
    imaginary_forms = k.matrix([[0]*g for _ in range(g)])+deepcopy(imaginary)
    assert k.product(jb, real_forms) == imaginary_forms
    assert k.product(jb, imaginary_forms) == k.scale(real_forms, k.element(-1))
    b1, cochains = rational.matrix(case['b1']), rational.matrix(case['cochains'])
    for point in case['points']:
        path = rational.matrix(point['path'])
        target = [[Q(int(i == point['vertex'])-int(i == 0))] for i in range(len(b1))]
        assert rational.product(b1, path) == target
        expected_period = [row[0] % 1 for row in rational.product(rational.transpose(cochains), path)] if h else []
        assert expected_period == list(map(Q, point['period_coordinates']))
        coordinates = [sum(row[i]*expected_period[i] for i in range(h)) % 1 for row in rational.transpose(qbasis)]
        assert coordinates == list(map(Q, point['lattice_coordinates']))
        a = k.matrix([[v] for v in coordinates[:g]])
        b = k.matrix([[v] for v in coordinates[g:]])
        expected_real, expected_imag = k.sum(a, k.product(real, b)), k.product(imaginary, b)
        assert [[list(expected_real[i][0]), list(expected_imag[i][0])] for i in range(g)] == [[[Q(q) for q in half] for half in value] for value in point['coordinates']]
    return dict(name=case['name'], period_rank=h, field_degree=k.d, sector_dimensions=dimensions, points=len(case['points']))


def prototype_cycles(p):
    r, u, n = p['right'], p['up'], p['n']
    s, f = len(r), len(r)*n*n
    parent, direction, order = [-1]*s, [0]*s, [0]
    seen = {0}
    for square in order:
        for d, target in enumerate([r[square], u[square]]):
            if target not in seen:
                seen.add(target)
                parent[target], direction[target] = square, d
                order.append(target)
    def step(values, square, d, sign):
        for t in range(n):
            values[(square*n+t)*n if d == 0 else f+square*n*n+t] += sign
    def path(values, square, sign):
        while parent[square] >= 0:
            step(values, parent[square], direction[square], sign)
            square = parent[square]
    columns = []
    for name in p['cycles']:
        values = [Q(0)]*(2*f)
        if name[0] in ['a', 'b']:
            for square in name[2:-1]:
                step(values, int(square), 0 if name[0] == 'a' else 1, 1)
        else:
            match = re.fullmatch(r'g\((\d+)([RU])\)', name)
            assert match
            square, d = int(match[1]), int(match[2] == 'U')
            target = r[square] if d == 0 else u[square]
            path(values, square, 1)
            step(values, square, d, 1)
            path(values, target, -1)
        columns.append(values)
    return rational.transpose(columns)


def parse_prototype(k, value, radicand):
    value = str(value)
    value = re.sub(r'(^|[+-])sqrt', lambda m: m[1]+'1*sqrt', value)
    if 'sqrt' not in value:
        return k.element(Q(value))
    match = re.fullmatch(r'([^*]+)\*sqrt\(([^)]+)\)', value)
    assert match
    leading = match[1]
    split = max(leading.rfind('+'), leading.rfind('-', 1))
    a, b = (Q(leading[:split]), Q(leading[split:])) if split > 0 else (Q(0), Q(leading))
    assert Q(match[2]) == radicand
    assert k.d == 2
    ratio = radicand/k.radicand
    top, bottom = isqrt(ratio.numerator), isqrt(ratio.denominator)
    assert top*top == ratio.numerator and bottom*bottom == ratio.denominator
    return k.element([a, b*Q(top, bottom)])


def align_prototype(case, prototype):
    k = ExactField(case['polynomial'], case['embedding'])
    cycles = prototype_cycles(prototype)
    assert not any(x for row in rational.product(rational.matrix(case['b1']), cycles) for x in row)
    cycle_change = rational.product(rational.transpose(rational.matrix(case['cochains'])), cycles)
    assert all(x.denominator == 1 for row in cycle_change for x in row) and abs(rational.determinant(cycle_change)) == 1
    cochain_change = rational.transpose(rational.inverse(cycle_change))
    a, ai = cycle_change, rational.inverse(cycle_change)
    assert rational.product(ai, rational.product(rational.matrix(case['gram']), rational.transpose(ai))) == rational.matrix(prototype['G'])
    assert rational.product(ai, rational.product(rational.matrix(case['cup']), rational.transpose(ai))) == rational.matrix(prototype['Omega'])
    radicand = Q(prototype['field_radicand'])
    parse = lambda a: [[parse_prototype(k, x, radicand) for x in row] for row in a]
    transported_j = k.product(k.matrix(rational.transpose(a)), k.product(k.matrix(case['J']), k.matrix(rational.transpose(ai))))
    assert transported_j == parse(prototype['J'])
    old_basis = rational.product(cochain_change, rational.matrix(prototype['symplectic_basis']))
    assert all(x.denominator == 1 for row in old_basis for x in row) and abs(rational.determinant(old_basis)) == 1
    x, y = periods(k, k.matrix(case['cup']), k.matrix(case['J']), k.matrix(old_basis))
    assert x == parse(prototype['tau_X']) and y == parse(prototype['tau_Y'])
    encode_q = lambda a: [[str(x) for x in row] for row in a]
    encode_e = lambda a: [[[str(q) for q in x] for x in row] for row in a]
    return dict(name=case['name'], prototype_name=prototype['name'], n=prototype['n'], cycle_change=encode_q(cycle_change), cochain_change=encode_q(cochain_change), symplectic_in_native_frame=encode_q(old_basis), real=encode_e(x), imaginary=encode_e(y), exact_period_tables_reproduced=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('--prototype-input', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    cases = json.loads(args.input.read_text())
    results = [verify(c) for c in cases]
    by_name = {c['name']: c for c in cases}
    alignments = [align_prototype(by_name[p['native_name']], p) for p in json.loads(args.prototype_input.read_text())['cases']] if args.prototype_input else []
    controls = []
    for kind in ['missing-frequency', 'duplicate-frequency', 'wrong-root', 'negative-root', 'zero-root', 'wrong-projector', 'wrong-normalizer', 'wrong-J', 'wrong-basis', 'wrong-period', 'wrong-point', 'wrong-embedding']:
        bad = deepcopy(cases[5])
        if kind == 'missing-frequency':
            bad['sectors'].pop()
        elif kind == 'duplicate-frequency':
            bad['sectors'].append(deepcopy(bad['sectors'][0]))
        elif kind == 'wrong-root':
            bad['sectors'][1]['root'][0] = '3'
        elif kind == 'negative-root':
            bad['sectors'][1]['root'][0] = '-2'
        elif kind == 'zero-root':
            bad['sectors'][1]['root'][0] = '0'
        elif kind == 'wrong-projector':
            bad['sectors'][0]['projector'][0][0][0] = str(Q(bad['sectors'][0]['projector'][0][0][0])+1)
        elif kind == 'wrong-normalizer':
            bad['normalization'][0][0][0] = str(Q(bad['normalization'][0][0][0])+1)
        elif kind == 'wrong-J':
            bad['J'][0][0][0] = str(Q(bad['J'][0][0][0])+1)
        elif kind == 'wrong-basis':
            for row in bad['basis']:
                row[0] = str(2*Q(row[0]))
        elif kind == 'wrong-period':
            bad['imaginary'][0][0][0] = str(Q(bad['imaginary'][0][0][0])+1)
        elif kind == 'wrong-point':
            bad['points'][0]['coordinates'][0][0][0] = '1'
        else:
            bad = deepcopy(cases[7])
            bad['embedding'] = ['-2', '-1']
        try:
            verify(bad)
        except (AssertionError, ValueError, IndexError, StopIteration, ZeroDivisionError):
            controls.append(kind)
        else:
            raise AssertionError('reference admitted '+kind)
    report = dict(status='passed', case_count=len(results), cases=results, prototype_alignments=alignments, oracle_refusal_controls=controls, arithmetic='stdlib Fraction quotient arithmetic, rational regular inverses and exact real quadratic signs')
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print('Independent normalizations passed:', len(results), 'cases,', len(alignments), 'original prototype tables and', len(controls), 'tamper controls')


if __name__ == '__main__':
    main()
