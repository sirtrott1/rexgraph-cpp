"""Independent complete fields and one common free carrier, before decoding."""
from fractions import Fraction as Q
import json
import subprocess
import sys


def apply(matrix, values):
    return [sum((a*b for a, b in zip(row, values)), Q(0)) for row in matrix]


def product(a, b):
    return [[sum((x*y for x, y in zip(row, col)), Q(0)) for col in zip(*b)] for row in a]


def add(a, b):
    return [x+y for x, y in zip(a, b)]


# Observing x=(1,0,0,0) on the first whole stalk identifies u=2,v=-1.
# w and both inactive/zero intrinsic map parameters remain free together.
p = list(map(Q, [2, -1, 0, 0, 0]))
k = [[Q(0)]*3, [Q(0)]*3, [Q(1), Q(0), Q(0)],
     [Q(0), Q(1), Q(0)], [Q(0), Q(0), Q(1)]]
later = [[Q(v) for v in row] for row in
         [[1, 0, 1, 0, 0, 0, 0], [0, 1, 0, 1, 0, 0, 0],
          [0, 0, 0, 0, 1, 1, 0], [1, 0, 0, 0, 0, 0, 1]]]
count = 0
for line in subprocess.check_output([sys.argv[1], '--emit'], text=True).splitlines():
    row = json.loads(line)
    a, b, c, d = x = list(map(Q, row['input']))
    base = [-a-2*b+2*c+d, -b+c+3*d, -a, -b, -a-b, -c+d, 2*c-2*d]
    h = [[a, b, 0, 0, 0], [0, a, b, 0, 0], [d/2, 0, c/2, 0, 0],
         [c/2, d/2, 0, 0, 0], [0, c/2, d/2, 0, 0], [0]*5, [0]*5]
    whole_p = add(base, apply(h, p))
    whole_k = product(h, k)
    flags = [0, 0, -a, -b, -a-b, -a-2*b, -b, *([Q(0)]*5),
             2*c+d, c+3*d, 0, 0, 0, -c+d, 2*c-2*d]
    flag_h = h[:2] + [[Q(0)]*5 for _ in range(12)] + h[2:5] + [[Q(0)]*5 for _ in range(2)]
    for name, expected_p, expected_k in [
        ('boundary', whole_p, whole_k),
        ('flags', add(flags, apply(flag_h, p)), product(flag_h, k)),
        ('middle', apply(later, whole_p), product(later, whole_k)),
    ]:
        assert list(map(Q, row[name]['particular'])) == expected_p, name
        assert [list(map(Q, r)) for r in row[name]['directions']] == expected_k, name
    free = list(map(Q, row['free']))
    assert list(map(Q, row['evaluated'])) == add(whole_p, apply(whole_k, free))
    assert len(flags) == 19 and row['flags']['directions'][7:12] == [['0']*3 for _ in range(5)]
    count += 1
assert count == 16
print(f'relational_available_evidence_fraction_cases={count}')
