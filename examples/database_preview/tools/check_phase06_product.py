#!/usr/bin/env python3
"""Run the original integrated product and round-trip both complete histories.
No new mathematical fixture replaces the original Phase 06 application.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import subprocess
import sys
sys.dont_write_bytecode = True
import reference_check as reference

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('product_executable', type=Path)
p.add_argument('database_executable', type=Path)
p.add_argument('output', type=Path)
args = p.parse_args()
args.output.mkdir(exist_ok=False, parents=True)
exe = str(args.database_executable.resolve())
checks = 0

def run(command: list[str], *, ok: bool = True) -> bytes:
    global checks
    r = subprocess.run(command, capture_output=True, timeout=240)
    checks += 1
    if (r.returncode == 0) != ok:
        raise RuntimeError((command, r.returncode, r.stdout, r.stderr))
    return r.stdout

log = run([str(args.product_executable.resolve()), str(args.output/'product'), '2', '1'])
(args.output/'product.log').write_bytes(log)
records = []
for i, source in enumerate(sorted((args.output/'product').rglob('*.rfcxs'))):
    directory = args.output/f'database-{i}'
    packed = args.output/f'database-{i}.rfcxdb'
    copy = args.output/f'copy-{i}'
    repacked = args.output/f'repacked-{i}.rfcxdb'
    exported = args.output/f'exported-{i}.rfcxs'
    run([exe, 'import-series', str(source), str(directory)])
    run([exe, 'pack', str(directory), str(packed)])
    run([exe, 'unpack', str(packed), str(copy)])
    run([exe, 'pack', str(copy), str(repacked)])
    run([exe, 'export-series', str(copy), str(exported)])
    assert packed.read_bytes() == repacked.read_bytes()
    assert source.read_bytes() == exported.read_bytes()
    checks += 2
    observation = json.loads(run([exe, 'inspect', str(packed)]))
    assert observation['first'] == 0 and observation['last'] == 16
    checks += 1
    run([exe, 'sample-catalogue', str(packed)], ok=False)
    decoded = reference.decode(packed.read_bytes())
    assert reference.encode_series(decoded) == source.read_bytes()
    checks += 1
    records.append(dict(source=str(source.relative_to(args.output)),
                        archive_bytes=len(source.read_bytes()), database_bytes=len(packed.read_bytes()),
                        logical_component_bytes=decoded['logical_component_bytes'],
                        unique_component_bytes=decoded['unique_component_bytes'],
                        unique_objects=decoded['unique_objects']))
assert len(records) == 2
checks += 1
report = dict(cli_and_roundtrip_checks=checks, independent_outer_checks=reference.checks,
              native_product_output=log.decode().strip(), histories=records,
              scope='Original Phase 06 17-endpoint full/repacked histories; full bytes preserved; not a performance trial')
(args.output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
