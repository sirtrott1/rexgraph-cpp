"""Independent complete rational fields on separately admitted numeric origins."""
from fractions import Fraction as Q
import json
import subprocess
import sys

count = 0
for line in subprocess.check_output([sys.argv[1], '--emit'], text=True).splitlines():
    row = json.loads(line)
    a, b = map(Q, row['input'])
    boundary = [-a+b, b-a, -a, -b, -a-b]
    flags = [a+2*b, b, -a, -b, -a-b, -2*a-b, -a, *([Q(0)]*5)]
    assert list(map(Q, row['boundary'])) == boundary
    assert list(map(Q, row['occurrences'])) == flags
    assert list(map(Q, row['later'])) == [-2*a, -2*a]
    assert row['first'] == 3 and row['asset_cut'] == 4 and row['value_cut'] == 12
    count += 1
assert count == 24
print(f'relational_value_admission_fraction_cases={count}')
