"""Independent rational rank, kernel, image and full Hodge projection checks."""
from fractions import Fraction as Q
import json
import subprocess
import sys


def matrix(raw, rows, cols):
    assert len(raw) == rows and all(len(row) == cols for row in raw)
    return [list(map(Q, row)) for row in raw]


def transpose(a, cols):
    return [[a[i][j] for i in range(len(a))] for j in range(cols)]


def multiply(a, b, cols):
    assert all(len(row) == len(b) for row in a)
    return [[sum(row[k] * b[k][j] for k in range(len(b))) for j in range(cols)] for row in a]


def apply(a, x):
    assert all(len(row) == len(x) for row in a)
    return [sum(q * value for q, value in zip(row, x)) for row in a]


def rref(a, cols):
    a = [row[:] for row in a]
    pivots = []
    for j in range(cols):
        k = len(pivots)
        found = next((i for i in range(k, len(a)) if a[i][j]), None)
        if found is None:
            continue
        a[k], a[found] = a[found], a[k]
        scale = a[k][j]
        a[k] = [value / scale for value in a[k]]
        for i in range(len(a)):
            if i != k:
                scale = a[i][j]
                a[i] = [value - scale * pivot for value, pivot in zip(a[i], a[k])]
        pivots.append(j)
    return a, pivots


def rank(a, cols):
    return len(rref(a, cols)[1])


def solve(a, rhs):
    n = len(rhs)
    assert len(a) == n and all(len(row) == n for row in a)
    reduced, pivots = rref([row + [value] for row, value in zip(a, rhs)], n)
    assert len(pivots) == n
    return [row[-1] for row in reduced]


def zero(a):
    return not any(any(row) for row in a)


done = subprocess.run([sys.argv[1], '--emit'], capture_output=True, text=True, check=True)
assert not done.stderr
records = [json.loads(line) for line in done.stdout.splitlines()]
numeric = native = empty = nonidentity = 0
for item in records:
    if item['kind'] == 'numeric':
        m, n, r = item['rows'], item['cols'], item['rank']
        a = matrix(item['A'], m, n)
        f, c, k = matrix(item['F'], m, r), matrix(item['R'], r, n), matrix(item['N'], n, n-r)
        assert rank(a, n) == rank(f, r) == r and rank(k, n-r) == n-r
        assert multiply(f, c, n) == a and zero(multiply(a, k, n-r))
        reduced, pivots = rref(a, n)
        assert f == [[row[j] for j in pivots] for row in a]
        assert c == reduced[:r]
        numeric += 1
        continue
    n, low, up, g, c, h = [item[k] for k in ['n', 'lower', 'upper', 'g', 'c', 'h']]
    b, u, m = matrix(item['B'], low, n), matrix(item['U'], n, up), matrix(item['M'], n, n)
    frames = [matrix(item[k], n, size) for k, size in zip(['G', 'C', 'H'], [g, c, h])]
    x = list(map(Q, item['x']))
    assert len(x) == n and g+c+h == n
    assert rank(b, n) == g and rank(u, up) == c
    constraint = b + multiply(transpose(u, up), m, n)
    assert n-rank(constraint, n) == h
    assert zero(multiply(b, u, up)) and zero(multiply(constraint, frames[2], h))
    assert m == transpose(m, n)
    # Exact LDL leading pivots certify positivity of the complete declared form.
    lower = [[Q(0)] * n for _ in range(n)]
    diagonal = []
    for i in range(n):
        for j in range(i):
            lower[i][j] = (m[i][j] - sum(lower[i][k]*diagonal[k]*lower[j][k] for k in range(j))) / diagonal[j]
        pivot = m[i][i] - sum(lower[i][k]**2 * diagonal[k] for k in range(i))
        assert pivot > 0
        diagonal.append(pivot)
    projected = []
    for frame, size, name, coordinates in zip(frames, [g, c, h], ['gradient', 'curl', 'harmonic'], ['gc', 'cc', 'hc']):
        assert rank(frame, size) == size
        dual = multiply(transpose(frame, size), m, n)
        coeff = solve(multiply(dual, frame, size), apply(dual, x))
        value = apply(frame, coeff)
        assert coeff == list(map(Q, item[coordinates])) and value == list(map(Q, item[name]))
        projected.append(value)
    for i in range(3):
        for j in range(i+1, 3):
            assert zero(multiply(multiply(transpose(frames[i], [g, c, h][i]), m, n), frames[j], [g, c, h][j]))
    # Certify the gradient spans M^-1 im(B^T) without assuming C++'s frame.
    bt = transpose(b, n)
    adjoint_span = [[Q(0)] * low for _ in range(n)]
    for j in range(low):
        solution = solve(m, [row[j] for row in bt])
        for i, value in enumerate(solution):
            adjoint_span[i][j] = value
    assert rank([left + right for left, right in zip(frames[0], adjoint_span)], g+low) == g
    assert rank([left + right for left, right in zip(frames[1], u)], c+up) == c
    assert [sum(part[i] for part in projected) for i in range(n)] == x == list(map(Q, item['reconstructed']))
    native += 1
    empty += n == 0
    nonidentity += m != [[Q(i == j) for j in range(n)] for i in range(n)]
assert numeric == 144 and native == 120
print(f'native_frame_fraction_numeric={numeric} native={native} empty={empty} nonidentity={nonidentity}')
