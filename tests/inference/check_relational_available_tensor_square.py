import json
import subprocess
import sys
from fractions import Fraction

rows = subprocess.check_output([sys.argv[1], "--json"], text=True).splitlines()
assert len(rows) == 20
for i, line in enumerate(rows):
    actual = {key: list(map(Fraction, values)) for key, values in json.loads(line).items()}
    a, b = Fraction(i - 7, 3), Fraction(i % 11 - 4, 5)
    product = [a * a, a * b, b * a, b * b]
    q = a + 2 * b + a * b
    assert actual == {
        "original": [a, b],
        "square": product,
        "lifted": [a, b] + product,
        "boundary": [0, -q, 0],
        "occurrences": [q, -q, -q, 0, 0],
    }
print("20 independent Fraction complete original/product/input/boundary/occurrence controls passed")
