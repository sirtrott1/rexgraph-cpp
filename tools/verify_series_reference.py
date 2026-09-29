#!/usr/bin/env python3
"""Independent Fraction checks and outer RFCXS re-encoding (inner codecs opaque)."""
from __future__ import annotations
import argparse
from fractions import Fraction as F
import json
from pathlib import Path
import struct


def fnv(data: bytes) -> int:
    value = 1469598103934665603
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return value


class Reader:
    def __init__(self, data: bytes):
        self.data, self.pos = data, 0

    def raw(self, n: int) -> bytes:
        if n < 0 or n > len(self.data) - self.pos:
            raise ValueError("truncated independent record")
        out = self.data[self.pos:self.pos+n]
        self.pos += n
        return out

    def u64(self) -> int:
        return struct.unpack("<Q", self.raw(8))[0]

    def blob(self) -> bytes:
        return self.raw(self.u64())


def frame(data: bytes, magic: bytes) -> bytes:
    if len(data) < 32:
        raise ValueError("short frame")
    if data[:8] != magic:
        raise ValueError("magic")
    version, flags, length, digest = struct.unpack("<IIQQ", data[8:32])
    if version != 1 or length != len(data) or digest != fnv(data[32:]):
        raise ValueError("frame envelope")
    if magic == b"RFCXS\x01\x00\x00" and flags:
        raise ValueError("series flags")
    return data[32:]


def p64(n: int) -> bytes:
    return struct.pack("<Q", n)


def blob(b: bytes) -> bytes:
    return p64(len(b)) + b


def archive(path: Path) -> dict:
    data = path.read_bytes()
    r = Reader(frame(data, b"RFCXS\x01\x00\x00"))
    namespace, source, channels = r.u64(), r.blob(), r.blob()
    seed = Reader(frame(channels, b"RFCXE\x01\x00\x00"))
    first = sequence = seed.u64()
    source_hash = seed.u64()
    _source_image_hash = seed.u64()
    endpoint_hash = seed.u64()
    n = r.u64()
    body = p64(namespace) + blob(source) + blob(channels) + p64(n)
    for _ in range(n):
        raw = r.blob()
        f = Reader(frame(raw, b"RFCXT\x01\x00\x00"))
        before, after, old_endpoint, new_endpoint, old_source, new_source = [f.u64() for _ in range(6)]
        assert before == sequence and after == before + 1
        assert old_endpoint == endpoint_hash and old_source == source_hash
        sequence, endpoint_hash, source_hash = after, new_endpoint, new_source
        body += blob(raw)
    count = r.u64()
    body += p64(count)
    checkpoints = []
    for _ in range(count):
        seq, cp_source, cp_channels = r.u64(), r.blob(), r.blob()
        assert first < seq <= sequence and (not checkpoints or seq > checkpoints[-1])
        assert Reader(frame(cp_channels, b"RFCXE\x01\x00\x00")).u64() == seq
        checkpoints.append(seq)
        body += p64(seq) + blob(cp_source) + blob(cp_channels)
    assert r.pos == len(r.data)
    rebuilt = b"RFCXS\x01\x00\x00" + struct.pack("<IIQQ", 1, 0, 32+len(body), fnv(body)) + body
    assert rebuilt == data
    return {"file": path.name, "bytes": len(data), "frames": n, "checkpoints": checkpoints,
            "first_sequence": first, "last_sequence": sequence}


def mat(line: str):
    values = line.split()
    rows, cols = map(int, values[:2])
    xs = list(map(F, values[2:]))
    assert len(xs) == rows*cols
    return [xs[i*cols:(i+1)*cols] for i in range(rows)]


def vec(line: str):
    values = line.split()
    result = list(map(F, values[1:]))
    assert len(result) == int(values[0])
    return result


def multiply(a, b):
    assert len(a[0]) == len(b)
    return [[sum(a[i][k]*b[k][j] for k in range(len(b))) for j in range(len(b[0]))] for i in range(len(a))]


def apply(a, x):
    assert len(a[0]) == len(x)
    return [sum(q*v for q, v in zip(row, x)) for row in a]


def plus(a, b):
    assert len(a) == len(b)
    return [x+y for x, y in zip(a, b)]


def channels(initial, maps, innovations):
    dimension = len(maps[-1]) if maps else len(initial)
    suffix = [[F(i == j) for j in range(dimension)] for i in range(dimension)]
    tails = []
    for a, eta in zip(reversed(maps), reversed(innovations)):
        tails.append(apply(suffix, eta))
        suffix = multiply(suffix, a)
    return [apply(suffix, initial)] + list(reversed(tails))


def gram(xs):
    return [[sum(a*b for a, b in zip(x, y)) for y in xs] for x in xs]


def verify(directory: Path):
    lines = (directory/"paper.txt").read_text().splitlines()
    j0, j1 = mat(lines[0]), mat(lines[1])
    x0, eta0, eta1 = map(vec, lines[2:5])
    expected = channels(x0, [j0, j1], [eta0, eta1])
    assert expected == list(map(vec, lines[5:8]))
    terminal = [sum(xs[i] for xs in expected) for i in range(2)]
    assert terminal == vec(lines[8]) == [13, -7]
    assert gram(expected) == mat(lines[9])
    assert sum(gram(expected)[i][j] for i in (1, 2) for j in (1, 2)) == 117
    lines = (directory/"random.txt").read_text().splitlines()
    state = initial = vec(lines[0]); maps, innovations = [], []
    for t in range(32):
        a, eta, target = mat(lines[1+3*t]), vec(lines[2+3*t]), vec(lines[3+3*t])
        state = plus(apply(a, state), eta)
        assert state == target
        maps.append(a); innovations.append(eta)
    xs = channels(initial, maps, innovations)
    assert xs == [vec(line) for line in lines[97:130]]
    matrix = gram(xs)
    assert matrix == mat(lines[130])
    assert len(lines) == 131
    files = [archive(path) for path in sorted(directory.glob("*.rfcxs")) if path.name != "wrong-checkpoint.rfcxs"]
    return {"status": "passed", "random_intervals": 32, "random_channels": 33, "random_gram_entries": 1089,
            "paper_innovation_quadrance": 117, "outer_archives": files,
            "scope": "Independent Fraction algebra and outer envelope re-encoding; inner source/declaration codecs are opaque. Checkpoint/history equality is tested in C++."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixtures", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    result = verify(args.fixtures)
    text = json.dumps(result, indent=2)
    if args.report:
        args.report.write_text(text+"\n")
    print(text)


if __name__ == "__main__":
    main()
