"""Exercise CI selection and complete test ownership, including a real CMake build."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


ci = load('native_ci', ROOT / 'tools/ci.py')
headers = load('native_headers', ROOT / 'tools/check_headers.py')
repository = load('native_repository', ROOT / 'tools/check_public_repository.py')


class Selection(unittest.TestCase):
    def test_documentation_avoids_compilation(self):
        self.assertFalse(ci.changes(['README.md', 'examples/native_gluing/README.md'])['code'])

    def test_workflow_and_ci_tests_keep_companions_separate(self):
        self.assertEqual(ci.changes(['.github/workflows/ci.yml', 'tools/ci.py', 'tests/audit/test_ci.py']),
                         dict(code=False, headers=False, companions=False, full=False))

    def test_source_header_selects_complete_regressions_and_header_checks(self):
        selected = ci.changes(['include/rfc/field/metric.hpp'])
        self.assertTrue(selected['code'] and selected['headers'])

    def test_native_test_selects_regressions(self):
        self.assertTrue(ci.changes(['tests/field/test_metric.cpp'])['code'])

    def test_frozen_fixture_selects_regressions(self):
        self.assertTrue(ci.changes(['tests/storage/fixtures/base.rfcx'])['code'])

    def test_independent_oracle_selects_regressions(self):
        self.assertTrue(ci.changes(['tools/verify_lossless_reference.py'])['code'])

    def test_cmake_changes_select_both(self):
        self.assertEqual(ci.changes(['cmake/RFCInstall.cmake']),
                         dict(code=True, headers=True, companions=False, full=False))

    def test_manual_full_run_selects_every_profile(self):
        self.assertTrue(ci.changes([], full=True)['code'])
        self.assertTrue(ci.changes([], full=True)['headers'])
        self.assertFalse(ci.changes([], full=True)['companions'])

    def test_tag_push_runs_all_profiles_even_for_documentation_updates(self):
        with patch.dict(os.environ, {'GITHUB_EVENT_NAME': 'push'}):
            for before in ['0' * 40, 'a' * 40]:
                with self.subTest(before=before):
                    selected = ci.select({'ref': 'refs/tags/v0.3.0', 'before': before})
                    self.assertEqual(selected, dict(code=True, headers=True, companions=False, full=True))

    def test_missing_event_base_is_ignored_outside_valid_existing_pushes(self):
        for kind, before in [('pull_request', 'a' * 40), ('push', '0' * 40),
                             ('push', ''), ('push', 'invalid revision')]:
            with self.subTest(kind=kind, before=before), \
                    patch.dict(os.environ, {'GITHUB_EVENT_NAME': kind}), \
                    patch.object(ci.subprocess, 'run') as run:
                ci.restore_event_base({'before': before})
                run.assert_not_called()

    def test_replaced_annotated_tag_is_restored_in_a_cold_checkout(self):
        with tempfile.TemporaryDirectory(prefix='rfc-ci-retag-') as directory:
            root = Path(directory)
            source = root / 'producer'
            source.mkdir()

            def git(path, *args):
                return subprocess.check_output(['git', '-c', 'user.name=Repository Test',
                    '-c', 'user.email=test@example.com', *args], cwd=path,
                    stderr=subprocess.DEVNULL).decode().strip()

            git(source, 'init', '-b', 'main')
            (source / 'README.md').write_text('# Exact fields\n')
            git(source, 'add', '--', 'README.md')
            git(source, 'commit', '-m', 'docs: describe fields')
            git(source, 'tag', '-a', 'v0.3.0', '-m', 'Exact fields')
            old_tag = git(source, 'rev-parse', 'v0.3.0')
            (source / 'README.md').write_text('# Exact fields\n\nRetain source identities.\n')
            git(source, 'add', '--', 'README.md')
            git(source, 'commit', '-m', 'docs: retain source identities')
            current = git(source, 'rev-parse', 'HEAD')
            git(source, 'tag', '-f', '-a', 'v0.3.0', '-m', 'Exact field sources')
            clone = root / 'consumer'
            git(root, 'clone', '--no-local', '--no-tags', str(source), str(clone))
            event = {'before': old_tag, 'ref': 'refs/tags/v0.3.0', 'forced': True}
            with patch.object(ci, 'ROOT', clone), patch.object(repository, 'ROOT', clone), \
                    patch.dict(os.environ, {'GITHUB_EVENT_NAME': 'push'}):
                with self.assertRaises(subprocess.CalledProcessError):
                    repository.history_commits(event=event, event_name='push')
                ci.restore_event_base(event)
                self.assertEqual(repository.history_commits(event=event, event_name='push'), [current])
                self.assertEqual(list(repository.history_errors([current])), [])
                with patch.object(ci.subprocess, 'run', wraps=subprocess.run) as run:
                    ci.restore_event_base(event)
                    self.assertEqual(run.call_count, 1)

    def test_optional_components_include_non_cpp_inputs(self):
        for path in ['extensions/rexlogic/cmake/RexLogicRFCHeaders.sha256',
                     'extensions/relational_temporal/test_reproduction.py',
                     'extensions/relational_temporal/application/example.rex']:
            with self.subTest(path=path):
                self.assertFalse(ci.changes([path]).get('companions', False))

    def test_core_changes_keep_private_companion_jobs_separate(self):
        self.assertFalse(ci.changes(['include/rfc/storage/file_io.hpp']).get('companions', False))

    def test_companion_documentation_avoids_compilation(self):
        self.assertFalse(ci.changes(['extensions/rexlogic/README.md']).get('companions', False))


class Partition(unittest.TestCase):
    def test_companion_plan_discovers_and_executes_both_optional_components(self):
        with tempfile.TemporaryDirectory(prefix='rfc-ci-companions-') as directory:
            root = Path(directory)
            source = root / 'source'
            source.mkdir()
            (source / 'main.cpp').write_text('int main() { return 0; }\n')
            (source / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.24)
project(companion_fixture LANGUAGES CXX)
enable_testing()
if(RFC_BUILD_TESTS)
  add_executable(core main.cpp)
  add_test(NAME core COMMAND core)
endif()
if(RFC_BUILD_RELATIONAL_TEMPORAL AND REXLOGIC_BUILD_TESTS AND REX_TEMPORAL_BUILD_TESTS)
  foreach(component IN ITEMS rexlogic temporal)
    add_executable(${component} main.cpp)
    add_test(NAME ${component} COMMAND ${component})
  endforeach()
endif()
''')
            build = root / 'build'
            plans = ci.make_plan(source, build, 2, companions=True)
            self.assertEqual(sorted(t for plan in plans for t in plan['tests']), ['rexlogic', 'temporal'])
            self.assertEqual(sorted(t for plan in plans for t in plan['targets']), ['rexlogic', 'temporal'])
            for plan in plans:
                ci.execute(build, plan)

    def test_shared_executables_have_one_owner(self):
        commands = {'a': ['/b/a'], 'b': ['python', '/b/a', '/b/b'], 'c': ['/b/b'], 'd': ['/b/c']}
        plans = ci.partition(commands, {'/b/a': 'a', '/b/b': 'b', '/b/c': 'c'}, 2)
        owner = next(p for p in plans if 'a' in p['tests'])
        self.assertEqual(set(owner['tests']), {'a', 'b', 'c'})
        self.assertEqual(set(owner['targets']), {'a', 'b'})
        self.assertEqual(sorted(t for p in plans for t in p['tests']), sorted(commands))

    def test_invalid_or_empty_shards_reject(self):
        for count in [0, 2]:
            with self.subTest(count=count), self.assertRaises(ValueError):
                ci.partition({'a': ['a']}, {}, count)

    def test_header_partition_covers_all_headers_once(self):
        names = sorted(str(p.relative_to(ROOT / 'include')) for p in (ROOT / 'include/rfc').rglob('*.hpp'))
        groups = [headers.partition(names, i, 4) for i in range(4)]
        self.assertEqual(sorted(name for group in groups for name in group), names)
        self.assertLessEqual(max(map(len, groups)) - min(map(len, groups)), 1)

    def test_invalid_header_shards_reject(self):
        for index, count in [(0, 0), (-1, 2), (2, 2), (0, 3)]:
            with self.subTest(index=index, count=count), self.assertRaises(ValueError):
                headers.partition(['a', 'b'], index, count)

    def test_real_generated_commands_build_and_execute(self):
        with tempfile.TemporaryDirectory(prefix='rfc-ci-test-') as directory:
            root = Path(directory); source = root / 'source'; source.mkdir()
            (source / 'main.cpp').write_text('int main() { return 0; }\n')
            (source / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.24)
project(ci_fixture LANGUAGES CXX)
enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_executable(alpha main.cpp)
add_executable(beta main.cpp)
add_library(faults SHARED main.cpp)
add_test(NAME alpha COMMAND alpha)
add_test(NAME wrapped COMMAND ${Python3_EXECUTABLE} -c "import subprocess,sys;sys.exit(subprocess.call(sys.argv[1:]))" $<TARGET_FILE:alpha>)
add_test(NAME beta COMMAND beta)
add_test(NAME z_path COMMAND ${CMAKE_COMMAND} -E env "PATH=$<TARGET_FILE_DIR:beta>:$ENV{PATH}" beta)
set_tests_properties(z_path PROPERTIES REQUIRED_FILES "$<TARGET_FILE:beta>")
add_test(NAME runtime COMMAND ${Python3_EXECUTABLE} -c "from pathlib import Path;import sys;assert Path(sys.argv[1]).is_file()" $<TARGET_FILE:faults>)
''')
            build = root / 'build'
            plans = ci.make_plan(source, build, 2)
            self.assertEqual(sorted(t for p in plans for t in p['tests']), ['alpha', 'beta', 'runtime', 'wrapped', 'z_path'])
            self.assertEqual(sorted(t for p in plans for t in p['targets']), ['alpha', 'beta', 'faults'])
            self.assertIn('z_path', next(p for p in plans if 'beta' in p['tests'])['tests'])
            for plan in plans:
                ci.execute(build, plan)


if __name__ == '__main__':
    unittest.main()
