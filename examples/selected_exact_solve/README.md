# Selected exact solves

The example reads coordinates `3, 0, 3` of two exact Green solutions on the
same retained square complex. Its repeated and inactive occurrences remain in
the source. The declared unit metrics give the finite system
`A = I + BᵀB` on the complete grade-one carrier.

Build against the source tree:

```sh
cmake -S rexgraphcpp/examples/selected_exact_solve -B build/selected-solve-example \
  -DRFC_SOURCE_DIR="$PWD/rexgraphcpp"
cmake --build build/selected-solve-example -j1
build/selected-solve-example/selected_exact_solve
```

For an installed library, omit `RFC_SOURCE_DIR` and set `CMAKE_PREFIX_PATH` to
its installation prefix. The executable links `rfc::field`.

`PreparedExactSolve::prepare_selected(coordinates)` retains the exact original
operator and its prepared factors. A proper subset prepares dual certificates
`Aᵀ zᵢ = eᵢ`, then returns `zᵢᵀ b` for subsequent right-hand sides. It validates
the complete original operator and input and rechecks those certificates on
every application. Coordinate order and repeats are preserved. An empty
request still validates the owner and complete input. A request covering every
coordinate uses the full primal solve and its original-system residual check.

These are explicit coordinate observations, not new carriers, restrictions of
the source complex or transports between publications. `apply_transpose` uses
the algebraic transpose, which differs from a metric adjoint. Native global
field actions continue to produce their complete declared output fields.

The support index visits actual pivot-column members and keeps the existing
pivot rule. Preparation statistics report pivot candidates, eliminated rows,
active-column index updates, separate rational/index entry peaks, and observed coefficient
bits. These counts cover the stated containers and scalar operations, not
process memory or elapsed time. Fill-in and dual preparation can grow; a
selected readout can cost more than a full solve. Use `ExactActionLimits` and
an optional cancellation checkpoint for preparation and each application.
