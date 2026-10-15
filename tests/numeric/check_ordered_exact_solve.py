"""Independent Fraction equations, permutation signs and pivot receipts."""
import argparse
from fractions import Fraction as F
import json
from pathlib import Path
import subprocess


def solve(matrix, rhs):
    n = len(rhs)
    rows = [list(a) + [b] for a, b in zip(matrix, rhs)]
    for k in range(n):
        pivot = next(i for i in range(k, n) if rows[i][k])
        rows[k], rows[pivot] = rows[pivot], rows[k]
        divisor = rows[k][k]
        rows[k] = [q / divisor for q in rows[k]]
        for i in range(n):
            if i != k:
                ratio = rows[i][k]
                rows[i] = [q - ratio * p for q, p in zip(rows[i], rows[k])]
    return [row[-1] for row in rows]


def determinant(matrix):
    rows = [list(row) for row in matrix]
    answer = F(1)
    for k in range(len(rows)):
        pivot = next(i for i in range(k, len(rows)) if rows[i][k])
        if pivot != k:
            answer = -answer
            rows[k], rows[pivot] = rows[pivot], rows[k]
        divisor = rows[k][k]
        answer *= divisor
        for i in range(k + 1, len(rows)):
            ratio = rows[i][k] / divisor
            rows[i] = [q - ratio * p for q, p in zip(rows[i], rows[k])]
    return answer


def parity(order):
    inversions = sum(a > b for i, a in enumerate(order) for b in order[i + 1:])
    return -1 if inversions % 2 else 1


def degree_order(matrix):
    n = len(matrix)
    neighbors = {i: {j for j in range(n) if i != j and (matrix[i][j] or matrix[j][i])}
                 for i in range(n)}
    result = []
    while neighbors:
        chosen = min(neighbors, key=lambda i: (len(neighbors[i]), i))
        remaining = neighbors.pop(chosen)
        for i in remaining:
            neighbors[i].discard(chosen)
            neighbors[i].update(remaining - {i})
        result.append(chosen)
    return result


def replay_receipt(record, matrix):
    row_order, col_order = record['rows'], record['columns']
    n = len(matrix)
    assert sorted(row_order) == sorted(col_order) == list(range(n))
    rows = [[matrix[i][j] for j in col_order] for i in row_order]
    sign, fill, cancellations, eliminations = 1, 0, 0, 0
    pivots = []
    for k, slot in enumerate(record['pivot_slots']):
        candidates = [i for i in range(k, n) if rows[i][k]]
        expected = min(candidates, key=lambda i: (sum(q != 0 for q in rows[i]), i))
        assert slot == expected
        if k != slot:
            rows[k], rows[slot] = rows[slot], rows[k]
            sign = -sign
        pivot = rows[k][k]
        pivots.append(pivot)
        for i in range(k + 1, n):
            if not rows[i][k]:
                continue
            eliminations += 1
            ratio = rows[i][k] / pivot
            rows[i][k] = F(0)
            for j in range(k + 1, n):
                old = rows[i][j]
                new = old - ratio * rows[k][j]
                fill += old == 0 and new != 0
                cancellations += old != 0 and new == 0
                rows[i][j] = new
    signs = [parity(row_order), parity(col_order), sign]
    assert signs == record['signs']
    assert pivots == list(map(F, record['pivot_values']))
    assert fill == record['fill'] and cancellations == record['cancellations']
    assert eliminations == record['elimination_entries']
    assert sum(q != 0 for row in rows for q in row) == record['upper_entries']
    answer = F(signs[0] * signs[1] * signs[2])
    for q in pivots:
        answer *= q
    assert answer == F(record['determinant']) == determinant(matrix)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    done = subprocess.run([args.executable, '--emit'], check=True, capture_output=True, text=True)
    records = [json.loads(line) for line in done.stdout.splitlines()]
    assert len(records) == 324 and not done.stderr
    groups = []
    for start in range(0, len(records), 6):
        group = records[start:start + 6]
        assert [r['mode'] for r in group] == [0, 0, 1, 1, 2, 2]
        matrix = [list(map(F, row)) for row in group[0]['matrix']]
        order = degree_order(matrix)
        for record in group:
            assert record['matrix'] == group[0]['matrix']
            rhs = list(map(F, record['rhs']))
            primal = solve(matrix, rhs)
            assert primal == list(map(F, record['primal']))
            assert solve(list(zip(*matrix)), rhs) == list(map(F, record['transpose']))
            assert [primal[-1], primal[0], primal[-1]] == list(map(F, record['selected']))
            replay_receipt(record, matrix)
            if record['mode'] == 1:
                assert record['rows'] == record['columns'] == order
            elif record['mode'] == 0:
                assert record['rows'] == record['columns'] == list(range(len(matrix)))
        groups.append(group)
    metrics = ['fill', 'upper_entries', 'elimination_entries', 'peak_rational_entries',
               'peak_index_entries', 'prepare_steps']
    comparison = {}
    for metric in metrics + ['preparation_plus_planning_steps']:
        totals = dict(wins=0, ties=0, losses=0)
        for group in groups:
            natural, ordered = group[0], group[2]
            if metric == 'preparation_plus_planning_steps':
                a = natural['prepare_steps']
                b = ordered['prepare_steps'] + ordered['planning_steps']
            else:
                a, b = natural[metric], ordered[metric]
            totals['wins' if b < a else 'losses' if b > a else 'ties'] += 1
        comparison[metric] = totals
    profile = dict(exact_cases=len(records), matrices=len(groups),
                   comparison='Declared greedy minimum degree versus unchanged natural default; lower is better',
                   metrics=comparison, elapsed_time_or_total_memory_measured=False)
    if args.output:
        args.output.write_text(json.dumps(profile, indent=2) + '\n')
    print(f'ordered_exact_fraction_cases={len(records)} matrices={len(groups)}')


if __name__ == '__main__':
    main()
