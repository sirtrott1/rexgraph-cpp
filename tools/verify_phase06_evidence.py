#!/usr/bin/env python3
"""Compare complete canonical new fixtures; do not compare timing or RSS values."""
from __future__ import annotations
import argparse
import hashlib
import json
from zipfile import ZipFile
from pathlib import PurePosixPath
from pathlib import Path


def files(root: Path) -> dict[str, str]:
    if not root.is_dir():
        raise ValueError(f'Fixture directory absent: {root}')
    result = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in sorted(root.rglob('*')) if p.is_file()}
    if not result:
        raise ValueError(f'Empty fixture directory: {root}')
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('work', type=Path)
    parser.add_argument('--cross-build', action='store_true')
    parser.add_argument('--phase05-evidence', type=Path)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    report = {}
    for item in ('product', 'boundary'):
        reference = files(args.work / f'gcc_{item}_fixtures')
        for mode in ('clang', 'sanitize') if args.cross_build else ():
            other = files(args.work / f'{mode}_{item}_fixtures')
            if other != reference:
                raise ValueError(f'Cross-build mismatch: {mode}/{item}')
        report[item] = {'files': len(reference), 'sha256': reference}
    if args.phase05_evidence:
        frozen = []
        seen = set()
        with ZipFile(args.phase05_evidence) as archive:
            for name in archive.namelist():
                p = PurePosixPath(name)
                group = p.parent.name
                wire = p.suffix.startswith('.rfcx') or p.suffix in ('.rfca', '.rfco')
                if not (group.startswith('gcc_') and group.endswith('_fixtures') and wire):
                    continue
                if group == 'gcc_series_o2_fixtures' or p.stem in ('mutation', 'rejected', 'truncated'):
                    continue
                key = (group, p.name)
                if key in seen:
                    raise ValueError(f'Ambiguous frozen artifact: {key}')
                seen.add(key)
                data = (args.work / group / p.name).read_bytes()
                if data != archive.read(name):
                    raise ValueError(f'Phase 05 bytes changed: {key}')
                frozen.append({'group': group, 'file': p.name, 'sha256': hashlib.sha256(data).hexdigest()})
        if not frozen:
            raise ValueError('No Phase 05 fixture bytes found')
        report['frozen_phase05'] = frozen
    report['all_compared_bytes_equal'] = True
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'fixture_files': sum(report[k]['files'] for k in ('product', 'boundary')),
                      'all_compared_bytes_equal': True}))


if __name__ == '__main__':
    main()
