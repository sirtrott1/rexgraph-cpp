#!/usr/bin/env python3
"""Independent Fraction product/chain-map oracle and bounded RFCXAM codec.

This deliberately knows only the emitted F4 hierarchy fixture: it is not a
second general implementation or a security proof of the native atlas reader.
"""
from __future__ import annotations
import argparse
import csv
from collections import defaultdict
from fractions import Fraction as Q
import hashlib
import json
from pathlib import Path
import struct


def require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def fnv(data: bytes) -> int:
    h = 1469598103934665603  # Retained RFCX legacy basis, intentionally not the standard FNV offset.
    for byte in data:
        h = ((h ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return h


class Reader:
    def __init__(self, data: bytes):
        self.data, self.pos = data, 0

    def take(self, n: int) -> bytes:
        require(n >= 0 and n <= len(self.data) - self.pos, "truncated oracle input")
        v = self.data[self.pos:self.pos + n]
        self.pos += n
        return v

    def u(self, width: int = 8) -> int:
        return int.from_bytes(self.take(width), "little")

    def integer(self) -> int:
        sign, size = self.u(1), self.u(4)
        require(sign in (0, 1), "invalid sign")
        b = self.take(size)
        require(not b or b[0] != 0, "noncanonical magnitude")
        require(b or not sign, "negative zero")
        return (-1 if sign else 1) * int.from_bytes(b, "big")

    def rational(self) -> Q:
        n, d = self.integer(), self.integer()
        require(d > 0, "invalid denominator")
        q = Q(n, d)
        require(q.numerator == n and q.denominator == d, "unreduced rational")
        return q

    def cell(self) -> tuple[int, int]:
        return self.u(2), self.u()

    def columns(self) -> dict:
        out = {}
        for _ in range(self.u()):
            source = self.cell()
            require(source not in out, "duplicate source column")
            image = {}
            for _ in range(self.u()):
                target, value = self.cell(), self.rational()
                require(target not in image and value != 0, "bad chain term")
                image[target] = value
            out[source] = image
        return out


def uint(n: int, width: int = 8) -> bytes:
    return n.to_bytes(width, "little")


def integer(n: int) -> bytes:
    b = abs(n).to_bytes((abs(n).bit_length() + 7) // 8, "big")
    return uint(n < 0, 1) + uint(len(b), 4) + b


def rational(q: Q) -> bytes:
    return integer(q.numerator) + integer(q.denominator)


def columns(cs: dict) -> bytes:
    out = uint(len(cs))
    for source, terms in sorted(cs.items()):
        out += uint(source[0], 2) + uint(source[1]) + uint(len(terms))
        for target, q in sorted(terms.items()):
            out += uint(target[0], 2) + uint(target[1]) + rational(q)
    return out


def read_blueprint(data: bytes) -> tuple:
    r = Reader(data)
    require(r.take(8) == b"RFCXAM\x01\x00", "manifest magic")
    require((r.u(4), r.u(4)) == (1, 0), "manifest version")
    require(r.u() == len(data) and r.u() == fnv(data[32:]), "manifest header")
    key, revision = r.u(), r.u()
    require([r.u(), r.u(), r.u()] == [0, 0, 0], "fixture must have no embeddings/pushouts/products")
    maps = []
    for _ in range(r.u()):
        k, a, b = r.u(), r.u(), r.u()
        f = r.columns()
        flag = r.u(1)
        require(flag in (0, 1), "retraction flag")
        back = r.columns() if flag else None
        maps.append((k, a, b, f, back))
    paths = []
    for _ in range(r.u()):
        k = r.u()
        left = [r.u() for _ in range(r.u())]
        right = [r.u() for _ in range(r.u())]
        paths.append((k, left, right))
    require((r.u(), r.u()) == (0, 0), "unexpected interface fixture")
    require(r.pos == len(data), "trailing bytes")
    return key, revision, maps, paths


def encode_blueprint(key: int, revision: int, maps: list, paths: list) -> bytes:
    body = uint(key) + uint(revision) + uint(0) * 3 + uint(len(maps))
    for k, a, b, f, back in maps:
        body += uint(k) + uint(a) + uint(b) + columns(f) + uint(back is not None, 1)
        if back is not None:
            body += columns(back)
    body += uint(len(paths))
    for k, left, right in paths:
        body += uint(k) + uint(len(left)) + b"".join(map(uint, left))
        body += uint(len(right)) + b"".join(map(uint, right))
    body += uint(0) * 2
    return b"RFCXAM\x01\x00" + struct.pack("<IIQQ", 1, 0, 32 + len(body), fnv(body)) + body


def apply(f: dict, x: dict) -> dict:
    y = defaultdict(Q)
    for k, q in x.items():
        for t, v in f[k].items():
            y[t] += q * v
    return {k: v for k, v in y.items() if v}


def boundary(sid: int, cell: tuple[int, int]) -> dict:
    grade, key = cell
    if grade == 0:
        return {}
    require(grade == 1, "unexpected fixture grade")
    base, first, count = {4: (1, 400, 1), 5: (10, 500, 2), 6: (20, 600, 2)}[sid]
    require(first <= key < first + count, "unexpected fixture edge")
    v = base + key - first
    return {(0, v): Q(-1), (0, v + 1): Q(1)}


def verify(root: Path) -> dict:
    trials = defaultdict(lambda: ({}, {}))
    with (root / "products.tsv").open(newline="") as stream:
        for row in csv.DictReader(stream, delimiter="\t"):
            inputs, outputs = trials[int(row["trial"])]
            if int(row["lower_left_grade"]) == -1:
                inputs[int(row["upper_left"]), int(row["upper_right"])] = Q(row["coefficient"])
            else:
                cell = tuple(int(row[k]) for k in ("lower_left_grade", "lower_left", "lower_right_grade", "lower_right"))
                require(cell not in outputs, "duplicate product support")
                outputs[cell] = Q(row["result"])
    require(len(trials) == 128, "missing product trials")
    for n, (inputs, outputs) in trials.items():
        expected = defaultdict(Q)
        for (left, right), q in inputs.items():
            x, y = 10 + left - 100, 20 + right - 200
            # Preserve support even for cancellations and explicit zero inputs.
            expected[0, x, 1, right] -= q
            expected[0, x + 1, 1, right] += q
            expected[1, left, 0, y] += q   # Koszul minus times right minus
            expected[1, left, 0, y + 1] -= q
        require(dict(expected) == outputs, f"exact product/support mismatch at trial {n}")
    data = (root / "hierarchy-change.rfcxam").read_bytes()
    key, revision, maps, paths = read_blueprint(data)
    require(encode_blueprint(key, revision, maps, paths) == data, "independent manifest bytes differ")
    require((key, revision) == (701, 2), "unexpected manifest identity")
    require(len(maps) == 3 and paths == [(1, [1, 2], [3])], "unexpected hierarchy diagram")
    checked_columns, retractions = 0, 0
    indexed = {}
    for k, source, target, f, back in maps:
        indexed[k] = f
        for c, terms in f.items():
            lhs = defaultdict(Q)
            for t, q in terms.items():
                for u, v in boundary(target, t).items():
                    lhs[u] += q * v
            lhs = {c: q for c, q in lhs.items() if q}
            require(lhs == apply(f, boundary(source, c)), "independent hierarchy chain defect")
            checked_columns += 1
            if back is not None:
                require(apply(back, terms) == {c: Q(1)}, "independent retraction not identity")
                retractions += 1
    for c, terms in indexed[1].items():
        require(apply(indexed[2], terms) == indexed[3][c], "independent path disagreement")
    require(indexed[1][(1, 400)] == {(1, 500): Q(2), (1, 501): Q(2)}, "new complete map not retained")
    archives = []
    for p in sorted(root.glob("*.rfcxa")):
        b = p.read_bytes()
        require(b[:8] == b"RFCXA\x01\x00\x00", "archive magic")
        v, flags, size, check = struct.unpack_from("<IIQQ", b, 8)
        require((v, flags, size, check) == (1, 0, len(b), fnv(b[32:])), "archive envelope mismatch")
        archives.append(dict(file=p.name, bytes=len(b), sha256=hashlib.sha256(b).hexdigest()))
    return dict(status="passed", fraction_product_cases=len(trials), exact_chain_columns=checked_columns,
                left_inverse_columns=retractions, path_equalities=len(paths),
                independently_reencoded_manifest_bytes=len(data), archive_envelopes=archives,
                scope="independent bounded Fraction/manifest oracle; not a full RFCX decoder or proof")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixtures", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    text = json.dumps(verify(args.fixtures), indent=2) + "\n"
    if args.report:
        args.report.write_text(text)
    print(text, end="")


if __name__ == "__main__":
    main()
