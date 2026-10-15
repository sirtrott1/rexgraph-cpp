"""Independent Fraction equations for the complete native temporal comparison."""
from fractions import Fraction as F
import json
import subprocess
import sys


def matrix(raw, rows, columns):
    assert len(raw) == rows and all(len(row) == columns for row in raw)
    return [list(map(F, row)) for row in raw]


def mv(a, x):
    assert all(len(row) == len(x) for row in a)
    return [sum(q * v for q, v in zip(row, x)) for row in a]


def mm(a, b, columns):
    assert all(len(row) == len(b) for row in a)
    return [[sum(row[k] * b[k][j] for k in range(len(b))) for j in range(columns)] for row in a]


def sub(a, b):
    assert len(a) == len(b)
    return [x - y for x, y in zip(a, b)]


def difference(a, b):
    return [sub(row, other) for row, other in zip(a, b)]


def solve(a, rhs):
    assert len(a) == len(rhs) and all(len(row) == len(rhs) for row in a)
    rows = [list(row) + [value] for row, value in zip(a, rhs)]
    for k in range(len(rhs)):
        pivot = next(i for i in range(k, len(rhs)) if rows[i][k])
        rows[k], rows[pivot] = rows[pivot], rows[k]
        divisor = rows[k][k]
        rows[k] = [q / divisor for q in rows[k]]
        for i in range(len(rhs)):
            if i != k:
                coefficient = rows[i][k]
                rows[i] = sub(rows[i], [coefficient * q for q in rows[k]])
    return [row[-1] for row in rows]


done = subprocess.run([sys.argv[1], '--emit'], capture_output=True, text=True, check=True)
assert not done.stderr
records = [json.loads(line) for line in done.stdout.splitlines()]
assert len(records) == 432
rectangular = empty_outputs = zero_shifts = 0
for case in records:
    n, m, p, q, o, on = [case[k] for k in ['n', 'm', 'p', 'q', 'o', 'on']]
    shapes = {'L': (n, n), 'Lp': (m, m), 'I': (n, p), 'Ip': (m, q), 'J': (m, n), 'K': (q, p),
              'A': (o, n), 'Ap': (on, m), 'KA': (on, o), 'EI': (m, p), 'EL': (m, n), 'EA': (on, n)}
    maps = {k: matrix(case[k], *shape) for k, shape in shapes.items()}
    values = {k: list(map(F, case[k])) for k in ['u', 'up', 'g', 'gp', 'zeta', 'realization',
        'innovation', 'operator', 'aggregate', 'accession', 'obs_realization', 'obs_innovation', 'obs_operator', 'observed']}
    assert len(values['u']) == p and len(values['up']) == q
    lam = F(case['lambda'])
    old_system = [[F(i == j) + lam * maps['L'][i][j] for j in range(n)] for i in range(n)]
    new_system = [[F(i == j) + lam * maps['Lp'][i][j] for j in range(m)] for i in range(m)]
    g, gp = solve(old_system, mv(maps['I'], values['u'])), solve(new_system, mv(maps['Ip'], values['up']))
    assert g == values['g'] and gp == values['gp']
    ei = difference(mm(maps['Ip'], maps['K'], p), mm(maps['J'], maps['I'], p))
    el = difference(mm(maps['Lp'], maps['J'], n), mm(maps['J'], maps['L'], n))
    ea = difference(mm(maps['Ap'], maps['J'], n), mm(maps['KA'], maps['A'], n))
    assert ei == maps['EI'] and el == maps['EL'] and ea == maps['EA']
    zeta = sub(values['up'], mv(maps['K'], values['u']))
    terms = [solve(new_system, mv(ei, values['u'])), solve(new_system, mv(maps['Ip'], zeta)),
             [-lam * x for x in solve(new_system, mv(el, g))]]
    assert zeta == values['zeta']
    assert terms == [values[k] for k in ['realization', 'innovation', 'operator']]
    assert [sum(term[i] for term in terms) for i in range(m)] == values['aggregate'] == sub(gp, mv(maps['J'], g))
    observed_terms = [mv(ea, g)] + [mv(maps['Ap'], term) for term in terms]
    assert observed_terms == [values[k] for k in ['accession', 'obs_realization', 'obs_innovation', 'obs_operator']]
    assert [sum(term[i] for term in observed_terms) for i in range(on)] == values['observed'] == sub(mv(maps['Ap'], gp), mv(maps['KA'], mv(maps['A'], g)))
    rectangular += n != m or p != q or o != on
    empty_outputs += on == 0
    zero_shifts += lam == 0
assert rectangular == 420 and empty_outputs == zero_shifts == 144
print(f'native_green_fraction_cases={len(records)} rectangular_cases={rectangular} empty_outputs={empty_outputs} zero_shifts={zero_shifts}')
