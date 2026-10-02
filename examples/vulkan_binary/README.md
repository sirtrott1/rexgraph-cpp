# Exact packed Vulkan consumer

This example evaluates the declared existence/orientation query with exact bits.
It measures serial CPU, parallel CPU, Vulkan and mixed partitions for the same
frozen input, checks each against the CPU result, and selects the lowest measured
warm median among candidates that fit the cooperating CPU budget.

```sh
cmake -S examples/vulkan_binary -B build/vulkan-example -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/vulkan-example --parallel 1
./build/vulkan-example/vulkan_binary
```

Dependencies: GMP, Vulkan 1.1 headers/loader and `glslangValidator`. The optional
compiled adapter is `rfc::vulkan_binary`; enable `RFC_ENABLE_VULKAN` in a parent
build. The mathematical targets do not link Vulkan. Generated SPIR-V stays in the
build directory.

The output reports API-visible devices, UUIDs, unique heap capacities, memory type
access properties, advisory budgets and complete warm candidate costs. On an
integrated GPU, several memory types can address one heap; count that heap once.
A host-visible heap still requires the adapter's synchronization and coherence
operations. Reported capacity or budget does not promise a successful allocation.

A physical GPU is required by default. `--allow-software` explicitly admits a CPU
Vulkan driver for API/shader checks; its measurements do not describe GPU speed.
Preparation, device/workspace construction, inventory, reference comparison and
output destruction are excluded from the warm candidate samples. Each sampled
call includes result allocation, input copying/flushing, command recording,
submission, CPU work/join, fence wait, readback and output reconstruction.

The backend implements all nine ordered binary opcodes over complete source planes
and preserves duplicate outputs, register aliasing, unwritten zero registers and
logical tails. A native adapter additionally pins the authenticated source,
declarations and program key. It does not lower rational fields or temporal words.
One mixed call submits a prefix of complete words to Vulkan and processes the
remaining words on caller-inclusive CPU workers. Workers and GPU work finish before
success or an error releases resources. Cancellation waits for a submitted kernel.
Workspaces reject overlapping use. Allocation limits cover actual Vulkan buffer
allocations; CPU byte reservations cover declared result words, not process heap.

Run `tools/qualify_vulkan.sh` from the repository to build and test a physical GPU.
The script does not enable software-device admission. Driver device nodes must be
visible to the calling process. Current qualification covers one selected GPU;
peer transfer and multiple-device execution are not implemented.
