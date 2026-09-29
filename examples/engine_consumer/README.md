# Engine consumer

This standalone C++20 application calls a pinned RFC library through its own
`engine_math` adapter. The adapter links `rfc::core`; application lifecycle and
renderer conversions belong to the consuming engine.

From the library repository root:

```sh
cmake -S examples/engine_consumer -B build/engine -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DRFC_SOURCE_DIR="$PWD"
cmake --build build/engine --parallel 1
./build/engine/engine_math_smoke
```

This example has no Godot dependency. Pin a reviewed source revision and rebuild
the adapter when upgrading; the library has no frozen API or ABI.
