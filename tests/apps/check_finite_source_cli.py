"""Exercise the public solver process, admission ordering and original archive preservation."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile
import unittest

SOLVER, FIXTURE = map(lambda value: str(Path(value).resolve()), sys.argv[1:3])


class FiniteSourceCLI(unittest.TestCase):
    def invoke(self, *arguments):
        return subprocess.run([SOLVER, *map(str, arguments)], capture_output=True,
                              text=True, timeout=10)

    def test_invalid_limits_are_rejected_before_archive_access(self):
        with tempfile.TemporaryDirectory(prefix='rfc-cli-') as directory:
            missing = Path(directory) / 'archive-must-not-be-opened'
            for value in ['', '-1', '+1', '1x', '1.0', ' 1', '1 ', '0x10', '9' * 100]:
                with self.subTest(limit=value):
                    result = self.invoke(missing, value)
                    self.assertEqual(result.returncode, 2)
                    self.assertEqual(result.stdout, '')
                    self.assertNotIn(str(missing), result.stderr)
                    self.assertNotIn('filesystem error', result.stderr)

    def test_invalid_relation_indices_are_rejected_before_archive_access(self):
        with tempfile.TemporaryDirectory(prefix='rfc-cli-') as directory:
            missing = Path(directory) / 'archive-must-not-be-opened'
            for indices in ['', ',', '0,', '0,,1', '-1', '+1', '4294967296', '0,0', '00,0']:
                with self.subTest(indices=indices):
                    result = self.invoke(missing, '1000', 'distinct', indices)
                    self.assertEqual(result.returncode, 2)
                    self.assertEqual(result.stdout, '')
                    self.assertNotIn(str(missing), result.stderr)
                    self.assertNotIn('filesystem error', result.stderr)

    def test_real_source_search_profiles_and_zero_budget(self):
        with tempfile.TemporaryDirectory(prefix='rfc-cli-') as directory:
            archive = Path(directory) / 'pair.rfcfs'
            subprocess.run([FIXTURE, str(archive)], check=True, capture_output=True, text=True, timeout=10)
            original = archive.read_bytes()
            for profile in ['bounded', 'propagated', 'distinct']:
                suffix = ['0'] if profile == 'distinct' else []
                with self.subTest(profile=profile):
                    result = self.invoke(archive, '100000', profile, *suffix)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    output = json.loads(result.stdout)
                    self.assertEqual(output['status'], 'satisfiable')
                    self.assertTrue(output['source_canonical'])
                    self.assertEqual(output['profile'], profile)
                    self.assertIn(output['witness'], [[11, 12], [12, 11]])
                    limited = self.invoke(archive, '0', profile, *suffix)
                    self.assertEqual(limited.returncode, 0, limited.stderr)
                    exhausted = json.loads(limited.stdout)
                    self.assertEqual(exhausted['status'], 'budget_exceeded')
                    self.assertIsNone(exhausted['witness'])
                    self.assertEqual(archive.read_bytes(), original)

    def test_oversized_archive_refuses_before_decode(self):
        with tempfile.TemporaryDirectory(prefix='rfc-cli-') as directory:
            archive = Path(directory) / 'too-large.rfcfs'
            with archive.open('wb') as stream:
                stream.truncate((256 << 20) + 1)
            result = self.invoke(archive, '1000')
            self.assertEqual(result.returncode, 2)
            self.assertEqual(result.stdout, '')
            self.assertTrue('256 MiB' in result.stderr or 'byte budget' in result.stderr, result.stderr)


if __name__ == '__main__':
    unittest.main(argv=[sys.argv[0]])
