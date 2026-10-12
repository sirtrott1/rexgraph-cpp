"""Independent rational checks of complete finite-branch intermediary fields."""
import json
import subprocess
import sys
import tempfile
import itertools
from fractions import Fraction as Q
from pathlib import Path


def vector(values):
    return [Q(v) for v in values]


def response(x, theta):
    a, b, c, d = x
    u, v, w = theta
    return [-a-2*b+2*c+d+u*a+v*b, -b+c+3*d+v*a+w*b,
            -a+(w*c+u*d)/2, -b+(u*c+v*d)/2,
            -a-b+(v*c+w*d)/2, -c+d, 2*c-2*d]


def occurrences(x, theta):
    a, b, c, d = x
    u, v, w = theta
    return [u*a+v*b, v*a+w*b,
            -a, -b, -a-b, -a-2*b, -b,
            0, 0, 0, 0, 0, 2*c+d, c+3*d,
            (w*c+u*d)/2, (u*c+v*d)/2, (v*c+w*d)/2,
            -c+d, 2*c-2*d]


def run(binary):
    all_rows = [json.loads(line) for line in subprocess.check_output([binary, '--emit'], text=True).splitlines()]
    rows = [r for r in all_rows if 'xi' in r]
    finite_rows = [r for r in all_rows if 'charts' in r]
    assert len(rows) == 12 and len(finite_rows) == 64
    for row in finite_rows:
        expected = [list(a) for a in itertools.product((101,102,103), repeat=3)
                    if list(a[:2]) in row['charts'][0] and list(a[1:]) in row['charts'][1]]
        assert row['assignments'] == expected
    for row in rows:
        xi, x = vector(row['xi']), vector(row['input'])
        theta = [-10+4*xi[0], 5-2*xi[0], xi[0]]
        expected = response(x, theta)
        assert vector(row['response']) == expected
        assert vector(row['occurrences']) == occurrences(x, theta)
        assert vector(row['next']) == [expected[0]+expected[2], expected[1]+expected[3],
                                       expected[4]+expected[5], expected[0]+expected[6]]
        p = response(x, [-10, 5, 0])
        direction = [a-b for a,b in zip(response(x, [-6,3,1]), p)]
        assert vector(row['particular']) == p
        assert [[Q(q) for q in r] for r in row['directions']] == [[q, Q(0), Q(0)] for q in direction]
    with tempfile.TemporaryDirectory(prefix='rexgraph-finite-standalone-') as tmp:
        packet = str(Path(tmp)/'branch.packet')
        subprocess.run([binary, '--write', packet], check=True)
        subprocess.run([binary, '--read', packet], check=True)
        choice = str(Path(tmp)/'choice.packet')
        subprocess.run([binary, '--write-choice', choice], check=True)
        subprocess.run([binary, '--read-choice', choice], check=True)
    print('12 complete affine Fraction cases plus64 finite compatibility cases and two separate cold processes passed')


if __name__ == '__main__':
    run(sys.argv[1])
