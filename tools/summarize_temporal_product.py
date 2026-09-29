#!/usr/bin/env python3
"""Summarize raw Phase 06 instrumented trials without mixing event kinds.

Nearest-rank p95 from three trials is their maximum, not a reliable population
latency estimate. Raw trials and per-step counts remain authoritative.
"""
from __future__ import annotations
import argparse
import csv
import json
import math
import statistics
from collections import defaultdict
from pathlib import Path


def summarize(roots: list[Path], output: Path) -> dict:
    groups = defaultdict(list)
    checkpoints, work, metadata = [], [], []
    for root in roots:
        info = json.loads((root / 'summary.json').read_text())
        if not info['exact_reference_equal'] or not info['canonical_history_preserved']:
            raise ValueError('Only a verified complete trial may be summarized')
        metadata.append(info)
        with (root / 'timings.csv').open(newline='') as stream:
            for row in csv.DictReader(stream):
                groups[(int(row['triangles']), int(row['step']), row['operation'])].append(row)
        for name, destination in [('checkpoints.csv', checkpoints), ('work.csv', work)]:
            with (root / name).open(newline='') as stream:
                destination.extend(dict(row) for row in csv.DictReader(stream))
    output.mkdir(parents=True, exist_ok=True)
    with (output / 'latency_allocation_summary.csv').open('w', newline='') as stream:
        writer = csv.writer(stream)
        writer.writerow(['triangles', 'step', 'operation', 'samples', 'median_ns', 'p95_nearest_rank_ns',
                         'min_ns', 'max_ns', 'median_cpp_allocations', 'median_cpp_requested_bytes'])
        for key, rows in sorted(groups.items()):
            timings = sorted(int(row['nanoseconds']) for row in rows)
            writer.writerow([*key, len(rows), statistics.median(timings),
                             timings[math.ceil(.95 * len(timings)) - 1], min(timings), max(timings),
                             statistics.median(int(row['cpp_allocations']) for row in rows),
                             statistics.median(int(row['cpp_requested_bytes']) for row in rows)])
    # Byte/work records must agree across trials of the same endpoint. Never
    # summarize differing semantic workloads as interchangeable repetitions.
    stable_work = {}
    for row in work:
        key = (row['triangles'], row['step'])
        data = {k: v for k, v in row.items() if k != 'trial'}
        if key in stable_work and stable_work[key] != data:
            raise ValueError(f'Non-deterministic work/semantic record: {key}')
        stable_work[key] = data
    stable_cp = {}
    for row in checkpoints:
        key = row['triangles']
        data = {k: v for k, v in row.items() if k != 'trial'}
        if key in stable_cp and stable_cp[key] != data:
            raise ValueError(f'Non-deterministic storage/checkpoint record: {key}')
        stable_cp[key] = data
    report = {'runs': metadata, 'work': list(stable_work.values()),
              'checkpoints': list(stable_cp.values()),
              'timing_groups': len(groups), 'instrumented': True,
              'allocation_exclusions': ['GMP/internal', 'aligned allocations', 'mmap/OS', 'peak live bytes'],
              'max_rss_includes_reference_verification': True, 'controlled_cold_os_cache': False,
              'p95_note': 'Nearest rank; with three trials it is the maximum of three, not a population tail estimate.'}
    (output / 'benchmark_summary.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runs', type=Path, nargs='+')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = summarize(args.runs, args.output)
    print(json.dumps({'runs': len(result['runs']), 'timing_groups': result['timing_groups'],
                      'deterministic_work': True}))


if __name__ == '__main__':
    main()
