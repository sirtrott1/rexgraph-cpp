"""Independent exact two-axis contraction and dependency support."""
import json
import subprocess
import sys
from fractions import Fraction as F

records = subprocess.run([sys.argv[1]], check=True, capture_output=True, text=True)
checks = 0
cases = 0
for line in records.stdout.splitlines():
    record = json.loads(line)
    a = [[F(v) for v in row] for row in record['a']]
    b = [[F(v) for v in row] for row in record['b']]
    fields = [[None if v is None else F(v) for v in row] for row in record['fields']]
    for i, row in enumerate(record['output']):
        for j, observed in enumerate(row):
            total = F(0)
            supported = True
            for k in range(record['n']):
                for l in range(record['m']):
                    coefficient = a[i][k] * b[j][l]
                    if coefficient:
                        value = fields[l][k]
                        if value is None:
                            supported = False
                        else:
                            total += coefficient * value
            assert (None if observed is None else F(observed)) == (total if supported else None)
            checks += 1
    cases += 1
assert cases == 300
print(json.dumps(dict(status='PASS', cases=cases, exact_coordinates=checks)))
