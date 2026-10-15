#!/usr/bin/env python3
"""Exercise rejected dependency graphs and guard invocation outside the source."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from check_dependencies import audit
from check_exact_types import CORE, check


class DependencyGuards(unittest.TestCase):
    def setUp(self):
        self.work = tempfile.TemporaryDirectory()
        self.root = Path(self.work.name)
        self.addCleanup(self.work.cleanup)

    def header(self, name, text='#pragma once\n'):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def test_valid_graph(self):
        self.header('machine/a.hpp')
        self.header('numeric/a.hpp', '# include <rfc/machine/a.hpp>\n')
        result = audit(self.root)
        self.assertEqual(result['errors'], [])
        self.assertEqual(result['edges'], 1)

    def test_angle_bracket_upward_edge(self):
        self.header('numeric/a.hpp', '#include <rfc/core/a.hpp>\n')
        self.header('core/a.hpp')
        self.assertTrue(any('layer violation' in e for e in audit(self.root)['errors']))

    def test_same_module_cycle(self):
        self.header('numeric/a.hpp', '#include "rfc/numeric/b.hpp"\n')
        self.header('numeric/b.hpp', '#include "a.hpp"\n')
        self.assertTrue(any('include cycle' in e for e in audit(self.root)['errors']))

    def test_missing_include(self):
        self.header('numeric/a.hpp', '#include <rfc/numeric/missing.hpp>\n')
        self.assertTrue(any('missing internal' in e for e in audit(self.root)['errors']))

    def test_comments_are_not_imports(self):
        self.header('numeric/a.hpp', '/*\n#include "rfc/missing/a.hpp"\n*/\n// #include <rfc/x.hpp>\n')
        self.assertEqual(audit(self.root)['errors'], [])

    def test_unregistered_module(self):
        self.header('unknown/a.hpp')
        self.assertTrue(any('unregistered' in e for e in audit(self.root)['errors']))

    def test_fundamental_import_leak(self):
        self.header('fundamentals.hpp', '#include <rfc/storage/a.hpp>\n')
        self.header('storage/a.hpp')
        self.assertTrue(any('fundamental import leak' in e for e in audit(self.root)['errors']))

    def test_runtime_same_rank_storage_leak(self):
        self.header('runtime/a.hpp', '#include <rfc/storage/a.hpp>\n')
        self.header('storage/a.hpp')
        self.assertTrue(any('runtime import leak' in e for e in audit(self.root)['errors']))

    def test_module_cannot_import_umbrella(self):
        self.header('numeric/a.hpp', '#include <rfc/rfc.hpp>\n')
        self.header('rfc.hpp')
        self.assertTrue(any('imports umbrella' in e for e in audit(self.root)['errors']))

    def test_empty_root_rejects(self):
        with self.assertRaises(ValueError):
            audit(self.root)

    def test_exact_missing_and_injected_float(self):
        with self.assertRaises(ValueError):
            check(self.root, 'core')
        for module in CORE:
            self.header(module + '/a.hpp')
        self.assertEqual(check(self.root, 'core')[1], [])
        self.header('geometry/a.hpp', 'using Coordinate = double;\n')
        self.assertEqual(len(check(self.root, 'core')[1]), 1)

    def test_outside_source_and_concurrent_wrappers(self):
        scripts = ['check_layering.sh', 'check_exact_core_no_float.sh',
                   'check_exact_metric_learning_no_float.sh', 'check_machine_binary_learning_no_float.sh']
        processes = [subprocess.Popen(['bash', str(ROOT / 'tools' / name)], cwd=self.root,
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE) for name in scripts * 2]
        try:
            results = [(process, process.communicate(timeout=30)) for process in processes]
        finally:
            for process in processes:
                if process.poll() is None:
                    process.kill()
                process.communicate()
        for process, (out, error) in results:
            self.assertEqual(process.returncode, 0, (out + error).decode())
            self.assertIn(b'passed', out)


if __name__ == '__main__':
    unittest.main()
