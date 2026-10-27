#!/usr/bin/env python3
"""Independent exact rational factor/saturation/isogeny reference.

Consumes test_elliptic_factor_construction --dump. Quotient field and rational
matrix references are independent of native Smith actions. Prototype comparison
retains explicit SL2(Z) words for fixed finite examples; no modular reduction
cap or discovery claim enters the production API.
"""
import argparse
from copy import deepcopy
from fractions import Fraction as Q
from functools import reduce
from itertools import combinations
from math import gcd, lcm, prod
from pathlib import Path
import json
import verify_period_construction_reference as rational
from verify_finite_normalization_reference import ExactField


def primitive_basis(a):
    assert all(len(row) == 2 for row in a)
    assert all(x.denominator == 1 for row in a for x in row)
    minors = [a[i][0]*a[j][1]-a[i][1]*a[j][0] for i, j in combinations(range(len(a)), 2)]
    assert reduce(gcd, (abs(d.numerator) for d in minors), 0) == 1


def verify(case):
    k = ExactField(case['polynomial'], case['embedding'])
    gram, cup, square = [rational.matrix(case[x]) for x in ['gram', 'cup', 'square']]
    h = len(gram)
    star = rational.product(rational.inverse(cup), gram)
    assert square == rational.product(star, star)
    fsquare, fstar, identity = k.matrix(square), k.matrix(star), k.identity(h)
    mus = [k.element(s['mu']) for s in case['sectors']]
    assert len(set(mus)) == len(mus)
    full = k.matrix([[0]*h for _ in range(h)])
    normalization = deepcopy(full)
    rational_frequencies = True
    for i, sector in enumerate(case['sectors']):
        mu, root = mus[i], k.element(sector['root'])
        assert k.times(root, root) == mu and k.sign(root) > 0 and k.sign(mu) > 0
        projector = identity
        for j in range(len(mus)):
            if i != j:
                projector = k.product(projector, k.scale(k.sum(fsquare, k.scale(identity, mus[j])), k.inverse(k.minus(mus[j], mu))))
        assert k.product(projector, projector) == projector
        assert len(k.eliminate(projector)[1]) == sector['dimension'] and sector['dimension'] > 0
        full = k.sum(full, projector)
        normalization = k.sum(normalization, k.scale(projector, k.inverse(root)))
        rational_frequencies = rational_frequencies and all(x == 0 for x in mu[1:])
    assert full == identity and sum(s['dimension'] for s in case['sectors']) == h
    structure = k.matrix(case['J'])
    assert structure == k.product(fstar, normalization)
    assert k.product(structure, structure) == k.scale(identity, k.element(-1))
    k.positive(k.product(k.matrix(cup), structure))
    eligible = rational_frequencies and all(s['dimension'] == 2 for s in case['sectors'])
    if case['status'] == 'unsupported_profile':
        assert not eligible and 'factors' not in case and 'index' not in case
        return dict(name=case['name'], status=case['status'], normalization_remains_valid=True)
    assert case['status'] == 'constructed' and eligible and len(case['factors'])*2 == h
    assert len(case['factors']) == len(case['sectors'])
    inclusion = [[] for _ in range(h)]
    polarizations, lattices = [], []
    for i, factor in enumerate(case['factors']):
        mu = Q(factor['mu'])
        assert k.element(mu) == mus[i]
        equation = deepcopy(square)
        for j in range(h):
            equation[j][j] += mu
        denominator = lcm(*(x.denominator for row in equation for x in row))
        assert Q(factor['denominator']) == denominator
        integral = rational.matrix(factor['equation'])
        assert integral == [[denominator*x for x in row] for row in equation]
        assert len(rational.pivots(integral)) == h-2
        kernel, lattice = rational.matrix(factor['kernel']), rational.matrix(factor['saturated'])
        primitive_basis(kernel)
        primitive_basis(lattice)
        assert rational.matrix(factor['declared']) == kernel
        assert all(x == 0 for row in rational.product(integral, kernel) for x in row)
        assert all(x == 0 for row in rational.product(integral, lattice) for x in row)
        pairing = rational.product(rational.transpose(lattice), rational.product(cup, lattice))
        w = Q(factor['intersection'])
        assert w > 0 and w.denominator == 1 and pairing == [[0, w], [-w, 0]]
        restricted = rational.product(rational.transpose(lattice), rational.product(gram, lattice))
        assert restricted == rational.matrix(factor['gram'])
        assert restricted[0][0] > 0 and rational.determinant(restricted) == mu*w*w
        root = k.element(case['sectors'][i]['root'])
        real = k.element(-restricted[0][1]/restricted[1][1])
        imaginary = k.times(k.element(w/restricted[1][1]), root)
        assert [list(real), list(imaginary)] == [[Q(x) for x in part] for part in factor['modulus']]
        assert k.sign(imaginary) > 0
        assert k.product(structure, k.matrix(lattice)) == k.scale(k.product(fstar, k.matrix(lattice)), k.inverse(root))
        for earlier in lattices:
            assert all(x == 0 for row in rational.product(rational.transpose(earlier), rational.product(cup, lattice)) for x in row)
        lattices.append(lattice)
        polarizations.append(int(w))
        for row, values in zip(inclusion, lattice):
            row.extend(values)
    assert inclusion == rational.matrix(case['inclusion'])
    index = abs(rational.determinant(inclusion))
    assert index.denominator == 1 and index >= 1 and index == Q(case['index'])
    assert index == prod(polarizations)
    return dict(name=case['name'], status=case['status'], factors=len(lattices), period_rank=h, index=int(index), polarizations=polarizations)


