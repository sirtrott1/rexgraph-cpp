"""Independent Fraction pushout, retained-fiber and chain-map verification."""
from fractions import Fraction as Q
import json
import subprocess
import sys


def values(data):
    out = {i: Q(q) for i, q in data}
    assert len(out) == len(data)
    return out


def boundary(source, grade, data, structural=True):
    out = {}
    for upper, lower, _, active, coefficient, _ in source['occurrences'][grade]:
        if upper not in data or (not structural and not active):
            continue
        out.setdefault(lower, Q(0))
        if active:
            out[lower] += data[upper] * Q(coefficient)
    return out


def push(mapping, grade, data):
    out = {}
    for i, q in data.items():
        j = mapping['cells'][grade][i]
        out[j] = out.get(j, Q(0)) + q
    return out


def nonzero(data):
    return {i: q for i, q in data.items() if q}


def chain_laws(source):
    count = 0
    for grade in range(2, len(source['sizes'])):
        for i in range(source['sizes'][grade]):
            assert not nonzero(boundary(source, grade-1, boundary(source, grade, {i: Q(1)})))
        count += 1
    return count


def strict_map(a, b, mapping, injective):
    assert len(mapping['cells']) == len(mapping['occurrences']) == len(a['sizes'])
    directions = 0
    for grade, n in enumerate(a['sizes']):
        cells = mapping['cells'][grade]
        occurrence_map = mapping['occurrences'][grade]
        assert len(cells) == n and all(0 <= i < b['sizes'][grade] for i in cells)
        assert len(occurrence_map) == len(a['occurrences'][grade])
        if injective:
            assert len(set(cells)) == len(cells)
            assert len(set(occurrence_map)) == len(occurrence_map)
        for slot, (upper, lower, _, active, coefficient, aspects) in enumerate(a['occurrences'][grade]):
            tu, tl, _, ta, tc, tb = b['occurrences'][grade][occurrence_map[slot]]
            assert tu == cells[upper] and tl == mapping['cells'][grade-1][lower]
            assert active == ta and Q(coefficient) == Q(tc) and aspects == tb
        for i in range(n):
            original = [s for s, o in enumerate(a['occurrences'][grade]) if o[0] == i]
            target = [s for s, o in enumerate(b['occurrences'][grade]) if o[0] == cells[i]]
            assert {occurrence_map[s] for s in original} == set(target)
            assert len(original) == len(target)
            if grade:
                lhs = boundary(b, grade, push(mapping, grade, {i: Q(1)}))
                rhs = push(mapping, grade-1, boundary(a, grade, {i: Q(1)}))
                assert lhs == rhs
                directions += 1
    return directions


class Equivalence:
    def __init__(self):
        self.parents = {}

    def root(self, x):
        while x in self.parents:
            x = self.parents[x]
        return x

    def join(self, a, b):
        a, b = self.root(a), self.root(b)
        if a != b:
            self.parents[b] = a


def quotient(a, b, common_left, common_right, left_glued, right_glued, p, kind):
    for grade in range(len(p['sizes'])):
        total = lambda source: source['sizes'][grade] if kind == 'cells' else len(source['occurrences'][grade])
        relation = Equivalence()
        for x, y in zip(common_left[kind][grade], common_right[kind][grade], strict=True):
            relation.join((0, x), (1, y))
        actual = {}
        for side, source, inclusion in [(0, a, left_glued), (1, b, right_glued)]:
            for i in range(total(source)):
                key = relation.root((side, i))
                value = inclusion[kind][grade][i]
                assert key not in actual or actual[key] == value
                actual[key] = value
        assert len(actual) == total(p)
        assert set(actual.values()) == set(range(total(p)))


def main(exe):
    run = subprocess.run([exe, '--json'], capture_output=True, text=True, check=True)
    assert not run.stderr
    stats = dict(native_chain_cases=0, complete_section_cases=0, supported_zero_inputs=0,
                 whole_chain_families=0, complete_map_directions=0,
                 retained_pushout_occurrences=0, retained_inactive_occurrences=0,
                 active_zero_occurrences=0, arithmetic='fractions.Fraction')
    for line in run.stdout.splitlines():
        record = json.loads(line)
        grade = record['grade']
        if record['kind'] == 'section':
            output = {}
            for source, mapping in zip(['left', 'right'], record['maps'], strict=True):
                for i, q in values(record[source]).items():
                    target = mapping['cells'][grade][i]
                    assert target not in output or output[target] == q
                    output[target] = q
            assert output == values(record['output'])
            stats['complete_section_cases'] += 1
            continue
        common, a, b, p = record['sources']
        ca, cb, ap, bp = record['square']
        for source, target, mapping in [(common,a,ca),(common,b,cb),(a,p,ap),(b,p,bp)]:
            strict_map(source, target, mapping, True)
        for kind in ['cells', 'occurrences']:
            quotient(a,b,ca,cb,ap,bp,p,kind)
        ma, mb, mediator = record['maps']
        target = record['target']
        for source, mapping in [(a,ma),(b,mb),(p,mediator)]:
            stats['complete_map_directions'] += strict_map(source,target,mapping,False)
        for k in range(len(p['sizes'])):
            for source_map, inclusion in [(ma,ap),(mb,bp)]:
                for i,j in enumerate(inclusion['cells'][k]):
                    assert mediator['cells'][k][j] == source_map['cells'][k][i]
                for i,j in enumerate(inclusion['occurrences'][k]):
                    assert mediator['occurrences'][k][j] == source_map['occurrences'][k][i]
        for source in [common,a,b,p,target]:
            stats['whole_chain_families'] += chain_laws(source)
        input_data, output = values(record['input']), values(record['output'])
        assert push(mediator,grade,input_data) == output
        if grade:
            assert boundary(p,grade,input_data) == values(record['source_boundary'])
            assert boundary(target,grade,output) == values(record['target_boundary'])
            assert push(mediator,grade-1,values(record['source_boundary'])) == values(record['target_boundary'])
        else:
            assert record['source_boundary'] is record['target_boundary'] is None
        for occurrences in p['occurrences']:
            for _, _, _, active, coefficient, _ in occurrences:
                stats['retained_pushout_occurrences'] += 1
                if not active:
                    stats['retained_inactive_occurrences'] += 1
                elif Q(coefficient) == 0:
                    stats['active_zero_occurrences'] += 1
        stats['supported_zero_inputs'] += sum(q == 0 for q in input_data.values())
        stats['native_chain_cases'] += 1
    assert stats['native_chain_cases'] == 312 and stats['complete_section_cases'] == 48
    print(json.dumps(stats, sort_keys=True))


if __name__ == '__main__':
    main(sys.argv[1])
