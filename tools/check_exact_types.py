#!/usr/bin/env python3
"""Check explicit exact arithmetic profiles without shared temporary files."""
import argparse
from pathlib import Path
import re
import sys

CORE = ('machine', 'numeric', 'composite', 'core', 'algebra', 'geometry',
        'local', 'topology', 'field', 'view', 'path', 'statistics', 'temporal',
        'storage', 'database', 'runtime')
METRIC = ('numeric/exact_matrix.hpp', 'field/exact_metric.hpp',
          'field/exact_hodge.hpp', 'field/exact_dirac.hpp',
          'field/augmented_green.hpp', 'learning/exact_metric_channels.hpp')
BINARY = ('composite/program.hpp', 'learning/binary_gate.hpp',
          'learning/composite_features.hpp', 'machine/bitsliced_int.hpp',
          'learning/bitsliced_threshold.hpp', 'learning/bitsliced_affine.hpp',
          'learning/integer_perceptron.hpp', 'learning/integer_relational_layer.hpp',
          'learning/operator_channel.hpp', 'learning/operator_basis.hpp',
          'learning/integer_network.hpp', 'learning/tower_operator.hpp',
          'learning/tower_basis.hpp', 'learning/tower_network.hpp',
          'learning/semantic_network.hpp', 'field/lattice_field.hpp',
          'field/lattice_tower.hpp', 'field/lattice_boundary.hpp',
          'numeric/checked_int.hpp')


def check(root, profile):
    root = Path(root)
    if profile == 'core':
        files = []
        for module in CORE:
            found = sorted((root / module).rglob('*.hpp'))
            if not found:
                raise ValueError(f"exact guard found no headers for {module}")
            files.extend(found)
    else:
        files = [root / name for name in {'metric': METRIC, 'binary': BINARY}[profile]]
    errors = []
    for path in files:
        for line, text in enumerate(path.read_text().splitlines(), 1):
            if re.search(r'\b(?:float|double)\b', text):
                errors.append(f"floating type in {profile} profile: {path}:{line}: {text}")
    return files, errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('profile', choices=['core', 'metric', 'binary'])
    parser.add_argument('root', nargs='?', type=Path,
                        default=Path(__file__).resolve().parents[1] / 'include/rfc')
    args = parser.parse_args()
    try:
        files, errors = check(args.root, args.profile)
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 1
    if errors:
        print('\n'.join(errors), file=sys.stderr)
        return 1
    print(f"Exact {args.profile} profile passed: {len(files)} headers")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
