# RexGraphCPP

RexGraphCPP is a C++20 mathematical engine for relational complexes, exact tensor
fields, and their evolution. It implements relational field calculus and tensor
algebra over retained sources, with explicit coefficient domains, declared maps,
and reusable sparse operators.

## Relational complexes

Relations are primary. Each relation retains its identity, type, ordered boundary
occurrences, orientations, and exact coefficients. Objects occur at the boundaries
of relations; higher grades describe relations among those relations. Repeated
participants, inactive occurrences, supported zeros, and cancellation remain
distinct in the source.

Cells carry local vector spaces, called stalks, and occurrences carry maps between
those spaces. These maps assemble into graded boundary actions. On a strict
complex, adjacent boundaries satisfy $B_{k-1}B_k=0$. For rational fields with
positive definite grade metrics $G_k$, the metric adjoint and Hodge operator are

$$
B_k^\dagger=G_k^{-1}B_k^T G_{k-1},\qquad
L_k=B_k^\dagger B_k+B_{k+1}B_{k+1}^\dagger.
$$

The orthogonal gradient, curl, and harmonic components reconstruct the complete
field. Exact image and kernel frames, metric solves, and Green actions compute
these components without requiring an eigendecomposition. Curved realizations
retain their chain defects and use their separately declared operations.

Integers and rationals are the primary arithmetic. Algebraic extensions require
explicit fields, embeddings, and root certificates. Approximation is selected
through its own APIs; exceeding an exact work budget returns an explicit refusal.

## The library

The SDK is organized into CMake targets. Each target includes its mathematical
dependencies; applications link the layers they use.

| Target | Operations |
| --- | --- |
| `rfc::core` | Typed relational sources, occurrence identities, exact boundaries |
| `rfc::algebra`, `rfc::geometry` | Declared maps, products, realizations, parameter domains |
| `rfc::local` | Stalks, fiber maps, sections, compatible gluing |
| `rfc::topology` | Integral homology, coverings, homotopies, equivariant chains |
| `rfc::field` | Metrics, Hodge sectors, Dirac and Green actions, periods |
| `rfc::temporal` | Ordered operation words, transport, finite evolution, jets |
| `rfc::fundamentals` | The mathematical layers through temporal calculus |
| `rfc::storage`, `rfc::database` | Canonical archives, publications, historical queries |
| `rfc::inference`, `rfc::learning` | Prepared field programs, parameter families, evidence |
| `rfc::rfc` | The complete library surface |

