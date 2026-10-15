"""Fraction reconstruction from original factor flags and declared map sites."""
from collections import Counter
from fractions import Fraction as Q
import json
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
import check_native_product_chain_map as base

counts = Counter()


def compose(p):
    first, second = p['first'], p['second']
    assert first['target'] == second['source']
    assert first['source'] == p['direct']['source'] and second['target'] == p['direct']['target']
    base.check_plan(first)
    base.check_plan(second)
    expected_maps = []
    for f, (a, b, actual) in enumerate(zip(first['maps'], second['maps'], p['factors'])):
        assert len(actual) == len(b)
        factor = []
        for k, (g, receipt) in enumerate(zip(b, actual)):
            rows, middle = g['shape']
            cols = first['source'][f]['sizes'][k] if k < len(first['source'][f]['sizes']) else 0
            assert receipt['factor'] == f and receipt['grade'] == k
            assert receipt['sizes'] == [cols, middle, rows]
            before = a[k]['support'] if k < len(a) else []
            terms, support = [], {}
            for j, i, fq in sorted(before):
                for h, l, gq in sorted(g['support']):
                    if j != l:
                        continue
                    fq, gq = Q(fq), Q(gq)
                    terms.append(((j, i), (h, l), (h, i), fq, gq, gq*fq))
                    support[h, i] = support.get((h, i), Q(0)) + gq*fq
            actual_terms = [(tuple(x), tuple(y), tuple(z), Q(fq), Q(gq), Q(value))
                            for x, y, z, fq, gq, value in receipt['contributions']]
            assert terms == actual_terms
            actual_support = {(i, j): Q(q) for i, j, q in receipt['support']}
            assert len(actual_support) == len(receipt['support']) and actual_support == support
            result = dict(shape=[rows, cols], support=[[i, j, str(q)] for (i, j), q in sorted(support.items())])
            assert p['direct']['maps'][f][k]['shape'] == result['shape']
            assert [(i, j, Q(q)) for i, j, q in p['direct']['maps'][f][k]['support']] == [(i, j, Q(q)) for i, j, q in result['support']]
            factor.append(result)
            counts['factor_composition_grades'] += 1
            counts['factor_contributions'] += len(terms)
            counts['zero_factor_contributions'] += sum(t[-1] == 0 for t in terms)
            for site, value in support.items():
                if value == 0:
                    counts['supported_zero_composed_directions'] += 1
                    counts['canceled_composed_directions'] += any(t[2] == site and t[-1] for t in terms)
        expected_maps.append(factor)
    expected = dict(source=first['source'], target=second['target'], maps=expected_maps)
    base.check_plan(expected)
    assert expected == p['direct']
    # Two square matrices at grade zero deliberately fail the reversed order.
    if p['sample'] == 8:
        f, g, h = first['maps'][0][0], second['maps'][0][0], expected_maps[0][0]
        fm, gm, hm = base.matrix(f), base.matrix(g), base.matrix(h)
        assert hm == base.product(gm, fm, 2, 2, 2)
        assert hm != base.product(fm, gm, 2, 2, 2)
        counts['noncommuting_order_controls'] += 1
    counts['composition_plans'] += 1
    return p


def lift(p, raw, expected_input):
    first = base.check_lift(p['first'], raw['first'], expected_input)
    staged = base.check_lift(p['second'], raw['second'], first)
    direct = base.check_lift(p['direct'], raw['direct'], expected_input)
    base.check_comparison(raw['comparison'], direct, staged)
    assert direct == staged  # Path support, including canceled zeros, is retained.
    counts['composition_prefixes'] += 1
    return staged


