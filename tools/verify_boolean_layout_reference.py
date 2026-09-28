#!/usr/bin/env python3
"""Independent Python integer/Fraction model of RFCXD/RFCXV v1.

Verify emitted fixture bytes and produce independent interoperability fixtures.
No library bindings, floating arithmetic, source modification, or external I/O.
"""
from __future__ import annotations
import argparse
from fractions import Fraction
import hashlib
import json
from pathlib import Path

D_MAGIC = b"RFCXD\x01\0\0"
V_MAGIC = b"RFCXV\x01\0\0"


def vu(x: int) -> bytes:
    if not 0 <= x < 2**64:
        raise ValueError("varuint range")
    groups = []
    while x >= 128:
        groups.append((x % 128) | 128)
        x //= 128
    return bytes(groups + [x])


class Reader:
    def __init__(self, data: bytes):
        self.data, self.pos = data, 0

    def read(self, n: int) -> bytes:
        if n < 0 or n > len(self.data) - self.pos:
            raise ValueError("truncated octets")
        out = self.data[self.pos:self.pos + n]
        self.pos += n
        return out

    def fixed(self, n: int) -> int:
        return int.from_bytes(self.read(n), "little")

    def var(self) -> int:
        start = self.pos
        result = 0
        for shift in range(0, 70, 7):
            byte = self.fixed(1)
            result += (byte % 128) * 2**shift
            if byte < 128:
                if self.data[start:self.pos] != vu(result):
                    raise ValueError("noncanonical varuint")
                return result
        raise ValueError("varuint overflow")

    def identity(self) -> tuple[int, int, int]:
        return tuple(self.fixed(8) for _ in range(3))

    def finish(self) -> None:
        if self.pos != len(self.data):
            raise ValueError("trailing octets")


def encode_id(identity: tuple[int, int, int]) -> bytes:
    return b"".join(v.to_bytes(8, "little") for v in identity)


def decode_declarations(data: bytes):
    r = Reader(data)
    if r.read(8) != D_MAGIC or r.fixed(1) != 1:
        raise ValueError("profile")
    root, count = r.identity(), r.var()
    graph = {}
    previous = (0, 0, 0)
    for _ in range(count):
        key, kind = r.identity(), r.fixed(1)
        if not all(key) or not previous < key:
            raise ValueError("declaration identity order")
        previous = key
        if kind in (0, 2, 3):
            payload = None
        elif kind == 1:
            payload = [r.fixed(8) for _ in range(r.var())]
        elif kind in (4, 5):
            payload = []
            for _ in range(r.var()):
                factor = r.fixed(8)
                child = r.identity()
                grade = r.fixed(2) if kind == 4 else 0
                payload.append((factor, child, grade))
        elif kind == 6:
            payload = r.identity()
        elif kind == 7:
            child = r.identity()
            payload = (child, r.var(), r.var())
        elif kind == 8:
            payload = []
            for _ in range(r.var()):
                axis = r.fixed(8)
                extent = r.var()
                coordinates = [r.fixed(8) for _ in range(r.var())]
                payload.append((axis, extent, coordinates))
        else:
            raise ValueError("constructor")
        graph[key] = (kind, payload)
    r.finish()
    if root not in graph:
        raise ValueError("root")
    return root, graph


def encode_declarations(root, graph) -> bytes:
    out = bytearray(D_MAGIC + b"\x01" + encode_id(root) + vu(len(graph)))
    for identity, (kind, p) in sorted(graph.items()):
        out += encode_id(identity) + bytes([kind])
        if kind in (0, 2, 3):
            pass
        elif kind == 1:
            out += vu(len(p)) + b"".join(x.to_bytes(8, "little") for x in p)
        elif kind in (4, 5):
            out += vu(len(p))
            for factor, child, grade in p:
                out += factor.to_bytes(8, "little") + encode_id(child)
                if kind == 4:
                    out += grade.to_bytes(2, "little")
        elif kind == 6:
            out += encode_id(p)
        elif kind == 7:
            out += encode_id(p[0]) + vu(p[1]) + vu(p[2])
        elif kind == 8:
            out += vu(len(p))
            for axis, extent, coordinates in p:
                out += axis.to_bytes(8, "little") + vu(extent) + vu(len(coordinates))
                out += b"".join(x.to_bytes(8, "little") for x in coordinates)
        else:
            raise ValueError("constructor")
    return bytes(out)


