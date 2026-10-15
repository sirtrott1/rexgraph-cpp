"""Independent finite-star reconstruction and exact signed-prism checks."""
from collections import Counter, deque
from fractions import Fraction as Q
import json
import subprocess
import sys

from check_native_relational_homotopy import strict, boundary, verify_source, verify_witness


def compose(a, b):
    return {name: [[b[name][k][i] for i in row] for k, row in enumerate(a[name])]
            for name in ['cells', 'flags']}


def chains(c, counts):
    for k in range(2, len(c['sizes'])):
        for i in range(c['sizes'][k]):
            assert not any(boundary(c, k-1, boundary(c, k, {i: Q(1)})).values())
        counts['complete_chain_families'] += 1


def covering(e, b, p, counts):
    strict(e, b, p)
    assert len(e['sizes']) == len(b['sizes'])
    for k, n in enumerate(e['sizes']):
        assert set(p['cells'][k]) == set(range(b['sizes'][k]))
        if k+1 < len(e['sizes']):
            for i in range(n):
                actual = [p['flags'][k+1][q] for q, f in enumerate(e['flags'][k+1]) if f[1] == i]
                expected = [q for q, f in enumerate(b['flags'][k+1]) if f[1] == p['cells'][k][i]]
                assert sorted(actual) == expected
                counts['complete_upward_stars'] += 1
    chains(e, counts)
    chains(b, counts)
    counts['coverings'] += 1


class Obstruction(Exception):
    pass


def reconstruct(c, e, p, h, initial, start, counts):
    """Scan complete stars, independently of the C++ traversal and its indexes."""
    images, flags, queue = {}, {}, deque()

    def assign(cell, image):
        if cell in images:
            if images[cell] != image:
                raise Obstruction('shared cell')
        else:
            images[cell] = image
            queue.append(cell)

    for k, row in enumerate(start['cells']):
        for i, cell in enumerate(row):
            assign((k, cell), initial['cells'][k][i])
    seeds = len(images)
    while queue:
        grade, cell = queue.popleft()
        # Choose upward first, opposite to the production traversal.
        for k, upward in [(grade+1, True), (grade, False)]:
            if not 0 < k < len(c['sizes']):
                continue
            for q, f in enumerate(c['flags'][k]):
                if f[1 if upward else 0] != cell:
                    continue
                possible = [r for r, g in enumerate(e['flags'][k])
                            if g[1 if upward else 0] == images[grade, cell] and p['flags'][k][r] == h['flags'][k][q]]
                assert len(possible) == 1
                r = possible[0]
                if (k, q) in flags and flags[k, q] != r:
                    raise Obstruction('original occurrence')
                flags[k, q] = r
                g = e['flags'][k][r]
                assign((k if upward else k-1, f[0 if upward else 1]), g[0 if upward else 1])
                counts['unique_flag_steps'] += 1
    assert len(images) == sum(c['sizes']) and len(flags) == sum(len(row) for row in c['flags'])
    lifted = dict(cells=[[images[k, i] for i in range(n)] for k, n in enumerate(c['sizes'])],
                  flags=[[flags[k, q] for q in range(len(row))] for k, row in enumerate(c['flags'])])
    strict(c, e, lifted)
    assert compose(lifted, p) == h and compose(start, lifted) == initial
    return lifted, seeds


def verify_lift(s, r, counts):
    c, e, b, y = s['carrier'], s['target'], r['base'], r['original_target']
    covering(e, b, r['projection'], counts)
    strict(y, b, r['base_map'])
    strict(c, y, r['original_h'])
    strict(s['source'], e, r['initial'])
    assert compose(r['original_h'], r['base_map']) == r['projected_h']
    assert compose(r['original_first'], r['base_map']) == r['projected_first']
    assert compose(r['original_second'], r['base_map']) == r['projected_second']
    assert compose(r['initial'], r['projection']) == r['projected_first']
    start = {'cells': [[s['cell_map'][k, i, 0, 0] for i in range(n)] for k, n in enumerate(s['source']['sizes'])],
             'flags': []}
    flag_map = {(tuple(cell), side, key): slot for cell, side, key, slot in s['flags']}
    for k, row in enumerate(s['source']['flags']):
        start['flags'].append([flag_map[(k, f[0], 0, 0), 0, f[2]] for f in row])
    lifted, seeds = reconstruct(c, e, r['projection'], r['projected_h'], r['initial'], start, counts)
    assert lifted == s['h'] and seeds == r['seeds']
    assert compose(s['second'], r['projection']) == r['projected_second']
    reached = {(k, cell) for k, row in enumerate(start['cells']) for cell in row}
    for grade, cell, k, q, mapped, up in r['routes']:
        assert (grade, cell) in reached
        f, g = c['flags'][k][q], e['flags'][k][mapped]
        assert f[1 if up else 0] == cell and g[1 if up else 0] == lifted['cells'][grade][cell]
        assert lifted['flags'][k][q] == mapped
        other = (k if up else k-1, f[0 if up else 1])
        assert other not in reached
        reached.add(other)
    assert len(reached) == sum(c['sizes']) == seeds + len(r['routes'])
    counts['forcing_routes'] += len(r['routes'])
    counts['seeds'] += seeds
    counts['lifts'] += 1
    counts['sheet_changes'] += r['id'] in [12, 13]
    counts['noninjective_base_maps'] += any(len(set(row)) < len(row) for row in r['base_map']['cells'])
    counts['original_zero_flag_permutations'] += r['id'] in [18, 19]
    counts['independent_empty_components'] += r['id'] in [16, 17]


def main():
    process = subprocess.run([sys.argv[1], '--records'], check=True, capture_output=True, text=True)
    assert not process.stderr
    counts, sources = Counter(), {}
    for line in process.stdout.splitlines():
        r = json.loads(line)
        if r['kind'] == 'source':
            assert r['id'] not in sources
            sources[r['id']] = r
            verify_source(r, counts)
        elif r['kind'] == 'lift':
            verify_lift(sources[r['id']], r, counts)
        elif r['kind'] == 'witness':
            verify_witness(sources[r['id']], r, counts)
        else:
            assert r['kind'] == 'obstruction'
            covering(r['cover'], r['base'], r['projection'], counts)
            strict(r['source'], r['cover'], r['initial'])
            strict(r['carrier'], r['base'], r['h'])
            try:
                reconstruct(r['carrier'], r['cover'], r['projection'], r['h'], r['initial'], r['start'], counts)
            except Obstruction:
                counts['verified_obstructions'] += 1
            else:
                raise AssertionError('reported obstruction admits a full lift')
    assert counts['carriers'] == counts['lifts'] == 20
    assert counts['verified_obstructions'] == 3 and counts['sheet_changes'] == 2
    assert counts['noninjective_base_maps'] == counts['original_zero_flag_permutations'] == counts['independent_empty_components'] == 2
    assert counts['inactive_flags'] and counts['active_zero_flags'] and counts['supported_zero_outputs']
    result = dict(sorted(counts.items()))
    result['arithmetic'] = 'fractions.Fraction'
    print(json.dumps(result, sort_keys=True))


if __name__ == '__main__':
    main()
