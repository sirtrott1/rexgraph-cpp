"""Independent complete-pair mediation and exact signed-prism naturality."""
from collections import Counter
from copy import deepcopy
from fractions import Fraction as Q
import json
import subprocess
import sys

from check_native_covering_homotopy import compose, covering
from check_native_relational_homotopy import combine, strict, values, verify_source, verify_witness


def push(v, m, k):
    out = {}
    for i, q in v.items():
        j = m['cells'][k][i]
        out[j] = out.get(j, Q(0)) + q
    return out


def certify(a, b, s, counts):
    e, p, base, d = a['target'], b['target'], s['base'], s['new_base']
    covering(e, base, s['projection'], counts)
    covering(p, d, s['new_projection'], counts)
    for x, y, m in [(p, e, s['into']), (d, base, s['base_map']),
                     (s['y'], s['z'], s['comparison']), (s['y'], base, s['v']),
                     (s['z'], d, s['g']), (a['carrier'], s['y'], s['h']),
                     (b['carrier'], s['z'], s['hp'])]:
        strict(x, y, m)
    assert a['cells'] == b['cells'] and a['flags'] == b['flags']
    assert a['type_interpretation'] == b['type_interpretation']
    assert compose(s['h'], s['comparison']) == s['hp']
    assert compose(compose(s['comparison'], s['g']), s['base_map']) == s['v']
    assert compose(s['into'], s['projection']) == compose(s['new_projection'], s['base_map'])
    lookup = {'cells': [], 'flags': []}
    for name, size in [('cells', 'sizes'), ('flags', 'flags')]:
        for k, row in enumerate(s['into'][name]):
            pairs = {(i, j): slot for slot, (i, j) in enumerate(zip(row, s['new_projection'][name][k]))}
            assert len(pairs) == len(row)
            ne = e[size][k] if name == 'cells' else len(e[size][k])
            nd = d[size][k] if name == 'cells' else len(d[size][k])
            expected = {(i, j) for i in range(ne) for j in range(nd)
                        if s['projection'][name][k][i] == s['base_map'][name][k][j]}
            assert set(pairs) == expected
            lookup[name].append(pairs)
            counts['compatible_cell_pairs' if name == 'cells' else 'compatible_flag_pairs'] += len(pairs)
    # Derive all three universal maps independently from the compatible-pair
    # lookup, not from the C++ mediator or incidence-lifting traversal.
    for label, cone in [('h', compose(s['hp'], s['g'])), ('first', None), ('second', None)]:
        if label != 'h':
            end = 0 if label == 'first' else 1
            inclusion = {'cells': [[a['cell_map'][k, i, 0, end] for i in range(n)]
                                   for k, n in enumerate(a['source']['sizes'])], 'flags': []}
            slots = {(tuple(cell), side, key): slot for cell, side, key, slot in a['flags']}
            for k, row in enumerate(a['source']['flags']):
                inclusion['flags'].append([slots[((k, f[0], 0, end), 0, f[2])] for f in row])
            cone = compose(compose(inclusion, s['hp']), s['g'])
        mediator = {name: [[lookup[name][k][i, j] for i, j in zip(row, cone[name][k])]
                           for k, row in enumerate(a[label][name])] for name in ['cells', 'flags']}
        assert mediator == b[label]
        assert compose(mediator, s['into']) == a[label]
        assert compose(mediator, s['new_projection']) == cone
        counts['mediators'] += 1
    counts['noninjective_base_maps'] += any(len(set(row)) < len(row) for row in s['base_map']['cells'])
    counts['noninjective_comparisons'] += any(len(set(row)) < len(row) for row in s['comparison']['cells'])
    # A full comparison of supported-zero flags cannot be recovered from its
    # chain action. Swapping identical flags changes the source square.
    for k, row in enumerate(s['y']['flags']):
        pair = next(((i, j) for i, f in enumerate(row) for j, g in enumerate(row)
                     if i < j and f[:2] == g[:2] and f[3:] == g[3:] and Q(f[3]) == 0), None)
        if pair:
            bad = deepcopy(s['comparison']); i, j = pair
            bad['flags'][k][i], bad['flags'][k][j] = bad['flags'][k][j], bad['flags'][k][i]
            strict(s['y'], s['z'], bad)
            assert bad['cells'] == s['comparison']['cells']
            assert compose(s['h'], bad) != s['hp']
            counts['zero_flag_obstructions'] += 1
            break
    counts['base_changes'] += 1


def main():
    process = subprocess.run([sys.argv[1], '--records'], capture_output=True, text=True, check=True)
    assert not process.stderr
    counts, old, changed, squares, witness = Counter(), {}, {}, {}, {}
    labels = ['first', 'second', 'h', 'dh', 'hd', 'difference', 'residual']
    for line in process.stdout.splitlines():
        r = json.loads(line)
        kind, id = r['kind'], r['id']
        if kind in ['source', 'changed_source']:
            table = old if kind == 'source' else changed
            assert id not in table
            table[id] = r
            verify_source(r, counts)
        elif kind == 'square':
            certify(old[id], changed[id], r, counts)
            squares[id] = r
        elif kind in ['witness', 'changed_witness']:
            verify_witness((old if kind == 'witness' else changed)[id], r, counts)
            witness[kind] = r
        else:
            assert kind == 'projection'
            a, b = witness['witness'], witness['changed_witness']
            assert a['grade'] == b['grade'] and a['input'] == b['input'] and a['id'] == b['id'] == id
            for j, label in enumerate(labels):
                k = a['grade'] + (label == 'h')
                projected = push(values(b[label]), squares[id]['into'], k)
                residual = combine(projected, values(a[label]), -1)
                assert values(r['values'][j]) == projected and values(r['residuals'][j]) == residual
                assert not any(residual.values())
                counts['projection_identities'] += 1
            counts['naturality_witnesses'] += 1
    assert counts['base_changes'] == 60 and counts['mediators'] == 180
    assert counts['naturality_witnesses'] == 888 and counts['witnesses'] == 1776
    assert counts['noninjective_comparisons'] and counts['noninjective_base_maps'] and counts['zero_flag_obstructions']
    result = dict(sorted(counts.items()), arithmetic='fractions.Fraction')
    print(json.dumps(result, sort_keys=True))


if __name__ == '__main__':
    main()
