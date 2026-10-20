"""A public example catalog must not read adjacent private workspaces."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('public_catalog', ROOT / 'examples/temporal_dynamics/catalog.py')
catalog = importlib.util.module_from_spec(spec)
spec.loader.exec_module(catalog)
spec = importlib.util.spec_from_file_location('public_repository', ROOT / 'tools/check_public_repository.py')
repository = importlib.util.module_from_spec(spec)
spec.loader.exec_module(repository)


class PublicCatalog(unittest.TestCase):
    def test_private_subtrees_are_rejected_at_any_depth(self):
        for path in ['experiments/run.py', 'examples/internal/data.json', 'tests/research/report.py']:
            with self.subTest(path=path):
                self.assertEqual(repository.reason(path), 'private workspace material')

    def test_catalog_stays_inside_its_library(self):
        with tempfile.TemporaryDirectory(prefix='rfc-catalog-test-') as directory:
            root = Path(directory); library = root / 'rexgraphcpp'
            example = library / 'examples/example'; example.mkdir(parents=True)
            for name, contents in [('CMakeLists.txt', 'project(example)'), ('main.cpp', 'int main() {}'), ('README.md', '# Example')]:
                (example / name).write_text(contents)
            private = root / 'rfc-notebook/notebooks'; private.mkdir(parents=True)
            (private / 'local-research.rfnb').write_text('private sentinel')
            original = Path.read_bytes
            def read(path):
                self.assertTrue(path.is_relative_to(library), str(path))
                return original(path)
            with patch.object(Path, 'read_bytes', autospec=True, side_effect=read):
                result = catalog.catalog(library)
            self.assertEqual(set(result), {'examples', 'source'})
            self.assertEqual([item['name'] for item in result['examples']], ['example'])

    def test_current_catalog_has_no_research_or_notebook_payload(self):
        result = catalog.catalog(ROOT)
        self.assertEqual(set(result), {'examples', 'source'})
        self.assertGreater(len(result['examples']), 0)
        self.assertTrue(all(item['source'].startswith('rexgraphcpp/examples/') for item in result['examples']))


if __name__ == '__main__':
    unittest.main()
