#!/usr/bin/env python3
"""Export figures from completed exact native observations.

Matplotlib converts exact fractions to display coordinates only. Each figure
uses only the requested publication prefix and labels its native cutoff.
"""
import argparse
from fractions import Fraction
import json
from pathlib import Path


def export(directory):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    matplotlib.rcParams.update({"font.family": "DejaVu Sans", "font.size": 10,
                                "svg.fonttype": "none", "axes.spines.top": False,
                                "axes.spines.right": False, "savefig.facecolor": "white"})
    root = Path(directory)
    records = json.loads((root/"observations.json").read_text())
    figures = root/"figures"
    figures.mkdir(exist_ok=True)
    database = [x for x in records if x["chapter"] == "database"]
    for frame in database:
        cutoff = frame["sequence"]
        selected = [x for x in records if x["chapter"] == "query" and x["sequence"] <= cutoff]
        sectors = [x for x in records if x["chapter"] == "field" and x["sequence"] <= cutoff]
        fig, axs = plt.subplots(3, 1, figsize=(10, 10), layout="constrained")
        ax = axs[0]
        for i, event in enumerate(frame["events"]):
            start, end = Fraction(event["start"]), Fraction(event["end"])
            ax.barh(i, float(end-start), left=float(start), height=.5, color="#527797",
                    edgecolor="#263f55", linewidth=.6)
            ax.text(float(start), i-.35, f"[{start}, {end})", fontsize=9)
        ax.set_yticks(range(len(frame["events"])), [e["event"] for e in frame["events"]])
        ax.set_xlabel("Valid time in minutes")
        ax.set_title(f"Retained time spans at publication {cutoff}", loc="left")
        ax.invert_yaxis()
        ax = axs[1]
        sequences = [x["sequence"] for x in selected]
        sums = [Fraction(x["reading_sum"] or "0") for x in selected]
        ax.bar(sequences, [float(x) for x in sums], color="#527797", width=.5)
        for x, value in zip(sequences, sums):
            ax.text(x, float(value)+.1, str(value), ha="center")
        ax.set_xticks(sequences)
        ax.set_xlabel("Native publication")
        ax.set_ylabel("Exact reading sum")
        ax.set_title("Native overlap query for [5, 15)", loc="left")
        ax.set_ylim(0, max(map(float,sums), default=0)+1)
        ax = axs[2]
        for i, (name, color) in enumerate(zip(("Gradient", "Curl", "Harmonic"), ("#315d83", "#725f84", "#4f7562"))):
            ax.step([x["sequence"] for x in sectors], [x["sector_dimensions"][i] for x in sectors],
                    where="post", label=name, color=color)
        ax.set_xticks(sequences)
        ax.set_yticks(range(max(max(x["sector_dimensions"]) for x in sectors)+1))
        ax.set_xlabel("Native publication")
        ax.set_ylabel("Exact sector dimension")
        ax.set_title("Declared grade one Hodge sectors", loc="left")
        ax.legend(frameon=False, ncols=3)
        fig.suptitle(f"Temporal relations and field dynamics\nAvailability cutoff {cutoff}, minute {frame['available_minute']}", ha="left", x=.08, fontsize=13)
        for extension in ("svg", "pdf"):
            fig.savefig(figures/f"publication-{cutoff}.{extension}")
        plt.close(fig)
    return len(database)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    print(f"Exported {export(args.directory)} publication figures as SVG and PDF")
