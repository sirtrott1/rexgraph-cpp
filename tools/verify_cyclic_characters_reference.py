#!/usr/bin/env python3
"""Independent Fraction ranks of regular and cyclotomic rational blocks.

Input is --reference output from rfc_cyclic_character_tests. Blocks are rebuilt
from the original retained occurrence coefficients/voltages, never from the
production aggregated formal boundary or a production rank/specialization.
"""
import argparse
from fractions import Fraction as Q
import json
from math import gcd
from pathlib import Path


def rank(a, cols):
    a = [list(row) for row in a]
    pivot = 0
    for col in range(cols):
        chosen = next((j for j in range(pivot, len(a)) if a[j][col]), None)
        if chosen is None:
            continue
        a[pivot], a[chosen] = a[chosen], a[pivot]
        q = a[pivot][col]
        a[pivot] = [v/q for v in a[pivot]]
        for j in range(len(a)):
            if j != pivot and a[j][col]:
                q = a[j][col]
                a[j] = [x-q*y for x,y in zip(a[j], a[pivot])]
        pivot += 1
    return pivot


def trim(p):
    while p and p[-1] == 0:
        p.pop()
    return p


def divide(a, b):
    a = list(a)
    quotient = [Q(0)]*max(0,len(a)-len(b)+1)
    while len(a) >= len(b):
        shift = len(a)-len(b)
        q = a[-1]/b[-1]
        quotient[shift] += q
        for j, value in enumerate(b):
            a[shift+j] -= q*value
        trim(a)
    return trim(quotient), a


def cyclotomic(m):
    factors = {}
    for d in range(1,m+1):
        if m % d:
            continue
        p = [Q(-1)]+[Q(0)]*(d-1)+[Q(1)]
        for k,factor in factors.items():
            if d % k == 0:
                p,remainder = divide(p,factor)
                assert not remainder
        factors[d] = p
    return factors[m]


def power_column(exponent, polynomial):
    return divide([Q(0)]*exponent+[Q(1)],polynomial)[1]


def boundaries(case, m=None):
    size = case['n'] if m is None else len(cyclotomic(m))-1
    polynomial = None if m is None else cyclotomic(m)
    cells = case['cells']
    matrices = [None]+[[[Q(0) for _ in range(cells[k]*size)]
                        for _ in range(cells[k-1]*size)] for k in range(1,len(cells))]
    for k,row,col,voltage,exists,q in case['occurrences']:
        assert 0 <= voltage < case['n']
        if not exists:
            continue
        for j in range(size):
            if m is None:
                matrices[k][row*size+(j+voltage)%size][col*size+j] += Q(q)
            else:
                for i,value in enumerate(power_column(j+voltage,polynomial)):
                    matrices[k][row*size+i][col*size+j] += Q(q)*value
    return size,matrices


def homology(case, m=None):
    size,matrices = boundaries(case,m)
    cells = case['cells']
    ranks = [0]+[rank(matrices[k],cells[k]*size) for k in range(1,len(cells))]+[0]
    for k in range(2,len(cells)):
        a,b = matrices[k-1],matrices[k]
        for row in a:
            for j in range(cells[k]*size):
                assert sum(row[i]*b[i][j] for i in range(cells[k-1]*size)) == 0
    out = [cells[k]*size-ranks[k]-ranks[k+1] for k in range(len(cells))]
    if m is not None:
        assert all(x % size == 0 for x in out)
        out = [x//size for x in out]
    return out


def verify(cases):
    results = []
    original_covers = set()
    for case in cases:
        n = case['n']
        assert n in [1,2,3,4,5,6,7,8,12] and max(case['cells'],default=0) <= 3
        expected = [d for d in range(1,n+1) if n%d == 0]
        assert [x['order'] for x in case['sectors']] == expected
        total = [0]*len(case['cells'])
        for sector in case['sectors']:
            m = sector['order']
            phi = sum(gcd(j,m)==1 for j in range(1,m+1))
            assert sector['weight'] == phi == len(cyclotomic(m))-1
            assert homology(case,m) == sector['betti']
            total = [x+phi*y for x,y in zip(total,sector['betti'])]
        regular = homology(case)
        assert regular == total == case['total'], case
        expected_named = {'torus-self':[1,2,1], 'klein-orientation':[1,2,1],
                          'projective-orientation':[1,0,1], 'moore':[1,0,n-1],
                          'lens':[1,0,0,1], 'lens-direct-sum':[2,0,0,2]}
        if case['name'] in expected_named:
            assert regular == expected_named[case['name']]
        if ((case['name']=='cycle' and n in [2,4]) or case['name'] in
            ['torus-self','klein-orientation','projective-orientation','moore']):
            original_covers.add((case['name'],n))
        results.append(dict(name=case['name'],n=n,basis=case['basis'],betti=regular,
                            independently_checked_sectors=len(expected),passed=True))
    assert len(original_covers) == 10, original_covers
    return dict(status='passed', cases=results, case_count=len(results), original_cover_cases=10,
                sector_count=sum(x['independently_checked_sectors'] for x in results),
                arithmetic='stdlib Fraction; independent full rational block ranks in every grade',
                geometric_scope='The ten named source recipes match the original experiment; the API certifies free group-ring chains, not arbitrary geometric covers.')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('input',type=Path)
    p.add_argument('--output',type=Path)
    args = p.parse_args()
    out = verify(json.loads(args.input.read_text()))
    if args.output:
        args.output.write_text(json.dumps(out,indent=2)+'\n')
    print(json.dumps({k:v for k,v in out.items() if k != 'cases'}))


if __name__ == '__main__':
    main()
