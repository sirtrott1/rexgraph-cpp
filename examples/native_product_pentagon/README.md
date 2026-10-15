# Native product pentagon

Construct four finite strict rational factors with all declared type directions,
including unused types. The five carriers retain their actual nested pairs:

```text
(((A B) C) D) -> ((A (B C)) D) -> (A ((B C) D)) -> (A (B (C D)))
       |                                                  ^
       +--------------> ((A B) (C D)) --------------------+
```

Each arrow declares a complete type bijection. The inner ABC and BCD associators
retain the common BC constructor. Cell witnesses retain all five ordered flag
words, original identities and aspect words, and the separate Koszul bits at
each nesting level. Chain witnesses keep both routes and all five exact boundary
comparisons, including supported zeros and cancellations.

```sh
cmake -S rexgraphcpp/examples/native_product_pentagon -B build/product-pentagon \
  -DRFC_SOURCE_DIR="$PWD/rexgraphcpp"
cmake --build build/product-pentagon -j1
build/product-pentagon/native_product_pentagon
```

For an installed library, supply `CMAKE_PREFIX_PATH` instead of `RFC_SOURCE_DIR`.
The native request requires four owning empty Chain anchors, declared factor
identities and four availability cutoffs. Every action rechecks their authority
and exact resource limits. Derived fields keep their input and edge receipt;
equal outputs do not erase the two distinct routes. No endpoint is published.

This is a bounded scalar rational constructor. It requires strict source chain
families and complete signatures. It does not declare braiding, general stalk
products, metric transport or GPU execution.
