"""Reconstruct every retained stage and cumulative source map with Fraction."""
from fractions import Fraction as Q
import json
import subprocess
import sys
import check_native_homotopy_composition as composition

base = composition.base
compose = composition.compose
push = composition.push


def main():
    run = subprocess.run([sys.argv[1], '--records'], check=True, capture_output=True, text=True)
    assert not run.stderr
    counters = dict(arithmetic='fractions.Fraction', carriers=0, product_cells=0, retained_product_flags=0,
        inactive_flags=0, active_zero_flags=0, endpoint_cells=0, endpoint_flags=0, complete_chain_families=0,
        chain_naturality_directions=0, noninjective_homotopies=0, witnesses=0, supported_zero_outputs=0,
        empty_inputs=0, compositions=0, composed_map_cells=0, composed_map_flags=0,
        source_map_naturality_directions=0, noninjective_before=0, noninjective_after=0,
        explicit_zero_flag_permutations=0, retained_stages=0, cumulative_certificates=0,
        maximum_depth=0, exact_associations=0, noncommuting_zero_flag_pairs=0, explicit_identity_pairs=0)
    sources, stages = {}, {}
    for line in run.stdout.splitlines():
        record = json.loads(line)
        if record['kind'] == 'source':
            assert record['id'] not in sources
            sources[record['id']] = record
            base.verify_source(record, counters)
        elif record['kind'] == 'composition':
            assert record['id'] not in stages
            stages[record['id']] = record
            new, old, root = (sources[record[key]] for key in ['id', 'original', 'root'])
            composition.verify_composition(old, new, record, counters)
            c = record['cumulative']
            if record['depth'] == 1:
                assert c == {key: record[key] for key in ['before', 'interval', 'after', 'transport']}
            else:
                previous = stages[record['original']]['cumulative']
                assert c == dict(before=compose(record['before'], previous['before']),
                    interval=compose(record['interval'], previous['interval']),
                    after=compose(previous['after'], record['after']),
                    transport=compose(record['transport'], previous['transport']))
            # A separate direct source-level reconstruction checks the entire
            # cumulative product transport and original H/endpoint restrictions.
            composition.verify_composition(root, new, c, counters)
            counters['retained_stages'] += 1
            counters['cumulative_certificates'] += 1
            counters['maximum_depth'] = max(counters['maximum_depth'], record['depth'])
        elif record['kind'] == 'witness':
            current = sources[record['id']]
            base.verify_witness(current, record, counters)
            if record['id'] in stages:
                stage = stages[record['id']]
                root, c = sources[stage['root']], stage['cumulative']
                k, x = record['grade'], base.values(record['input'])
                ux = push(c['before'], k, x)
                expected = push(c['after'], k+1, base.push_product(root, base.embed(ux, k)))
                assert expected == base.values(record['h'])
                for label, end in [('first', 0), ('second', 1)]:
                    expected = push(c['after'], k, base.push_product(root, base.embed(ux, k, end)))
                    assert expected == base.values(record[label])
        else:
            assert record['kind'] == 'association'
            identity = record['source_identity']
            a, b = json.loads(json.dumps(identity)), json.loads(json.dumps(identity))
            a['flags'][1][4], a['flags'][1][5] = a['flags'][1][5], a['flags'][1][4]
            b['flags'][1][5], b['flags'][1][6] = b['flags'][1][6], b['flags'][1][5]
            assert record['ab'] == compose(b, a) and record['ba'] == compose(a, b)
            assert record['ab'] != record['ba']
            assert compose(a, a) == record['identity'] == identity
            assert record['sequential_h'] == record['flat_h']
            counters['exact_associations'] += 1
            counters['noncommuting_zero_flag_pairs'] += 1
            counters['explicit_identity_pairs'] += 1
    assert counters['carriers'] == 40 and counters['retained_stages'] == counters['cumulative_certificates'] == 30
    assert counters['maximum_depth'] == 3
    assert counters['compositions'] == 60 and counters['exact_associations'] == 1
    assert all(counters[key] for key in ['inactive_flags', 'active_zero_flags', 'supported_zero_outputs',
        'empty_inputs', 'noninjective_before', 'noninjective_after', 'explicit_zero_flag_permutations'])
    print(json.dumps(counters, sort_keys=True))


if __name__ == '__main__':
    main()
