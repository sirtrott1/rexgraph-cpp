"""Independent Fraction actions, complete auxiliaries and standalone packets."""
from fractions import Fraction as Q
import json
from pathlib import Path
import subprocess
import sys
import tempfile

def matrix(rows):
    return [[Q(x) for x in row] for row in rows]
def transpose(a):
    return list(map(list, zip(*a))) if a else []
def multiply(a, b):
    return [[sum((x*y for x, y in zip(row, col)), Q(0)) for col in zip(*b)] for row in a]
def apply(a, x):
    return [sum((p*q for p, q in zip(row, x)), Q(0)) for row in a]
def solve(a, b):
    rows = [list(row)+[x] for row, x in zip(a, b)]
    n = len(rows)
    for k in range(n):
        p = next(i for i in range(k, n) if rows[i][k])
        rows[k], rows[p] = rows[p], rows[k]
        q = rows[k][k]
        rows[k] = [x/q for x in rows[k]]
        for i in range(n):
            if i != k:
                q = rows[i][k]
                rows[i] = [x-q*y for x, y in zip(rows[i], rows[k])]
    return [row[-1] for row in rows]
def inverse(a):
    n = len(a)
    columns = [solve(a, [Q(i == j) for i in range(n)]) for j in range(n)]
    return transpose(columns)
def rank(a):
    rows = [list(row) for row in a]
    k = 0
    for j in range(len(rows[0]) if rows else 0):
        p = next((i for i in range(k, len(rows)) if rows[i][j]), None)
        if p is None:
            continue
        rows[k], rows[p] = rows[p], rows[k]
        q = rows[k][j]
        rows[k] = [x/q for x in rows[k]]
        for i in range(k+1, len(rows)):
            q = rows[i][j]
            rows[i] = [x-q*y for x, y in zip(rows[i], rows[k])]
        k += 1
    return k

binary = str(Path(sys.argv[1]).resolve())
cases = json.loads(subprocess.check_output([binary, '--json'], text=True))
assert len(cases) == 24
for case in cases:
    b, c, n, nb, nu, h = [matrix(case[key]) for key in ('B', 'C', 'N', 'Nb', 'Nu', 'H')]
    assert b == matrix([[1,1,1,1],[2,2,2,2],[-1,-1,-1,-1],[1,1,1,1],[0,0,0,0],[0,0,0,0],[0,0,0,0]])
    assert c == matrix([[1,2,0],[-1,-2,0],[1,2,0],[-1,-2,0]])
    assert multiply(b, c) == [[0]*3 for _ in b]
    l = multiply(multiply(multiply(inverse(n), transpose(b)), nb), b)
    up = multiply(multiply(multiply(c, inverse(nu)), transpose(c)), n)
    l = [[x+y for x,y in zip(row,col)] for row,col in zip(l,up)]
    j = [Q(x) for x in case['j']]
    lam = Q(case['lambda'])
    if case['kind']:
        constraints = b + multiply(transpose(c), n)
        assert len(h[0]) == 2 and rank(h) == 2 and rank(constraints) == 2
        assert multiply(constraints, h) == [[0]*2 for _ in constraints]
        dual = multiply(transpose(h), n)
        system = [l[i]+h[i] for i in range(4)] + [row+[Q(0)]*2 for row in dual]
        response = solve(system, j+[Q(0)]*2)
        x, t = response[:4], response[4:]
    else:
        system = [[Q(i == k)+lam*l[i][k] for k in range(4)] for i in range(4)]
        x, t = solve(system, j), []
    assert x == [Q(q) for q in case['x']]
    a = apply(n, x)
    lower = apply(b, x)
    lower_dual = apply(nb, lower)
    upper_dual = apply(transpose(c), a)
    upper = solve(nu, upper_dual)
    f = apply(c, upper)
    g = apply(n, f)
    full = a+lower+lower_dual+upper_dual+f+g+upper+t+x
    assert full == [Q(q) for q in case['full']]
    assert apply(matrix(case['aug']), full) == [Q(q) for q in case['rhs']]
    if not case['kind'] and not lam:
        assert x == j
with tempfile.TemporaryDirectory(prefix='rexgraph-local-independent-') as directory:
    folder = Path(directory)
    subprocess.check_call([binary, '--export', directory])
    assert sorted(p.name for p in folder.iterdir()) == ['authority.bin', 'child.packet', 'fixed.packet']
    for name in ('fixed.packet', 'child.packet'):
        subprocess.check_call([binary, '--serve', str(folder/name), str(folder/'authority.bin')], cwd=directory)
print('24 independent complete Fraction cases; 2 standalone model-file-absent packet processes')
