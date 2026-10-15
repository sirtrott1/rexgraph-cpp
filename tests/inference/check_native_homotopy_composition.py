"""Exact independent reconstruction of complete source homotopy composition."""
from fractions import Fraction as Q
import json
import subprocess
import sys
import check_native_relational_homotopy as base


def push(m, k, v):
    out = {}
    for i, q in v.items():
        j = m['cells'][k][i]
        out[j] = out.get(j, Q(0)) + q
    return out


def compose(a, b):
    return {label: [[b[label][k][i] for i in row] for k, row in enumerate(a[label])]
            for label in ['cells', 'flags']}


def verify_composition(old, new, r, counters):
    u, interval, v, bridge = (r[k] for k in ['before', 'interval', 'after', 'transport'])
    for a, b, m in [(new['source'], old['source'], u), (new['interval'], old['interval'], interval),
                    (new['carrier'], old['carrier'], bridge), (old['target'], new['target'], v)]:
        base.strict(a, b, m)
        counters['composed_map_cells'] += sum(len(row) for row in m['cells'])
        counters['composed_map_flags'] += sum(len(row) for row in m['flags'])
        for k, n in enumerate(a['sizes']):
            if not k:
                continue
            for i in range(n):
                lhs = base.boundary(b, k, push(m, k, {i: Q(1)}))
                rhs = push(m, k-1, base.boundary(a, k, {i: Q(1)}))
                assert not any(base.combine(lhs, rhs, -1).values())
                counters['source_map_naturality_directions'] += 1
    assert interval['cells'] == [[0, 1], [0]]
    assert old['type_interpretation'] == new['type_interpretation']
    # Independently transport ordered product identities and ORIGINAL flags.
    old_flags = {(tuple(id), side, occurrence): to for id, side, occurrence, to in old['flags']}
    for id, to in new['cells']:
        k, i, l, j = id
        mapped = (k, u['cells'][k][i], l, interval['cells'][l][j])
        assert bridge['cells'][k+l][to] == old['cell_map'][mapped]
    for id, side, occurrence, to in new['flags']:
        k, i, l, j = id
        source, target, m, g = (new['interval'], old['interval'], interval, l) if side else (new['source'], old['source'], u, k)
        p = next(p for p, f in enumerate(source['flags'][g]) if f[2] == occurrence)
        mapped_occurrence = target['flags'][g][m['flags'][g][p]][2]
        mapped = (k, u['cells'][k][i], l, interval['cells'][l][j])
        assert bridge['flags'][k+l][to] == old_flags[mapped, side, mapped_occurrence]
    assert new['h'] == compose(compose(bridge, old['h']), v)
    assert new['first'] == compose(compose(u, old['first']), v)
    assert new['second'] == compose(compose(u, old['second']), v)
    counters['compositions'] += 1
    counters['noninjective_before'] += any(len(set(row)) < len(row) for row in u['cells'])
    counters['noninjective_after'] += any(len(set(row)) < len(row) for row in v['cells'])
    counters['explicit_zero_flag_permutations'] += any(row != list(range(len(row))) for row in u['flags'])


def main():
    p = subprocess.run([sys.argv[1], '--records'], check=True, capture_output=True, text=True)
    assert not p.stderr
    counters = dict(arithmetic='fractions.Fraction', carriers=0, product_cells=0, retained_product_flags=0,
        inactive_flags=0, active_zero_flags=0, endpoint_cells=0, endpoint_flags=0, complete_chain_families=0,
        chain_naturality_directions=0, noninjective_homotopies=0, witnesses=0, supported_zero_outputs=0,
        empty_inputs=0, compositions=0, composed_map_cells=0, composed_map_flags=0,
        source_map_naturality_directions=0, noninjective_before=0, noninjective_after=0, explicit_zero_flag_permutations=0)
    sources, compositions = {}, {}
    for line in p.stdout.splitlines():
        r = json.loads(line)
        if r['kind'] == 'source':
            assert r['id'] not in sources
            sources[r['id']] = r
            base.verify_source(r, counters)
        elif r['kind'] == 'composition':
            assert r['id'] not in compositions
            compositions[r['id']] = r
            verify_composition(sources[r['original']], sources[r['id']], r, counters)
        else:
            assert r['kind'] == 'witness'
            base.verify_witness(sources[r['id']], r, counters)
            m = compositions[r['id']]
            old = sources[m['original']]
            k, x = r['grade'], base.values(r['input'])
            ux = push(m['before'], k, x)
            h = push(m['after'], k+1, base.push_product(old, base.embed(ux, k)))
            assert h == base.values(r['h'])
            for label, end in [('first', 0), ('second', 1)]:
                expected = push(m['after'], k, base.push_product(old, base.embed(ux, k, end)))
                assert expected == base.values(r[label])
    assert counters['compositions'] == 9 and counters['carriers'] == 18
    assert counters['noninjective_before'] == counters['noninjective_after'] == counters['explicit_zero_flag_permutations'] == 1
    assert counters['inactive_flags'] and counters['active_zero_flags'] and counters['supported_zero_outputs']
    print(json.dumps(counters, sort_keys=True))


if __name__ == '__main__':
    main()
