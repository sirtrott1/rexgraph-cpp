"""Independent whole-field Fraction contraction on original ordered flags."""
from fractions import Fraction as Q
import json
import subprocess
import sys


def apply(matrix, field):
    return [sum((Q(a) * b for a, b in zip(row, field)), Q(0)) for row in matrix]


def boundary(record, field):
    dimensions = [2, 3, 2]
    offsets = [0, 2, 5]
    assembled = [Q(0)] * 7
    occurrences = []
    for flag in record['maps']:
        value = apply(flag['matrix'], field[2 * flag['upper']:2 * flag['upper'] + 2])
        coefficient = Q(flag['coefficient'])
        if not flag['active']:
            value = [Q(0)] * len(value)
        elif not flag['realized']:
            value = [coefficient * q for q in value]
        assert len(value) == dimensions[flag['lower']]
        occurrences.extend(value)
        for index, q in enumerate(value):
            assembled[offsets[flag['lower']] + index] += q
    return assembled, occurrences


count = 0
for line in subprocess.check_output([sys.argv[1], '--emit'], text=True).splitlines():
    record = json.loads(line)
    input_field = list(map(Q, record['input']))
    first, occurrences = boundary(record, input_field)
    middle = apply(record['application'], first)
    final, final_occurrences = boundary(record, middle)
    assert first == list(map(Q, record['boundary']))
    assert occurrences == list(map(Q, record['occurrences']))
    assert middle == list(map(Q, record['middle']))
    assert final == list(map(Q, record['final']))
    assert final_occurrences == list(map(Q, record['final_occurrences']))
    assert len(occurrences) == 19 and occurrences[7:12] == [Q(0)] * 5
    count += 1
assert count == 60
print(f'relational_field_program_fraction_cases={count}')
