#!/usr/bin/env python3
"""Export RexGraphCpp source and its available local Git history."""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile

from check_public_repository import reason

ROOT = Path(__file__).resolve().parents[1]
PREFIX = "rexgraph-cpp/rexgraphcpp/"


def git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args])


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(1024 * 1024):
            value.update(block)
    return value.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    if args.out.exists():
        parser.error("The output archive must be new")
    names = git("ls-files", "--cached", "--others", "--exclude-standard", "-z").decode().split("\0")
    names = sorted({name for name in names if name and (ROOT / name).is_file()})
    errors = [f"{name}: {why}" for name in names if (why := reason(name))]
    if errors:
        raise SystemExit("\n".join(errors))
    files = [ROOT / name for name in names]
    assert all(not path.is_symlink() for path in files), "Public source symlinks require an export declaration"
    inventory = {PREFIX + name: digest(ROOT / name) for name in names}
    refs = git("for-each-ref", "--format=%(refname) %(objectname)")
    head = git("rev-parse", "HEAD").decode().strip()
    original = Path(git("rev-parse", "--absolute-git-dir").decode().strip())
    assert not (original / "objects/info/alternates").exists(), "Git objects require an independent store"
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="rexgraph-export-") as directory:
        history = Path(directory) / "history.git"
        subprocess.run(["git", "clone", "--quiet", "--mirror", "--no-hardlinks",
                        str(ROOT), str(history)], check=True)
        shutil.copytree(original / "objects", history / "objects", dirs_exist_ok=True)
        if (original / "logs").is_dir():
            shutil.copytree(original / "logs", history / "logs", dirs_exist_ok=True)
        shutil.copy2(original / "index", history / "index")
        (history / "config").write_text("[core]\n\trepositoryformatversion = 0\n"
                                      "\tfilemode = true\n\tbare = false\n\tlogallrefupdates = true\n")
        shutil.rmtree(history / "hooks", ignore_errors=True)
        subprocess.run(["git", "--git-dir", str(history), "fsck", "--full"],
                       check=True, stdout=subprocess.DEVNULL)
        assert subprocess.check_output(["git", "--git-dir", str(history), "rev-parse", "HEAD"]).decode().strip() == head
        assert subprocess.check_output(["git", "--git-dir", str(history),
                                        "for-each-ref", "--format=%(refname) %(objectname)"]) == refs
        histories = sorted(path for path in history.rglob("*") if path.is_file())
        assert all(not path.is_symlink() for path in histories), "Unsupported Git symlink"
        inventory.update({PREFIX + ".git/" + path.relative_to(history).as_posix(): digest(path)
                          for path in histories})
        with zipfile.ZipFile(args.out, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for path in files:
                archive.write(path, PREFIX + path.relative_to(ROOT).as_posix())
            for path in histories:
                archive.write(path, PREFIX + ".git/" + path.relative_to(history).as_posix())
            archive.write(history / "refs", PREFIX + ".git/refs/")
            archive.writestr("SHA256SUMS", "".join(f"{value}  {name}\n" for name, value in sorted(inventory.items())))
    with zipfile.ZipFile(args.out) as archive:
        for name, expected in inventory.items():
            value = hashlib.sha256()
            with archive.open(name) as stream:
                while block := stream.read(1024 * 1024):
                    value.update(block)
            assert value.hexdigest() == expected, name
    assert git("rev-parse", "HEAD").decode().strip() == head and git("for-each-ref", "--format=%(refname) %(objectname)") == refs
    assert all(digest(ROOT / name) == inventory[PREFIX + name] for name in names), "Source changed during export"
    current = {name for name in git("ls-files", "--cached", "--others", "--exclude-standard", "-z").decode().split("\0")
               if name and (ROOT / name).is_file()}
    assert current == set(names), "Public file inventory changed during export"
    print(f"Verified {len(files)} RexGraphCpp public files; Git HEAD {head}")
    print(f"SHA256 {digest(args.out)}")


if __name__ == "__main__":
    main()
