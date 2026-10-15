#!/usr/bin/env python3
"""Independent exact checks for the native temporal demonstration.

Fraction elimination below is a test oracle. The demonstration itself calls
the existing C++ native query, field, temporal, product and storage APIs.
"""
import argparse
import csv
from fractions import Fraction as F
import json
from pathlib import Path
import subprocess
import tempfile


def v(values):
    return [F(x) for x in values]


def m(values):
    return [v(row) for row in values]


def eye(n):
    return [[F(i == j) for j in range(n)] for i in range(n)]


def tr(a):
    return [list(col) for col in zip(*a)]


def add(a, b):
    return [[x + y for x, y in zip(ar, br)] for ar, br in zip(a, b)]


def scale(a, q):
    return [[x * q for x in row] for row in a]


def mv(a, x):
    return [sum((p * q for p, q in zip(row, x)), F(0)) for row in a]


def mm(a, b):
    return [[sum((x * y for x, y in zip(row, col)), F(0)) for col in tr(b)] for row in a]


def solve(a, y):
    n = len(a)
    rows = [list(row) + [rhs] for row, rhs in zip(a, y)]
    for j in range(n):
        p = next(i for i in range(j, n) if rows[i][j])
        rows[j], rows[p] = rows[p], rows[j]
        q = rows[j][j]
        rows[j] = [x / q for x in rows[j]]
        for i in range(n):
            if i != j:
                q = rows[i][j]
                rows[i] = [x - q * z for x, z in zip(rows[i], rows[j])]
    return [row[-1] for row in rows]


def rank(a):
    if not a:
        return 0
    a = [row[:] for row in a]
    i = 0
    for j in range(len(a[0])):
        p = next((k for k in range(i, len(a)) if a[k][j]), None)
        if p is None:
            continue
        a[i], a[p] = a[p], a[i]
        q = a[i][j]
        a[i] = [x / q for x in a[i]]
        for k in range(i + 1, len(a)):
            q = a[k][j]
            a[k] = [x - q * y for x, y in zip(a[k], a[i])]
        i += 1
        if i == len(a):
            break
    return i


def project(frame, metric, x):
    basis = []
    for column in tr(frame):
        candidate = basis + [column]
        if rank(tr(candidate)) > len(basis):
            basis.append(column)
    if not basis:
        return [F(0)] * len(x)
    a = tr(basis)
    c = solve(mm(tr(a), mm(metric, a)), mv(tr(a), mv(metric, x)))
    return mv(a, c)


def pairing(x, metric, y):
    return sum((a * b for a, b in zip(x, mv(metric, y))), F(0))


def minus(a, b):
    return [x - y for x, y in zip(a, b)]


def invoke(exe, *args, ok=True):
    result = subprocess.run([str(exe), *map(str, args)], text=True, capture_output=True, timeout=120)
    if (result.returncode == 0) != ok:
        raise AssertionError(f"command exit {result.returncode}: {result.stderr}")
    return result


def records(exe, events, directory):
    result = invoke(exe, "run", events, directory)
    return [json.loads(line) for line in result.stdout.splitlines()]


def semantic(record):
    return {k: x for k, x in record.items() if not k.endswith("_ns")}


