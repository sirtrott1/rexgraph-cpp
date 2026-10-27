"""Check public prose, attribution, workspace references and commit descriptions."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('public_repository', ROOT / 'tools/check_public_repository.py')
repository = importlib.util.module_from_spec(spec)
spec.loader.exec_module(repository)


class PublicContent(unittest.TestCase):
    def test_companion_sources_and_source_links_reject(self):
        for name in ['extensions/rexlogic', 'extensions/rexlogic/include/example.hpp',
                     'extensions/relational_temporal', 'extensions/relational_temporal/README.md']:
            with self.subTest(name=name):
                self.assertIsNotNone(repository.reason(name))

    def test_automated_attribution_rejects(self):
        text = 'Co-' + 'authored-by: ' + 'Cod' + 'ex <agent@example.com>'
        self.assertTrue(repository.message_reasons('Store exact source maps\n\n' + text))

    def test_private_file_links_reject(self):
        text = 'See ../' + 'internal/' + 'notes.md'
        self.assertTrue(repository.content_reasons('README.md', text.encode()))

    def test_absolute_workspace_paths_reject_in_source_and_binary(self):
        text = bytes([0]) + ('/home/' + 'example/project/log.txt').encode()
        self.assertTrue(repository.content_reasons('fixture.bin', text))

    def test_tracking_labels_reject_in_comments_and_messages(self):
        label = 'C' + '03'
        self.assertTrue(repository.content_reasons('include/example.hpp', ('// ' + label + ' update').encode()))
        self.assertTrue(repository.message_reasons('Implement ' + label))

    def test_decorative_dividers_and_promotional_prose_reject(self):
        for text in ['# Example\n' + '-' * 7, 'A game-' + 'changing query engine', 'We lever' + 'age new math']:
            with self.subTest(text=text):
                self.assertTrue(repository.content_reasons('README.md', text.encode()))

    def test_mathematical_symbols_protocol_names_and_tables_remain_valid(self):
        text = '// The boundary satisfies B1 B2 = 0.\nconst auto legacy_c03_pin = 1;'
        self.assertEqual(repository.content_reasons('include/example.hpp', text.encode()), [])
        self.assertEqual(repository.content_reasons('README.md', b'| source | value |\n| --- | --- |\n'), [])

    def test_cpp_strings_do_not_become_comments(self):
        text = 'const char* url = "https://example.com/"; // Retain the original source.'
        self.assertEqual(repository.content_reasons('example.cpp', text.encode()), [])

    def test_operation_based_commit_description_passes(self):
        self.assertEqual(repository.message_reasons('storage: retain exact source identities\n\nReject mismatched occurrence maps.'), [])

    def test_history_checks_old_blobs_and_commit_messages(self):
        with tempfile.TemporaryDirectory(prefix='rfc-public-history-') as directory:
            root = Path(directory)
            def git(*args):
                return subprocess.check_output(['git', '-c', 'user.name=Repository Test',
                                                '-c', 'user.email=test@example.com', *args], cwd=root,
                                               stderr=subprocess.DEVNULL)
            git('init', '-b', 'main')
            (root / 'README.md').write_text('See ../' + 'internal/' + 'notes.md\n')
            git('add', 'README.md')
            git('commit', '-m', 'Implement ' + 'C' + '03')
            (root / 'README.md').write_text('# Exact fields\n\nRetain original source identities.\n')
            git('add', 'README.md')
            git('commit', '-m', 'Document exact fields')
            with patch.object(repository, 'ROOT', root):
                commits = git('rev-list', '--all').decode().splitlines()
                errors = list(repository.history_errors(commits))
                self.assertTrue(any('document reference' in e for e in errors))
                self.assertTrue(any('tracking label' in e for e in errors))
                self.assertEqual(repository.content_reasons('README.md', (root / 'README.md').read_bytes()), [])
                outgoing = repository.history_commits(base=commits[-1])
                self.assertEqual(outgoing, [commits[0]])
                self.assertEqual(list(repository.history_errors(outgoing)), [])
                self.assertEqual(repository.history_commits(event={'before': commits[-1]}, event_name='push'), outgoing)
                self.assertEqual(repository.history_commits(event={'pull_request': {'base': {'sha': commits[-1]}}},
                                                           event_name='pull_request'), outgoing)
                self.assertEqual(repository.history_commits(event={}, event_name='schedule'), [commits[0]])
                self.assertEqual(repository.history_commits(event={'before': '0' * 40}, event_name='push'), list(reversed(commits)))
                git('update-ref', 'refs/remotes/origin/main', commits[-1])
                new_branch = {'before': '0' * 40, 'ref': 'refs/heads/topic', 'repository': {'default_branch': 'main'}}
                self.assertEqual(repository.history_commits(event=new_branch, event_name='push'), outgoing)
                git('commit', '--allow-empty', '-m', 'Implement ' + 'C' + '03')
                scheduled = repository.history_commits(event={}, event_name='schedule')
                self.assertTrue(any('tracking label' in e for e in repository.history_errors(scheduled)))
                git('-c', 'user.name=Cod' + 'ex', 'commit', '--allow-empty', '-m', 'Preserve source identities')
                attribution = repository.history_commits(event={}, event_name='schedule')
                self.assertTrue(any('automated attribution' in e for e in repository.history_errors(attribution)))


if __name__ == '__main__':
    unittest.main()
