#!/usr/bin/env python3
"""Compile each public header independently, then link opposite include orders."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='c++')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    headers = sorted(p.relative_to(source / 'include').as_posix()
                     for p in (source / 'include/rfc').rglob('*.hpp'))
    if not headers:
        parser.error('no public headers')
    cflags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', 'gmpxx', 'gmp'], text=True))
    libs = shlex.split(subprocess.check_output(['pkg-config', '--libs', 'gmpxx', 'gmp'], text=True))
    base = [args.compiler, '-std=c++20', '-Wall', '-Wextra', '-Wpedantic',
            '-Wconversion', '-Wshadow', '-Werror', '-pthread',
            '-I' + str(source / 'include'), *cflags]
    result = dict(compiler=subprocess.check_output([args.compiler, '--version'], text=True).splitlines()[0],
                  headers=[], opposite_include_order_link=False)
    for index, name in enumerate(headers, 1):
        start = time.monotonic()
        completed = subprocess.run([*base, '-fsyntax-only', '-x', 'c++', '-'],
                                   input=f'#include <{name}>\n#include <{name}>\n',
                                   text=True, capture_output=True)
        result['headers'].append(dict(header=name, exit_code=completed.returncode,
                                      seconds=time.monotonic() - start,
                                      diagnostics=completed.stdout + completed.stderr))
        if completed.returncode:
            print(name + ':\n' + completed.stderr, flush=True)
        if index % 20 == 0:
            print(f'Checked {index}/{len(headers)} isolated headers', flush=True)
    if all(item['exit_code'] == 0 for item in result['headers']):
        with tempfile.TemporaryDirectory(prefix='rfc-header-odr-') as directory:
            work = Path(directory)
            first = work / 'first.cpp'
            second = work / 'second.cpp'
            first.write_text(''.join(f'#include <{name}>\n' for name in headers) +
                             'rfc::numeric::Rational rfc_header_probe() { return rfc::numeric::rational(1,2); }\n')
            second.write_text(''.join(f'#include <{name}>\n' for name in reversed(headers)) +
                              'rfc::numeric::Rational rfc_header_probe();\n'
                              'int main() { return rfc_header_probe() != rfc::numeric::rational(1,2); }\n')
            completed = subprocess.run([*base, str(first), str(second), *libs, '-o', str(work / 'probe')],
                                       text=True, capture_output=True)
            result['link_diagnostics'] = completed.stdout + completed.stderr
            if completed.returncode == 0:
                completed = subprocess.run([str(work / 'probe')], text=True, capture_output=True)
                result['opposite_include_order_link'] = completed.returncode == 0
            if not result['opposite_include_order_link']:
                print(result['link_diagnostics'] + completed.stderr, flush=True)
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + '\n')
    success = all(item['exit_code'] == 0 for item in result['headers']) and result['opposite_include_order_link']
    print(f"Header contract {'passed' if success else 'failed'}: {len(headers)} headers and two linked translation units")
    return 0 if success else 1


if __name__ == '__main__':
    raise SystemExit(main())
