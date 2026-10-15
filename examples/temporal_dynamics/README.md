# Temporal relations and field dynamics

A complete native demonstration of an evolving database and its declared field
observations. The synthetic cooling circuit incident includes a late arrival,
a correction, repeated and inactive participants, missing readings, supported
zero, and an explicitly asserted higher relation.

Run from the directory containing `rexgraphcpp/`:

```sh
python3 rexgraphcpp/examples/temporal_dynamics/run.py --verify --package --serve
```

The first run visibly configures and compiles a C++20 consumer. Dependencies are
CMake 3.24 or later, a C++20 compiler, GMP and Python 3. Linux is required for the
durable store used here. Later runs reuse the build. Every run creates a new
directory under `build/temporal-dynamics-demo/`. The command prints its location
and the local presentation URL. Stop the server with Ctrl+C.

Open the generated `index.html` directly for an offline presentation. Its
publication selector changes the historical view; the next publication and
next chapter buttons guide the walkthrough. The local server enables additional
interval queries by invoking the native C++ executable. The offline report
displays completed native observations.

## Walkthrough

1. Begin at publication 0. Startup spans minutes `[0, 10)` and has reading 1.
2. At publication 1, alarm and baseline enter. Querying overlap with `[5, 15)`
   returns three primary relations and exact reading sum 6.
3. At publication 2, an inspection of `[5, 15)` arrives late. It was unavailable
   in publications 0 and 1. Its missing reading does not add a zero scalar.
4. At publication 3, repair and the enclosing episode enter. The repair's
   declared asset transfer occurs after the alarm transfer; reversing them
   gives a different result.
5. At publication 4, baseline is corrected from 2 to supported zero. Its
   identity persists. Earlier files and query results keep the earlier value.
6. At publication 5, the higher closure relation is explicitly asserted with
   boundary `startup + alarm + repair - episode`. Its second boundary is zero.
   The grade one operator gains one curl direction.
7. Return to publication 0. Its original native query and prepared field action
   remain available against their actual historical owner.

The other chapters inspect typed group queries, boundary occurrences, exact
Hodge sectors, finite update attribution, complete channel pairings, native
stalk actions, tensor products and three map receipt continuation. The catalog
indexes all distributed standalone examples and the nineteen reviewed research
families. Catalogued examples retain their separate instructions and scope;
cataloguing is not a new execution result.

## Native files and queries

Each run creates:

| Artifact | Contents |
| --- | --- |
| `database/publication-N.rfcxi` | Exact typed database endpoint |
| `database/transition-N.rfcxiu` | Lossless interpreted update |
| `fields/transition-N.rfcxt` | Exact carrier and tensor transition |
| `fields/history.rfcxs` | Portable native field history |
| `field-store/` | Exclusive durable native history |
| `temporal-dynamics.rfnb` | Native query notebook |
| `observations.json` | Exact derived results with source witnesses |
| `manifest.json` | Artifact hashes and execution scope |

Query an existing publication with the compiled consumer:

```sh
build/temporal-dynamics-demo/cpp/temporal_dynamics query \
    PATH_TO_DEMO/database/publication-1.rfcxi 1 5 15 --exact
```

Arguments are the native input file, availability cutoff, interval start and
interval end. Endpoints accept integers and fractions. The exact overlap rule
is `start < query_end && end > query_start`. Results include original event
keys, nullable reading sum and groups by the declared category with original
membership. Supplying publication 5 with cutoff 0 rejects.

To serve an existing presentation without rebuilding:

```sh
python3 rexgraphcpp/examples/temporal_dynamics/serve.py PATH_TO_DEMO \
    --exe build/temporal-dynamics-demo/cpp/temporal_dynamics
```

The server binds loopback, serves only this presentation directory and accepts
bounded native queries against its named snapshots. It does not accept shell
commands, arbitrary file paths or another source namespace.

For the notebook application:

```sh
python3 rexgraphcpp/examples/temporal_dynamics/run.py \
    --notebook-exe build/notebook/rfcnb --verify
build/notebook/rfcnb serve PATH_TO_DEMO/temporal-dynamics.rfnb \
    PATH_TO_DEMO/notebook-state
```

The generator stages the actual native snapshots and runs the query cells. Each
cell explicitly selects its input endpoint. The notebook uses RFC's native
query language, not SQL. Opening it does not execute or compile cells.

For an existing packaged demonstration, first build `rfcnb` using the adjacent
notebook repository's README. From the unpacked `temporal-demo/` directory,
stage its native inputs into a new local notebook store:

```sh
for n in 0 1 2 3 4 5; do
    build/notebook/rfcnb stage-input temporal-dynamics.rfnb \
        build/temporal-notebook-state "publication-$n.rfcxi" "publication-$n.rfcxi"
done
build/notebook/rfcnb run temporal-dynamics.rfnb build/temporal-notebook-state
build/notebook/rfcnb serve temporal-dynamics.rfnb build/temporal-notebook-state
```

The packaged query notebook and all six inputs are included. Its generated local
cache directory is omitted; the commands above recreate its native retained
results from the portable inputs.

