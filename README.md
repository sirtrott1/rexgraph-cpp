# RexGraphCPP

C++20 implementation of Relational Field Calculus and Tensor Algebra.
Retained relational sources define the semantics of derived fields and views.

Build with CMake 3.24 or later, a C++20 compiler, pkg-config, and GMP/GMPXX.
Python 3 is required for tests. Compile with `cmake --build build --parallel 1`
after configuring with `cmake -S . -B build`. Run `ctest --test-dir build`.
