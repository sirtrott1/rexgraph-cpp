#!/usr/bin/env python3
"""Select CI work and build the target artifacts needed by each test shard."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def changes(paths, full=False):
    native = full
    headers = full
    for name in paths:
        path = Path(name)
        if name.startswith(('include/', 'src/', 'cmake/')) or path.name == 'CMakeLists.txt':
            native = True
            headers = True
        elif path.suffix in {'.cpp', '.hpp', '.h', '.c', '.comp'}:
            native = True
        elif name.startswith(('tests/', 'data/fixtures/')) and not name.startswith('tests/audit/'):
            native = True
        elif name.startswith('tools/verify_') and '_reference' in name:
            native = True
    return dict(code=native, headers=headers, full=full)


def select(event, head='HEAD'):
    kind = os.environ.get('GITHUB_EVENT_NAME', '')
    if kind in {'schedule', 'workflow_dispatch'}:
        return changes([], full=True)
    base = event.get('pull_request', {}).get('base', {}).get('sha') or event.get('before', '')
    if not re.fullmatch(r'[0-9a-f]{40}', base) or base == '0' * 40:
        return changes([], full=True)
    exists = subprocess.run(['git', 'cat-file', '-e', base + '^{commit}'], cwd=ROOT,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if exists.returncode:
        return changes([], full=True)
    if 'pull_request' in event:
        base = subprocess.check_output(['git', 'merge-base', base, head], cwd=ROOT, text=True).strip()
    paths = subprocess.check_output(['git', 'diff', '--name-only', '-z', base, head], cwd=ROOT)
    return changes([os.fsdecode(name) for name in paths.split(b'\0') if name])


def test_commands(build):
    commands = {}
    for path in sorted(build.rglob('CTestTestfile.cmake')):
        for line in path.read_text().splitlines():
            if not line.startswith('add_test('):
                continue
            if not line.endswith(')'):
                raise ValueError('Unsupported generated test declaration: ' + line)
            args = shlex.split(line[len('add_test('):-1])
            name = args.pop(0)
            bracket = re.fullmatch(r'\[(=*)\[(.*)\]\1\]', name)
            if bracket:
                name = bracket.group(2)
            if name in commands:
                raise ValueError('Duplicate test name: ' + name)
            commands[name] = args
    listed = json.loads(subprocess.check_output(
        ['ctest', '--test-dir', str(build), '--show-only=json-v1'], text=True))
    if set(commands) != {test['name'] for test in listed['tests']}:
        raise ValueError('Generated commands do not cover the complete CTest inventory')
    return commands


def artifact_targets(build):
    reply = build / '.cmake/api/v1/reply'
    index = json.loads(sorted(reply.glob('index-*.json'))[-1].read_text())
    model = json.loads((reply / index['reply']['codemodel-v2']['jsonFile']).read_text())
    targets = {}
    for entry in model['configurations'][0]['targets']:
        target = json.loads((reply / entry['jsonFile']).read_text())
        for artifact in target.get('artifacts', []):
            targets[str((build / artifact['path']).resolve())] = target['name']
    return targets


def partition(commands, artifacts, count):
    if count < 1:
        raise ValueError('Shard count must be positive')
    groups = []
    for name, command in sorted(commands.items()):
        needed = {artifacts[arg] for arg in command if arg in artifacts}
        group = dict(tests={name}, targets=needed)
        overlap = [old for old in groups if old['targets'] & needed]
        for old in overlap:
            group['tests'].update(old['tests'])
            group['targets'].update(old['targets'])
            groups.remove(old)
        groups.append(group)
    if count > len(groups):
        raise ValueError('More shards than independent test groups')
    shards = [dict(tests=[], targets=[]) for _ in range(count)]
    for group in sorted(groups, key=lambda g: (-max(1, len(g['targets'])), min(g['tests']))):
        chosen = min(shards, key=lambda s: (len(s['targets']), len(s['tests'])))
        chosen['tests'].extend(sorted(group['tests']))
        chosen['targets'].extend(sorted(group['targets']))
    tests = [name for shard in shards for name in shard['tests']]
    targets = [name for shard in shards for name in shard['targets']]
    if sorted(tests) != sorted(commands) or len(targets) != len(set(targets)):
        raise ValueError('Test coverage or executable ownership was lost during partitioning')
    return shards


def configure(source, build, launcher=None):
    query = build / '.cmake/api/v1/query/codemodel-v2'
    query.parent.mkdir(parents=True, exist_ok=True)
    query.touch()
    args = ['cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE=Release', '-DRFC_ENABLE_SQLITE_INTERPRETATION=ON',
            '-DBUILD_TESTING=ON', '-DRFC_BUILD_TESTS=ON', '-DRFC_BUILD_APPS=ON',
            '-DRFC_BUILD_LOCAL_TOOLS=ON', '-DRFC_BUILD_BENCHMARKS=ON', '-DRFC_BUILD_LEARNING=ON']
    if launcher:
        args.append('-DCMAKE_CXX_COMPILER_LAUNCHER=' + launcher)
    subprocess.run(args, check=True)


def make_plan(source, build, count, launcher=None):
    configure(source, build, launcher)
    return partition(test_commands(build), artifact_targets(build), count)


def execute(build, plan):
    if plan['targets']:
        subprocess.run(['cmake', '--build', str(build), '--parallel', '1', '--target', *plan['targets']], check=True)
    expression = '^(' + '|'.join(re.escape(name) for name in plan['tests']) + ')$'
    subprocess.run(['ctest', '--test-dir', str(build), '--output-on-failure', '-j1',
                    '--no-tests=error', '-R', expression], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('select')
    for name in ['plan', 'test']:
        item = sub.add_parser(name)
        item.add_argument('--source', type=Path, default=ROOT)
        item.add_argument('--build', type=Path, required=True)
        item.add_argument('--shards', type=int, default=6)
        item.add_argument('--output', type=Path)
        item.add_argument('--launcher')
        if name == 'test':
            item.add_argument('--shard', type=int, required=True)
    args = parser.parse_args()
    if args.command == 'select':
        event = json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_text())
        selected = select(event)
        if 'GITHUB_OUTPUT' in os.environ:
            with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
                output.write(''.join(f'{name}={str(value).lower()}\n' for name, value in selected.items()))
        print(json.dumps(selected))
        return
    build = args.build.resolve()
    plans = make_plan(args.source.resolve(), build, args.shards, args.launcher)
    if args.output:
        args.output.write_text(json.dumps(plans, indent=2) + '\n')
    print(json.dumps([dict(shard=i, tests=len(p['tests']), executables=len(p['targets'])) for i, p in enumerate(plans)]), flush=True)
    if args.command == 'test':
        if not 0 <= args.shard < args.shards:
            parser.error('Shard index is outside the configured shard count')
        execute(build, plans[args.shard])


if __name__ == '__main__':
    main()
