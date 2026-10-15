#!/usr/bin/env python3
"""Validate the optional numerical observations and rerun their historical fits."""
import argparse
import copy
from fractions import Fraction
import hashlib
import json
from pathlib import Path
import numpy as np
from reference import evaluate


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory",type=Path)
    parser.add_argument("--report",type=Path,required=True)
    args=parser.parse_args()
    records=json.loads((args.directory/"observations.json").read_text())
    measured=json.loads((args.directory/"references.json").read_text())
    checks=0
    def check(value,message):
        nonlocal checks
        checks+=1
        if not value: raise AssertionError(message)
    def close(a,b,message):
        check(np.allclose(np.asarray(a,dtype=float),np.asarray(b,dtype=float),rtol=1e-10,atol=1e-10),message)
    def same_model(a,b):
        check(a["input_sha256"]==b["input_sha256"] and a["fit_cutoff"]==b["fit_cutoff"],"fit source witness changed")
        if a["pca"] is not None:
            for key in ("components","mean","coordinates","explained_variance","explained_variance_ratio","discarded_squared_error"):
                close(a["pca"][key],b["pca"][key],"historical PCA observation changed")
        check(len(a["umap"])==len(b["umap"]),"historical UMAP population changed")
        for x,y in zip(a["umap"],b["umap"]):
            check(x["seed"]==y["seed"],"UMAP seed changed")
            close(x["coordinates"],y["coordinates"],"historical UMAP observation changed")
    for model in measured["models"]:
        seq=model["sequence"]
        native=next(r for r in records if r["chapter"]==model["chapter"] and r.get("sequence")==seq)
        check(model["fit_cutoff"]==seq,"fit admitted a different cutoff")
        witness=model["input_witness"]
        check(witness["source_namespace"]==native["source_namespace"] and witness["carrier"]==native["carrier"],"reference source carrier")
        digest=hashlib.sha256(json.dumps(witness,sort_keys=True,separators=(",", ":")).encode()).hexdigest()
        check(digest==model["input_sha256"],"selected input hash")
        raw=native["values"] if native["chapter"]=="native_vectors" else native["samples"]
        check(witness["values"]==raw,"selected input values")
        data=np.array([[float(Fraction(x)) for x in r] for r in raw])
        if model["pca"]:
            p=model["pca"]; components=np.array(p["components"]);coordinates=np.array(p["coordinates"])
            close(p["mean"],data.mean(axis=0),"PCA mean from selected data")
            close(coordinates,(data-np.array(p["mean"]))@components.T,"PCA component coordinates")
            residual=data-(coordinates@components+np.array(p["mean"]))
            close(p["discarded_squared_error"],float(np.sum(residual**2)),"PCA retained reconstruction error")
            if native["chapter"]=="principal":
                expected=[float(Fraction(native["variances"][i]))*len(data)/(len(data)-1) for i in native["principal_order"][:len(components)]]
                close(p["explained_variance"],expected,"PCA reference versus exact diagonal certificate")
                covariance=np.array([[float(Fraction(x)) for x in r] for r in native["covariance"]])*len(data)/(len(data)-1)
                for direction,variance in zip(components,p["explained_variance"]):
                    close(covariance@direction,variance*direction,"exact-case principal equation")
        for umap in model["umap"]:
            check(seq>=2 and len(data)>=4,"UMAP admitted a future observation")
            check(len(umap["coordinates"])==len(data) and np.isfinite(np.array(umap["coordinates"])).all(),"UMAP point population")
    # Refit each final member of a genuinely truncated record list.
    for cutoff in range(6):
        prefix=[r for r in records if r.get("sequence",99)<=cutoff]
        last=[r for r in prefix if r.get("sequence")==cutoff]
        fresh=evaluate(last)
        for model in fresh["models"]:
            original=next(r for r in measured["models"] if r["chapter"]==model["chapter"] and r["sequence"]==cutoff)
            same_model(original,model)
    poisoned=copy.deepcopy(records)
    for row in poisoned:
        if row.get("sequence",0)>2 and row["chapter"] in ("native_vectors","principal"):
            key="values" if row["chapter"]=="native_vectors" else "samples"
            row[key]=[["173/19" for x in values] for values in row[key]]
            row["source_namespace"]+=9000
    first=evaluate([r for r in poisoned if r.get("sequence",99)<=2])
    for model in first["models"]:
        original=next(r for r in measured["models"] if r["chapter"]==model["chapter"] and r["sequence"]==model["sequence"])
        same_model(original,model)
    ordinary=[r for r in measured["models"] if r["chapter"]=="native_vectors"]
    a,b=ordinary[4],ordinary[5]
    check(a["input_witness"]["values"]==b["input_witness"]["values"],"final closure control needs equal ordinary data")
    for x,y in zip(a["umap"],b["umap"]):close(x["coordinates"],y["coordinates"],"same UMAP values at closure")
    close(a["pca"]["coordinates"],b["pca"]["coordinates"],"same PCA values at closure")
    f4=next(r for r in records if r["chapter"]=="field" and r.get("sequence")==4)
    f5=next(r for r in records if r["chapter"]=="field" and r.get("sequence")==5)
    check(f4["sector_dimensions"]!=f5["sector_dimensions"],"native higher relation must change sector structure")
    report={"status":"passed","assertions":checks,"models":len(measured["models"]),
        "historical_refits":6,"poisoned_future_cutoff":2,"closure_control":True,
        "tolerance":{"relative":1e-10,"absolute":1e-10},"versions":measured["versions"],
        "scope":"This host's numerical observations; exact native results are verified separately"}
    args.report.write_text(json.dumps(report,indent=2)+"\n");print(json.dumps(report,indent=2))


if __name__=="__main__":main()