class Bits:
    """Arbitrary-precision bit arithmetic, rather than the C++ byte cursor."""
    def __init__(self, data: bytes, nbits: int):
        self.word = int.from_bytes(data, "little")
        self.nbits, self.pos = nbits, 0
        if len(data) != (nbits + 7) // 8 or self.word >> nbits:
            raise ValueError("extent/tail")

    def take(self, width: int) -> int:
        if not 0 <= width <= self.nbits - self.pos:
            raise ValueError("truncated bits")
        result = (self.word >> self.pos) % 2**width
        self.pos += width
        return result

    def var(self) -> int:
        raw = bytearray()
        for _ in range(10):
            v = self.take(8)
            raw.append(v)
            if v < 128:
                return Reader(bytes(raw)).var()
        raise ValueError("long varuint")

    def magnitude(self) -> int:
        count = self.var()
        value = self.take(count * 8)
        if count and value < 2**(8*(count-1)):
            raise ValueError("nonminimal magnitude")
        return value

    def integer(self) -> int:
        sign = self.take(1)
        x = self.magnitude()
        if sign and not x:
            raise ValueError("negative zero")
        return -x if sign else x

    def rational(self) -> Fraction:
        n, d = self.integer(), self.magnitude()
        q = Fraction(n, d)
        if (q.numerator, q.denominator) != (n, d):
            raise ValueError("noncanonical rational")
        return q


