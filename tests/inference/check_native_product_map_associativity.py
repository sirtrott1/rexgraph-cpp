"""Exact three-map reconstruction from original flags and declared map sites."""
from collections import Counter
from fractions import Fraction as Q
import json
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
import check_native_product_chain_map as base
import check_native_product_map_composition as composition

counts = Counter()


def plan(p):
    for name in ['prefix', 'tail', 'left', 'right']:
        # The two-map verifier reconstructs support and every ordered term;
        # its special fixture-specific order control is checked separately here.
        composition.compose(dict(p[name], sample=-1))
    prefix, tail, left, right, last = (p[x] for x in ['prefix', 'tail', 'left', 'right', 'last'])
    assert prefix['second'] == tail['first'] and last == tail['second'] == left['second']
    assert left['first'] == prefix['direct'] and right['first'] == prefix['first']
    assert right['second'] == tail['direct'] and left['direct'] == right['direct']
    base.check_plan(last)
    for f, (a, b, c, grades) in enumerate(zip(prefix['first']['maps'], prefix['second']['maps'], last['maps'], p['triples'])):
        assert len(grades) == len(c)
        for k, (h, receipt) in enumerate(zip(c, grades)):
            cols = prefix['first']['source'][f]['sizes'][k] if k < len(prefix['first']['source'][f]['sizes']) else 0
            middle1 = prefix['first']['target'][f]['sizes'][k] if k < len(prefix['first']['target'][f]['sizes']) else 0
            middle2, rows = h['shape'][1], h['shape'][0]
            assert receipt['factor'] == f and receipt['grade'] == k
            assert receipt['sizes'] == [cols, middle1, middle2, rows]
            terms, support = [], {}
            for j, i, fq in sorted(a[k]['support'] if k < len(a) else []):
                for l, j2, gq in sorted(b[k]['support']):
                    if j != j2:
                        continue
                    for m, l2, hq in sorted(h['support']):
                        if l != l2:
                            continue
                        fq, gq, hq = Q(fq), Q(gq), Q(hq)
                        gf, hg = gq*fq, hq*gq
                        value = hq*gf
                        assert value == hg*fq
                        terms.append(dict(positions=[[j, i], [l, j2], [m, l2]], position=[m, i],
                                          coefficients=[fq, gq, hq], pairs=[gf, hg], values=[value, value]))
                        support[m, i] = support.get((m, i), Q(0)) + value
            actual = [dict(t, coefficients=[Q(q) for q in t['coefficients']],
                           pairs=[Q(q) for q in t['pairs']], values=[Q(q) for q in t['values']])
                      for t in receipt['contributions']]
            assert actual == terms
            assert {(i, j): Q(q) for i, j, q in receipt['support']} == support
            assert len(support) == len(receipt['support'])
            for name in ['left', 'right']:
                m = p[name]['direct']['maps'][f][k]
                assert m['shape'] == [rows, cols]
                assert {(i, j): Q(q) for i, j, q in m['support']} == support
            counts['triple_factor_grades'] += 1
            counts['triple_paths'] += len(terms)
            counts['zero_triple_paths'] += sum(t['values'][0] == 0 for t in terms)
            counts['canceled_triple_directions'] += sum(q == 0 and any(t['position'] == list(site) and t['values'][0]
                                                                     for t in terms) for site, q in support.items())
    if p['sample'] == 8:
        fm, gm, hm = [base.matrix(m['maps'][0][0]) for m in [prefix['first'], prefix['second'], last]]
        expected = base.product(hm, base.product(gm, fm, 2, 2, 2), 2, 2, 2)
        wrong = base.product(gm, base.product(hm, fm, 2, 2, 2), 2, 2, 2)
        assert expected != wrong
        counts['noncommuting_order_controls'] += 1
    counts['plans'] += 1


def lift(p, raw, x):
    z = composition.lift(p['prefix'], raw['prefix'], x)
    staged = base.check_lift(p['last'], raw['last'], z)
    left = composition.lift(p['left'], raw['left'], x)
    right = composition.lift(p['right'], raw['right'], x)
    base.check_comparison(raw['parents'], left, right)
    base.check_comparison(raw['staged'], left, staged)
    assert left == right == staged
    counts['associativity_prefixes'] += 1
    return staged


def witness(p, w):
    x = base.field(w['routes'][0]['prefixes'][0]['prefix']['first']['input'])
    ops = [[0], [1], [2], base.word(x[0], 0), base.word(x[0], 1)]
    assert len(w['routes']) == 5
    ends = []
    for r, operations in zip(w['routes'], ops):
        assert r['operations'] == operations
        assert len(r['prefixes']) == len(r['path']) == len(operations)+1
        assert len(r['comparisons']) == len(operations)
        source = x
        image = lift(p, r['prefixes'][0], source)
        assert base.field(r['path'][0]) == image
        for j, op in enumerate(operations):
            source = base.step(source, op)
            current = lift(p, r['prefixes'][j+1], source)
            image = base.step(image, op)
            assert base.field(r['path'][j+1]) == image
            base.check_comparison(r['comparisons'][j], image, current)
            counts['primitive_steps'] += 1
        ends.append(image)
    base.check_comparison(w['yang_baxter'], ends[3], ends[4])
    if x[1]:
        b = w['boundary']
        image = lift(p, b['input'], x)
        db = base.boundary(p['prefix']['first']['source'], x)
        mapped = lift(p, b['boundary'], db)
        bd = base.boundary(p['last']['target'], image)
        assert base.field(b['staged_boundary']) == bd
        base.check_comparison(b['comparison'], bd, mapped)
        counts['boundary_identities'] += 1
    else:
        assert w['boundary'] is None
    counts['witnesses'] += 1


def main():
    plans = {}
    with tempfile.TemporaryFile(mode='w+') as err:
        process = subprocess.Popen([sys.argv[1], '--emit'], stdout=subprocess.PIPE, stderr=err, text=True)
        try:
            for line in process.stdout:
                r = json.loads(line)
                if r['phase'] == 'plan':
                    assert r['sample'] not in plans
                    plan(r)
                    plans[r['sample']] = r
                elif r['phase'] == 'witness':
                    witness(plans[r['sample']], r)
                elif r['phase'] == 'continuation':
                    x = base.field(r['lift']['prefix']['first']['input'])
                    lift(plans[r['sample']], r['lift'], x)
                    counts['continuations'] += 1
                else:
                    assert r['phase'] == 'summary'
                    assert r['witnesses'] == counts['witnesses']
                    assert r['prefixes'] == counts['associativity_prefixes']
                    assert r['triple_paths'] == counts['triple_paths']
                    assert r['zero_paths'] == counts['zero_triple_paths']
            status = process.wait()
            err.seek(0)
            errors = err.read()
            assert status == 0, errors
            assert not errors
        finally:
            process.stdout.close()
            if process.poll() is None:
                process.kill()
                process.wait()
    assert len(plans) == 9 and counts['witnesses'] == counts['continuations'] == 552
    assert counts['zero_triple_paths'] and counts['canceled_triple_directions']
    assert base.counts['supported_zero_differences']
    print(json.dumps(dict(arithmetic='fractions.Fraction', **dict(sorted(counts.items())),
                          exact_reconstruction=dict(sorted(base.counts.items()))), sort_keys=True))


if __name__ == '__main__':
    main()
