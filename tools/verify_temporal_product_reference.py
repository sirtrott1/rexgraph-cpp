#!/usr/bin/env python3
"""Independent Fraction oracle for the retained fan and temporal history fixture.

Validates every exported occurrence-pair product, exact moments, work counters,
and complete outer-series framing. Embedded RFCX/RFCXE/RFCXT grammars are opaque
here; the C++ codec/regression suites validate them. No third-party dependencies.
"""
from __future__ import annotations
import argparse
import csv
import json
import struct
from fractions import Fraction as F
from pathlib import Path

if not __debug__:
    raise RuntimeError("Verification requires Python assertions enabled")

I = ((F(1), F(0)), (F(0), F(1)))
ZERO = ((F(0), F(0)), (F(0), F(0)))
H = ((F(2), F(1, 3)), (F(0), F(1)))
G = ((F(1), F(2, 5)), (F(3, 7), F(1)))
D = ((F(1, 5), F(2, 9)), (F(-3, 11), F(0)))
J = ((F(1), F(17, 19)), (F(0), F(1)))


def mul(a, b):
    return tuple(tuple(sum((a[i][k] * b[k][j] for k in range(len(b))), F(0))
                       for j in range(len(b[0]))) for i in range(len(a)))


def apply(a, x):
    return tuple(sum((u * v for u, v in zip(row, x)), F(0)) for row in a)


def scale(a, q):
    return tuple(tuple(q * v for v in row) for row in a)


def add(a, b):
    return tuple(tuple(x + y for x, y in zip(ar, br)) for ar, br in zip(a, b))


def model(n, t):
    """Return source occurrence maps, separate from any serialized C++ output."""
    lower, upper = {}, {}
    count = n + (9 <= t < 15)
    for triangle in range(count):
        vertices = (1, 10 + 4 * triangle, 11 + 4 * triangle)
        for edge in range(3):
            factor = F(1) if triangle or edge or t < 2 else F(-1) if t < 3 else F(-7, 3)
            middle = 100 + 4 * triangle + edge
            for leg in range(5):
                if t == 16 and triangle == edge == 0 and leg in (2, 3):
                    continue
                key = 1000 + 32 * triangle + 8 * edge + leg
                c = (-factor, factor, F(2, 7), F(-2, 7), F(0))[leg]
                active = leg != 4 or (t >= 10 and triangle == edge == 0)
                participant = vertices[(edge + 1) % 3] if leg == 1 else (
                    12 + 4 * triangle if leg == 4 else vertices[edge])
                matrix = scale(I, c)
                explicit = False
                if t >= 4 and triangle == edge == 0 and leg in (2, 3):
                    matrix, explicit = scale(H, c), True
                if 6 <= t < 14 and triangle == edge == leg == 0:
                    matrix, explicit = add(matrix, D), True
                support_change = t >= 11 and triangle == edge == 0 and leg == 2
                lower[key] = (middle, participant, active, c, matrix, explicit, support_change)
            key = 1000000 + 4 * triangle + edge
            upper[key] = (100 + triangle, middle, True, 1 / factor, scale(G if t >= 4 and triangle == 0 else I, 1 / factor))
    return lower, upper


def routes(n, t):
    lower, upper = model(n, t)
    fibers = {}
    for key, leg in lower.items():
        fibers.setdefault(leg[0], []).append((key, leg))
    result = {}
    for key, top in upper.items():
        for lower_key, leg in fibers[top[1]]:
            active = top[2] and leg[2]
            result[(key, lower_key)] = (top[0], top[1], leg[1], active,
                                        mul(leg[4], top[4]) if active else ZERO)
    return result


def read_csv(path):
    with path.open(newline="", encoding="utf-8") as stream:
        return list(csv.DictReader(stream))


def fnv(data):
    value = 1469598103934665603
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return value


def series_envelope(path):
    data = path.read_bytes()
    assert data[:8] == b"RFCXS\x01\0\0", "series magic"
    version, flags, size, checksum = struct.unpack_from("<IIQQ", data, 8)
    assert (version, flags, size) == (1, 0, len(data)), "series envelope header"
    assert checksum == fnv(data[32:]), "series outer integrity"
    offset = 32

    def u64():
        nonlocal offset
        assert offset + 8 <= len(data), "short u64"
        value, = struct.unpack_from("<Q", data, offset)
        offset += 8
        return value

    def blob():
        nonlocal offset
        length = u64()
        assert length <= len(data) - offset, "short blob"
        result = data[offset:offset + length]
        offset += length
        return result

    namespace, source, endpoint = u64(), blob(), blob()
    frames = [blob() for _ in range(u64())]
    checkpoints = []
    for _ in range(u64()):
        checkpoints.append((u64(), blob(), blob()))
    assert offset == len(data), "trailing bytes"
    return namespace, source, endpoint, frames, checkpoints


