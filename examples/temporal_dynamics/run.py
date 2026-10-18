#!/usr/bin/env python3
"""Build and run the temporal product demonstration with visible progress."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import zipfile

HERE = Path(__file__).resolve().parent
LIBRARY = HERE.parents[1]


def command(args, log):
    print("Running:", shlex.join(map(str, args)), flush=True)
    with log.open("w") as output:
        proc = subprocess.Popen(list(map(str, args)), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"})
        for line in proc.stdout:
            if not line.lstrip().startswith("{"):
                print(line, end="", flush=True)
            output.write(line)
        if proc.wait():
            raise RuntimeError(f"Command failed. See {log}")


def build(build_dir, installed=None):
    build_dir.mkdir(parents=True, exist_ok=True)
    print("Preparing C++ demonstration. A first run compiles the native consumer.", flush=True)
    configure = ["cmake", "-S", HERE, "-B", build_dir, "-DCMAKE_BUILD_TYPE=Release"]
    if installed:
        configure += [f"-DCMAKE_PREFIX_PATH={installed}"]
    else:
        configure += [f"-DRFC_SOURCE_DIR={LIBRARY}"]
    command(configure, build_dir/"configure.log")
    command(["cmake", "--build", build_dir, "--parallel", "1"], build_dir/"build.log")
    return build_dir/"temporal_dynamics"


def notebook(records, out):
    cells = ["%% cell 1 md\n# Temporal relations and field dynamics\n\n"
             "This notebook uses the generated native publications. Each query cell names its exact input endpoint. "
             "A later inspection or correction cannot enter an earlier file.\n\n"
             "Open index.html for the illustrated walkthrough. The query language is the native RFC notebook query language.\n"]
    n = 2
    for row in (r for r in records if r["chapter"] == "database"):
        seq = row["sequence"]
        cells.append(f"%% cell {n} md\n## Publication {seq}\n\n"
                     f"Available at minute {row['available_minute']}. Native input: publication-{seq}.rfcxi.\n"
                     "The overlap interval is [5, 15). Missing readings contribute no scalar; supported zero remains a value.\n")
        n += 1
        cells.append(f"%% cell {n} query in=publication-{seq}.rfcxi out=query-{seq}.json\n"
                     "select 10\nwhere start < rational:15\nwhere end > rational:5\nsum reading\nfigure reading\nroles 13 retained\npage 0 1000\n")
        n += 1
    cells.append(f"%% cell {n} md\n## Field realization\n\n"
                 "Every ordinary time span has signed boundary end minus start. The last publication explicitly asserts "
                 "startup plus alarm plus repair minus episode as a higher boundary. Its second boundary is zero.\n\n"
                 "The positive grade one metric is diagonal duration in minutes. The native Hodge action resolves "
                 "(I + L)x = f exactly. The presentation shows the complete finite update channels and their cross pairings.\n\n"
                 "These are declared field responses, not a physical cooling simulation or a statistical significance test.\n")
    path = out/"temporal-dynamics.rfnb"
    path.write_text("\n".join(cells))
    return path


def portable_files(out):
    return [file for file in sorted(out.rglob("*"))
            if file.is_file() and not file.is_symlink()
            and not {"field-store", "notebook-state", "__pycache__"}.intersection(file.relative_to(out).parts)]


def write_manifest(out, exe, events, records, verification):
    files = {file.relative_to(out).as_posix(): hashlib.sha256(file.read_bytes()).hexdigest()
             for file in portable_files(out) if file.name != "manifest.json"}
    manifest = {"format": "rfc-temporal-demo-v1", "events_sha256": hashlib.sha256(events.read_bytes()).hexdigest(),
                "consumer_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
                "files": files, "native_checks": records[-1]["checks"], "verification": verification,
                "terms": "RexGraphCPP: Apache-2.0; Copyright (c) 2026 Andrew Trotter. Other components retain their own licenses.",
                "display": "Recorded native observations; pixel geometry is illustrative.",
                "file_scope": "Portable presentation, inputs, native histories and completed observations. Live stores and notebook caches are excluded."}
    (out/"manifest.json").write_text(json.dumps(manifest, indent=2)+"\n")


def package(out, library):
    path = out.parent/(out.name+".zip")
    if path.exists():
        raise FileExistsError(f"Package already exists: {path}")
    from catalog import inventory
    repositories = [("rexgraphcpp", library)]
    notebook_repo = library.parent/"rfc-notebook"
    if (notebook_repo/"CMakeLists.txt").is_file():
        repositories.append(("rfc-notebook", notebook_repo))
    sources = {name: {file: hashlib.sha256((repo/file).read_bytes()).hexdigest()
                      for file in inventory(repo)} for name, repo in repositories}
    with zipfile.ZipFile(path, "x", compression=zipfile.ZIP_DEFLATED) as archive:
        for file in portable_files(out):
            archive.write(file, "temporal-demo/"+file.relative_to(out).as_posix())
        for name, repo in repositories:
            for file in sources[name]:
                archive.write(repo/file, "temporal-demo/"+name+"/"+file)
        archive.writestr("temporal-demo/source-manifest.json", json.dumps(sources, indent=2)+"\n")
    print("Portable source and presentation package:", path, flush=True)
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=LIBRARY.parent/"build/temporal-dynamics-demo/cpp")
    parser.add_argument("--exe", type=Path, help="Use an already built native consumer")
    parser.add_argument("--prefix", type=Path, help="Use an installed RFC package")
    parser.add_argument("--events", type=Path, default=HERE/"events.tsv")
    parser.add_argument("--directory", type=Path, help="New output directory; existing work is preserved")
    parser.add_argument("--verify", action="store_true", help="Replay every prefix and perturb every future suffix")
    parser.add_argument("--package", action="store_true", help="Write an exclusive zip with native files and public C++ sources")
    parser.add_argument("--notebook-exe", type=Path, help="Stage and run the generated query notebook using rfcnb")
    parser.add_argument("--figures", action="store_true", help="Export SVG and PDF figures using Matplotlib")
    parser.add_argument("--serve", action="store_true", help="Open a local presentation server with real native queries")
    args = parser.parse_args()
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    out = (args.directory or LIBRARY.parent/"build/temporal-dynamics-demo"/f"presentation-{stamp}").resolve()
    if out.exists():
        raise FileExistsError(f"{out} already exists. Open its index.html or choose a new --directory.")
    exe = args.exe.resolve() if args.exe else build(args.build.resolve(), args.prefix)
    if not exe.is_file():
        raise FileNotFoundError(f"Native consumer does not exist: {exe}")
    out.parent.mkdir(parents=True, exist_ok=True)
    print("Publishing temporal demonstration:", out, flush=True)
    result = subprocess.run([str(exe), "run", str(args.events.resolve()), str(out)], stdout=subprocess.PIPE, text=True)
    (out/"native-observations.jsonl").write_text(result.stdout)
    if result.returncode:
        raise RuntimeError("Native demonstration failed; the partial output is retained for inspection.")
    records = [json.loads(line) for line in result.stdout.splitlines()]
    shutil.copyfile(args.events, out/"events.tsv")
    verification = None
    if args.verify:
        print("Verifying exact outputs, historical prefixes and altered future values and structures.", flush=True)
        command([sys.executable, HERE/"verify.py", "--exe", exe, "--events", args.events,
                 "--report", out/"verification.json"], out/"verification.log")
        verification = json.loads((out/"verification.json").read_text())
    from catalog import catalog
    from presentation import render
    sources = catalog(LIBRARY)
    (out/"catalog.json").write_text(json.dumps(sources, indent=2)+"\n")
    (out/"observations.json").write_text(json.dumps(records, indent=2)+"\n")
    book = notebook(records, out)
    for r in records:
        if r["chapter"] == "database":
            shutil.copyfile(out/r["snapshot"], out/f"publication-{r['sequence']}.rfcxi")
    if args.notebook_exe:
        nb = args.notebook_exe.resolve()
        for r in (x for x in records if x["chapter"] == "database"):
            artifact=f"publication-{r['sequence']}.rfcxi"
            command([nb, "stage-input", book, out/"notebook-state", artifact, out/artifact], out/f"notebook-stage-{r['sequence']}.log")
        command([nb, "run", book, out/"notebook-state"], out/"notebook-run.log")
    render(records, sources, verification, out)
    if args.figures:
        print("Exporting figures with explicit publication cutoffs.", flush=True)
        from export_figures import export
        export(out)
    write_manifest(out, exe, args.events, records, verification)
    print("Presentation:", out/"index.html", flush=True)
    print("Native query:", shlex.join([str(exe), "query", str(out/"database/publication-0.rfcxi"), "0", "5", "15", "--exact"]), flush=True)
    if args.package:
        package(out, LIBRARY)
    if args.serve:
        from serve import serve
        serve(out, exe)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, ImportError) as error:
        print(f"Temporal demo: {error}", file=sys.stderr)
        sys.exit(1)
