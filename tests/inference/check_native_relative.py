#!/usr/bin/env python3
"""Independent Fraction model of retained relative pairs and complete pair maps."""
from fractions import Fraction as Q
import json
import subprocess
import sys


def values(rows):
    assert len({i for i, _ in rows}) == len(rows)
    return {i: Q(q) for i, q in rows}


def boundary(source, coordinates, grade, field):
    assert grade > 0
    inverse = {v: i for i, v in enumerate(coordinates[grade - 1])}
    result = {}
    for local, value in field.items():
        cell = coordinates[grade][local]
        for upper, lower, _, active, coefficient, _ in source['occurrences'][grade]:
            if upper == cell and lower in inverse:
                key = inverse[lower]
                result.setdefault(key, Q(0))
                if active:
                    result[key] += Q(coefficient) * value
    return result


def ambient(source):
    return [list(range(n)) for n in source['sizes']]


def chain_family(source, coordinates):
    count = 0
    for k in range(2, len(coordinates)):
        for i in range(len(coordinates[k])):
            out = boundary(source, coordinates, k, {i: Q(1)})
            assert all(q == 0 for q in boundary(source, coordinates, k - 1, out).values())
            count += 1
    return count


def pair(source, coordinates):
    whole, sub, quotient = coordinates
    assert len(whole) == len(sub) == len(quotient) == len(source['sizes'])
    for k, size in enumerate(source['sizes']):
        for row in coordinates:
            assert row[k] == sorted(set(row[k])) and set(row[k]) <= set(range(size))
        assert set(sub[k]) <= set(whole[k])
        assert quotient[k] == sorted(set(whole[k]) - set(sub[k]))
        if k:
            for upper, lower, *_ in source['occurrences'][k]:
                for row in [whole, sub]:
                    if upper in row[k]:
                        assert lower in row[k - 1]  # inactive and zero routes count
    return sum(chain_family(source, row) for row in coordinates)


def complete_map(source, target, coordinates, target_coordinates, declaration):
    cells, occurrences = declaration['cells'], declaration['occurrences']
    assert len(cells) == len(occurrences) == len(source['sizes'])
    directions = 0
    for k, size in enumerate(source['sizes']):
        assert len(cells[k]) == size
        assert all(0 <= i < target['sizes'][k] for i in cells[k])
        original, images = source['occurrences'][k], target['occurrences'][k]
        assert len(occurrences[k]) == len(original)
        for i in range(size):
            positions = [p for p, o in enumerate(original) if o[0] == i]
            fiber = [p for p, o in enumerate(images) if o[0] == cells[k][i]]
            assert sorted(occurrences[k][p] for p in positions) == fiber
        for p, (upper, lower, _, active, coefficient, aspects) in enumerate(original):
            q = occurrences[k][p]
            tu, tl, _, ta, tc, taspects = images[q]
            assert tu == cells[k][upper] and tl == cells[k - 1][lower]
            assert (active, Q(coefficient), aspects) == (ta, Q(tc), taspects)
        for s in [0, 1]:
            assert {cells[k][i] for i in coordinates[s][k]} <= set(target_coordinates[s][k])
        directions += size + len(original)
    return directions


def apply_map(coordinates, target_coordinates, declaration, space, grade, field):
    source = coordinates[space][grade]
    target = {v: i for i, v in enumerate(target_coordinates[space][grade])}
    result = {}
    for i, q in field.items():
        mapped = declaration['cells'][grade][source[i]]
        if mapped in target:
            result.setdefault(target[mapped], Q(0))
            result[target[mapped]] += q
        else:
            assert space == 2
    return result


def connecting(record):
    source, coordinates, k = record['source'], record['coordinates'], record['grade']
    field = values(record['input'])
    assert all(q == 0 for q in boundary(source, coordinates[2], k, field).values())
    whole_inverse = {v: i for i, v in enumerate(coordinates[0][k])}
    representative = {whole_inverse[coordinates[2][k][i]]: q for i, q in field.items()}
    assert representative == values(record['representative'])
    d = boundary(source, coordinates[0], k, representative)
    assert d == values(record['whole_boundary'])
    sub_inverse = {v: i for i, v in enumerate(coordinates[1][k - 1])}
    result = {}
    for i, q in d.items():
        cell = coordinates[0][k - 1][i]
        if cell in sub_inverse:
            result[sub_inverse[cell]] = q
        else:
            assert q == 0
    assert result == values(record['sub_boundary'])
    if k > 1:
        assert all(q == 0 for q in boundary(source, coordinates[1], k - 1, result).values())
    return sum(q == 0 for i, q in d.items() if coordinates[0][k - 1][i] not in sub_inverse)


def main():
    run = subprocess.run([sys.argv[1], '--json'], check=True, capture_output=True, text=True)
    assert not run.stderr
    stats = dict(relative_basis_cases=0, native_map_cases=0, connecting_witnesses=0,
                 supported_zero_inputs=0, retained_occurrences=0,
                 retained_inactive_occurrences=0, active_zero_occurrences=0,
                 complete_map_directions=0, whole_chain_families=0,
                 supported_zero_outside_sub=0, arithmetic='fractions.Fraction')
    for line in run.stdout.splitlines():
        r = json.loads(line)
        source, coordinates = r['source'], r['coordinates']
        stats['whole_chain_families'] += pair(source, coordinates) + chain_family(source, ambient(source))
        if r['kind'] == 'witness':
            stats['supported_zero_outside_sub'] += connecting(r)
            stats['connecting_witnesses'] += 1
            continue
        assert r['kind'] == 'chain'
        target, target_coordinates, declaration = r['target'], r['target_coordinates'], r['map']
        stats['whole_chain_families'] += pair(target, target_coordinates) + chain_family(target, ambient(target))
        stats['complete_map_directions'] += complete_map(source, target, coordinates, target_coordinates, declaration)
        s, k, field = r['space'], r['grade'], values(r['input'])
        out = apply_map(coordinates, target_coordinates, declaration, s, k, field)
        assert out == values(r['output'])
        if k:
            d = boundary(source, coordinates[s], k, field)
            assert d == values(r['boundary'])
            td = boundary(target, target_coordinates[s], k, out)
            assert td == values(r['target_boundary'])
            assert apply_map(coordinates, target_coordinates, declaration, s, k - 1, d) == td
        else:
            assert r['boundary'] is r['target_boundary'] is None
        stats['relative_basis_cases'] += r['family'] == 0
        stats['native_map_cases'] += 1
        stats['supported_zero_inputs'] += sum(q == 0 for q in field.values())
        for row in source['occurrences']:
            stats['retained_occurrences'] += len(row)
            stats['retained_inactive_occurrences'] += sum(not o[3] for o in row)
            stats['active_zero_occurrences'] += sum(o[3] and Q(o[4]) == 0 for o in row)
    assert stats['relative_basis_cases'] > 0 and stats['native_map_cases'] == 2 * stats['relative_basis_cases']
    assert stats['connecting_witnesses'] > 0 and stats['supported_zero_outside_sub'] > 0
    print(json.dumps(stats, sort_keys=True))


if __name__ == '__main__':
    main()
