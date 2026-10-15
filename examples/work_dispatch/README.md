# Work dispatch

This standalone consumer dispatches an exact rational field action and a packed
Boolean program through one shared CPU budget. Each job owns its prepared source,
workspace and completion. Build the CPU version without a GPU SDK:

```sh
cmake -S rexgraphcpp/examples/work_dispatch -B build/work-dispatch -G Ninja \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/work-dispatch --parallel 1
build/work-dispatch/work_dispatch --profile
```

For Vulkan, add `-DRFC_ENABLE_VULKAN=ON` to configuration. The default selects a
physical compute device; `--allow-software` explicitly admits a software driver.
The JSON output includes admitted workers, the selected device, each job's worker
count, GPU/CPU word partitions, transfer phases and complete batch timings for serial, shared CPU, independent CPU,
GPU and mixed CPU/GPU jobs. `--profile` measures three sizes with five samples
per mode and reports the fastest measured candidate for each size. Source preparation is reported separately. Batch time includes graph
construction, thread launches, GPU workspace creation, upload, fence completion
and readback. Compare modes within the same build and driver configuration.

Add `--reuse` to compare fresh jobs, exact field result reuse, and, in Vulkan
builds, retained GPU workspaces. Each row reports its first cold batch and later
warm samples separately. Source and service preparation are separate costs.
The input and complete requested outputs are fixed within each row. Warm field
result hits skip GMP computation; GPU workspace hits still execute the full
packed program and perform upload/readback. These are distinct reuse contracts.
Cold means empty job services; the common source and prepared reference are
already available. The first field cache request adopts its sealed plan and
input without another compilation. Compilation/adoption/application/workspace counters describe the cache
services; the fresh row uses the separately prepared field and per job workspaces.

`make_cached_field_work` accepts an authentic analysis or sealed prepared operation
and a shared `NativeFieldCache`.
Its namespace/publication cutoff is admitted before submission, including warm
hits. Result provenance names the requesting publication and preserves the
producer's execution facts separately. Field readiness edges keep their original
declared inputs.

Use `--locality --profile` to compare target row and incidence fiber layouts,
uniform and incidence/working-set tiles, and admitted worker counts. Each candidate
returns the same complete field and is checked against the sample's independent
definition. Source, kernel, partition and scratch preparation are reported
separately from application time. The selection is local to that prepared source
and retained scratch; timing samples do not measure hardware cache misses.

The same mode compares CPU, full Vulkan and mixed packed execution with identical
field result cache availability. Services start empty in each row; later samples
reuse the field result and available device workspace. The reported selection
uses the sum of measured batches and service setup. Common source preparation
and field calibration are reported separately. Rational field work executes on CPU.

Independent region jobs then partition the requested lower stalks, retain all
incoming cofaces and reconstruct the complete field alongside the packed job.
Each native job records resolve, boundary action and publication stages. Its
Rational product for each request is released before publication, workers are released
on completion, and output object charges survive with the returned owner.
Shared immutable preparation and source pins, GMP limbs and other allocator
overhead are outside these object charges. No region receipt narrows global cache
or writer authority.

Vulkan factories also accept a shared `VulkanBinaryWorkspaceCache`. It owns one
device context and lends workspaces exclusively by exact prepared owner identity.
Equal input bits or sizes do not establish that identity. Limits cover total
active/idle workspace count and actual Vulkan buffer allocation bytes, in addition
to the device context's own limits. Incompatible idle owners are evicted before
fresh allocation. Successful jobs retain pipelines and buffers; failures and
cancellation discard loans after fence completion. `clear()` releases idle
residency and prevents current loans from returning to the cache. The service
retains source ownership and requires all callers to finish before destruction.

Use `rfc::field_work` and `rfc::binary_work` for CPU dispatch, or the optional
`rfc::vulkan_work` target. `WorkGraph` validates finite dependencies and admits
jobs against one cooperating `CpuResourceCoordinator`. Delegated budgets avoid
charging the same job twice when an existing executor reserves its workers.
Optional phased jobs own an initial reservation and resize it atomically at stage
boundaries. Busy upgrades fail with the previous charge intact. The graph rejects
retained-result/transient envelopes that could block its remaining flat jobs.
Worker peaks for phased graphs are admitted bounds; stage receipts describe
changing reservations. Resource stages and readiness edges are execution records.
A busy shared budget queues work. The run owns execution threads; destruction
cancels and joins them. Completion handles retain their owned results.

Dependencies establish readiness. A field job keeps its original native inputs.
For packed data flow, `PackedWorkRecipe` binds every input aspect to a predecessor
key, generation and output ordinal. Producers and consumers must share an
explicit `PackedWorkCarrier` declaration. Equal slot counts do not establish
carrier identity. Results retain predecessor operations and source publications.
Vulkan jobs accept an owning device context; dependencies across contexts use
host readback and upload. Device metadata records the selected enumeration index.

Native jobs can require a namespace and maximum publication sequence through
`WorkAvailability`; derived packed jobs validate all inherited native sources.
A later directory publication does not change a submitted job. Scheduling order
is separate from mathematical chronological order. `WorkResultSlot` accepts only
the explicitly expected completion and rejects late generations and foreign runs.
Results remain prospective until the caller makes a separate accession decision.

Worker limits govern cooperating execution threads, not OS CPU affinity.
Declared byte limits cover specified containers, not total process memory or GMP
allocation. Vulkan allocation limits remain per device context. Graph limits bound
job count, dependency count, depth, simultaneous jobs and declared result bytes.
Generic `WorkJob` callbacks are trusted host code: their declared resource demands
must cover their work, and child reservations must quiesce before they return.
