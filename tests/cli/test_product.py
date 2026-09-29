#!/usr/bin/env python3
"""Exercise the benchmark CLI and independent exact output checker, not timings."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
if not __debug__:
    raise RuntimeError("CLI verification requires Python assertions enabled")


def main():
    product, boundary = map(lambda x: Path(x).resolve(), sys.argv[1:3])
    checks = 0
    with tempfile.TemporaryDirectory(prefix="rfc-product-") as temporary:
        root = Path(temporary)
        def run(executable, args, success=True):
            nonlocal checks
            p = subprocess.run([str(executable), *map(str, args)], capture_output=True, text=True)
            assert (p.returncode == 0) == success, (p.returncode, p.stdout, p.stderr)
            checks += 1
            return p
        for args in ([], [root / "invalid", "0"], [root / "invalid", "513"],
                     [root / "invalid", "3junk"], [root / "invalid", "3", "0"],
                     [root / "invalid", "3", "12"], [root / "invalid", "-2"]):
            run(product, args, False)
        assert not (root / "invalid").exists()
        out = root / "corpus"
        run(product, [out, "3", "1"])
        paths = {p.relative_to(out): hashlib.sha256(p.read_bytes()).hexdigest() for p in out.rglob("*") if p.is_file()}
        run(product, [out, "3", "1"], False)
        assert paths == {p.relative_to(out): hashlib.sha256(p.read_bytes()).hexdigest() for p in out.rglob("*") if p.is_file()}
        script = Path(__file__).resolve().parents[2] / "tools/verify_temporal_product_reference.py"
        spec = importlib.util.spec_from_file_location("product_reference", script)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        result = module.verify(out)
        assert result["all_exact"]
        checks += result["route_products_checked"] + result["moment_entries_checked"]
        b = root / "boundary"
        run(boundary, [b, "9"])
        run(boundary, [b, "9"], False)
        run(boundary, [root / "bad_boundary", "2"], False)
        run(boundary, [root / "bad_boundary", "257"], False)
        assert (b / "base.rfcxi").stat().st_size > 0
    print(json.dumps({"checks": checks, "exact_outputs_verified": True, "existing_outputs_preserved": True}))


if __name__ == "__main__":
    main()
