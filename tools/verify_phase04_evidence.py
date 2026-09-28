#!/usr/bin/env python3
"""Full-byte checks of Phase 04 compiler fixtures and frozen native wire files."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import zipfile

WIRE = {'.rfcx', '.rfcxl', '.rfcxe', '.rfcxt', '.rfcxd', '.rfcxv', '.rfca', '.rfco',
        '.rfcxds', '.rfcxdu', '.rfcxsm', '.rfcxa', '.rfcxam', '.rfcxi', '.rfcxiu'}


def compare(root: Path, archive: Path | None, cross: bool) -> dict[str, object]:
    result: dict[str, object] = {'passed': True, 'cross_build': [], 'frozen_f6': []}
    if cross:
        files = sorted(p for p in (root / 'gcc_defect_fixtures').iterdir() if p.is_file())
        if not files:
            raise ValueError('GCC defect fixtures missing')
        for p in files:
            for mode in ('clang', 'sanitize'):
                directory = root / f'{mode}_defect_final_fixtures'
                if not directory.exists():
                    directory = root / f'{mode}_defect_fixtures'
                if (directory / p.name).read_bytes() != p.read_bytes():
                    raise ValueError(f'cross-build mismatch: {directory / p.name}')
            result['cross_build'].append({'file': p.name, 'wire': p.suffix in WIRE,
                'sha256': hashlib.sha256(p.read_bytes()).hexdigest()})
    if archive:
        with zipfile.ZipFile(archive) as z:
            seen: set[tuple[str, str]] = set()
            for n in z.namelist():
                p = PurePosixPath(n); group = p.parent.name
                if not (group.startswith('gcc_') and group.endswith('_fixtures') and p.suffix in WIRE):
                    continue
                if p.stem in ('rejected', 'truncated', 'mutation'):
                    continue
                key = (group, p.name)
                if key in seen:
                    raise ValueError(f'ambiguous frozen fixture: {key}')
                seen.add(key)
                target = root / group / p.name
                if target.read_bytes() != z.read(n):
                    raise ValueError(f'frozen F6 mismatch: {target}')
                result['frozen_f6'].append({'group': group, 'file': p.name,
                    'sha256': hashlib.sha256(target.read_bytes()).hexdigest()})
            if not seen:
                raise ValueError('No frozen F6 canonical fixtures found')
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('work', type=Path)
    parser.add_argument('--f6-evidence', type=Path)
    parser.add_argument('--cross-build', action='store_true')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    result = compare(args.work, args.f6_evidence, args.cross_build)
    text = json.dumps(result, indent=2) + '\n'
    if args.report:
        args.report.write_text(text, encoding='utf-8')
    print(f"Exact full-byte matches: {len(result['cross_build'])} cross-build, {len(result['frozen_f6'])} frozen F6")


if __name__ == '__main__':
    main()
