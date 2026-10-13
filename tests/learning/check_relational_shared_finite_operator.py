"""Independent byte reconstruction and rational whole-field verification."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
from check_relational_finite_operator import vector, response, occurrences, Q


def original_bytes(packet):
    assert packet[:8] == b"RFCSP\x01\x00\x00"
    version, flags, length, checksum = struct.unpack_from("<IIQQ", packet, 8)
    assert version == 1 and flags == 0 and length == len(packet)
    actual = 1469598103934665603
    for byte in packet[32:]:
        actual = ((actual ^ byte) * 1099511628211) & ((1 << 64) - 1)
    assert actual == checksum
    offset = 32

    def word():
        nonlocal offset
        value, = struct.unpack_from("<Q", packet, offset)
        offset += 8
        return value

    expanded, count = word(), word()
    chunks = []
    for _ in range(count):
        size = word()
        assert 0 < size <= 4096 and offset + size <= len(packet)
        chunks.append(packet[offset:offset + size])
        offset += size
    assert len(set(chunks)) == len(chunks)
    roots = [word() for _ in range(word())]
    assert offset == len(packet) and all(i < count for i in roots)
    assert list(dict.fromkeys(roots)) == list(range(count))
    result = b"".join(chunks[i] for i in roots)
    assert len(result) == expanded
    return result


def run(binary):
    rows = [json.loads(line) for line in subprocess.check_output([binary, '--emit'], text=True).splitlines()]
    assert len(rows) == 12
    for row in rows:
        xi, x = vector(row['xi']), vector(row['input'])
        expected = response(x, [-10 + 4*xi[0], 5 - 2*xi[0], xi[0]])
        assert vector(row['response']) == expected
        assert vector(row['occurrences']) == occurrences(x, [-10 + 4*xi[0], 5 - 2*xi[0], xi[0]])
        assert vector(row['next']) == [expected[0]+expected[2], expected[1]+expected[3], expected[4]+expected[5], expected[0]+expected[6]]
        p = response(x, [-10, 5, 0])
        direction = [a-b for a, b in zip(response(x, [-6, 3, 1]), p)]
        assert vector(row['particular']) == p
        assert [[Q(q) for q in r] for r in row['directions']] == [[q, Q(0), Q(0)] for q in direction]
    with tempfile.TemporaryDirectory(prefix='rexgraph-shared-operator-') as tmp:
        root = Path(tmp)
        subprocess.run([binary, '--write', tmp], check=True)
        golden = {'family': '08a5f6ca6f401ae8a0bd2558a2b39d716b5cbf4d3cbaf028a04b929b4d2f07a4',
                  'choice': '65fb8d7ee39dba9bfd8fe6d36993839da931d34bc0e972dbf195c7e026545372'}
        for name, digest in golden.items():
            compact = (root / (name + '.packet')).read_bytes()
            expanded = original_bytes(compact)
            assert expanded == (root / (name + '-original.packet')).read_bytes()
            assert hashlib.sha256(expanded).hexdigest() == digest
            assert len(compact) < len(expanded) // 20
        assert original_bytes((root/'authority.bin').read_bytes())
        subprocess.run([binary, '--read-family', str(root/'family.packet')], check=True)
        subprocess.run([binary, '--read-choice', str(root/'choice.packet')], check=True)
    print('12 complete Fraction fields, independent byte oracle, unchanged old packet hashes and two trusted-registry cold processes passed')


if __name__ == '__main__':
    run(sys.argv[1])
