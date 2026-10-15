#!/usr/bin/env python3
"""Independent Fraction checks and genuinely truncated native temporal histories."""
import argparse
from fractions import Fraction as F
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("temporal_oracle", HERE.parent/"temporal_dynamics/verify.py")
old = importlib.util.module_from_spec(spec)
spec.loader.exec_module(old)


def verify_new(items, source_rows):
    checks = 0
    def check(value, message):
        nonlocal checks
        checks += 1
        if not value:
            raise AssertionError(message)
    def reconstruct(frame, coefficients, rows, cols):
        return old.mm(frame, coefficients) if frame and len(frame[0]) else [[F(0)]*cols for _ in range(rows)]
    native = {r["sequence"]: r for r in items if r["chapter"] == "native_vectors"}
    for seq, row in native.items():
        latest = {}
        for event in source_rows:
            if int(event["publication"]) <= seq:
                latest[event["event"]] = event
        names = row["event_names"]
        check(set(names) == {name for name,e in latest.items() if e["subtype"] != "7"}, "future or wrong-grade sample")
        values = []
        durations = []
        for name in names:
            e = latest[name]
            start,end = F(e["start"]), F(e["end"])
            assets = e["assets"].split("|")
            duration = end-start
            durations.append(duration)
            values.append([F(e["forcing"]), F(e["reading"]) if e["reading"] != "-" else F(0),
                F(e["reading"] != "-"), start/10, end/10, duration/10,
                *[F(assets.count(asset)) for asset in ("pump", "valve", "controller")]])
        check(old.m(row["values"]) == values, "named numeric view differs from admitted source")
        mean = [sum((duration*value[j] for duration,value in zip(durations,values)),F(0))/sum(durations) for j in range(9)]
        centered = [[x-y for x,y in zip(value,mean)] for value in values]
        metric = [[durations[i] if i == j else F(0) for j in range(len(names))] for i in range(len(names))]
        gram = old.mm(old.mm(old.tr(centered), metric), centered)
        check(old.v(row["weighted_mean"]) == mean and old.m(row["centered"]) == centered, "weighted mean/centering")
        check(old.m(row["metric"]) == metric and old.m(row["gram"]) == gram, "native metric pairing")
        check(row["rank"] == old.rank(centered), "exact source rank")
        distances=[[sum(((a-b)**2 for a,b in zip(x,y)),F(0)) for y in values] for x in values]
        check(old.m(row["event_squared_distances"]) == distances, "full named-view distances")
        check(reconstruct(old.m(row["frame"]),old.m(row["coefficients"]),len(names),9) == centered, "complete frame reconstruction")
        dep = old.m(row["dependencies"])
        check(all(value == 0 for ar in old.mm(centered,dep) for value in ar), "dependent coefficient kernel")
        adjacency = {j:set() for j in range(9) if gram[j][j]}
        edges = []
        for i in adjacency:
            for j in adjacency:
                if i < j and gram[i][j] > 0 and gram[i][j]**2/(gram[i][i]*gram[j][j]) >= F(3,4):
                    adjacency[i].add(j); adjacency[j].add(i)
                    edges.append((row["feature_names"][i],row["feature_names"][j],gram[i][j]**2/(gram[i][i]*gram[j][j])))
        got = [(e["a"],e["b"],F(e["squared_cosine"])) for e in row["group_edges"]]
        check(got == edges, "direct positive metric affinity edges")
        remaining = set(adjacency); groups=[]
        while remaining:
            frontier={min(remaining)}; group=set()
            while frontier:
                group |= frontier
                frontier=set().union(*(adjacency[x] for x in frontier))-group
            remaining -= group
            groups.append(frozenset(row["feature_names"][j] for j in group))
        check(set(map(frozenset,row["groups"])) == set(groups), "exact pair-graph components")
        check(set(row["constant_fields"]) == {row["feature_names"][i] for i in range(9) if gram[i][i] == 0}, "supported constant fields")
    for row in (r for r in items if r["chapter"] == "span_geometry"):
        seq = row["sequence"]; names=row["event_names"]
        latest={}
        for e in source_rows:
            if int(e["publication"]) <= seq: latest[e["event"]]=e
        spans=[(F(latest[name]["start"]),F(latest[name]["end"])) for name in names]
        points=sorted(set(x for span in spans for x in span))
        lengths=[b-a for a,b in zip(points,points[1:])]
        activity=[[F(a<=left and b>=right) for a,b in spans] for left,right in zip(points,points[1:])]
        overlap=[[max(F(0),min(b,d)-max(a,c)) for c,d in spans] for a,b in spans]
        check(old.v(row["points"]) == points and old.v(row["lengths"]) == lengths, "future partition points")
        check(old.m(row["activity_map"]) == activity and old.m(row["overlap"]) == overlap, "exact temporal pullback")
        check(row["observation_rank"] == old.rank(activity), "activity observation rank")
        check(all(x==0 for ar in old.mm(activity, old.m(row["observation_kernel"])) for x in ar), "activity observation kernel")
    for row in (r for r in items if r["chapter"] == "historical_components"):
        seq=row["sequence"]; names=row["event_names"]
        states=[]
        for cutoff in range(seq+1):
            latest={}
            for e in source_rows:
                if int(e["publication"]) <= cutoff: latest[e["event"]]=e
            states.append([F(latest[name]["forcing"]) if name in latest else F(0) for name in names])
        samples=old.tr(states)
        mean=[sum(ar,F(0))/(seq+1) for ar in samples]
        centered=[[x-y for x in ar] for ar,y in zip(samples,mean)]
        covariance=old.scale(old.mm(centered,old.tr(centered)),F(1,seq+1))
        metric=old.m(native[seq]["metric"])
        gram=old.mm(old.mm(old.tr(centered),metric),centered)
        product=old.mm(metric,covariance)
        variance=sum((product[i][i] for i in range(len(names))),F(0))
        second=sum((product[i][j]*product[j][i] for i in range(len(names)) for j in range(len(names))),F(0))
        check(old.m(row["samples"]) == samples and row["terminal_cutoff"] == seq, "historical sample transport")
        check(old.v(row["mean"]) == mean and old.m(row["centered"]) == centered, "historical sample centering")
        check(old.m(row["covariance"]) == covariance and old.m(row["sample_gram"]) == gram, "exact sample moments")
        check(F(row["variance"]) == variance and F(row["second_contraction"]) == second, "eigenfree contractions")
        check(row["rank"] == old.rank(centered), "historical independent directions")
        check(reconstruct(old.m(row["frame"]),old.m(row["coefficients"]),len(names),seq+1) == centered, "historical complete reconstruction")
        if variance:
            check(F(row["effective_dimension"]) == variance**2/second, "effective dimension")
            check(1 <= F(row["effective_dimension"]) <= row["rank"], "effective dimension rank bound")
        else:
            check(row["effective_dimension"] is None, "zero variance has no effective dimension")
    for row in (r for r in items if r["chapter"] == "principal"):
        seq=row["sequence"]; axes=[0,1,2,1,2]; amplitude=[F(2),F(1),F(1,2),F(3),F(4)]
        samples=[]
        for pair in range(min(5,seq+1)):
            a=F(0) if seq >= 5 and pair == 0 else amplitude[pair]
            for sign in (1,-1):
                value=[F(0)]*3; value[axes[pair]]=sign*a; samples.append(value)
        covariance=old.scale(old.mm(old.tr(samples),samples),F(1,len(samples)))
        variance=[covariance[i][i] for i in range(3)]
        check(old.m(row["samples"]) == samples and old.m(row["covariance"]) == covariance, "declared principal reference")
        check(old.v(row["variances"]) == variance, "principal variances")
        check(row["principal_order"] == sorted(range(3),key=lambda i:-variance[i]), "principal ordering")
        check(all(covariance[i][j]==0 for i in range(3) for j in range(3) if i!=j), "principal certificate requires diagonal covariance")
        check(row["rank"] == old.rank(samples), "all independent principal reference directions retained")
    return checks


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe",type=Path,required=True)
    parser.add_argument("--events",type=Path,default=HERE.parent/"temporal_dynamics/events.tsv")
    parser.add_argument("--report",type=Path)
    args=parser.parse_args(); exe=args.exe.resolve(); lines=args.events.read_text().splitlines()
    import csv
    rows=list(csv.DictReader(lines,delimiter="\t"))
    if {int(row["publication"]) for row in rows} != set(range(6)):
        raise ValueError("This qualification requires all six publications. Run a shorter history without --verify.")
    with tempfile.TemporaryDirectory(prefix="rfc-sharing-check-") as directory:
        root=Path(directory)
        complete=old.records(exe,args.events.resolve(),root/"complete")
        base_checks=old.verify_records(complete,rows)
        added_checks=verify_new(complete,rows)
        prefix_checks=0; controls=0
        for cutoff in range(6):
            prefix=root/f"prefix-{cutoff}.tsv"
            prefix.write_text("\n".join([lines[0],*[line for line in lines[1:] if int(line.split("\t")[0])<=cutoff]])+"\n")
            partial=old.records(exe,prefix,root/f"prefix-{cutoff}")
            historical=[old.semantic(r) for r in complete if r.get("sequence",99)<=cutoff and r["chapter"] != "tensor_product"]
            check_partial=[old.semantic(r) for r in partial if r.get("sequence",99)<=cutoff and r["chapter"] != "tensor_product"]
            if historical != check_partial: raise AssertionError(f"truncated native history differs at {cutoff}")
            prefix_checks += 1
            for seq in range(cutoff+1):
                for name in (f"database/publication-{seq}.rfcxi",f"analysis/field-groups-{seq}.rfcx"):
                    if (root/"complete"/name).read_bytes() != (root/f"prefix-{cutoff}"/name).read_bytes():
                        raise AssertionError("canonical historical native bytes differ")
            if cutoff == 5: continue
            for mode in ("value","structure"):
                altered=[lines[0]]
                for line in lines[1:]:
                    fields=line.split("\t")
                    if int(fields[0])>cutoff:
                        fields[7]="37/3"
                        if fields[8]!="-": fields[8]="41/7"
                        if mode == "structure":
                            fields[9]+="|future-asset"
                            if cutoff < 2 and fields[2] == "inspection":
                                fields[2],fields[5],fields[6]="future-inspection","1/2","7/2"
                            if cutoff == 2 and fields[4] in ("5","6","7"):
                                fields[6]="25"
                            if cutoff == 3 and fields[4] == "3":
                                fields[6]="18"
                    altered.append("\t".join(fields))
                changed=root/f"changed-{cutoff}-{mode}.tsv"; changed.write_text("\n".join(altered)+"\n")
                result=old.records(exe,changed,root/f"changed-{cutoff}-{mode}")
                if [old.semantic(r) for r in result if r.get("sequence",99)<=cutoff and r["chapter"] != "tensor_product"] != historical:
                    raise AssertionError(f"future {mode} leakage at {cutoff}")
                controls+=1
    report={"status":"passed","original_fraction_assertions":base_checks,"new_fraction_assertions":added_checks,
        "native_assertions":complete[-1]["checks"],"truncated_histories":prefix_checks,"nonempty_future_controls":controls,
        "arithmetic":"Fraction oracle and native GMP rational arithmetic; no eigensolving in native analyses"}
    if args.report:
        args.report.parent.mkdir(parents=True,exist_ok=True); args.report.write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps(report,indent=2))


if __name__ == "__main__":
    main()