class Output:
    def __init__(self):
        self.word, self.bits = 0, 0

    def put(self, x: int, width: int):
        if not 0 <= x < 2**width:
            raise ValueError("bit range")
        self.word += x * 2**self.bits
        self.bits += width

    def var(self, x: int):
        for b in vu(x):
            self.put(b, 8)

    def magnitude(self, x: int):
        x = abs(x)
        n = (x.bit_length() + 7) // 8
        self.var(n)
        self.put(x, n*8)

    def integer(self, x: int):
        self.put(int(x < 0), 1)
        self.magnitude(x)

    def rational(self, q: Fraction):
        self.integer(q.numerator)
        self.magnitude(q.denominator)

    def finish(self):
        return self.bits, self.word.to_bytes((self.bits + 7)//8, "little")


def tag_bits(n: int) -> int:
    return (n-1).bit_length()


def read_value(bits: Bits, graph, root):
    k, p = graph[root]
    if k == 0:
        return bool(bits.take(1))
    if k == 1:
        code = bits.take(tag_bits(len(p)))
        if code >= len(p):
            raise ValueError("reserved finite symbol")
        return ("symbol", p[code])
    if k == 2:
        return bits.integer()
    if k == 3:
        return bits.rational()
    if k == 4:
        return ("product", [read_value(bits, graph, child) for _, child, _ in p])
    if k == 5:
        code = bits.take(tag_bits(len(p)))
        if code >= len(p):
            raise ValueError("reserved alternative")
        key, child, _ = p[code]
        return ("alternative", key, read_value(bits, graph, child))
    if k == 6:
        return ("optional", read_value(bits, graph, p) if bits.take(1) else None)
    if k == 7:
        count = bits.var()
        if not p[1] <= count <= p[2]:
            raise ValueError("repeat bounds")
        return ("repeated", [read_value(bits, graph, p[0]) for _ in range(count)])
    if k == 8:
        support = [bits.var() for _ in range(bits.var())]
        nz = []
        for _ in range(bits.var()):
            index = bits.var()
            nz.append((index, bits.rational()))
        return ("tensor", support, nz)
    raise ValueError("constructor")


def write_value(out: Output, graph, root, value):
    k, p = graph[root]
    if k == 0:
        out.put(int(value), 1)
    elif k == 1:
        out.put(p.index(value[1]), tag_bits(len(p)))
    elif k == 2:
        out.integer(value)
    elif k == 3:
        out.rational(value)
    elif k == 4:
        for (_, child, _), component in zip(p, value[1], strict=True):
            write_value(out, graph, child, component)
    elif k == 5:
        index = next(i for i, f in enumerate(p) if f[0] == value[1])
        out.put(index, tag_bits(len(p)))
        write_value(out, graph, p[index][1], value[2])
    elif k == 6:
        out.put(int(value[1] is not None), 1)
        if value[1] is not None:
            write_value(out, graph, p, value[1])
    elif k == 7:
        out.var(len(value[1]))
        for v in value[1]:
            write_value(out, graph, p[0], v)
    elif k == 8:
        out.var(len(value[1]))
        for index in value[1]:
            out.var(index)
        out.var(len(value[2]))
        for index, q in value[2]:
            out.var(index)
            out.rational(q)
    else:
        raise ValueError("constructor")


def decode_archive(data):
    r = Reader(data)
    if r.read(8) != V_MAGIC:
        raise ValueError("archive magic")
    root, graph = decode_declarations(r.read(r.var()))
    values = []
    for _ in range(r.var()):
        width = r.var()
        bits = Bits(r.read((width+7)//8), width)
        values.append(read_value(bits, graph, root))
        if bits.pos != width:
            raise ValueError("trailing value bits")
    r.finish()
    return root, graph, values


def encode_archive(root, graph, values):
    definitions = encode_declarations(root, graph)
    out = bytearray(V_MAGIC + vu(len(definitions)) + definitions + vu(len(values)))
    for value in values:
        bits = Output()
        write_value(bits, graph, root, value)
        n, data = bits.finish()
        out += vu(n) + data
    return bytes(out)


def generate(directory: Path):
    directory.mkdir(parents=True, exist_ok=True)
    identity = lambda k: (991, k, 1)
    graph = {
        identity(1): (0, None),
        identity(2): (1, [500, 100, 200, 300, 0, 400]),
        identity(3): (2, None),
        identity(4): (3, None),
        identity(5): (6, identity(4)),
        identity(6): (7, (identity(1), 0, 129)),
        identity(7): (4, [(10, identity(2), 0), (20, identity(3), 1),
                          (30, identity(5), 0), (40, identity(6), 2)]),
    }
    values = [
        ("product", [("symbol", 0), -(2**129+1), ("optional", Fraction(1, 3)),
                     ("repeated", [i % 3 == 0 for i in range(129)])]),
        ("product", [("symbol", 400), 2**64, ("optional", None), ("repeated", [])]),
    ]
    data = encode_archive(identity(7), graph, values)
    (directory/"independent.rfcxv").write_bytes(data)
    (directory/"independent.rfcxd").write_bytes(encode_declarations(identity(7), graph))
    (directory/"README.md").write_text(
        "# Independent F1 codec fixtures\n\nGenerated by the standalone Python integer/Fraction reference, not the C++ writer.\n"
        "The product retains a six-symbol state, exact integer, optional rational, and repeated Boolean values.\n"
        "The first integer is -(2^129+1); the second is 2^64.\n", encoding="utf-8")


def verify(directory: Path):
    records = []
    for path in sorted(directory.iterdir()):
        if path.suffix == ".rfcxd":
            data = path.read_bytes()
            root, graph = decode_declarations(data)
            assert encode_declarations(root, graph) == data, path.name
            records.append({"file": path.name, "values": 0, "sha256": hashlib.sha256(data).hexdigest()})
        elif path.suffix == ".rfcxv":
            data = path.read_bytes()
            root, graph, values = decode_archive(data)
            assert encode_archive(root, graph, values) == data, path.name
            if path.name == "integer.rfcxv":
                assert values == [-2**129, 0, 2**129+17]
            elif path.name == "rational.rfcxv":
                assert values == [Fraction(1, 3), Fraction(0), Fraction(-2**129, 2**129+1)]
            elif path.name == "optional.rfcxv":
                assert values == [("optional", None), ("optional", ("optional", None)),
                                  ("optional", ("optional", Fraction(0)))]
            elif path.name == "source_value.rfcxv":
                assert values == [("product", [(64*7+j*11) % 13 < 6 for j in range(131)])]
            elif path.name == "tensor.rfcxv":
                assert values[0] == ("tensor", [0, 2, 11], [(2, Fraction(1, 2)), (11, Fraction(-2))])
                assert values[1] == ("tensor", [2, 11], [(2, Fraction(1, 2)), (11, Fraction(-2))])
            records.append({"file": path.name, "values": len(values), "sha256": hashlib.sha256(data).hexdigest()})
    if not records:
        raise ValueError("no declared codec fixtures")
    return {"independent_reference": "Python integers and Fraction", "files": len(records),
            "values": sum(r["values"] for r in records), "records": records, "status": "passed"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--generate", action="store_true")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    if args.generate:
        generate(args.directory)
    report = verify(args.directory)
    text = json.dumps(report, indent=2)
    if args.report:
        args.report.write_text(text+"\n", encoding="utf-8")
    print(text)


if __name__ == "__main__":
    main()