def witness(p, w):
    base.check_witness(p['direct'], w['direct'])
    x = base.field(w['direct']['input'])
    assert len(w['routes']) == 5
    ends = []
    for route, declared in zip(w['routes'], w['direct']['routes']):
        ops = declared['operations']
        assert len(route['prefixes']) == len(route['path']) == len(ops)+1
        assert len(route['comparisons']) == len(ops)
        image = lift(p, route['prefixes'][0], x)
        assert base.field(route['path'][0]) == image
        for j, op in enumerate(ops):
            prefix_input = base.field(declared['source'][j+1])
            current = lift(p, route['prefixes'][j+1], prefix_input)
            image = base.step(image, op)
            assert base.field(route['path'][j+1]) == image
            base.check_comparison(route['comparisons'][j], image, current)
            counts['staged_primitive_steps'] += 1
        ends.append(image)
    base.check_comparison(w['staged_yang_baxter'], ends[3], ends[4])
    if x[1]:
        b = w['boundary']
        image = lift(p, b['input'], x)
        db = base.boundary(p['first']['source'], x)
        mapped_db = lift(p, b['boundary'], db)
        image_boundary = base.boundary(p['second']['target'], image)
        assert base.field(b['staged_boundary']) == image_boundary
        base.check_comparison(b['comparison'], image_boundary, mapped_db)
        direct_image, _ = base.apply_map(p['direct'], x)
        base.check_comparison(b['images'], base.boundary(p['direct']['target'], direct_image), image_boundary)
        counts['staged_boundary_identities'] += 1
    else:
        assert w['boundary'] is None
    counts['composition_witnesses'] += 1


def identity_plan(p, record):
    left, right = record['left'], record['right']
    for identity in [left, right]:
        assert identity['source'] == identity['target']
        for f, maps in enumerate(identity['maps']):
            assert len(maps) == len(identity['source'][f]['sizes'])
            for k, m in enumerate(maps):
                n = identity['source'][f]['sizes'][k]
                assert m['shape'] == [n, n]
                assert [(i, j, Q(q)) for i, j, q in m['support']] == [(i, i, Q(1)) for i in range(n)]
                counts['identity_grades'] += 1
                counts['identity_empty_grades'] += n == 0
        base.check_plan(identity)
    assert left['source'] == p['first']['source'] and right['source'] == p['first']['target']
    counts['identity_plans'] += 2
    return left, right


def identity(p, identities, record):
    left, right = identities
    lp = dict(first=left, second=p['first'], direct=p['first'])
    rp = dict(first=p['first'], second=right, direct=p['first'])
    x = base.field(record['left']['first']['input'])
    expected, _ = base.apply_map(p['first'], x)
    assert lift(lp, record['left'], x) == lift(rp, record['right'], x) == expected
    assert base.field(record['left']['first']['image']) == x
    counts['identity_comparisons'] += 2


def main():
    plans, identities = {}, {}
    with tempfile.TemporaryFile(mode='w+') as err:
        process = subprocess.Popen([sys.argv[1], '--emit'], stdout=subprocess.PIPE, stderr=err, text=True)
        try:
            for line in process.stdout:
                record = json.loads(line)
                sample = record['sample']
                if record['phase'] == 'plan':
                    assert sample not in plans
                    plans[sample] = compose(record)
                elif record['phase'] == 'witness':
                    witness(plans[sample], record)
                elif record['phase'] == 'identity_plan':
                    assert sample not in identities
                    identities[sample] = identity_plan(plans[sample], record)
                else:
                    assert record['phase'] == 'identity'
                    identity(plans[sample], identities[sample], record)
            assert process.wait() == 0
            err.seek(0)
            assert not err.read()
        finally:
            process.stdout.close()
            if process.poll() is None:
                process.kill()
                process.wait()
    assert len(plans) == len(identities) == 9
    assert counts['composition_witnesses'] > 400 and counts['identity_empty_grades'] > 0
    assert counts['canceled_composed_directions'] > 0 and counts['zero_factor_contributions'] > 0
    assert base.counts['supported_zero_differences'] > 0 and base.counts['unsigned_interchange_counterexamples'] > 0
    print(json.dumps(dict(arithmetic='fractions.Fraction', composition=dict(sorted(counts.items())),
                          tensor=dict(sorted(base.counts.items()))), sort_keys=True))


if __name__ == '__main__':
    main()
