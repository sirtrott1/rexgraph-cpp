#!/usr/bin/env python3
"""Full-byte series/compiler checks and frozen Phase 04 fixture comparison."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
from zipfile import ZipFile


def wire(path: PurePosixPath | Path) -> bool:
    return path.suffix.startswith('.rfcx') or path.suffix in {'.rfca', '.rfco'}


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('work', type=Path)
    p.add_argument('--phase04-evidence', type=Path)
    p.add_argument('--cross-build', action='store_true')
    p.add_argument('--report', type=Path)
    a = p.parse_args()
    result = {'passed': True, 'cross_build': [], 'frozen_phase04': []}
    if a.cross_build:
        base = a.work/'gcc_series_fixtures'
        files = sorted(x for x in base.iterdir() if x.is_file())
        if not files:
            raise ValueError('missing GCC series fixtures')
        for file in files:
            data = file.read_bytes()
            for mode in ('clang', 'sanitize'):
                other = a.work/f'{mode}_series_fixtures'/file.name
                if other.read_bytes() != data:
                    raise ValueError(f'cross-build mismatch: {other}')
            result['cross_build'].append({'file': file.name, 'wire': wire(file), 'sha256': hashlib.sha256(data).hexdigest()})
    if a.phase04_evidence:
        seen = set()
        with ZipFile(a.phase04_evidence) as z:
            for name in z.namelist():
                path = PurePosixPath(name)
                group = path.parent.name
                if not (group.startswith('gcc_') and group.endswith('_fixtures') and wire(path)):
                    continue
                if path.stem in ('rejected', 'truncated', 'mutation'):
                    continue
                key = (group, path.name)
                if key in seen:
                    raise ValueError(f'ambiguous frozen fixture: {key}')
                seen.add(key)
                current = a.work/group/path.name
                data = current.read_bytes()
                if data != z.read(name):
                    raise ValueError(f'Phase 04 fixture changed: {current}')
                result['frozen_phase04'].append({'group': group, 'file': path.name, 'sha256': hashlib.sha256(data).hexdigest()})
        if not seen:
            raise ValueError('no Phase 04 fixtures found')
    text = json.dumps(result, indent=2)+'\n'
    if a.report:
        a.report.write_text(text)
    print(f"cross_build={len(result['cross_build'])} frozen_phase04={len(result['frozen_phase04'])} full_byte_matches=true")


if __name__ == '__main__':
    main()