def verify(root):
    summary = json.loads((root / "summary.json").read_text())
    assert summary["format"] == "RFCX_PHASE06_PRODUCT_V1"
    n, trials = summary["triangles"], summary["trials"]
    actual_routes = {}
    for row in read_csv(root / "routes.csv"):
        step = int(row["step"])
        key = (int(row["upper_occ"]), int(row["lower_occ"]))
        value = (int(row["upper"]), int(row["middle"]), int(row["lower"]), bool(int(row["active"])),
                 ((F(row["m00"]), F(row["m01"])), (F(row["m10"]), F(row["m11"]))))
        assert key not in actual_routes.setdefault(step, {}), "duplicate route identity"
        actual_routes[step][key] = value
    assert set(actual_routes) == set(range(17)), "missing endpoint route fixture"
    checked_routes = checked_sums = 0
    for step in range(17):
        expected = routes(n, step)
        assert actual_routes[step] == expected, f"route identity/product mismatch at {step}"
        checked_routes += len(expected)
        sums = {}
        for top, middle, bottom, active, matrix in expected.values():
            del middle, active
            sums[(bottom, top)] = add(sums.get((bottom, top), ZERO), matrix)
        for (bottom, top), total in sums.items():
            wanted = scale(mul(D, G), F(-3, 7)) if (bottom, top) == (1, 100) and 6 <= step < 14 else ZERO
            assert total == wanted, f"complete defect at {step}/{bottom}/{top}"
            checked_sums += 1
    # All finite channels, including zero channels, remain ordered.
    xs = [(F(1), F(2)) if step < 8 else (F(5, 7), F(0)) for step in range(17)]
    maps = [J if step == 13 else I for step in range(1, 17)]
    channels = []
    for t in range(17):
        value = xs[0] if t == 0 else tuple(a - b for a, b in zip(xs[t], apply(maps[t - 1], xs[t - 1])))
        for matrix in maps[t:]:
            value = apply(matrix, value)
        channels.append(value)
    assert tuple(sum((c[i] for c in channels), F(0)) for i in range(2)) == xs[-1]
    metric = ((F(2), F(0)), (F(0), F(3)))
    gram = lambda c: [[sum((x * y for x, y in zip(a, apply(metric, b))), F(0)) for b in c] for a in c]
    typed = [c[0] + 2 * c[1] for c in channels]
    samples = []
    for t in (0, 8, 16):
        value = xs[t]
        for matrix in maps[t:]:
            value = apply(matrix, value)
        samples.append(value)
    expected_grams = {"innovation": gram(channels), "typed": [[a * b for b in typed] for a in typed], "samples": gram(samples)}
    seen = set()
    for row in read_csv(root / "moments.csv"):
        key = row["kind"], int(row["row"]), int(row["column"])
        assert key not in seen
        seen.add(key)
        name, i, j = key
        assert F(row["value"]) == expected_grams[name][i][j], f"moment mismatch {key}"
    assert len(seen) == 17 * 17 * 2 + 9
    work = read_csv(root / "work.csv")
    assert len(work) == 16 * trials
    for row in work:
        step = int(row["step"])
        old_lower, old_upper = model(n, step - 1)
        lower, upper = model(n, step)
        expected = routes(n, step)
        previous = routes(n, step - 1)
        reused = sum(key in previous and old_lower[key[1]] == lower[key[1]] and old_upper[key[0]] == upper[key[0]] for key in expected)
        products = sum(value[3] and not (key in previous and old_lower[key[1]] == lower[key[1]] and old_upper[key[0]] == upper[key[0]]) for key, value in expected.items())
        assert int(row["routes"]) == len(expected)
        assert int(row["reused_routes"]) == reused, f"reuse count {step}"
        assert int(row["updated_products"]) == products, f"product work count {step}"
        assert int(row["fresh_products"]) == sum(x[3] for x in expected.values())
        assert int(row["retained_occurrences"]) == len(lower) + len(upper)
        assert int(row["complete_status"]) == (3 if 6 <= step < 14 else 2)
        assert int(row["partial_status"]) in (0, 1)
        if step == 9:
            assert int(row["born_cells"]) == 7 and int(row["born_occurrences"]) == 18
            assert int(row["innovation_nonzeros"]) == 0
        if step == 15:
            assert int(row["removed_cells"]) == 7 and int(row["removed_occurrences"]) == 18
        if step == 16:
            assert int(row["removed_occurrences"]) == 2
        if step >= 8:
            assert int(row["max_numerator_bits"]) >= 257
    # No physical checkpoint operation may replace/reorder the canonical frames.
    for trial in range(trials):
        trial_dir = root / f"trial_{trial}"
        env = series_envelope(trial_dir / "history.rfcxs")
        assert env[0] == 6006 and len(env[3]) == 16
        assert [x[0] for x in env[4]] == [4, 8, 12, 16]
        assert env[1] == (trial_dir / "base.rfcx").read_bytes()
        assert env[2] == (trial_dir / "base.rfcxe").read_bytes()
        if trial == 0:
            for step, frame in enumerate(env[3], 1):
                assert frame == (trial_dir / f"frame_{step}.rfcxt").read_bytes()
            compact = series_envelope(trial_dir / "repacked.rfcxs")
            assert env[:4] == compact[:4]
            assert [x[0] for x in compact[4]] == [4, 12]
            assert env[4][-1][1:] == ((trial_dir / "target.rfcx").read_bytes(), (trial_dir / "target.rfcxe").read_bytes())
    for row in read_csv(root / "checkpoints.csv"):
        assert int(row["base_replays"]) == 15 and int(row["checkpoint_replays"]) == 3
        assert int(row["archive_bytes"]) > int(row["repacked_bytes"])
    for row in read_csv(root / "timings.csv"):
        assert int(row["nanoseconds"]) >= 0 and int(row["cpp_allocations"]) >= 0
        if row["operation"] in ("known_handle_10000", "packed_count_warm"):
            assert int(row["cpp_allocations"]) == 0, "warm-path allocation regression"
    return {"triangles": n, "trials": trials, "route_products_checked": checked_routes,
            "complete_blocks_checked": checked_sums, "moment_entries_checked": len(seen),
            "work_records_checked": len(work), "independent_outer_series": True,
            "all_exact": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    result = verify(args.directory)
    text = json.dumps(result, indent=2, sort_keys=True)
    if args.report:
        args.report.write_text(text + "\n")
    print(text)


if __name__ == "__main__":
    main()
