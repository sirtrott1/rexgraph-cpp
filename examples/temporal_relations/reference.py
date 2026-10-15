#!/usr/bin/env python3
"""Numerical PCA and UMAP observations of explicitly selected native data views."""
import argparse
from fractions import Fraction
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path

for name in ("NUMBA_NUM_THREADS", "OMP_NUM_THREADS", "OPENBLAS_NUM_THREADS", "MKL_NUM_THREADS"):
    os.environ[name] = "1"


def fit(record):
    import numpy as np
    from sklearn.decomposition import PCA
    from umap import UMAP
    from threadpoolctl import threadpool_limits

    raw = record["values"] if record["chapter"] == "native_vectors" else record["samples"]
    names = record.get("event_names", [str(key) for key in record.get("sample_keys", [])])
    data = np.array([[float(Fraction(value)) for value in row] for row in raw], dtype=np.float64)
    witness = {"cutoff": record["sequence"], "source_namespace": record["source_namespace"],
        "carrier": record["carrier"], "links": record.get("links", record.get("sample_keys")),
        "names": names, "columns": record.get("feature_names", record.get("channels")), "values": raw}
    digest = hashlib.sha256(json.dumps(witness, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    result = {"chapter": record["chapter"], "sequence": record["sequence"], "names": names,
              "fit_cutoff": record["sequence"], "input_sha256": digest, "input_witness": witness,
              "numeric_policy": "float64 observation of the exact named columns; no fitted scaling or future rows",
              "pca": None, "umap": []}
    with threadpool_limits(limits=1):
        if data.shape[0] >= 2:
            model = PCA(n_components=min(2, *data.shape), svd_solver="full")
            coordinates = model.fit_transform(data)
            residual = data - model.inverse_transform(coordinates)
            result["pca"] = {"components": model.components_.tolist(), "mean": model.mean_.tolist(),
                "coordinates": coordinates.tolist(), "explained_variance": model.explained_variance_.tolist(),
                "explained_variance_ratio": np.nan_to_num(model.explained_variance_ratio_).tolist(),
                "discarded_squared_error": float(np.sum(residual * residual)), "solver": "full SVD",
                "sample_covariance_denominator": data.shape[0] - 1}
        if record["chapter"] == "native_vectors" and data.shape[0] >= 4:
            for seed in (17, 41):
                print(f"Fitting UMAP at publication {record['sequence']}, seed {seed}.", flush=True)
                model = UMAP(n_components=2, n_neighbors=min(3, data.shape[0] - 1), min_dist=0.1,
                    metric="euclidean", init="random", n_epochs=500, random_state=seed,
                    transform_seed=seed, n_jobs=1, low_memory=True)
                coordinates = model.fit_transform(data)
                if not np.isfinite(coordinates).all():
                    raise ValueError("UMAP returned nonfinite display coordinates")
                result["umap"].append({"seed": seed, "neighbors": model.n_neighbors, "min_dist": model.min_dist,
                    "initialization": "random", "epochs": 500, "metric": "euclidean",
                    "coordinates": coordinates.tolist(), "scope": "small synthetic fitted display; no cluster or density claim"})
    return result


def evaluate(records):
    selected = [r for r in records if r["chapter"] in ("native_vectors", "principal")]
    return {"format": "rfc-temporal-reference-v1", "arithmetic": "numerical observation, separate from exact native data",
        "versions": {name: importlib.metadata.version(name) for name in ("numpy", "scipy", "scikit-learn", "umap-learn", "numba")},
        "models": [fit(row) for row in selected]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("observations", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(f"Reference output already exists: {args.output}")
    print("Preparing the optional numerical references. The first UMAP fit compiles its numerical kernels.", flush=True)
    result = evaluate(json.loads(args.observations.read_text()))
    args.output.write_text(json.dumps(result, indent=2, allow_nan=False)+"\n")
    print("Numerical references complete.", flush=True)


if __name__ == "__main__":
    main()