Add `--figures` to export each historical prefix as SVG and PDF with Matplotlib.
Those exported figures name their exact publication cutoff. Plot coordinates
are display conversions of the retained rational values.

## Mathematical declarations

Time spans are primary event relations with declared categories, subtypes and
time and asset roles. Known asset subjects are part of the initial declared
context. Time point subjects enter only with accepted event data. Event valid
time and publication availability are separate.

The field input is read back from the actual retained native payload after
ingestion. Time points are named by their canonical rational minute in this
scenario's one declared clock; complete links retain their actual database
subject keys and their separate chain identities. Overlap does not automatically
split a span or join otherwise distinct endpoints.

The typed database interpretation and signed chain realization have different
native namespaces. Complete links name the database assertion key and its
corresponding chain relation key at each publication. Matching numeric keys
alone never authorizes a cross source identity or transport.

Each ordinary span has boundary end minus start. An additional declared audit
pair cancels, an inactive end term contributes no coefficient, and an active
zero term retains support. Existence and orientation are separate packed
planes; coefficient magnitude is exact. The packed CPU program evaluates both
active negative and active positive masks. Inactive occurrences, absence,
supported zero and cancellation are not interchangeable.

The signature declares six span subtypes and one higher closure type. All grades
and types are independent of elapsed time and display position. The chain
condition is checked at every applicable adjacent interface. An overlapping
span does not infer a higher relation.

The grade one metric is the positive diagonal of duration in minutes; the
adjacent metrics are identities. Native metric adjoints define
`L1 = boundary1† boundary1 + boundary2 boundary2†`, where † denotes the
metric adjoint and coordinate transpose remains separate. The existing prepared native
solve resolves `(I + L1)x = forcing` over exact rationals. The Hodge sectors
decompose the forcing. This declared observation is not a physical model of
the cooling circuit.

For two endpoint responses, complete rectangular maps J and K identify their
actual declared carriers. The finite change is `x_new - J x_old`. The native
Green update separates source realization, coordinate innovation and operator
defect. Every channel pairing is retained, including mixed terms. Chronological
accumulation reconstructs the selected terminal response. A terminal pairing is
available at that terminal cutoff, never at an earlier endpoint.

Each chain cell has a declared two coordinate stalk. The constant fiber action
transports forcing and duration through signed occurrences. This downward
boundary realization is distinct from an upward observation sheaf. No sheaf
restriction is inferred from a metric adjoint. The native tensor product uses
separate time and asset owners and the graded Leibniz sign. Its second boundary
retains supported zeros.

The independent comparison cases are deliberately scoped:

- Reversible PageRank: conductances 1 and 2, metrics `M0 = D^-1`, `M1 = W^-1`
  give `L0 = I - T`. With damping 1/2 the exact rank and Green response are
  `(5/9, 1/3, 1/9)`. This does not assert equivalence for every directed graph.
- TextRank: retained sentence lengths 2, 4 and 8 have one common term and
  disjoint private terms. The common logarithm factor cancels during transition
  normalization. The exact ranks are
  `(3367/9432, 397/1179, 321/1048)`. General unequal logarithmic weights are not
  silently replaced by rational approximations.
- Group variation: category projections give the exact between and within
  quadrance identity for unit and duration metrics. The unit case is the
  deterministic ANOVA sum of squares decomposition. No F test, significance
  level or causal conclusion is reported.
- Phase: multiplication by z in `Q[z]/(z^2 - z + 1)` has exact period six and a
  declared involution. No numerical angle or evaluation of pi is needed for this
  carrier. It does not represent every possible periodic geometry.

All arithmetic in the C++ observations is exact GMP rational or certified
algebraic arithmetic. Browser conversions are used only for pixel geometry.
The offline presentation contains the entire completed demo; its cutoff control
is a view, not an access control mechanism. Native execution enforces source
availability separately.

## Verification and reproduction

```sh
python3 -B rexgraphcpp/examples/temporal_dynamics/verify.py \
    --exe build/temporal-dynamics-demo/cpp/temporal_dynamics
```

The independent Fraction oracle checks source data, typed joins and groups,
tensor support, boundaries, positive metrics, exact solves, Hodge projections,
finite channels, interaction pairings, accumulation, chronology, product signs
and comparison cases. It independently executes every truncated history and
perturbs all later forcing and reading values, and separately changes future
identities, participants and intervals. Earlier observations and canonical
native files must match. A deliberately broken higher boundary, wrong authority,
corrupt file and output collision must reject.

To build against an installed RFC package, use `--prefix PATH` with the generator,
or configure this standalone CMake project with `CMAKE_PREFIX_PATH`. To reuse a
compiled consumer, pass `--exe PATH`. Existing output directories reject; choose
a new `--directory` or open the previous presentation.

`--package` creates an exclusive zip containing the presentation, native portable
files, input, notebook and public library sources. When the adjacent notebook
repository is present, its public sources and existing example notebooks are
included too. Build products, local Git history, private notes, notebook caches
and the live durable store are excluded. Native field history remains in its
portable file. The presentation manifest covers portable report files; the
package source manifest covers each included repository file. The package
retains current proprietary terms. Distribution requires the author's
authorization.
