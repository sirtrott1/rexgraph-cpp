#!/usr/bin/env python3
"""Build, verify and package a self-contained temporal relations walkthrough."""
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

HERE=Path(__file__).resolve().parent
LIBRARY=HERE.parents[1]


def command(args, log):
    print("Running:", shlex.join(map(str,args)),flush=True)
    with log.open("w") as output:
        process=subprocess.Popen(list(map(str,args)),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
            text=True,env={**os.environ,"PYTHONDONTWRITEBYTECODE":"1"})
        for line in process.stdout:
            print(line,end="",flush=True);output.write(line)
        if process.wait(): raise RuntimeError(f"Command failed. See {log}")


def build(directory, prefix=None):
    directory.mkdir(parents=True,exist_ok=True)
    print("Preparing the C++ consumer. The first run compiles it; later runs reuse unchanged objects.",flush=True)
    options=[f"-DCMAKE_PREFIX_PATH={prefix}"] if prefix else [f"-DRFC_SOURCE_DIR={LIBRARY}"]
    command(["cmake","-S",HERE,"-B",directory,"-DCMAKE_BUILD_TYPE=Release",*options],directory/"configure.log")
    command(["cmake","--build",directory,"--parallel","1"],directory/"build.log")
    return directory/"temporal_relations"


def portable_files(directory):
    return [p for p in sorted(directory.rglob("*")) if p.is_file() and not p.is_symlink()
        and not {"field-store","__pycache__"}.intersection(p.relative_to(directory).parts)
        and p.suffix != ".log"]


def render(directory):
    records=json.loads((directory/"observations.json").read_text())
    references=json.loads((directory/"references.json").read_text()) if (directory/"references.json").exists() else None
    payload={"format":"rfc-temporal-relations-v1","records":records,"references":references,
        "last_sequence":max(r["sequence"] for r in records if r["chapter"]=="database"),
        "files":[p.relative_to(directory).as_posix() for p in portable_files(directory)]}
    data=json.dumps(payload,ensure_ascii=True,allow_nan=False).replace("<","\\u003c")
    (directory/"demo-data.js").write_text("window.RFC_TEMPORAL_RELATIONS="+data+";\n")
    for name in ("index.html","demo.css","demo.js","README.md","scenario.json"):
        shutil.copyfile(HERE/name,directory/name)