def verify_records(items, input_rows):
    checks = 0

    def check(condition, description):
        nonlocal checks
        if not condition:
            raise AssertionError(description)
        checks += 1

    db = {x["sequence"]: x for x in items if x["chapter"] == "database"}
    fields = {x["sequence"]: x for x in items if x["chapter"] == "field"}
    latest = {}
    for seq, d in db.items():
        for row in input_rows:
            if int(row["publication"]) == seq:
                latest[row["event"]] = row
        events = {e["event"]: e for e in d["events"]}
        check(set(events) == set(latest), "snapshot imports a future identity")
        check(all(e["first_known_or_corrected"] <= seq for e in events.values()), "future revision in snapshot")
        for name, row in latest.items():
            e = events[name]
            check(F(e["start"]) == F(row["start"]) and F(e["end"]) == F(row["end"]), "time span changed")
            check(F(e["forcing"]) == F(row["forcing"]), "forcing changed")
            check(e["reading"] == (None if row["reading"] == "-" else str(F(row["reading"]))), "nullable reading changed")
            assets = row["assets"].split("|")
            check(e["retained_asset_occurrences"] == len(assets), "repeated or inactive occurrence lost")
            check(e["active_asset_occurrences"] == sum(not a.startswith("!") for a in assets), "inactive role interpreted as active")
            check(e["tensor_support"] == (2 if row["reading"] == "-" else 3), "tensor support confused with a value")
            nonzeros = int(F(row["forcing"]) != 0) + int(row["reading"] != "-" and F(row["reading"]) != 0)
            check(e["tensor_nonzeros"] == nonzeros, "supported zero lost")
        retained = active = 0
        for a in latest.values():
            for b in latest.values():
                for x in a["assets"].split("|"):
                    for y in b["assets"].split("|"):
                        if x.lstrip("!") == y.lstrip("!"):
                            retained += 1
                            active += int(not x.startswith("!") and not y.startswith("!"))
        check(d["retained_join_matches"] == retained and d["active_join_matches"] == active, "role join collapsed multiplicity")
        q = next(x for x in items if x["chapter"] == "query" and x["sequence"] == seq)
        selected = sorted(name for name, row in latest.items() if F(row["start"]) < 15 and F(row["end"]) > 5)
        readings = [F(latest[name]["reading"]) for name in selected if latest[name]["reading"] != "-"]
        check(sorted(q["events"]) == selected and q["selected"] == len(selected), "native overlap query differs")
        check(q["numeric_contributions"] == len(readings), "nullable sum counts absence")
        check(q["reading_sum"] == (str(sum(readings, F(0))) if readings else None), "native sum differs")
        check(q["cache_hit"] and q["cutoff"] == seq and q["source_sequence"] == seq, "cache or cutoff differs")
        for group in q["groups"]:
            members = sorted(name for name in selected if int(latest[name]["category"]) == group["category"])
            scalars = [F(latest[name]["reading"]) for name in members if latest[name]["reading"] != "-"]
            check(sorted(group["member_events"]) == members and F(group["count"]) == len(members), "native group membership differs")
            check(group["numeric_contributions"] == len(scalars) and group["reading_sum"] == (str(sum(scalars, F(0))) if scalars else None), "native group sum differs")
        f = fields[seq]
        names = f["relations"]
        check(set(names) == {name for name, r in latest.items() if int(r["subtype"]) != 7}, "field imports undeclared relations")
        check(len(set(link["chain_key"] for link in f["declared_record_links"])) == len(names), "relation keys collapsed")
        for link in f["declared_record_links"]:
            check(link["database_key"] == events[link["event"]]["key"] == link["chain_key"], "declared endpoint link differs")
            check(link["database_namespace"] == d["namespace"] and link["field_namespace"] == f["namespace"], "link owner differs")
        b, c, metric, x = m(f["boundary_matrix"]), m(f["higher_boundary"]), m(f["metric"]), v(f["forcing"])
        check(x == [F(latest[name]["forcing"]) for name in names], "field input differs")
        check(metric == [[F(latest[name]["end"]) - F(latest[name]["start"]) if i == j else F(0) for j in range(len(names))] for i, name in enumerate(names)], "duration metric differs")
        if "time_coordinates" in f:
            time = v(f["time_coordinates"])
            expected = [[F(int(t == F(latest[name]["end"])) - int(t == F(latest[name]["start"]))) for name in names] for t in time]
            check(b == expected, "realized boundary differs from declared spans")
            source_subjects={F(x["minute"]):x["subject_key"] for x in d["time_points"]}
            check(set(source_subjects) == {F(r[k]) for r in latest.values() for k in ("start","end")}, "future time point entered the database")
            for link in f["declared_time_links"]:
                check(link["database_key"] == source_subjects[F(link["minute"])], "time point comparison lost its actual database subject")
                check(link["database_namespace"] == d["namespace"] and link["field_namespace"] == f["namespace"], "time point link owner differs")
        check(all(all(q == 0 for q in row) for row in mm(b, c)), "chain condition fails")
        invmetric = [[1 / metric[i][i] if i == j else F(0) for j in range(len(names))] for i in range(len(names))]
        grad_frame = mm(invmetric, tr(b))
        curl_l = mm(c, mm(tr(c), metric)) if c and c[0] else [[F(0)]*len(names) for _ in names]
        l = add(mm(grad_frame, b), curl_l)
        check(m(f["hodge_operator"]) == l, "weighted Hodge action differs")
        response = solve(add(eye(len(names)), l), x)
        check(v(f["response"]) == response, "Green response differs from independent rational solve")
        check(v(f["boundary_forcing"]) == mv(b, x), "boundary action differs")
        gradient, curl = project(grad_frame, metric, x), project(c, metric, x)
        harmonic = minus(minus(x, gradient), curl)
        check(v(f["gradient"]) == gradient and v(f["curl"]) == curl and v(f["harmonic"]) == harmonic, "Hodge sectors differ")
        check(f["sector_dimensions"] == [rank(b), rank(c), len(names) - rank(b) - rank(c)], "sector dimensions differ")
        check(F(f["response_quadrance"]) == pairing(response, metric, response), "response pairing differs")
        binary = next(x for x in items if x["chapter"] == "binary" and x["sequence"] == seq)
        check([binary[k] for k in ["occurrences", "active_negative", "active_positive", "inactive_retained", "active_zero"]] == [6*len(names), 2*len(names), 3*len(names), len(names), len(names)], "composite binary masks differ")
        fibers = next(x for x in items if x["chapter"] == "fibers" and x["sequence"] == seq)
        expected_fiber = []
        for row in b:
            expected_fiber.extend([sum((q * a for q, a in zip(row, x)), F(0)), sum((q * metric[j][j] for j, q in enumerate(row)), F(0))])
        check(v(fibers["boundary_values"]) == expected_fiber, "constant fiber action differs")
        check(fibers["source_sequence"] == seq and fibers["warm_result_hit"], "stalk cache owner differs")
        for weighted in (False, True):
            variation = next(x for x in items if x["chapter"] == "variation" and x["sequence"] == seq and x["metric"] == ("duration" if weighted else "unit"))
            weights = [metric[i][i] if weighted else F(1) for i in range(len(x))]
            mean = sum((a*q for a, q in zip(x, weights)), F(0)) / sum(weights)
            cats = [latest[name]["category"] for name in names]
            group_mean = [sum((a*q for a, q, cat in zip(x, weights, cats) if cat == c0), F(0))/sum(q for q, cat in zip(weights, cats) if cat == c0) for c0 in cats]
            between, within = [a - mean for a in group_mean], minus(x, group_mean)
            ssb = sum((q*a*a for q, a in zip(weights, between)), F(0))
            ssw = sum((q*a*a for q, a in zip(weights, within)), F(0))
            check(F(variation["global_mean"]) == mean and v(variation["group_mean_field"]) == group_mean, "typed group mean differs")
            check(F(variation["between_quadrance"]) == ssb and F(variation["within_quadrance"]) == ssw and F(variation["total_quadrance"]) == ssb+ssw, "variation pairing differs")
            check(variation["statistical_test"] is None, "deterministic identity relabeled as inference")
        chronology = next(x for x in items if x["chapter"] == "chronology" and x["sequence"] == seq)
        check(v(chronology["state"]) == ([F(1), F(0), F(0)] if seq == 0 else [F(0), F(1), F(0)] if seq < 3 else [F(0), F(0), F(1)]), "chronology used later contacts")
    for t in (x for x in items if x["chapter"] == "transition"):
        old, new = fields[t["old_sequence"]], fields[t["sequence"]]
        j = m(t["correspondence"])
        expected = [[F(a["chain_key"] == b["chain_key"]) for b in old["declared_record_links"]] for a in new["declared_record_links"]]
        check(j == expected, "rectangular comparison maps equal values instead of named identities")
        lo, ln, metric = m(old["hodge_operator"]), m(new["hodge_operator"]), m(new["metric"])
        eta = minus(v(new["forcing"]), mv(j, v(old["forcing"])))
        innovation = solve(add(eye(len(ln)), ln), eta)
        operator = solve(add(eye(len(ln)), ln), [-q for q in mv(add(mm(ln, j), scale(mm(j, lo), -1)), v(old["response"]))])
        delta = minus(v(new["response"]), mv(j, v(old["response"])))
        check(v(t["realization"]) == [F(0)] * len(delta), "identity injection acquired a realization defect")
        check(v(t["innovation"]) == innovation and v(t["operator"]) == operator and v(t["delta"]) == delta, "finite Green attribution differs")
        channels = [v(t[k]) for k in ("realization", "innovation", "operator")]
        check(m(t["channel_gram"]) == [[pairing(a, metric, b) for b in channels] for a in channels], "interaction Gram differs")
        check(F(t["quadrance"]) == pairing(delta, metric, delta), "delta quadrance differs")
        elapsed = db[t["sequence"]]["available_minute"] - db[t["old_sequence"]]["available_minute"]
        check(v(t["rate"]) == [q / elapsed for q in delta], "availability rate differs")
    for a in (x for x in items if x["chapter"] == "accumulation"):
        seq = a["sequence"]
        transported = v(fields[0]["response"])
        channels = [transported]
        for k in range(1, seq+1):
            t = next(t for t in items if t["chapter"] == "transition" and t["sequence"] == k)
            channels = [mv(m(t["correspondence"]), c) for c in channels] + [v(t["delta"])]
        metric = m(fields[seq]["metric"])
        check(v(a["terminal"]) == v(fields[seq]["response"]), "terminal reconstruction differs")
        check(m(a["gram"]) == [[pairing(x, metric, y) for y in channels] for x in channels], "multi-time cross terms lost")
        check(a["terminal_cutoff"] == seq, "terminal analysis attributed to an older endpoint")
    pr = next(x for x in items if x["chapter"] == "pagerank")
    check(v(pr["rank"]) == [F(5,9), F(1,3), F(1,9)] == v(pr["green_response"]), "reversible ranking reference differs")
    text = next(x for x in items if x["chapter"] == "textrank")
    check(v(text["rank"]) == [F(3367,9432), F(3176,9432), F(2889,9432)], "TextRank reference differs")
    check(v(text["rank"]) == solve(add(eye(3), scale(m(text["transition"]), F(-1,2))), [F(1,6)]*3), "TextRank residual fails")
    p = m(text["retained_sentence_terms"])
    check([sum(column) for column in tr(p)] == [2,4,8], "declared sentence lengths differ")
    finite = next(x for x in items if x["chapter"] == "finite_interaction")
    a, b, da, db0 = [m(finite[k]) for k in ("old_first", "old_second", "delta_first", "delta_second")]
    check(m(finite["interaction"]) == mm(db0, da), "finite mixed term lost")
    check(m(finite["complete_delta"]) == add(mm(add(b, db0), add(a, da)), scale(mm(b,a), -1)), "finite word expansion differs")
    phase = next(x for x in items if x["chapter"] == "phase")
    coords = [v(x) for x in phase["coordinates"]]
    for a, b in zip(coords, coords[1:]):
        check(b == [-a[1], a[0] + a[1]], "algebraic phase recurrence differs")
    check(coords[0] == coords[6] == [F(1), F(0)], "phase period differs")
    for product in (x for x in items if x["chapter"] == "tensor_product"):
        b = m(fields[product["sequence"]]["boundary_matrix"])
        expected = {(0, i, 1, 0): row[0] for i, row in enumerate(b) if row[0]}
        expected.update({(1, 0, 0, 0): F(1), (1, 0, 0, 1): F(-1)})
        actual = {(e["time_grade"], e["time_index"], e["asset_grade"], e["asset_index"]): F(e["value"]) for e in product["boundary"]}
        check(actual == expected and product["retained_occurrences"] == 8, "native graded product boundary differs")
        check(product["supported_second_boundary"] == 4 and product["boundary_squared_zero"], "native product support collapsed cancellation")
    for x in (x for x in items if x["chapter"] == "isolation"):
        check(x["future_rejected"], "future input admitted")
    check(items[-1]["status"] == "passed", "native verification incomplete")
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--events", type=Path, default=Path(__file__).with_name("events.tsv"))
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    exe, events = args.exe.resolve(), args.events.resolve()
    with events.open(newline="") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    lines = events.read_text().splitlines()
    with tempfile.TemporaryDirectory(prefix="rfc-temporal-check-") as directory:
        root = Path(directory)
        complete = records(exe, events, root/"complete")
        checks = verify_records(complete, rows)
        for cutoff in range(6):
            prefix = root/f"prefix-{cutoff}.tsv"
            prefix.write_text("\n".join([lines[0], *[line for line in lines[1:] if int(line.split("\t")[0]) <= cutoff]]) + "\n")
            before = records(exe, prefix, root/f"prefix-{cutoff}")
            scoped = lambda items: [semantic(r) for r in items if r.get("sequence", cutoff+1) <= cutoff and r["chapter"] != "tensor_product"]
            if scoped(complete) != scoped(before):
                raise AssertionError(f"historical prefix {cutoff} differs from complete execution")
            checks += verify_records(before, [row for row in rows if int(row["publication"]) <= cutoff])
            for seq in range(cutoff+1):
                for name in (f"database/publication-{seq}.rfcxi", *([f"fields/transition-{seq}.rfcxt", f"database/transition-{seq}.rfcxiu"] if seq else [])):
                    if (root/"complete"/name).read_bytes() != (root/f"prefix-{cutoff}"/name).read_bytes():
                        raise AssertionError(f"later records changed native bytes: {name}")
                    checks += 1
            poison = root/f"poison-{cutoff}.tsv"
            altered = [lines[0]]
            for line in lines[1:]:
                cells = line.split("\t")
                if int(cells[0]) > cutoff:
                    cells[7] = "123456789/97"
                    if cells[8] != "-":
                        cells[8] = "-999999999/101"
                altered.append("\t".join(cells))
            poison.write_text("\n".join(altered)+"\n")
            later = records(exe, poison, root/f"poison-{cutoff}")
            if scoped(complete) != scoped(later):
                raise AssertionError(f"future values leaked into prefix {cutoff}")
            checks += 1
            structure = root/f"structure-{cutoff}.tsv"
            altered = [lines[0]]
            for line in lines[1:]:
                cells = line.split("\t")
                if int(cells[0]) > cutoff:
                    cells[9] += "|future-asset"
                    if cells[4] == "4":
                        cells[2], cells[5], cells[6] = "future-inspection", "1/2", "7/2"
                altered.append("\t".join(cells))
            structure.write_text("\n".join(altered)+"\n")
            later = records(exe, structure, root/f"structure-{cutoff}")
            if scoped(complete) != scoped(later):
                raise AssertionError(f"future identities, roles or time points leaked into prefix {cutoff}")
            for seq in range(cutoff+1):
                path=f"database/publication-{seq}.rfcxi"
                if (root/"complete"/path).read_bytes() != (root/f"structure-{cutoff}"/path).read_bytes():
                    raise AssertionError("future structure changed historical native bytes")
                checks += 1
            checks += 1
        for seq in range(6):
            queried = json.loads(invoke(exe, "query", root/"complete/database"/f"publication-{seq}.rfcxi", seq, 5, 15, "--exact").stdout)
            expected = next(x for x in complete if x["chapter"] == "query" and x["sequence"] == seq)
            if queried != expected:
                raise AssertionError("standalone native query differs")
            checks += 1
        invoke(exe, "query", root/"complete/database/publication-5.rfcxi", 0, 5, 15, "--exact", ok=False)
        invoke(exe, "query", root/"complete/database/publication-0.rfcxi", 0, 15, 5, "--exact", ok=False)
        invoke(exe, "run", events, root/"complete", ok=False)
        bad = root/"corrupt.rfcxi"
        bad.write_bytes((root/"complete/database/publication-0.rfcxi").read_bytes()[:-1])
        invoke(exe, "query", bad, 0, 5, 15, "--exact", ok=False)
        checks += 4
        broken=root/"broken-chain.tsv"
        broken.write_text(events.read_text().replace("3\t30\tepisode\t1\t6\t0\t30", "3\t30\tepisode\t1\t6\t1\t30"))
        rejected=invoke(exe,"run",broken,root/"broken-chain",ok=False)
        if "adjacent chain axiom violated" not in rejected.stderr:
            raise AssertionError("invalid higher boundary did not reject through the chain check")
        checks += 1
        report = {"status": "passed", "independent_exact_checks": checks,
                  "historical_prefixes": 6, "future_suffix_controls": 10,
                  "future_value_controls": 5, "future_structure_controls": 5,
                  "terminal_empty_suffix_controls": 2,
                  "native_checks": complete[-1]["checks"],
                  "snapshots": 6, "arithmetic": "Fraction oracle and native GMP rationals",
                  "scope": "finite declared cooling incident and separate exact reference cases"}
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
