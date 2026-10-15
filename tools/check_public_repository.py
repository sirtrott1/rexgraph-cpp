#!/usr/bin/env python3
"""Check tracked public files and, optionally, reachable Git history."""
from pathlib import Path, PurePosixPath
import argparse
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PRIVATE = {"internal", "private", "handoff", "paper", "papers", "research",
           "experiments", "notes", "reports", "archives", "context", "evidence",
           "verification", "backups", "notebooks"}
GENERATED = {".o", ".obj", ".a", ".lib", ".so", ".dylib", ".dll", ".exe",
             ".pyc", ".log", ".html", ".zip", ".patch", ".pdf", ".ipynb",
             ".pch", ".gch", ".pdb", ".ilk", ".tex", ".bib", ".aux", ".bbl",
             ".blg", ".fls", ".fdb_latexmk", ".tgz", ".7z"}
SOURCE_HTML = {"examples/temporal_dynamics/index.html",
               "examples/temporal_relations/index.html"}


def paths(*args):
    data = subprocess.check_output(["git", "-C", str(ROOT), *args])
    return [p for p in data.decode().split("\0") if p]


def reason(rel):
    path = PurePosixPath(rel)
    if path.suffix.lower() == ".md" and path.name != "README.md":
        return "Markdown must be a canonical README.md"
    if path.parts[0] in PRIVATE:
        return "private workspace material"
    if any(p.startswith("build") or p in {"CMakeFiles", "__pycache__", ".venv", "Testing", "dist"} for p in path.parts[:-1]):
        return "generated build or tool directory"
    if ((path.suffix in GENERATED and rel not in SOURCE_HTML) or ".so." in path.name
            or path.name.endswith((".tar.gz", ".tar.xz", ".executed.ipynb", ".synctex.gz"))):
        return "generated output or intake archive"
    if path.suffix.startswith(".rfcx") and not rel.startswith(("data/fixtures/", "tests/fixtures/", "tests/storage/fixtures/")):
        return "native output outside the frozen fixture directories"
    if path.name in {"CMakeCache.txt", "compile_commands.json", "install_manifest.txt",
                     "AGENTS.md", "CHECKSUMS.sha256", "FILE_MANIFEST.txt"}:
        return "local build or intake manifest"
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--history", action="store_true", help="also check every locally reachable commit")
    parser.add_argument("--working-tree", action="store_true",
                        help="also check untracked files admitted by Git ignores")
    args = parser.parse_args()
    inventories = [("index", paths("ls-files", "-z"))]
    if args.working_tree:
        inventories.append(("working tree", paths("ls-files", "--others", "--exclude-standard", "-z")))
    if args.history:
        commits = subprocess.check_output(["git", "-C", str(ROOT), "rev-list", "--all"]).decode().splitlines()
        inventories.extend((commit, paths("ls-tree", "-rz", "--name-only", commit)) for commit in commits)
    errors = []
    for version, files in inventories:
        errors.extend(f"{version}: {rel}: {why}" for rel in files if (why := reason(rel)))
    if errors:
        print("\n".join(errors))
        return 1
    historical = len(inventories) - 1 - int(args.working_tree)
    untracked = len(inventories[1][1]) if args.working_tree else 0
    print(f"Public repository check passed: {len(inventories[0][1])} tracked files, "
          f"{untracked} untracked files, {historical} historical commits checked")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