def mobius(pair, mu, word):
    a, b = pair
    aa, ab = word[0]
    ba, bb = word[1]
    denominator = (ba*a+bb)**2+(ba*b)**2*mu
    assert denominator > 0 and aa*bb-ab*ba == 1
    return (((aa*a+ab)*(ba*a+bb)+aa*ba*b*b*mu)/denominator, b/denominator)


def align_modulus(a, b, mu, target):
    initial, word = (a, b), [[1, 0], [0, 1]]
    assert mu > 0 and b > 0
    for _ in range(4096):
        shift = (a+Q(1, 2)) // 1
        a -= shift
        word = [[word[0][j]-shift*word[1][j] for j in range(2)], word[1]]
        norm = a*a+b*b*mu
        if norm >= 1:
            if (a, b) != target and norm == 1:
                a = -a
                word = [[-word[1][j] for j in range(2)], word[0]]
            assert (a, b) == target
            assert mobius(initial, mu, word) == target
            return word
        a, b = -a/norm, b/norm
        word = [[-word[1][j] for j in range(2)], word[0]]
    raise AssertionError('fixed prototype alignment did not complete; no certificate issued')


def compare_prototype(case, prototype):
    assert case['status'] == 'constructed'
    blocks = prototype['sub_tori']['blocks']
    assert Q(case['index']) == Q(prototype['sub_tori']['isogeny_index'])
    assert len(blocks) == len(case['factors'])
    alignments = []
    for i, factor in enumerate(case['factors']):
        mu = Q(factor['mu'])
        old = next(b for b in blocks if Q(b['mu']) == mu)
        assert Q(old['polarization']) == Q(factor['intersection'])
        gram = rational.matrix(factor['gram'])
        a, b = -gram[0][1]/gram[1][1], Q(factor['intersection'])/gram[1][1]
        target = Q(old['tau_a']), Q(old['tau_b'])
        word = align_modulus(a, b, mu, target)
        alignments.append(dict(factor=i, mu=str(mu), intersection=factor['intersection'], word=word, expected_real=str(target[0]), expected_imaginary_root_multiple=str(target[1])))
    return dict(name=case['name'], original_index=prototype['sub_tori']['isogeny_index'], factor_alignments=alignments, exact_moduli_reproduced=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('--prototype-input', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    cases = json.loads(args.input.read_text())
    results = [verify(case) for case in cases]
    by_name = {c['name']: c for c in cases}
    prototypes = [compare_prototype(by_name[p['native_name']], p) for p in json.loads(args.prototype_input.read_text())['cases']] if args.prototype_input else []
    controls = []
    for kind in ['nonprimitive-kernel', 'incomplete-kernel', 'wrong-frequency', 'wrong-denominator', 'wrong-equation', 'nonprimitive-factor', 'wrong-intersection', 'wrong-gram', 'wrong-modulus', 'wrong-index', 'wrong-inclusion', 'false-profile-acceptance']:
        bad = deepcopy(next(c for c in cases if c['name'] == 'native-coupled-genus-two'))
        if kind == 'nonprimitive-kernel':
            for key in ['kernel', 'declared']:
                for row in bad['factors'][0][key]:
                    row[0] = str(2*Q(row[0]))
        elif kind == 'incomplete-kernel':
            for key in ['kernel', 'declared']:
                bad['factors'][0][key] = [row[:1] for row in bad['factors'][0][key]]
        elif kind == 'wrong-frequency':
            bad['factors'][0]['mu'] = '2'
        elif kind == 'wrong-denominator':
            bad['factors'][0]['denominator'] = str(2*Q(bad['factors'][0]['denominator']))
        elif kind == 'wrong-equation':
            bad['factors'][0]['equation'][0][0] = str(Q(bad['factors'][0]['equation'][0][0])+1)
        elif kind == 'nonprimitive-factor':
            for row in bad['factors'][0]['saturated']:
                row[0] = str(2*Q(row[0]))
        elif kind == 'wrong-intersection':
            bad['factors'][0]['intersection'] = '1'
        elif kind == 'wrong-gram':
            bad['factors'][0]['gram'][0][0] = str(Q(bad['factors'][0]['gram'][0][0])+1)
        elif kind == 'wrong-modulus':
            bad['factors'][0]['modulus'][0][0] = str(Q(bad['factors'][0]['modulus'][0][0])+1)
        elif kind == 'wrong-index':
            bad['index'] = '1'
        elif kind == 'wrong-inclusion':
            bad['inclusion'][0][0] = str(Q(bad['inclusion'][0][0])+1)
        else:
            bad = deepcopy(next(c for c in cases if c['name'] == 'native-repeated-rank-four'))
            bad['status'] = 'constructed'
        try:
            verify(bad)
        except (AssertionError, ValueError, IndexError, KeyError, StopIteration):
            controls.append(kind)
        else:
            raise AssertionError('reference admitted '+kind)
    report = dict(status='passed', case_count=len(results), admitted_cases=sum(c['status'] == 'constructed' for c in results), unsupported_cases=sum(c['status'] == 'unsupported_profile' for c in results), factor_count=sum(c.get('factors', 0) for c in results), cases=results, prototype_alignments=prototypes, oracle_refusal_controls=controls, arithmetic='stdlib Fraction matrices, quotient field references and exhaustive primitive rank-two minors')
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print('Independent factors passed:', report['admitted_cases'], 'admitted cases,', report['unsupported_cases'], 'profile refusals,', report['factor_count'], 'factors,', len(prototypes), 'prototype families and', len(controls), 'tamper controls')


if __name__ == '__main__':
    main()
