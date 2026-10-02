#!/usr/bin/env python3
"""CLI wall-clock profile plus explicit cache-line/transition COST MODELS.
No cache-miss, transistor-voltage, or energy counters are inferred from timings.
"""
from __future__ import annotations
import argparse, csv, json, platform, shutil, statistics, subprocess, tempfile, time
from fractions import Fraction
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("executable", type=Path)
p.add_argument("output", type=Path)
p.add_argument("--trials", type=int, default=5)
args = p.parse_args()
if not 3 <= args.trials <= 11:
    p.error("trials must be 3..11")
args.output.mkdir(exist_ok=False, parents=True)
exe = str(args.executable.resolve())
rows = []

def run(name: str, trial: int, *command: object) -> bytes:
    start = time.perf_counter_ns()
    done = subprocess.run([exe, *map(str, command)], capture_output=True, timeout=180)
    elapsed = (time.perf_counter_ns()-start)/1e6
    if done.returncode:
        raise RuntimeError((command, done.stdout, done.stderr))
    if trial >= 0:
        rows.append(dict(operation=name, trial=trial, ms=elapsed))
    return done.stdout

with tempfile.TemporaryDirectory(prefix="rfcdb-profile-") as root:
    tmp = Path(root)
    run("prepare", -1, "sample", tmp/"sample.db", 9)
    run("prepare", -1, "pack", tmp/"sample.db", tmp/"sample.rfcxdb")
    original = (tmp/"sample.rfcxdb").read_bytes()
    for trial in range(args.trials):
        run("packed_open_full_validation", trial, "verify", tmp/"sample.rfcxdb")
        run("directory_open_full_validation", trial, "verify", tmp/"sample.db")
        run("cold_process_field_at_9", trial, "field", tmp/"sample.rfcxdb", 9, 10, 10)
        run("read_directory_write_packed", trial, "pack", tmp/"sample.db", tmp/f"packed-{trial}")
        assert (tmp/f"packed-{trial}").read_bytes() == original
        run("read_packed_write_directory", trial, "unpack", tmp/"sample.rfcxdb", tmp/f"directory-{trial}")
        run("repack-check", -1, "pack", tmp/f"directory-{trial}", tmp/f"checked-{trial}")
        assert (tmp/f"checked-{trial}").read_bytes() == original
    size = len(original)

with (args.output/"latency.csv").open("w") as f:
    w = csv.DictWriter(f, fieldnames=["operation", "trial", "ms"])
    w.writeheader();w.writerows(rows)
summary = {}
for operation in sorted({r["operation"] for r in rows}):
    xs = [r["ms"] for r in rows if r["operation"] == operation]
    summary[operation] = dict(median_ms=statistics.median(xs), min_ms=min(xs), max_ms=max(xs), trials=len(xs))

# Explicit model: 1,024 eight-byte records, aligned base, 64-byte cache lines.
# Query hyperedges are the 64 disjoint families of 16 related records. P is a
# bijection from retained ordinal to physical slot, not a change of identity.
n = 1024
clustered = list(range(n))
interleaved = [(i % 16)*64+i//16 for i in range(n)]
assert sorted(interleaved) == clustered
values = [Fraction((i % 31)-15, (i % 13)+1) for i in range(n)]
scattered_values = [Fraction(0)]*n
for logical, physical in enumerate(interleaved):
    scattered_values[physical] = values[logical]
line_counts = []
for group in range(64):
    support = list(range(16*group, 16*group+16))
    x = sum((values[i] for i in support), Fraction(0))
    y = sum((scattered_values[interleaved[i]] for i in support), Fraction(0))
    assert x == y
    a = len({8*clustered[i]//64 for i in support})
    b = len({8*interleaved[i]//64 for i in support})
    line_counts.append((a, b))
assert set(line_counts) == {(2, 16)}
# The source query is exactly preserved; traffic estimate does not assert a
# cache-miss count on any particular CPU, prefetcher, or working set.
line_model = dict(records=n, record_bytes=8, line_bytes=64, queries=64,
                  clustered_lines_per_query=2, interleaved_lines_per_query=16,
                  exact_rational_query_equalities=64, hardware_misses_measured=False)
transition_model = dict(states=16,
    ordinal_code_bit_flips=sum((i^(i+1)).bit_count() for i in range(15)),
    reflected_code_bit_flips=sum(((i^(i>>1))^((i+1)^((i+1)>>1))).bit_count() for i in range(15)),
    statement="Known finite-code example, not claimed novelty or physical-energy measurement")
assert transition_model["ordinal_code_bit_flips"] == 26 and transition_model["reflected_code_bit_flips"] == 15
power = []
try:
    power = [str(x) for x in Path('/sys/class/powercap').glob('**/energy_uj')]
except OSError:
    pass
report = dict(platform=platform.platform(), machine=platform.machine(), database_bytes=size,
    timings=summary, cache_line_model=line_model, transition_model=transition_model,
    perf_executable=shutil.which('perf'), available_energy_paths=power,
    limitations="CLI measurements include subprocess startup, native reconstruction/validation and output. OS cache is not evicted; CPU affinity/load is uncontrolled; no energy/voltage/cache-event counters collected; physical write persistence beyond fsync not certified.")
(args.output/"profile.json").write_text(json.dumps(report, indent=2)+"\n")
print(json.dumps(report, indent=2))
