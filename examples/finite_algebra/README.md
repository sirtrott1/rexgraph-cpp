# A finite field presented by retained relations

The main paper §3.5, Worked example 3.7, presents `F3` through complete ternary
`Add(a,b,c)` and `Mul(a,b,c)` relations. `PreparedFiniteAlgebra` consumes that
native declaration directly. It owns the complete source, operation type
identities and explicit signature aspects for the left input, right input and
output. The boundary matrix is not used to define or recover the operations.

`fixture.hpp` declares the modular tables with nonconsecutive carrier identities
and output-first occurrence order. Repeated operands retain distinct role
occurrences. `main.cpp` certifies the three-element field:

```cpp
auto algebra = PreparedFiniteAlgebra::prepare(source, operations, guard);
auto field = algebra.check_field(addition_index, multiplication_index,
                                 zero_identity, one_identity, guard);
```

`profiles()` reports totality and functionality separately. A missing input pair
is retained as a diagnostic. Conflicting output assertions keep their original
relation witnesses and prevent a functional operation. Matching duplicate
assertions remain separate evidence for the same value.

`check_law` checks associativity, commutativity, idempotence, left/right identity
or left/right distributivity over the complete finite carrier. A failed law
returns a concrete variable assignment, unequal output identities and original
relation/occurrence traces. An undefined or multivalued required operation is
`Undetermined`, and cannot produce a satisfied field report. Source-bound law
receipts keep the declaration alive and cannot be reassigned to an equal-valued
but separately prepared owner.

`check_field` verifies both complete functional operations, distinct zero and
one, their associative and commutative laws, both identity laws, both
distributive laws, every additive inverse, and every nonzero multiplicative
inverse. It retains exact inverse witnesses. It is a finite check of the
provided relation set, not an assertion that any relation type name presents
an algebra. The tests certify prime modular examples and reject `Z/4Z` as a
field while preserving its valid ring laws and identifying its noninvertible
element.

The implementation retains sparse input-pair indices and every original table
relation. It performs bounded finite identity comparisons with cancellation and
exact-action admission checks. It does not construct dense algebra transforms,
infer multiplication from numerical coefficients, or solve a general symbolic
axiom or polynomial ideal problem.

Build from the repository root with
`cmake -S examples/finite_algebra -B build/finite_algebra -DRFC_SOURCE_DIR="$PWD"`,
then `cmake --build build/finite_algebra` and `build/finite_algebra/finite_algebra`.
An installed package can be used by omitting `RFC_SOURCE_DIR` and supplying its
`CMAKE_PREFIX_PATH`. The main build also exposes `rfc_finite_algebra_demo` when
`RFC_BUILD_APPS=ON`.
