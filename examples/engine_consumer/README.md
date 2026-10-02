# Engine consumer

This standalone C++20 application calls a pinned RFC library through its own
`engine_math` adapter. The adapter links `rfc::native_execution`, declares a common
scalar coordinate, and prepares an owning rational field action with reusable
compiled scratch. Each amplitude update prepares the action and workspace before
replacing the old state. Calls require exclusive synchronous access. Returned
values are derived; they do not modify the retained relation. Application lifecycle
and renderer conversions belong to the consuming engine.

From the library repository root:

```sh
cmake -S examples/engine_consumer -B build/engine -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DRFC_SOURCE_DIR="$PWD"
cmake --build build/engine --parallel 1
./build/engine/engine_math_smoke
```

This example has no Godot dependency. Pin a reviewed source revision and rebuild
the adapter when upgrading; the library has no frozen API or ABI.
