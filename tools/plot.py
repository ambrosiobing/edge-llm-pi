#!/usr/bin/env python3
"""One event log to one PNG: the first raw value over time, the rule's decision as
the band it sits in, and every row where the model disagreed with the rule marked.

    python tools/plot.py projects/e06-too-close/evidence/run.csv --out run.png \\
        --ylabel "distance, mm" --title "VL53L4CD approach run, Thursday 29 October 2026"

Reads the CSV common/c/src/elog.c writes. The raw column holds the event's values
joined by ';'; --value picks which one (0 by default). The model's decision is taken
the way tools/agreement.py takes it. The PNG is evidence, so the caption text (title)
is expected to carry the full date and the instrument.
"""
import argparse
import csv
import json
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

# Chart ink and one series colour, light surface. Disagreements use the reserved
# critical status colour with a marker and a legend entry, never colour alone.
SURFACE = "#fcfcfb"
INK = "#0b0b0b"
INK_SECONDARY = "#52514e"
MUTED = "#898781"
GRID = "#e1e0d9"
BASELINE = "#c3c2b7"
SERIES = "#2a78d6"
CRITICAL = "#d03b3b"


def model_decision(text):
    text = text.strip()
    if not text:
        return ""
    try:
        value = json.loads(text)
    except ValueError:
        return text
    if isinstance(value, dict) and "decision" in value:
        return str(value["decision"])
    return text


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("log")
    parser.add_argument("--out", required=True, help="PNG path")
    parser.add_argument("--value", type=int, default=0, help="index into the raw values")
    parser.add_argument("--ylabel", default="value")
    parser.add_argument("--title", default="")
    args = parser.parse_args()

    t, y, rules, disagree_t, disagree_y = [], [], [], [], []
    with open(args.log, newline="", encoding="utf-8") as fh:
        for row in csv.DictReader(fh):
            values = row.get("raw", "").split(";")
            if len(values) <= args.value or not values[args.value]:
                continue
            tt = float(row["t_s"])
            yy = float(values[args.value])
            t.append(tt)
            y.append(yy)
            rules.append(row.get("rule", ""))
            model = model_decision(row.get("model", ""))
            if model and model != row.get("rule", ""):
                disagree_t.append(tt)
                disagree_y.append(yy)
    if not t:
        raise SystemExit("plot: no rows with a value at index %d" % args.value)

    fig, ax = plt.subplots(figsize=(9, 4.2), dpi=150)
    fig.patch.set_facecolor(SURFACE)
    ax.set_facecolor(SURFACE)
    ax.plot(t, y, color=SERIES, linewidth=2, solid_capstyle="round", label=args.ylabel)

    # A hairline wherever the rule's decision changes, labelled once with the new name.
    for i in range(1, len(t)):
        if rules[i] != rules[i - 1]:
            ax.axvline(t[i], color=MUTED, linewidth=0.8, alpha=0.7)
            ax.text(t[i], ax.get_ylim()[1], rules[i], color=INK_SECONDARY, fontsize=8,
                    rotation=90, va="top", ha="right")
    if disagree_t:
        ax.scatter(disagree_t, disagree_y, s=48, marker="x", color=CRITICAL, linewidths=2,
                   zorder=3, label="model disagreed with the rule (%d)" % len(disagree_t))

    ax.set_xlabel("t, s", color=INK_SECONDARY)
    ax.set_ylabel(args.ylabel, color=INK_SECONDARY)
    if args.title:
        ax.set_title(args.title, color=INK, loc="left", fontsize=11)
    ax.grid(True, axis="y", color=GRID, linewidth=0.8)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(BASELINE)
    ax.tick_params(colors=MUTED, labelsize=9)
    ax.legend(loc="upper right", frameon=False, fontsize=9, labelcolor=INK_SECONDARY)
    fig.tight_layout()
    fig.savefig(args.out, facecolor=SURFACE)
    print("plot: %d rows, %d disagreements, written %s" % (len(t), len(disagree_t), args.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
