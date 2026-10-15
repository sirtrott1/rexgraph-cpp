# Complex root certificates

Select nonreal roots of certified algebraic fields using exact rational regions.
The examples cover degrees two, three, four, five, six and eight, both conjugate
roots, native algebraic values and explicit rectangular field comparisons.

```sh
cmake -S rexgraphcpp/examples/complex_root_certificates -B build/complex-roots \
  -DRFC_SOURCE_DIR="$PWD/rexgraphcpp"
cmake --build build/complex-roots -j1
build/complex-roots/complex_root_certificates
```

For an installed library, replace `RFC_SOURCE_DIR` with `CMAKE_PREFIX_PATH`.
The consumer prints complete JSON certificates, sixteen refinement steps and
enclosures for native field values.

`SelectedComplexAlgebraicEmbedding::isolate(field, {real, imaginary}, radius)`
requires rational inputs. Its region is the closed diamond
`|Re(z-center)| + |Im(z-center)| <= radius`, strictly off the real axis. A strict
contraction and self map certificate proves that exactly one simple root lies
inside. Failure of this sufficient test does not determine the root count of
an arbitrary region. Candidate region discovery is caller owned.

`refine(steps)` retains a replayable sequence of strictly nested enclosures.
Each rational grid adjustment records its exact error. Field coefficients and
the original root certificate remain unchanged. Refine once and reuse that
owned result through `observe(value).enclosure(refinement)` for field or tensor
coefficients. Different embedding declarations require their own refinements.

`conjugated()` explicitly selects the conjugate root. A field involution or
Hermitian metric requires its own declaration. `agrees_with(map, target)` checks
root agreement for an existing complete field comparison. Work, coordinate and
bit budgets reject exhausted requests; no floating fallback is used.
