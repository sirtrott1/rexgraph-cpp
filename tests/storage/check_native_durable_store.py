#!/usr/bin/env python3
"""Independently inspect the durable envelope, lineage, index and original frames."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

checks = 0


def check(value, message):
    global checks
    checks += 1
    if not value:
        raise AssertionError(message)


def fnv(data):
    value = 1469598103934665603  # RFCX wire contract's retained offset basis
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return value


class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def take(self, count):
        check(0 <= count <= len(self.data) - self.pos, 'truncated reference input')
        value = self.data[self.pos:self.pos + count]
        self.pos += count
        return value

    def u64(self):
        return struct.unpack('<Q', self.take(8))[0]

    def identity(self):
        return f'{self.u64():016x}{self.u64():016x}'

    def blob(self):
        return self.take(self.u64())

    def end(self):
        check(self.pos == len(self.data), 'trailing reference input')


zero = '0' * 32


def envelope(path, kind):
    data = path.read_bytes()
    check(len(data) >= 32 and data[:8] == b'RFCXD\x01\x00\x00', 'new envelope magic')
    version, found, length, checksum = struct.unpack('<IIQQ', data[8:32])
    check((version, found, length, checksum) == (1, kind, len(data), fnv(data[32:])), 'new envelope fields')
    return data[32:]


def verify(directory):
    objects = directory / 'store' / 'objects'
    head = Reader(envelope(directory / 'store' / 'HEAD', 5))
    current = head.identity()
    head.end()
    chain = []
    while current != zero:
        r = Reader(envelope(objects / current, 4))
        previous, content, index = r.identity(), r.identity(), r.identity()
        ns, generation, transaction, first, last, count = (r.u64() for _ in range(6))
        r.end()
        chain.append(dict(identity=current, previous=previous, content=content, index=index,
                          ns=ns, generation=generation, transaction=transaction,
                          first=first, last=last, count=count))
        current = previous
        check(len(chain) <= 3, 'unexpected lineage/cycle')
    chain.reverse()
    check(len(chain) == 3, 'reference grouped commits')
    base = chain[0]
    check((base['ns'], base['generation'], base['transaction'], base['first'], base['last'], base['count']) ==
          (777, 0, 0, 40, 40, 0), 'reference genesis')
    check(base['index'] == zero, 'genesis index')
    base_archive = envelope(objects / base['content'], 1)
    r = Reader(base_archive[32:])
    check(base_archive[:8] == b'RFCXS\x01\x00\x00', 'original series grammar')
    check(r.u64() == 777, 'original namespace')
    source, channels = r.blob(), r.blob()
    check(r.u64() == 0 and r.u64() == 0, 'nonempty genesis')
    r.end()
    all_frames = []
    transactions = {}
    pages = {}

    def page(identity):
        if identity not in pages:
            r = Reader(envelope(objects / identity, 3))
            nodes = []
            for _ in range(65):
                left = (r.identity(), r.u64())
                right = (r.identity(), r.u64())
                commit = r.identity()
                nodes.append((left, right, commit))
            r.end()
            pages[identity] = nodes
        return pages[identity]

    def lookup(index, key):
        pointer = (index, 64)
        for depth in range(64):
            if pointer[0] == zero:
                return zero
            node = page(pointer[0])[pointer[1]]
            check(node[2] == zero, 'index internal node became a leaf')
            pointer = node[(key >> (63 - depth)) & 1]
        if pointer[0] == zero:
            return zero
        node = page(pointer[0])[pointer[1]]
        check(node[:2] == ((zero, 0), (zero, 0)), 'index leaf has branches')
        return node[2]

    for number, commit in enumerate(chain[1:], 1):
        prior = chain[number - 1]
        check(commit['previous'] == prior['identity'] and commit['first'] == prior['last'], 'predecessor changed')
        check(commit['generation'] == number and commit['ns'] == 777, 'generation/namespace changed')
        r = Reader(envelope(objects / commit['content'], 2))
        count = r.u64()
        frames = [r.blob() for _ in range(count)]
        r.end()
        check(count == commit['count'] == 1 and commit['last'] == commit['first'] + 1, 'frame grouping/order')
        check(frames[0] == (directory / ('first.rfcxt' if number == 1 else 'second.rfcxt')).read_bytes(), 'original frame bytes changed')
        all_frames.extend(frames)
        # Validate sharing, separately from the C++ update implementation.
        old_pointer = (prior['index'], 0 if prior['index'] == zero else 64)
        new_pointer = (commit['index'], 64)
        for depth in range(64):
            old_node = (((zero, 0), (zero, 0), zero) if old_pointer[0] == zero
                        else page(old_pointer[0])[old_pointer[1]])
            new_node = page(new_pointer[0])[new_pointer[1]]
            branch = (commit['transaction'] >> (63 - depth)) & 1
            check(new_node[1 - branch] == old_node[1 - branch], 'shared branch rewritten')
            check(new_node[branch] == (commit['index'], 63 - depth), 'new path not grouped in one page')
            old_pointer, new_pointer = old_node[branch], new_node[branch]
        transactions[commit['transaction']] = commit['identity']
        for key, value in transactions.items():
            check(lookup(commit['index'], key) == value, 'persistent transaction lookup changed')
        check(lookup(commit['index'], 1234) == zero, 'absent transaction invented')

    r = Reader((directory / 'expected.rfcxs').read_bytes()[32:])
    check(r.u64() == 777 and r.blob() == source and r.blob() == channels, 'base retained bytes changed')
    check(r.u64() == len(all_frames), 'export frame count changed')
    check([r.blob() for _ in all_frames] == all_frames and r.u64() == 0, 'export history changed')
    r.end()
    check(len(list(objects.iterdir())) == 8, 'unexpected per-primitive files')
    costs = (directory / 'write-cost.txt').read_text().splitlines()
    check(costs == ['commits 70', 'new_object_bytes 4516', 'head_bytes 48'], 'constant physical append cost changed')
    return {'checks': checks, 'commits': len(chain) - 1, 'canonical_frames': len(all_frames),
            'equal_output_commits': 70, 'new_object_bytes_per_equal_output_commit': 4516,
            'head_bytes_per_commit': 48}


with tempfile.TemporaryDirectory(prefix='rfc-durable-reference-') as tmp:
    directory = Path(tmp) / 'fixtures'
    subprocess.run([sys.argv[1], str(directory)], check=True)
    report = verify(directory)
    print(json.dumps(report, sort_keys=True))
    if len(sys.argv) == 3:
        Path(sys.argv[2]).write_text(json.dumps(report, indent=2) + '\n')
