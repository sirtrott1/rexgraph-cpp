#!/usr/bin/env python3
"""Independent F5 outer-archive codec and lineage checks.

Parses and re-encodes every new metadata field, owner lineage and frame boundary.
Embedded RFCX/RFCXE/RFCXT frames remain opaque: their mathematical correctness is
covered by native replay and existing codec tests, not claimed by this checker.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path


def u(n: int, size: int = 8) -> bytes:
    return n.to_bytes(size, 'little')


def blob(value: bytes) -> bytes:
    return u(len(value)) + value


def fnv(value: bytes) -> int:
    state = 1469598103934665603  # Existing RFCX legacy basis, deliberately retained.
    for byte in value:
        state = ((state ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return state


class Reader:
    def __init__(self, data: bytes):
        self.data, self.pos = data, 0

    def take(self, n: int) -> bytes:
        if n < 0 or n > len(self.data) - self.pos:
            raise ValueError('truncated reference input')
        value = self.data[self.pos:self.pos+n]
        self.pos += n
        return value

    def u(self, n: int = 8) -> int:
        return int.from_bytes(self.take(n), 'little')

    def blob(self) -> bytes:
        return self.take(self.u())

    def many(self, read):
        count = self.u()
        if count > 100000:
            raise ValueError('reference fixture record cap')
        return [read() for _ in range(count)]


def many(values, write) -> bytes:
    return u(len(values)) + b''.join(write(v) for v in values)


def read_origin(r):
    return r.u(), r.blob()


def origin(o):
    return u(o[0]) + blob(o[1])


def read_ledger(r):
    subjects = r.many(lambda: (read_origin(r), r.u()))
    assertions = r.many(lambda: (read_origin(r), r.u()))
    legs = r.many(lambda: ((read_origin(r), r.blob()), r.u()))
    for records in (subjects, assertions, legs):
        keys = [entry[0] for entry in records]
        native = [entry[1] for entry in records]
        if keys != sorted(set(keys)) or len(native) != len(set(native)) or any(k <= 0 for k in native):
            raise ValueError('noncanonical or ambiguous lineage')
    owners = dict(assertions)
    if any(entry[0][0] not in owners for entry in legs):
        raise ValueError('unowned occurrence')
    return subjects, assertions, legs


def ledger(v):
    return (many(v[0], lambda x: origin(x[0])+u(x[1])) +
            many(v[1], lambda x: origin(x[0])+u(x[1])) +
            many(v[2], lambda x: origin(x[0][0])+blob(x[0][1])+u(x[1])))


def read_schema(r):
    namespaces = [r.u() for _ in range(3)]
    subjects = r.many(lambda: (r.u(), r.blob(), r.blob(), r.blob()))

    def assertion():
        key, label = r.u(), r.blob()
        payload = [r.u() for _ in range(3)]
        roles = r.many(lambda: ([r.u() for _ in range(4)] + [r.u(1), r.u(1)]))
        fields = r.many(lambda: (r.u(), r.blob()))
        lineage_label, lineage_type = r.blob(), r.blob()
        references = r.many(lambda: (r.u(), r.blob()))
        if any(flag not in (0, 1) for role in roles for flag in role[-2:]):
            raise ValueError('invalid role flag')
        return key, label, payload, roles, fields, lineage_label, lineage_type, references

    return namespaces, subjects, r.many(assertion)


def schema(s):
    def assertion(a):
        key, label, payload, roles, fields, lineage_label, lineage_type, refs = a
        return (u(key)+blob(label)+b''.join(map(u, payload)) +
                many(roles, lambda r: b''.join(map(u, r[:4]))+u(r[4], 1)+u(r[5], 1)) +
                many(fields, lambda f: u(f[0])+blob(f[1]))+blob(lineage_label)+blob(lineage_type) +
                many(refs, lambda f: u(f[0])+blob(f[1])))
    return (b''.join(map(u, s[0])) +
            many(s[1], lambda x: u(x[0])+b''.join(blob(t) for t in x[1:])) +
            many(s[2], assertion))


def verify_file(path: Path) -> dict:
    data = path.read_bytes()
    r = Reader(data)
    magic, version, flags, size, digest = r.take(8), r.u(4), r.u(4), r.u(), r.u()
    if version != 1 or flags or size != len(data) or digest != fnv(data[32:]):
        raise ValueError('bad reference frame header')
    if magic == b'RFCXI\x01\0\0':
        s, revision, identities = read_schema(r), r.blob(), read_ledger(r)
        source, endpoint = r.blob(), r.blob()
        if not source.startswith(b'RFCX') or not endpoint.startswith(b'RFCXE'):
            raise ValueError('embedded source/endpoint framing')
        payload = schema(s)+blob(revision)+ledger(identities)+blob(source)+blob(endpoint)
        kind = 'snapshot'
    elif magic == b'RFCXIU\x01\0':
        namespaces = [r.u() for _ in range(3)]
        old_revision, new_revision = r.blob(), r.blob()
        old, identities = read_ledger(r), read_ledger(r)
        if old_revision == new_revision:
            raise ValueError('reused producer revision')
        for a, b in zip(old, identities):
            if any(dict(b).get(key) != value for key, value in a):
                raise ValueError('lineage was removed or reassigned')
        segment = r.blob()
        if not segment.startswith(b'RFCXT'):
            raise ValueError('embedded temporal frame')
        payload = b''.join(map(u, namespaces))+blob(old_revision)+blob(new_revision)+ledger(old)+ledger(identities)+blob(segment)
        kind = 'update'
    else:
        raise ValueError('unknown reference magic')
    rebuilt = magic+u(1, 4)+u(0, 4)+u(32+len(payload))+u(fnv(payload))+payload
    if r.pos != len(data) or rebuilt != data:
        raise ValueError('independent complete re-encoding differs')
    return {'file': path.name, 'kind': kind, 'bytes': len(data),
            'lineage': [len(x) for x in identities], 'sha256': hashlib.sha256(data).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    records = [verify_file(p) for p in sorted(args.directory.iterdir()) if p.suffix in ('.rfcxi', '.rfcxiu')]
    if not records:
        parser.error('no interpretation fixtures')
    result = {'status': 'passed', 'count': len(records), 'records': records}
    if args.report:
        args.report.write_text(json.dumps(result, indent=2)+'\n')
    print(len(records), 'outer archives independently re-encoded with monotone lineage checks')


if __name__ == '__main__':
    main()
