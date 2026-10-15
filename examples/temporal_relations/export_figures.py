#!/usr/bin/env python3
"""Standalone figures from completed observations, with explicit temporal cutoffs."""
from fractions import Fraction
import json
from pathlib import Path


def export(directory):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.backends.backend_pdf import PdfPages
    import numpy as np

    root=Path(directory); figures=root/"figures"; figures.mkdir(exist_ok=False)
    scenario=json.loads((root/"scenario.json").read_text()) if (root/"scenario.json").exists() else {}
    vocabulary=scenario.get("vocabulary",{})
    label=lambda name:vocabulary.get(name,name.capitalize())
    records=json.loads((root/"observations.json").read_text())
    sequences=[r["sequence"] for r in records if r["chapter"]=="database"]
    terminal=max(sequences)
    references=json.loads((root/"references.json").read_text()) if (root/"references.json").exists() else None
    get=lambda kind,sequence:next(r for r in records if r["chapter"]==kind and r.get("sequence")==sequence)
    number=lambda value:float(Fraction(value))
    plt.rcParams.update({"font.size":10,"axes.spines.top":False,"axes.spines.right":False,
        "axes.labelcolor":"#20272e","text.color":"#20272e","axes.edgecolor":"#768493",
        "figure.facecolor":"white","savefig.facecolor":"white","font.family":"DejaVu Sans"})
    produced=[]
    with PdfPages(figures/"figures.pdf",metadata={"Title":"Relational complexes in temporal biomedical research",
        "Subject":"Exact native computations with separately declared numerical PCA and UMAP display observations"}) as book:
        def save(fig,name,cutoff,description):
            fig.savefig(figures/(name+".svg"),bbox_inches="tight")
            fig.savefig(figures/(name+".pdf"),bbox_inches="tight")
            book.savefig(fig,bbox_inches="tight");plt.close(fig)
            produced.append({"name":name,"cutoff":cutoff,"description":description,
                "geometry":"display coordinates derived from completed observations"})
        for sequence in sequences:
            source=get("database",sequence); spans=get("span_geometry",sequence)
            events=[e for e in source["events"] if e["subtype"]!=7]
            fig,axes=plt.subplots(1,2,figsize=(12,4.7),layout="constrained")
            ax=axes[0]
            for i,e in enumerate(events):
                ax.barh(i,number(e["end"])-number(e["start"]),left=number(e["start"]),height=.55,color="#66839e")
            ax.set_yticks(range(len(events)),[label(e["event"]) for e in events]);ax.invert_yaxis()
            ax.set_xlabel("Experimental time (minutes)");ax.set_title("Admitted time spans")
            overlap=np.array([[number(x) for x in r] for r in spans["overlap"]])
            ax=axes[1];image=ax.imshow(overlap,cmap="Blues",vmin=0)
            ax.set_xticks(range(len(events)),[label(n) for n in spans["event_names"]],rotation=45,ha="right")
            ax.set_yticks(range(len(events)),[label(n) for n in spans["event_names"]])
            for i in range(len(events)):
                for j in range(len(events)):
                    ax.text(j,i,spans["overlap"][i][j],ha="center",va="center",
                        color="white" if overlap[i,j]>.6*max(overlap.max(),1) else "#20272e",fontsize=9)
            fig.colorbar(image,ax=ax,label="Exact overlap (minutes)",shrink=.85)
            ax.set_title("Temporal overlap")
            fig.suptitle(f"Publication {sequence}: available at minute {source['available_minute']}",fontsize=14)
            save(fig,f"time-spans-{sequence}",sequence,"Time spans and exact overlap pullback; no later endpoints")
            if references:
                ref=next(r for r in references["models"] if r["chapter"]=="native_vectors" and r["sequence"]==sequence)
                if not ref["pca"]: continue
                models=[("PCA, two feature directions",ref["pca"]["coordinates"])]+[(f"UMAP, seed {x['seed']}",x["coordinates"]) for x in ref["umap"]]
                fig,axes=plt.subplots(1,len(models),figsize=(4.3*len(models),4.5),layout="constrained",squeeze=False)
                for ax,(title,coordinates) in zip(axes[0],models):
                    xy=np.array([p+[0] if len(p)==1 else p for p in coordinates])
                    ax.scatter(xy[:,0],xy[:,1],s=35,color="#355b7c")
                    for name,p in zip(ref["names"],xy):ax.annotate(label(name),p,xytext=(4,5),textcoords="offset points",fontsize=8)
                    ax.set_xlabel("Fitted display coordinate 1");ax.set_ylabel("Fitted display coordinate 2");ax.set_title(title)
                fig.suptitle(f"Numerical references fitted only at publication {sequence}",fontsize=13)
                save(fig,f"reference-displays-{sequence}",sequence,"Actual separately fitted float64 PCA/UMAP observations; not cluster or density certificates")
        field=get("field",terminal);names=field["relations"];x=np.arange(len(names))
        fig,ax=plt.subplots(figsize=(10,5),layout="constrained")
        for index,(name,color) in enumerate(zip(("gradient","curl","harmonic"),("#355b7c","#879aac","#c6d1db"))):
            ax.bar(x+(index-1)*.22,[number(v) for v in field[name]],width=.22,label=name.capitalize(),color=color)
        ax.set_xticks(x,[label(n) for n in names],rotation=20,ha="right");ax.set_ylabel("Field value");ax.legend();ax.axhline(0,color="#bac2cc",linewidth=.7)
        ax.set_title(f"Publication {terminal}: full native assigned input decomposition")
        save(fig,"native-field-components",terminal,"Complete exact gradient, curl and harmonic fields; plot coordinates are numerical display values")
        fig,axes=plt.subplots(1,2,figsize=(11,4.5),layout="constrained")
        history=[get("historical_components",i) for i in sequences]
        axes[0].plot(sequences,[r["rank"] for r in history],marker="o",color="#355b7c",label="Exact rank")
        varying=[r for r in history if r["effective_dimension"] is not None]
        axes[0].plot([r["sequence"] for r in varying],[number(r["effective_dimension"]) for r in varying],marker="s",color="#879aac",label="Effective dimension of order two")
        axes[0].set_xlabel("Selected publication");axes[0].set_ylabel("Directions");axes[0].legend();axes[0].set_title("Variation of admitted historical fields")
        principal=[get("principal",i) for i in sequences]
        for i,name in enumerate(("A","B","C")):
            axes[1].plot(sequences,[number(r["variances"][i]) for r in principal],marker=("o","s","^")[i],label=name)
        axes[1].set_xlabel("Selected publication");axes[1].set_ylabel("Exact population variance (display)");axes[1].legend();axes[1].set_title("Named principal axes in the diagonal example")
        save(fig,"temporal-variation",terminal,"Each plotted point uses only its own historical prefix; diagonal principal example is separate from the synthetic perfusion dataset")
    (figures/"captions.json").write_text(json.dumps(produced,indent=2)+"\n")
    print(f"Exported {len(produced)} SVG/PDF figure pairs and figures/figures.pdf",flush=True)