def write_manifest(directory, exe, events, verification):
    files={p.relative_to(directory).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()
        for p in portable_files(directory) if p.name != "manifest.json"}
    manifest={"format":"rfc-temporal-relations-v1","files":files,
        "events_sha256":hashlib.sha256(events.read_bytes()).hexdigest(),
        "native_consumer_sha256":hashlib.sha256(exe.read_bytes()).hexdigest(),"verification":verification,
        "native_arithmetic":"GMP rationals; exact images, kernels, Gram solves and temporal maps",
        "references":"Optional numerical PCA/UMAP observations fitted separately at each cutoff",
        "presentation":"Scientific display coordinates are derived observations, not native source identities",
        "terms":"RexGraphCPP: Apache-2.0; Copyright (c) 2026. Third party terms remain separate."}
    (directory/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")


def source_files():
    sys.path.insert(0,str(HERE.parent/"temporal_dynamics"))
    from catalog import inventory
    return inventory(LIBRARY)


def package(directory, target=None):
    target=target or directory.parent/(directory.name+".zip")
    if target.exists(): raise FileExistsError(f"Archive already exists: {target}")
    source={p:hashlib.sha256((LIBRARY/p).read_bytes()).hexdigest() for p in source_files()}
    start=("# Relational complexes in temporal biomedical research\n\nThe completed analysis is available in presentation/index.html. "
        "The figures and saved results work offline. presentation/walkthrough.pdf is the printable version when included.\n\n"
        "To build and verify from this directory, run:\n\n```sh\n"
        "python3 -B rexgraphcpp/examples/temporal_relations/run.py --directory build/walkthrough --verify\n```\n\n"
        "For native interval queries on the completed presentation:\n\n```sh\n"
        "python3 -B rexgraphcpp/examples/temporal_relations/serve.py presentation --exe build/temporal-relations-demo/cpp/temporal_relations\n```\n\n"
        "See presentation/README.md for optional numerical dependencies, analysis declarations and reproduction commands. "
        "Native data and public source are included; compiled binaries and live caches are excluded. "
        "RexGraphCPP is licensed under Apache-2.0. Copyright (c) 2026. Third party terms remain separate.\n")
    with zipfile.ZipFile(target,"x",compression=zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("temporal-relations/README.md",start)
        for p in portable_files(directory):
            archive.write(p,"temporal-relations/presentation/"+p.relative_to(directory).as_posix())
        for p in source:
            archive.write(LIBRARY/p,"temporal-relations/rexgraphcpp/"+p)
        archive.writestr("temporal-relations/source-manifest.json",json.dumps(source,indent=2)+"\n")
    print("Shareable source and presentation package:",target,flush=True)
    return target


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build",type=Path,default=LIBRARY.parent/"build/temporal-relations-demo/cpp")
    parser.add_argument("--exe",type=Path)
    parser.add_argument("--prefix",type=Path,help="Build against an installed RFC package")
    parser.add_argument("--events",type=Path,default=HERE.parent/"temporal_dynamics/events.tsv")
    parser.add_argument("--directory",type=Path,help="New output directory; existing work is preserved")
    parser.add_argument("--verify",action="store_true")
    parser.add_argument("--figures",action="store_true",help="Export SVG/PDF scientific figures; requires Matplotlib")
    parser.add_argument("--reference-python",type=Path,help="Interpreter containing the optional PCA/UMAP dependencies")
    parser.add_argument("--package",action="store_true")
    parser.add_argument("--serve",action="store_true")
    args=parser.parse_args()
    stamp=datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    directory=(args.directory or LIBRARY.parent/"build/temporal-relations-demo"/f"walkthrough-{stamp}").resolve()
    if directory.exists(): raise FileExistsError(f"{directory} already exists. Open its index.html or choose a new --directory.")
    exe=args.exe.resolve() if args.exe else build(args.build.resolve(),args.prefix)
    events=args.events.resolve()
    directory.parent.mkdir(parents=True,exist_ok=True)
    print("Publishing exact native observations:",directory,flush=True)
    result=subprocess.run([str(exe),"run",str(events),str(directory)],capture_output=True,text=True)
    print(result.stderr,end="",flush=True)
    if not directory.exists(): raise RuntimeError(result.stderr.strip() or "Native publication failed")
    (directory/"native-observations.jsonl").write_text(result.stdout)
    if result.returncode: raise RuntimeError("Native publication failed; partial output is retained")
    records=[json.loads(line) for line in result.stdout.splitlines()]
    (directory/"observations.json").write_text(json.dumps(records,indent=2)+"\n")
    shutil.copyfile(events,directory/"events.tsv")
    shutil.copyfile(HERE/"scenario.json",directory/"scenario.json")
    verification=None
    if args.verify:
        print("Verifying rational identities, truncated histories and changed future suffixes.",flush=True)
        command([sys.executable,HERE/"verify.py","--exe",exe,"--events",events,"--report",directory/"verification.json"],directory/"verification.log")
        verification=json.loads((directory/"verification.json").read_text())
    if args.reference_python:
        print("Preparing optional numerical comparisons. Initial UMAP kernel compilation can take time.",flush=True)
        # Preserve an invoked virtual-environment path; resolving this symlink would bypass its environment.
        interpreter=Path(os.path.abspath(args.reference_python))
        command([interpreter,"-B",HERE/"reference.py",directory/"observations.json",directory/"references.json"],directory/"reference.log")
    if args.figures:
        print("Exporting scientific figures from the selected native and numerical observations.",flush=True)
        from export_figures import export
        export(directory)
    render(directory)
    write_manifest(directory,exe,events,verification)
    print("Open:",directory/"index.html",flush=True)
    if args.package: package(directory)
    if args.serve:
        from serve import serve
        serve(directory,exe)


if __name__=="__main__":
    try: main()
    except (OSError,RuntimeError,ValueError,ImportError) as error:
        print(f"Temporal relations: {error}",file=sys.stderr);sys.exit(1)
