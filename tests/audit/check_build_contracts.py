#!/usr/bin/env python3
"""Check embedded builds, truthful options, installed graphs and relocation."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

UNIMPLEMENTED = ('REMOTE_MODEL_HTTP', 'LOCAL_MODEL_RUNTIME', 'ONNX', 'CUDA',
                 'SPEX', 'FLINT', 'GRAPHBLAS', 'LINBOX')
MINIMAL = ['-DBUILD_TESTING=OFF', '-DRFC_BUILD_TESTS=OFF', '-DRFC_BUILD_APPS=OFF',
           '-DRFC_BUILD_LOCAL_TOOLS=OFF', '-DRFC_BUILD_BENCHMARKS=OFF']


def check(source, cmake, compiler, work, sqlite=False, vulkan_include=None, vulkan_library=None):
    count = 0

    def run(args, succeeds=True):
        nonlocal count
        result = subprocess.run(args, text=True, capture_output=True, timeout=180)
        count += 1
        (work / f'command-{count}.log').write_text(result.stdout + result.stderr)
        if (result.returncode == 0) != succeeds:
            raise RuntimeError(f"Unexpected status {result.returncode}: {args}\n{result.stdout}{result.stderr}")
        return result.stdout + result.stderr

    def configure(src, directory, *flags, succeeds=True):
        return run([cmake, '-S', str(src), '-B', str(directory),
                    '-DCMAKE_CXX_COMPILER=' + compiler, *flags], succeeds)

    for adapter in UNIMPLEMENTED:
        output = configure(source, work / adapter, *MINIMAL,
                           f'-DRFC_ENABLE_{adapter}=ON', succeeds=False)
        if 'has no implemented adapter' not in output:
            raise RuntimeError('Adapter rejected for an unrelated reason: ' + adapter)

    output = configure(source, work / 'software-without-vulkan', *MINIMAL,
                       '-DRFC_VULKAN_TEST_SOFTWARE=ON', succeeds=False)
    if 'requires RFC_ENABLE_VULKAN=ON' not in output:
        raise RuntimeError('Software Vulkan switch rejected for an unrelated reason')

    nested = work / 'embedded'
    nested.mkdir()
    (nested / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.24)
project(embedded LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)
include(CTest)
add_subdirectory("{source.as_posix()}" rfc)
if(NOT CMAKE_CXX_STANDARD EQUAL 17 OR NOT CMAKE_CXX_EXTENSIONS)
  message(FATAL_ERROR "RFC changed host language defaults")
endif()
foreach(option IN ITEMS RFC_BUILD_APPS RFC_BUILD_LOCAL_TOOLS RFC_BUILD_TESTS RFC_BUILD_BENCHMARKS RFC_BUILD_LEARNING)
  if(${{option}})
    message(FATAL_ERROR "RFC enabled compiled artifacts inside its consumer")
  endif()
endforeach()
get_property(tests DIRECTORY "{source.as_posix()}" PROPERTY TESTS)
if(tests)
  message(FATAL_ERROR "RFC leaked tests into its consumer")
endif()
get_target_property(warnings rfc_core INTERFACE_COMPILE_OPTIONS)
if(warnings)
  message(FATAL_ERROR "RFC forced consumer warning flags")
endif()
''')
    configure(nested, work / 'embedded-build')

    # Turning off learning must actually remove its compiled artifacts.
    no_learning = work / 'no-learning'
    configure(source, no_learning, '-DRFC_BUILD_LEARNING=OFF')
    output = run([cmake, '--build', str(no_learning), '--target', 'help'])
    for name in ('rfc_binary_learning_tests', 'rfc_integer_learning_demo',
                 'rfc_integer_relational_bench', 'rfc_prepared_consumer_tests',
                 'rfc_affine_transport_completion_tests', 'rfc_transport_family_gluing_tests',
                 'rfc_assertion_diagnosis_tests', 'rfc_multiplicative_relation_tests',
                 'rfc_transport_family_gluing_demo'):
        if name in output:
            raise RuntimeError('Disabled learning artifact remains: ' + name)

    build = work / 'package-build'
    optional = []
    if sqlite:
        optional.append('-DRFC_ENABLE_SQLITE_INTERPRETATION=ON')
    if vulkan_include:
        optional.extend(['-DRFC_ENABLE_VULKAN=ON', '-DVulkan_INCLUDE_DIR=' + vulkan_include, '-DVulkan_LIBRARY=' + vulkan_library])
    configure(source, build, *MINIMAL, *optional)
    if vulkan_include:
        run([cmake, '--build', str(build), '--target', 'rfc_vulkan_binary', '--parallel', '1'])
    original = work / 'original-prefix'
    run([cmake, '--install', str(build), '--prefix', str(original)])
    prefix = work / 'relocated-prefix'
    original.rename(prefix)
    installed_headers = sorted(p.relative_to(prefix / 'include').as_posix()
                               for p in (prefix / 'include/rfc').rglob('*.hpp'))
    expected = sorted(p.relative_to(source / 'include').as_posix()
                      for p in (source / 'include/rfc').rglob('*.hpp'))
    if installed_headers != expected:
        raise RuntimeError('Incomplete installed header inventory')
    for config in prefix.rglob('*.cmake'):
        if str(source) in config.read_text() or str(original) in config.read_text():
            raise RuntimeError('Source/install path leak in ' + str(config))

    consumer = work / 'consumer'
    consumer.mkdir()
    (consumer / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.24)
project(installed LANGUAGES CXX)
set(CMAKE_DISABLE_FIND_PACKAGE_Vulkan TRUE)
set(CMAKE_DISABLE_FIND_PACKAGE_SQLite3 TRUE)
find_package(RFC 0.2 CONFIG REQUIRED)
if(TARGET Vulkan::Vulkan OR TARGET SQLite::SQLite3 OR TARGET SQLite3::SQLite3
   OR TARGET rfc::vulkan_binary OR TARGET rfc::sqlite_interpretation)
  message(FATAL_ERROR "Optional adapter leaked into the base package")
endif()
get_property(targets DIRECTORY PROPERTY IMPORTED_TARGETS)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/target-graph.txt" "")
foreach(target IN LISTS targets)
  if(target MATCHES "^rfc::")
    get_target_property(deps ${target} INTERFACE_LINK_LIBRARIES)
    get_target_property(warnings ${target} INTERFACE_COMPILE_OPTIONS)
    if(warnings)
      message(FATAL_ERROR "Installed target forces consumer warnings")
    endif()
    file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/target-graph.txt" "${target}|${deps}\\n")
  endif()
endforeach()
add_executable(probe first.cpp second.cpp)
target_link_libraries(probe PRIVATE rfc::fundamentals rfc::native_query_execution)
''')
    (consumer / 'first.cpp').write_text('#include <rfc/fundamentals.hpp>\n'
                                      'rfc::numeric::Rational value() { return rfc::numeric::rational(1,2); }\n')
    (consumer / 'second.cpp').write_text('#include <rfc/database/query_tensor_work.hpp>\n'
                                       '#include <rfc/fundamentals.hpp>\n'
                                       'rfc::numeric::Rational value();\n'
                                       'int main() { return value() != rfc::numeric::rational(1,2); }\n')
    consumer_build = work / 'consumer-build'
    configure(consumer, consumer_build, '-DCMAKE_PREFIX_PATH=' + str(prefix))
    run([cmake, '--build', str(consumer_build), '--parallel', '1'])
    run([str(consumer_build / 'probe')])
    graph = {}
    for line in (consumer_build / 'target-graph.txt').read_text().splitlines():
        target, deps = line.split('|', 1)
        graph[target] = [dep for dep in deps.split(';') if dep.startswith('rfc::')]
    visiting, visited = set(), set()

    def visit(target):
        if target in visiting:
            raise RuntimeError('Circular installed targets: ' + target)
        if target in visited:
            return
        if target not in graph:
            raise RuntimeError('Missing exported dependency: ' + target)
        visiting.add(target)
        for dep in graph[target]:
            visit(dep)
        visiting.remove(target)
        visited.add(target)

    for target in graph:
        visit(target)
    available = []
    if sqlite:
        available.append(('SQLite', 'sqlite_interpretation', '#include <rfc/io/sqlite_interpretation.hpp>\nint main() { return sqlite3_libversion_number() <= 0; }\n', []))
    if vulkan_include:
        available.append(('Vulkan', 'vulkan_binary', '#include <rfc/backend/vulkan_binary.hpp>\nint main() { auto devices = rfc::backend::enumerate_vulkan_devices(); return devices.size() > 1000000; }\n',
                          ['-DVulkan_INCLUDE_DIR=' + vulkan_include, '-DVulkan_LIBRARY=' + vulkan_library]))
    for component, target, program, flags in available:
        (consumer / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.24)\n'
                                               'project(optional LANGUAGES CXX)\n'
                                               f'find_package(RFC CONFIG REQUIRED COMPONENTS {component})\n'
                                               'add_executable(probe optional.cpp)\n'
                                               f'target_link_libraries(probe PRIVATE rfc::{target})\n')
        (consumer / 'optional.cpp').write_text(program)
        directory = work / ('available-' + component)
        configure(consumer, directory, '-DCMAKE_PREFIX_PATH=' + str(prefix), *flags)
        run([cmake, '--build', str(directory), '--parallel', '1'])
        run([str(directory / 'probe')])
    unavailable = set(('Vulkan', 'SQLite', 'Unknown')) - {item[0] for item in available}
    for component in sorted(unavailable):
        (consumer / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.24)\n'
                                               'project(missing LANGUAGES CXX)\n'
                                               f'find_package(RFC CONFIG REQUIRED COMPONENTS {component})\n')
        output = configure(consumer, work / ('missing-' + component),
                           '-DCMAKE_PREFIX_PATH=' + str(prefix), succeeds=False)
        if 'unknown or was not built' not in output:
            raise RuntimeError('Component rejected for an unrelated reason: ' + component)
    return dict(commands=count, unavailable_adapters=len(UNIMPLEMENTED),
                installed_headers=len(installed_headers), exported_targets=len(graph),
                installed_graph_acyclic=True, relocation=True,
                embedded_defaults_preserved=True, disabled_learning_artifacts=True,
                optional_adapter_imports=False, consumer_warning_flags=False,
                positive_optional_components=[item[0] for item in available])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--compiler', default='c++')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--sqlite', action='store_true')
    parser.add_argument('--vulkan-include')
    parser.add_argument('--vulkan-library')
    args = parser.parse_args()
    if bool(args.vulkan_include) != bool(args.vulkan_library):
        parser.error('Vulkan include and library must both be supplied')
    with tempfile.TemporaryDirectory(prefix='rfc-build-contract-') as directory:
        result = check(args.source.resolve(), args.cmake, args.compiler, Path(directory), args.sqlite, args.vulkan_include, args.vulkan_library)
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
