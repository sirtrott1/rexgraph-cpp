# Native queries and tensor windows

This standalone consumer prepares exact predicates over declared payload factors,
returns retained assertion keys or an aggregate, and summarizes a sparse tensor
window. It also submits independent query/tensor jobs and a dependent read through
the owning work graph. Results retain their source publication and remain unpublished.

```sh
cmake -S examples/query_tensor -B build/query-tensor -DCMAKE_BUILD_TYPE=Release
cmake --build build/query-tensor --parallel 1
build/query-tensor/query_tensor
```

The JSON lines compare identical outputs through existing payload accessors and
prepared bindings. Source and plan preparation, first use, warm medians and seven-use
totals are separate. Four workers are a cooperating admission cap, not a core count.
No timing threshold is required.

Predicate conjunction distinguishes false, true and unknown. Optional nulls do not
contribute to sums; an empty numeric population has no sum. Tensor windows preserve
axis order, structural absence and supported zero, with owning coordinate pins and
a reversible map to the source's row-major positions. Sparse summaries do not
materialize dense values or replace the retained tensor.

`rfc::native_query_execution` is an opt-in exact CPU adapter. Preparation and each
execution require explicit namespace/publication availability, current limits and
cancellation admission. This profile has no automatic query cache, SQL parser,
rational GPU kernel or durable publication. Graph result/container accounting
excludes shared immutable source, GMP limbs and process memory overhead.