The [Python library](https://github.com/sirtrott1/rexgraph-python) provides an
approachable interface to construction, analysis, storage, and RCQL queries.
Its optional `rexgraph_cpp` bridge uses this SDK. The C++ engine builds
independently of Python and separately distributed application companions.

## Installation

Version **0.3.0** requires a C++20 compiler, CMake 3.24 or later, pkg-config, and
the GMP/GMPXX development libraries. The commands below also use Ninja.

```sh
git clone https://github.com/sirtrott1/rexgraph-cpp.git rexgraphcpp
cd rexgraphcpp
cmake -S . -B /tmp/rexgraphcpp-sdk-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DRFC_BUILD_APPS=OFF -DRFC_BUILD_LOCAL_TOOLS=OFF \
  -DRFC_BUILD_TESTS=OFF -DRFC_BUILD_BENCHMARKS=OFF \
  -DRFC_BUILD_LEARNING=OFF
cmake --build /tmp/rexgraphcpp-sdk-build --parallel 1
cmake --install /tmp/rexgraphcpp-sdk-build --prefix /tmp/rexgraphcpp-sdk
```

This installs public headers, relocatable CMake targets, and user guides under
`share/doc/rfc`. Mathematical targets are supplied as headers; tools and optional
adapters are enabled separately. `RFC_BUILD_LEARNING=OFF` disables compiled
learning applications and tests while retaining the header APIs.

### Use from CMake

Save the example below as `main.cpp` beside this `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.24)
project(relational_example LANGUAGES CXX)
find_package(RFC 0.3.0 EXACT CONFIG REQUIRED)
add_executable(relational_example main.cpp)
target_link_libraries(relational_example PRIVATE rfc::core)
```

Configure with `-DCMAKE_PREFIX_PATH=/tmp/rexgraphcpp-sdk`. An application can also
embed a selected source checkout using
`add_subdirectory(path/to/rexgraphcpp rfc EXCLUDE_FROM_ALL)` and the same targets.
Embedded builds leave applications, tools, tests, and benchmarks disabled by default.

### Optional adapters

| Adapter | Build option | Installed component and target | Requirements |
| --- | --- | --- | --- |
| SQLite import | `RFC_ENABLE_SQLITE_INTERPRETATION=ON` | `SQLite`, `rfc::sqlite_interpretation` | SQLite development libraries |
| Exact packed binary GPU operations | `RFC_ENABLE_VULKAN=ON` | `Vulkan`, `rfc::vulkan_binary` | Vulkan 1.1 headers/loader, `glslangValidator`, compatible device |

Request components with `find_package(RFC 0.3.0 CONFIG REQUIRED COMPONENTS SQLite)`
or `Vulkan`. Base imports do not load either dependency. Vulkan supplies packed
binary kernels; software devices require explicit `RFC_VULKAN_TEST_SOFTWARE=ON`
for qualification. General rational GPU execution is not provided by this adapter.

## Getting started with the library

This example declares three oriented relations forming a cycle. The boundary of
the exact unit circulation is zero at every vertex:

```cpp
#include <rfc/core/builder.hpp>
#include <iostream>

int main() {
    using namespace rfc;
    core::RelationSignature signature;
    const auto edge = signature.add_type(
        "edge", core::BoundaryPolicy::DeclaredExact, 2, 2);
    core::RelationalComplexBuilder builder(signature);
    for (std::uint64_t i = 0; i < 3; ++i) {
        builder.add_grade_one(core::RelationKey{10 + i}, edge, {
            {i, true, true, numeric::Rational{1}},
            {(i + 1) % 3, true, false, numeric::Rational{1}},
        });
    }
    const auto source = builder.freeze();
    const auto boundary = source.boundary_apply(
        core::Grade{1}, numeric::RationalVector{1, 1, 1});
    for (const auto& value : boundary) std::cout << value << ' ';
    std::cout << '\n';
}
```

Build and run from the directory containing those two files:

```sh
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/tmp/rexgraphcpp-sdk
cmake --build build --parallel 1
./build/relational_example
# 0 0 0
```

For complete field calculations, the
[prepared actions example](examples/prepared_global_actions/README.md) constructs
two fields on one source and reuses Green, harmonic, and gradient potential factors.
Its guide includes source and installed SDK build commands.

## Mathematical operations

| Operation | Technical scope | Guide |
| --- | --- | --- |
| Fields and exact solves | Sparse actions, complete kernel gauges, reusable metric and response factors | [Exact native system](docs/native-exact-system/README.md) |
| Local tensors | Owning stalk fields, declared tensor factors, exact reconstruction | [Stalk and tensor fields](docs/temporal-stalk-tensor-fields/README.md) |
| Metrics and refinement | Positive metrics, chain maps, adjoints, retained fine directions | [Metric and refinement certificates](docs/relational-metric-refinement/README.md) |
| Temporal calculus | Rational parameter domains, covariant and response jets, ordered derivatives, finite Cayley evolution | [Temporal calculus](docs/mathematical-operations/README.md#temporal-calculus) |
| Algebraic constructions | Certified roots, integral periods, finite Jacobians, cyclic characters, surface holonomy | [Integral and algebraic constructions](docs/mathematical-operations/README.md#integral-and-algebraic-constructions) |
| Learning and gluing | Affine transport families, unresolved directions, exact contradiction witnesses | [Transport family gluing](examples/transport_family_gluing/README.md) |
| Products and topology | Tensor products, coherence maps, coverings, declared homotopies | [Example catalog](docs/examples/README.md#products-coverings-and-homotopies) |
| Finite search | Retained domains, checked witnesses, explicit enumeration and propagation budgets | [Finite source search](docs/mathematical-operations/README.md#finite-source-search) |

Prepared operations retain their actual source, carrier, metric, and declared
maps. Reusing a factor requires compatible source ownership; equal dimensions or
values alone do not establish a transport. Each construction states its admitted
hypotheses and bounds. Finite certificates and rational time steps do not by
themselves establish a smooth limit or a continuous time exponential.

## Storage, queries, and execution

- Native archives retain complete sources, fields, declarations, and selected
  endpoints. Codecs check version, checksum, byte, record, and arithmetic bounds.
- Historical queries use explicit source identities and cutoffs. Cross-source
  joins require declared participant correspondences between the selected endpoints.
- Linux durable and concurrent history adapters supply their own publication,
  retry, and recovery contracts.
- CPU execution supports owning jobs, prepared workspaces, field caches, and
  declared resource admission. Sparse elimination can increase support and
  integer sizes; work receipts do not measure total process memory.

The [example catalog](docs/examples/README.md) links the storage, query, history,
CPU, and Vulkan guides. Enable `RFC_BUILD_LOCAL_TOOLS=ON` to build `rfc_local`;
the [CLI guide](docs/local/README.md) covers snapshots, queries, and SQLite import.

## Development and testing

Python 3 is required for the verification scripts. Run one of six regression
shards by choosing a number from 0 through 5:

```sh
python3 tools/ci.py test --build /tmp/rexgraphcpp-tests --shards 6 --shard 0
```

For the complete build and test suite:

```sh
cmake -S . -B /tmp/rexgraphcpp-tests -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/rexgraphcpp-tests --parallel 1
ctest --test-dir /tmp/rexgraphcpp-tests --output-on-failure -j1
```

Start with one compiler process and increase concurrency to suit available memory.
CI also checks isolated GCC/Clang headers, include order, installed consumers,
sanitizers, codec fuzzing, and the explicitly admitted software Vulkan profile.
`python3 tests/audit/check_build_contracts.py` validates SDK installation,
relocation, dependency boundaries, and offline documentation links.

## Documentation and support

Use the [example catalog](docs/examples/README.md) for complete programs and the
[mathematical operation reference](docs/mathematical-operations/README.md) for API
contracts. Report reproducible failures through
[GitHub Issues](https://github.com/sirtrott1/rexgraph-cpp/issues), including the
compiler, SDK version, declarations, and relevant error output.

## License

RexGraphCPP is distributed under the [Apache License 2.0](LICENSE). See
[NOTICE](NOTICE) for attribution. Third party dependencies retain their own terms.
