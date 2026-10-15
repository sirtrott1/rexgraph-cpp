# Exact carries on a declared positional carrier

`rfc/local/radix_carry.hpp` implements the carry invariant of the main paper
§49 and companion §21, together with the Gaussian example in companion
Example 25.3. A declaration supplies every digit-position identity and every
adjacent carry-relation key. The result owns that native source and its exact
occurrence maps.

For a rational radix `b`, the carry boundary has columns
`b e_i - e_(i+1)`. The value map is the row `(1,b,b²,...)` and annihilates the
carry boundary exactly. `apply_carries(field,c,guard)` realizes `field+B*c` and
returns an immutable receipt containing the original field, resulting field,
carry coordinates and preserved exact value. Fields from distinct declarations
cannot be mixed merely because their coordinates are equal.

```cpp
auto radix = PreparedRadixCarry::rational(base, position_ids, carry_keys, guard);
auto input = radix.field(all_digit_coordinates, guard);
auto receipt = radix.apply_carries(input, all_carry_coordinates, guard);
auto canonical = radix.normalize_integer(input, guard);
```

The rational invariant accepts any rational base. Integer normalization has the
additional hypotheses of an integral base with absolute value at least two and
integral input digits. Its canonical digit set is `0,...,abs(b)-1`, including
for negative bases. A field that requires more positions than declared is
rejected; its source and input remain intact.

`gaussian_minus_one_plus_i(position_ids,carry_keys,guard)` uses the complete
rational coordinate basis `(1,i)` at each stalk. Its two-dimensional occurrence
map is multiplication by `-1+i`, paired with the next-position map `-I`.
Coordinates are interleaved real and imaginary components, beginning with the
least significant position. Its value map is bilinear evaluation by radix
powers. This preserves the distinction from a Hermitian harmonic vector, whose
coefficients would be conjugated.

Gaussian normalization requires Gaussian integer digits and produces digits
`0` or `1`. `main.cpp` verifies the paper's `5-3i = 101110` example in exactly
six declared positions. The tests also reject that normalization with only five
positions. Neither profile appends an undeclared carrier cell.

`owner()`, `source()`, `boundary()` and `value_map()` expose the retained native
declaration and its complete exact actions. Sparse maps are assembled directly
from the declared occurrence actions. The Gaussian representation uses a fixed
two-dimensional scalar basis; no approximate complex conversion or metric
assumption is involved. Preparation and actions use the caller's exact action
dimension, entry, scalar-work and integer-size limits.

Build from the repository root with
`cmake -S examples/radix_carry -B build/radix_carry -DRFC_SOURCE_DIR="$PWD"`,
then `cmake --build build/radix_carry` and `build/radix_carry/radix_carry`.
An installed package can be used by omitting `RFC_SOURCE_DIR` and supplying its
`CMAKE_PREFIX_PATH`. The main build also exposes `rfc_radix_carry_demo` when
`RFC_BUILD_APPS=ON`.
