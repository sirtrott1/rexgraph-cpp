#!/usr/bin/env python3
"""Validate the complete RFC header graph and mathematical adapter boundaries."""
import argparse
import json
from pathlib import Path
import re
import sys

RANK = dict(machine=0, numeric=1, composite=2, core=3, algebra=4,
            geometry=5, local=6, topology=7, field=8, view=9, path=9,
            approximation=9, statistics=9, temporal=10, storage=11,
            runtime=11, database=12, capability=12, inference=13,
            learning=14, models=14, llm=15, multimodal=15, agent=16,
            generation=16, backend=17, io=17)
UMBRELLAS = {"fundamentals.hpp", "rfc.hpp", "capabilities.hpp"}
ROOT_MODULES = dict(exact='numeric', metric='field', operators='field',
                    packed_bits='machine', relational='core')
INCLUDE = re.compile(r'^\s*#\s*include\s*([<"])([^">]+)[">]', re.M)


def audit(root):
    root = Path(root).resolve()
    headers = sorted(root.rglob("*.hpp"))
    if not headers:
        raise ValueError(f"no RFC headers under {root}")
    graph = {p.relative_to(root).as_posix(): [] for p in headers}
    errors = []
    for path in headers:
        name = path.relative_to(root).as_posix()
        module = name.split('/')[0] if '/' in name else ROOT_MODULES.get(path.stem)
        if module not in RANK and name not in UMBRELLAS:
            errors.append(f"unregistered header module: {name}")
        # Includes in comments do not participate in the preprocessor graph.
        source = re.sub(r'/\*.*?\*/|//[^\n]*', '', path.read_text(), flags=re.S)
        for quote, inc in INCLUDE.findall(source):
            if inc.startswith('rfc/'):
                dep = inc[4:]
            elif quote == '"':
                target = (path.parent / inc).resolve()
                try:
                    dep = target.relative_to(root).as_posix()
                except ValueError:
                    errors.append(f"include escapes RFC headers: {name} -> {inc}")
                    continue
            else:
                continue
            if dep not in graph:
                errors.append(f"missing internal include: {name} -> {inc}")
                continue
            graph[name].append(dep)
            dep_module = dep.split('/')[0] if '/' in dep else ROOT_MODULES.get(Path(dep).stem)
            if module in RANK and dep_module in RANK and RANK[dep_module] > RANK[module]:
                errors.append(f"layer violation: {name} -> {dep}")
            if module in RANK and dep in UMBRELLAS:
                errors.append(f"module imports umbrella: {name} -> {dep}")
    color, stack = {}, []

    def visit(name):
        color[name] = 1
        stack.append(name)
        for dep in graph[name]:
            if color.get(dep) == 1:
                errors.append('include cycle: ' + ' -> '.join(stack[stack.index(dep):] + [dep]))
            elif not color.get(dep):
                visit(dep)
        stack.pop()
        color[name] = 2

    for name in graph:
        if not color.get(name):
            visit(name)

    def closure(name):
        seen, pending = set(), [name]
        while pending:
            node = pending.pop()
            if node not in seen:
                seen.add(node)
                pending.extend(graph[node])
        return seen

    if 'fundamentals.hpp' in graph:
        for name in closure('fundamentals.hpp') - {'fundamentals.hpp'}:
            module = name.split('/')[0] if '/' in name else ROOT_MODULES.get(Path(name).stem)
            if module not in RANK or RANK[module] > RANK['temporal']:
                errors.append(f"fundamental import leak: {name}")
    for name in graph:
        if name.startswith('runtime/'):
            for dep in closure(name) - {name}:
                if dep.split('/')[0] not in {'runtime', 'machine', 'numeric', 'composite'}:
                    errors.append(f"runtime import leak: {name} -> {dep}")
    return dict(headers=len(graph), edges=sum(map(len, graph.values())),
                modules=sorted({n.split('/')[0] for n in graph if '/' in n}),
                errors=sorted(set(errors)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', nargs='?', type=Path,
                        default=Path(__file__).resolve().parents[1] / 'include/rfc')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        result = audit(args.root)
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 1
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + '\n')
    if result['errors']:
        print('\n'.join(result['errors']), file=sys.stderr)
        return 1
    print(f"RFC dependencies passed: {result['headers']} headers, {result['edges']} includes")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
