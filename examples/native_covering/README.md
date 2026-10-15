# Native coverings

This example lifts a loop of the three cycle through its two sheeted six cycle
cover. One circuit exchanges the sheets. It also checks a complete deck map,
transfers a chain and certifies an explicitly declared covering pullback.

```sh
cmake -S examples/native_covering -B build/native_covering -G Ninja \
  -DRFC_SOURCE_DIR="$PWD"
cmake --build build/native_covering --parallel 1
build/native_covering/native_covering
```

For an installed library, replace `RFC_SOURCE_DIR` with `CMAKE_PREFIX_PATH`.

`PreparedNativeCovering` takes actual empty Chain anchors, a constructor identity
and revision, and a complete source map of cells and occurrences. It preserves
the canonical strict morphism and checks local bijections on every retained flag,
including inactive and zero occurrences. The complete strict chain families and
declared empty grades are checked. Equal keys never identify different owners.

`pushforward` adds cover Chain contributions at their declared base images.
`transfer` lifts a base Chain to every point in each fiber. Both commute with
the retained boundary action. Scalar `section` has an explicit field identity;
`pullback` duplicates its values, while `descend` requires equal values and
complete support in every selected fiber. Chain transfer and section pullback
remain distinct operations. Supported zeros and cancellation remain inspectable.

`walk` uses original occurrence identities with an explicit upward or downward
direction, at any declared grade. It can traverse branching or inactive retained
structure without creating pairwise relations. `pairwise_walk` is an explicit
adapter to the existing oriented pairwise profile. Walks keep their complete
ordered words. `lift` returns both the original walk and its unique lift.
`monodromy` retains every lift and the resulting permutation of the entire fiber.
This is an action of incidence walks; descent through relational homotopy needs
additional hypotheses. `prefix` owns only the selected word and exposes no suffix.
`deck` checks full cell and occurrence bijections, their inverses and projection.

`PreparedNativeCoveringPullback` certifies a supplied source and its full square:
every cell and occurrence must represent exactly one compatible pair, and all
pairs must be present. `mediate` retains a complete agreeing cone and its unique
source map into that pullback. The operation creates no new source identities or
native publication. All four square cutoffs and the cone's separate cutoff are
checked independently.

Every action rechecks full sources and maps under current exact rational CPU,
allocation, dimension, integer and work limits, including empty requests.
Results own their declarations, original inputs and endpoint witnesses.
Counts cover named containers and declared visits; total allocator/GMP memory,
RSS and performance require separate accounting. Tensor stalk coverings, general
cover materialization, homotopy quotients, GPU and durable services are separate.
