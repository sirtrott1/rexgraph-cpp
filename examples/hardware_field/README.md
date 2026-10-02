# Hardware-aware exact field consumer

Build and run from the repository root:

```sh
cmake -S examples/hardware_field -B build/hardware-field -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/hardware-field --parallel 1
./build/hardware-field/hardware_field
./build/hardware-field/hardware_field --inventory-only
```

The consumer prints a JSON inventory of Linux CPU affinity, package/core identities,
shared caches, kernel instruction flags, RAM, visible cgroup limits and PCI devices.
Unavailable observations stay unknown. PCI display devices and visible driver nodes
do not establish usable GPU compute devices. GPU memory, shared-memory coherence and
stacked 3D cache are unknown in this profile. Other platforms report the missing
inventory adapter. Probing does not allocate test buffers or initialize GPU runtimes.

The field example retains 1,024 exact scalar transfers, independently checks its result,
and measures one, two and four workers where admitted. It chooses the lowest median
among those warm calls. `rfc::resource_native_field` supplies the owning calibration
and scheduled result; `rfc::hardware_inventory` supplies observations and budgets.
Worker candidates and the cooperating-service budget are explicit. The calling
thread counts as one worker; physical-core policy limits the count and does not pin
threads. Calls through the same coordinator share its budget. Independent services
and other processes remain outside that budget.

Calibration belongs to the immutable prepared operation and CPU observations.
Changes require new calibration. Refresh the inventory before scheduled execution.
Current host, cancellation, worker and tile limits apply on every call. Required GPU
execution rejects; explicitly permitted exact CPU fallback is recorded in the field
provenance. The calibration measures reservation, admission, compute, joins and
owning result allocation. Inventory, preparation, result destruction and independent
comparison are excluded. It establishes no global optimum or cross-workload speedup.

RAM availability and cgroup headroom are advisory. Declared buffer reservations count
only those requested bytes; they neither allocate memory nor bound GMP/process heap.
The rational field adapter reserves workers and uses the existing exact container/
bit limits. It makes no whole-heap reservation. No HIP/CUDA/Vulkan kernel, device
residency, GPU/CPU overlap or multi-GPU dispatch is qualified by this example.
