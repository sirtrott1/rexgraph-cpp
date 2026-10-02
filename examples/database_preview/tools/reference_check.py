#!/usr/bin/env python3
"""Independent standard-library database envelope and discrete circuit oracle.
Native RFCX/RFCXE/RFCXT components are opaque here; C++ validates their semantics.
"""
from __future__ import annotations
import argparse, csv, hashlib, json, struct
from fractions import Fraction
from pathlib import Path

checks = 0

def check(condition: bool, why: str) -> None:
    global checks
    checks += 1
    if not condition:
        raise AssertionError(why)

def sha(data: bytes) -> bytes:
    return hashlib.sha256(data).digest()

def decode(data: bytes) -> dict:
    check(len(data) >= 64, "short file")
    magic, version, flags, total, msize, mh = struct.unpack_from("<8sIIQQ32s", data)
    check((magic, version, flags, total) == (b"RFCXDB\x01\0", 1, 0, len(data)), "header")
    manifest = data[64:64+msize]
    check(sha(manifest) == mh, "manifest digest")
    mm, ver, fl, ns, first, last, n = struct.unpack_from("<8sIIQQQQ", manifest)
    check((mm, ver, fl) == (b"RFCXDM\x01\0", 1, 0), "manifest header")
    check(len(manifest) == 48 + 64*n and 0 < ns and first <= last, "manifest extent")
    entries = [struct.unpack_from("<QQQQ32s", manifest, 48+64*i) for i in range(n)]
    expected = [(1, first), (2, first)] + [(3, i) for i in range(first+1, last+1)]
    check(n >= len(expected) and (n-len(expected)) % 2 == 0, "entry count")
    check([(e[0], e[1]) for e in entries[:len(expected)]] == expected, "history order")
    prior = None
    for i in range(len(expected), n, 2):
        a, b = entries[i:i+2]
        check(a[0] == 4 and b[0] == 5 and a[1] == b[1] and first <= a[1] <= last, "checkpoint pair")
        check(prior is None or a[1] > prior, "checkpoint order")
        prior = a[1]
    payload = data[64+msize:]
    seen = {}
    next_offset = 0
    components = []
    for kind, seq, off, length, digest in entries:
        check(length > 0 and off <= len(payload) and length <= len(payload)-off, "component bounds")
        component = payload[off:off+length]
        check(sha(component) == digest, "component digest")
        if digest not in seen:
            check(off == next_offset, "canonical first-use allocation")
            seen[digest] = (off, length, component)
            next_offset += length
        else:
            check(seen[digest] == (off, length, component), "dedup preserves complete contents")
        components.append(component)
    check(next_offset == len(payload), "no trailing bytes")
    # Encode independently and demand complete, not hash-only, equality.
    mb = struct.pack("<8sIIQQQQ", mm, 1, 0, ns, first, last, n) + b"".join(struct.pack("<QQQQ32s", *e) for e in entries)
    encoded = struct.pack("<8sIIQQ32s", magic, 1, 0, len(data), len(mb), sha(mb)) + mb + b"".join(v[2] for v in seen.values())
    check(encoded == data, "complete envelope re-encode")
    return dict(namespace=ns, first=first, last=last, entries=entries, components=components,
                manifest=manifest, bytes=len(data), logical_component_bytes=sum(e[3] for e in entries),
                unique_component_bytes=len(payload), unique_objects=len(seen))

def fnv(data: bytes) -> int:
    h = 1469598103934665603  # Existing RFC recurrence, not the standard FNV offset basis.
    for x in data:
        h = ((h ^ x) * 1099511628211) & ((1 << 64)-1)
    return h

def encode_series(db: dict) -> bytes:
    def u(n: int) -> bytes: return struct.pack("<Q", n)
    def blob(b: bytes) -> bytes: return u(len(b))+b
    comps = db["components"]
    count = db["last"]-db["first"]
    body = u(db["namespace"])+blob(comps[0])+blob(comps[1])+u(count)
    body += b"".join(blob(x) for x in comps[2:2+count])
    body += u((len(comps)-2-count)//2)
    for i in range(2+count, len(comps), 2):
        body += u(db["entries"][i][1])+blob(comps[i])+blob(comps[i+1])
    return struct.pack("<8sIIQQ", b"RFCXS\x01\0\0", 1, 0, 32+len(body), fnv(body))+body

def check_directory(path: Path, db: dict) -> None:
    check((path/"MANIFEST").read_bytes() == db["manifest"], "directory manifest bytes")
    check((path/"COMMIT").read_bytes() == sha(db["manifest"]).hex().encode()+b"\n", "directory commit")
    for entry, value in zip(db["entries"], db["components"]):
        check((path/"objects"/(entry[4].hex()+".blob")).read_bytes() == value, "directory object content")

def check_circuit(path: Path) -> list[dict]:
    v = Fraction(0)
    rows = list(csv.DictReader(path.open()))
    for t, row in enumerate(rows):
        previous = v
        drive = Fraction(1 if t <= 5 else -1)
        if t:
            v = (2*v+drive)/3
        check(int(row["sequence"]) == t and Fraction(row["voltage"]) == v, "voltage trajectory")
        check(Fraction(row["previous"]) == previous and Fraction(row["drive"]) == drive, "retained input/history")
        radius = Fraction(1, 100)
        valid = t != 7
        low = valid and v+radius <= Fraction(3, 10)
        high = valid and v-radius >= Fraction(7, 10)
        observed = (valid, low, high, valid and not low and not high, v < 0, v < previous)
        check(tuple(bool(int(row[k])) for k in ("valid", "low", "high", "uncertain", "negative", "falling")) == observed, "composite observation")
        if t:
            dv = v-previous
            check(2*dv+v == drive, "KCL")
            check((v*v-previous*previous)/2+dv*dv/2+v*v/2 == drive*v/2, "discrete energy identity")
    return rows

def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("fixtures", type=Path)
    p.add_argument("--report", type=Path)
    args = p.parse_args()
    parsed = {}
    for f in sorted(args.fixtures.glob("*.rfcxdb")):
        # The native filesystem tests deliberately emit a rejected symlink.
        # It is not a separate canonical archive and must not inflate this count.
        if f.is_symlink():
            continue
        parsed[f.name] = decode(f.read_bytes())
    db = parsed["sample.rfcxdb"]
    check_directory(args.fixtures/"sample.db", db)
    check(encode_series(db) == (args.fixtures/"sample.rfcxs").read_bytes(), "native outer series re-encode")
    rows = check_circuit(args.fixtures/"circuit.csv")
    report = dict(checks=checks, files=len(parsed), circuit_states=len(rows),
                  sample={k: db[k] for k in ("first", "last", "bytes", "logical_component_bytes", "unique_component_bytes", "unique_objects")},
                  scope="Independent SHA/layout/outer RFCXS and Fraction oracle; embedded native codecs opaque; synthetic model, no hardware measurement")
    text = json.dumps(report, indent=2)+"\n"
    if args.report: args.report.write_text(text)
    print(text)

if __name__ == "__main__":
    main()
