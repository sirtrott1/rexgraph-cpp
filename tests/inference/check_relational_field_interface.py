"""Independent Fraction fields, original flag contractions and faithful views."""
from fractions import Fraction as Q
import json
import subprocess
import sys


def apply(matrix, x):
    return [sum((Q(a) * b for a, b in zip(row, x)), Q(0)) for row in matrix]


def boundary(x):
    a, b, c, d = x
    # All eight original occurrences, including cancelling repeat, inactive and zero.
    flags = [[a + 2*b, b], [-a, -b, -a-b], [-a-2*b, -b],
             [Q(0)]*3, [Q(0)]*2, [2*c+d, c+3*d],
             [d/2, c/2, c-d/2], [-c+d, 2*c-2*d]]
    lower = [2*c+d, c+3*d, -a+d/2, -b+c/2, -a-b+c-d/2, -c+d, 2*c-2*d]
    return lower, [q for flag in flags for q in flag]


count = 0
for line in subprocess.check_output([sys.argv[1], '--emit'], text=True).splitlines():
    row = json.loads(line)
    x = list(map(Q, row['source']))
    encoded = apply([[1,1], [0,1], [1,2], [0,0]], x)
    first, occurrences = boundary(encoded)
    middle = [first[0]+first[2], first[1]+first[3], first[4]+first[5], first[0]+first[6]]
    final, _ = boundary(middle)
    original = first[:2]
    visible = apply([[1,1], [0,1], [1,2]], original)
    oblique = apply([[1,2], [0,1]], original)
    for name, expected in [('input', encoded), ('boundary', first), ('occurrences', occurrences),
                           ('middle', middle), ('final', final), ('original', original),
                           ('visible', visible), ('oblique', oblique), ('glued', original),
                           ('sum', [2*q for q in original])]:
        assert list(map(Q, row[name])) == expected, name
    metric = [list(map(Q, r)) for r in row['metric']]
    assert metric == [[2,-1,0],[-1,3,0],[0,0,0]]
    norm_visible = sum((a*b for a, b in zip(visible, apply(metric, visible))), Q(0))
    norm_original = sum((a*b for a, b in zip(original, apply([[2,1],[1,3]], original))), Q(0))
    assert norm_visible == norm_original
    assert apply([[1,-1,0], [0,1,0]], visible) == original
    assert visible[2] == visible[0]+visible[1]
    assert len(occurrences) == 19 and occurrences[7:12] == [Q(0)]*5
    count += 1
assert count == 40
print(f'relational_field_interface_fraction_cases={count}')
