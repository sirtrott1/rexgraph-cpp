# Relational complexes in temporal biomedical research

This reproducible example presents a constructed ex vivo perfusion study with
six publications, delayed assay reporting, a revised control signal and an
explicit episode consistency relation. Signals are dimensionless mathematical
values; this fixture contains no patient data or measured biological results.
The declared biomedical vocabulary is in `scenario.json`; original source
identifiers, native payloads and exact calculations are retained unchanged.

The presentation starts with the actual graded relational complex. Typed
interval relations share time identities; the higher relation has a signed
boundary whose boundary is exactly zero. Semantic roles, tensor support,
complete field decompositions and historical availability remain distinct.
Completed results work offline; a local server supports native interval queries.

## Execution

From the directory containing `rexgraphcpp/`, on Linux with a C++20 compiler,
CMake, GMP, pkg-config and Python:

```sh
python3 -B rexgraphcpp/examples/temporal_relations/run.py --directory build/walkthrough --verify
```

The first run announces C++ compilation. Subsequent builds reuse unchanged
objects. Output directories are exclusive: open an existing `index.html` or
choose a new `--directory` to retain previous work. `--exe PATH` uses an existing
consumer; `--prefix PATH` builds against an installed RFC package.

```sh
python3 -B rexgraphcpp/examples/temporal_relations/serve.py build/walkthrough --exe build/temporal-relations-demo/cpp/temporal_relations
```

Open the local address printed by the server. `Ctrl+C` stops it. The query page
accepts integer or rational interval endpoints and shows running, completion and
error states. Offline it shows the saved query for minutes 5 to 15 and clearly
disables new native queries.

```sh
build/temporal-relations-demo/cpp/temporal_relations query build/walkthrough/database/publication-2.rfcxi 2 5/2 31/2 --exact
```

## Optional numerical comparisons and figures

PCA and UMAP are independent reference observations, not native dependencies.
Install them into a local environment:

```sh
python3 -m venv build/reference-env
build/reference-env/bin/python -m pip install -r rexgraphcpp/examples/temporal_relations/requirements-reference.txt
python3 -B rexgraphcpp/examples/temporal_relations/run.py --directory build/with-references --verify --reference-python build/reference-env/bin/python
```

Every model fits only the numerical view of its selected publication. Fixed
named columns and time units are declared before fitting. No scaler, model or
parameter is fitted on future publications. UMAP uses two recorded seeds,
random initialization, three neighbours, minimum distance 0.1 and 500 epochs.
The first fit announces numerical kernel compilation. UMAP starts at publication
2, where four events are available. This is a small example, not a performance,
density or cluster quality benchmark.

Add `--figures` for standalone SVG/PDF figures; this option requires Matplotlib.
Add `--package` to create a new zip containing the presentation, native files and
public source. Archives exclude Git history, private notes, binaries and live
stores. Existing archives are never replaced.

## Native analyses

Events retain identity, ordered roles, repeated and inactive occurrences,
optional values and exact scalar payloads. Availability and event time are
separate. A historical query uses its own native publication and cutoff.

Ordinary spans realize a carrier at grade one with signed boundary end minus start.
Endpoints have grade zero; the final episode closure has grade two. The complete
boundary family satisfies the chain condition. The positive span metric is
diagonal duration; the adjacent metrics are identity. Stored fields, complete
maps and native preparation use the existing owning library APIs.

Each named column in the numerical view is a derived field on admitted span
identities. Missing readings use a value column plus a presence column in this
view; the original optional payload remains unchanged. Time uses fixed units of
ten minutes. Active equipment counts retain multiplicity. The view is not a
replacement for the full typed database or its role records.

Exact frame extraction retains `X = F R`, with independent source columns and
the complete coefficient map. Reintroducing the stored means reconstructs the
whole view. Hodge components reconstruct the forcing in full. Exact Gram solves,
image extraction and kernel elimination require no eigensolver.

Field groups use centering weighted by duration and positive metric pairings.
Pairs connect when squared cosine is at least `3/4`. Connected components are
computed as constant harmonic fields of the declared pair graph, using the
library's exact Hodge frames. The threshold is a declared analysis choice.
Transitive group membership does not assert that every pair passes it. Constant
fields have no changing direction and remain explicitly present.

Shared time is the pullback `K = Aᵀ D A`: `A` observes activity on
intervals bounded by currently admitted endpoints, and `D` measures their
lengths. Its entries equal exact overlap lengths. The map's kernel is retained;
equal time observations do not identify events or certify a positive source
metric. No future endpoint is used to partition an earlier view.

Historical forcing samples use declared complete forward maps into the selected
span carrier. Centered samples give covariance `Σ`, exact variance `tr(MΣ)` and
second contraction `tr(MΣMΣ)`. Their ratio gives effective dimension of order two
without eigenvectors. Complete fields remain retained. Zero variance has no
effective dimension. These are sample summaries, separate from causal claims.

## PCA and UMAP comparison

[PCA](https://scikit-learn.org/stable/modules/generated/sklearn.decomposition.PCA.html)
finds directions ordered by variance; this reference uses full SVD and displays two
components. PCA can keep all components. A complete structural frame is not
automatically a PCA frame that maximizes the selected variance.

The separate native history with three channels has deliberately diagonal covariance.
Its known axes satisfy the equations for principal directions by exact multiplication,
including changing variance order and a correction to supported zero. This
certificate does not supply arbitrary covariance eigenvectors. Native tables use
population covariance; `scikit-learn` reports sample covariance with denominator
`N - 1`. Axis signs and ties among zero variance directions do not establish different identities.

[UMAP](https://arxiv.org/abs/1802.03426) fits a neighbourhood embedding. Its
[randomness and reproducibility](https://umap-learn.readthedocs.io/en/latest/reproducibility.html)
are recorded explicitly. The plots are fitted display observations, not
[cluster or density certificates](https://umap-learn.readthedocs.io/en/latest/clustering.html).

Native field groups use the duration metric. The reference embeddings use the
Euclidean feature metric on the same declared numerical view. Their objectives
and metrics are distinct. Native component extraction, grouping, queries and
temporal pairing do not need an external learned embedding or dimension
reduction. Displaying results in two dimensions is a separate choice.

## Verification and artifacts

`verify.py` independently reconstructs the rational results with `Fraction`,
compares six genuinely truncated histories, and changes nonempty future value
and structure suffixes. Historical database bytes and derived group complexes
are compared exactly. These checks qualify this bounded example, not every
library platform, application or statistical objective.

`observations.json` retains exact native observations. `references.json`, when
present, retains numerical results, versions, parameters and selected input
hashes. `database/` contains native publications and updates; `fields/` contains
the portable field history. `analysis/` contains derived group complexes and the
native reference history for principal directions. `manifest.json` lists portable
artifact hashes. Scientific figure coordinates are display observations only.

The software is proprietary and available through its author's personal
distribution. This package grants no Apache or other open source license.
Third party terms remain separate.
