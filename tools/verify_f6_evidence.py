#!/usr/bin/env python3
"""Compare exact F6 fixture bytes; optionally compare the retained F5 evidence.

No network, source writes, image/float matching or digest-only identity tests.
Input evidence archives are read, not extracted/executed.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import zipfile

EXTENSIONS = {'.rfcx', '.rfcxl', '.rfcxe', '.rfcxt', '.rfcxd', '.rfcxv', '.rfca', '.rfco',
              '.rfcxds', '.rfcxdu', '.rfcxsm', '.rfcxa', '.rfcxam', '.rfcxi', '.rfcxiu'}


def compare(root: Path, archive: Path | None, cross: bool) -> dict:
    result = {'status': 'passed', 'cross_build': [], 'frozen_f5': []}
    if cross:
        for group in ('execution', 'declarations', 'code'):
            candidates = sorted((root / f'gcc_{group}_fixtures').glob('*'))
            if not candidates:
                raise ValueError(f'missing GCC {group} evidence')
            for p in candidates:
                if p.suffix not in EXTENSIONS or p.stem in ('rejected', 'truncated', 'mutation'):
                    continue
                for mode in ('clang', 'sanitize'):
                    target = root / f'{mode}_{group}_fixtures' / p.name
                    if target.read_bytes() != p.read_bytes():
                        raise ValueError(f'byte mismatch: {target}')
                result['cross_build'].append({'group': group, 'file': p.name,
                                             'sha256': hashlib.sha256(p.read_bytes()).hexdigest()})
        baseline = json.loads((root / 'gcc_cli.json').read_text())['fixtures']
        for mode in ('clang', 'sanitize'):
            report = root / f'{mode}_cli_final.json'
            if not report.exists():
                report = root / f'{mode}_cli.json'
            current = json.loads(report.read_text())['fixtures']
            if current != baseline:
                raise ValueError(f'CLI fixture mismatch: {report}')
            fixture_root = root / report.stem
            for name in baseline:
                if (fixture_root / name).read_bytes() != (root / 'gcc_cli' / name).read_bytes():
                    raise ValueError(f'CLI full-byte mismatch: {fixture_root / name}')
        result['cli_cross_build_files'] = len(baseline)
    if archive:
        with zipfile.ZipFile(archive) as z:
            seen = set()
            for name in z.namelist():
                path = PurePosixPath(name)
                group = path.parent.name
                if not (group.startswith('gcc_') and group.endswith('_fixtures')):
                    continue
                if path.suffix not in EXTENSIONS or path.stem in ('rejected', 'truncated', 'mutation'):
                    continue
                # Only published fixture groups are expected. Never infer paths
                # from an arbitrary archive name or extract untrusted members.
                key = (group, path.name)
                if key in seen:
                    raise ValueError(f'ambiguous baseline: {key}')
                seen.add(key)
                target = root / group / path.name
                if target.read_bytes() != z.read(name):
                    raise ValueError(f'changed/missing frozen F5 file: {target}')
                result['frozen_f5'].append({'group': group, 'file': path.name,
                                          'sha256': hashlib.sha256(target.read_bytes()).hexdigest()})
        if not result['frozen_f5']:
            raise ValueError('no frozen F5 fixture files found')
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('work', type=Path)
    p.add_argument('--f5-evidence', type=Path)
    p.add_argument('--cross-build', action='store_true')
    p.add_argument('--report', type=Path)
    a = p.parse_args()
    result = compare(a.work, a.f5_evidence, a.cross_build)
    text = json.dumps(result, indent=2) + '\n'
    if a.report:
        a.report.write_text(text)
    print(f"Exact files: {len(result['cross_build'])} cross-build; "
          f"{result.get('cli_cross_build_files', 0)} CLI; {len(result['frozen_f5'])} frozen F5")


if __name__ == '__main__':
    main()
