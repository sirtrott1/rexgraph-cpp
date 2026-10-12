"""Independent Fraction pair/factored forms and full native comparison actions."""
from fractions import Fraction as Q
import json
import subprocess
import sys


def zero(rows, columns):
    return [[Q(0) for _ in range(columns)] for _ in range(rows)]


def identity(size):
    return [[Q(i == j) for j in range(size)] for i in range(size)]


def transpose(a):
    return list(map(list, zip(*a)))


def multiply(a, b):
    return [[sum((x * y for x, y in zip(row, col)), Q(0)) for col in zip(*b)] for row in a]


def add(a, b, factor=Q(1)):
    return [[x + factor * y for x, y in zip(row, other)] for row, other in zip(a, b)]


def scale(a, q):
    return [[q * x for x in row] for row in a]


def inverse(a):
    n = len(a)
    rows = [list(row) + extra for row, extra in zip(a, identity(n))]
    for k in range(n):
        pivot = next(i for i in range(k, n) if rows[i][k])
        rows[k], rows[pivot] = rows[pivot], rows[k]
        q = rows[k][k]
        rows[k] = [x / q for x in rows[k]]
        for i in range(n):
            if i != k:
                q = rows[i][k]
                rows[i] = [x - q * y for x, y in zip(rows[i], rows[k])]
    return [row[n:] for row in rows]


def block_diagonal(blocks):
    n = sum(map(len, blocks)); result = zero(n, n); offset = 0
    for block in blocks:
        for i, row in enumerate(block):
            for j, q in enumerate(row):
                result[offset + i][offset + j] = q
        offset += len(block)
    return result


def gram(a, metric):
    return multiply(multiply(transpose(a), metric), a)


def field(a, x):
    return [sum((q * v for q, v in zip(row, x)), Q(0)) for row in a]


def energy(form, x):
    return sum((q * v for q, v in zip(x, field(form, x))), Q(0))


def forms(dimensions, metrics, flags, weights, count):
    width = sum(dimensions); offsets = []; offset = 0
    for n in dimensions:
        offsets.append(offset); offset += n
    k_t, k_g, k_c, k_f = [zero(width, width) for _ in range(4)]
    k_c_factored, k_f_factored = zero(width, width), zero(width, width)
    for lower, metric in enumerate(metrics):
        local = []
        for upper, boundary, coefficient, phi, active in flags:
            if boundary != lower:
                continue
            map_ = zero(len(metric), width)
            for i, row in enumerate(phi):
                for j, q in enumerate(row):
                    map_[i][offsets[upper] + j] = Q(q)
            c = Q(coefficient) if active else Q(0)
            beta = Q(bool(c)) if count else abs(c)
            local.append((upper, c, map_, beta))
        b, u, participation = [zero(len(metric), width) for _ in range(3)]
        for e, c, phi, beta in local:
            b = add(b, phi, weights[e] * c)
            u = add(u, phi, weights[e] * abs(c))
            participation = add(participation, phi, beta)
        k_t = add(k_t, gram(b, metric)); k_g = add(k_g, gram(u, metric))
        for index, (e, c, phi, beta) in enumerate(local):
            for f, d, psi, other_beta in local[index + 1:]:
                k_c = add(k_c, gram(add(phi, psi, Q(-1)), metric), beta * other_beta)
                product = weights[e] * weights[f]
                if c * d < 0 and product:
                    sign = Q(1) if product > 0 else Q(-1)
                    k_f = add(k_f, gram(add(phi, psi, -sign), metric), 2 * abs(product * c * d))
        mass = sum((row[3] for row in local), Q(0))
        diagonal = zero(width, width)
        for e, c, phi, beta in local:
            diagonal = add(diagonal, gram(phi, metric), mass * beta)
        k_c_factored = add(k_c_factored, add(diagonal, gram(participation, metric), Q(-1)))
        plus, minus = zero(len(metric), width), zero(len(metric), width)
        plus_mass = sum((abs(weights[e] * c) for e, c, _, _ in local if c > 0), Q(0))
        minus_mass = sum((abs(weights[e] * c) for e, c, _, _ in local if c < 0), Q(0))
        diagonal = zero(width, width)
        for e, c, phi, beta in local:
            del beta
            if c > 0:
                plus = add(plus, phi, weights[e] * abs(c))
                diagonal = add(diagonal, gram(phi, metric), 2 * abs(weights[e] * c) * minus_mass)
            elif c < 0:
                minus = add(minus, phi, weights[e] * abs(c))
                diagonal = add(diagonal, gram(phi, metric), 2 * abs(weights[e] * c) * plus_mass)
        cross = multiply(multiply(transpose(plus), metric), minus)
        k_f_factored = add(k_f_factored, add(diagonal, add(cross, transpose(cross)), Q(-2)))
    assert k_c == k_c_factored and k_f == k_f_factored
    return [k_t, k_g, k_f, k_c]


count = 0
for line in subprocess.check_output([sys.argv[1], '--emit'], text=True).splitlines():
    record = json.loads(line)
    x = list(map(Q, record['input']))
    flags = [(f['upper'], f['lower'], Q(f['coefficient']), [list(map(Q, row)) for row in f['phi']], True) for f in record['flags']]
    weights = list(map(Q, record['weights']))
    lower = [[list(map(Q, row)) for row in block] for block in record['lower_metrics']]
    source_metric = [list(map(Q, row)) for row in record['upper_metric']]
    kt, kg, kf, kc = forms([2,2], lower, flags, weights, record['count'])
    k = [kt,kg,kc,kf][record['channel']]
    action = field(multiply(inverse(source_metric), k), x)
    assert action == list(map(Q, record['output']))
    assert energy(k,x) == Q(record['quadrance']) >= 0
    offsets = [0,2,5]
    signed = [Q(0)]*7
    unsigned = [Q(0)]*7
    for upper, boundary, coefficient, phi, _ in flags:
        value = field(phi,x[2*upper:2*upper+2])
        for i,q in enumerate(value):
            signed[offsets[boundary]+i] += weights[upper]*coefficient*q
            unsigned[offsets[boundary]+i] += weights[upper]*abs(coefficient)*q
    assert signed == list(map(Q, record['signed']))
    assert unsigned == list(map(Q, record['unsigned']))
    count += 1
assert count == 160
print(f'relational_comparison_fraction_records={count}')
