#!/usr/bin/env python3
"""The agreement table for one event log.

Reads the CSV common/c/src/elog.c writes (columns t_s, tag, raw, rule, model, ...),
takes the model's decision from the model column (a JSON object with a "decision" key
in the house grammar, otherwise the bare text), and prints N, the agreement rate, the
confusion table (rule rows, model columns) and every disagreement with its row number.

    python tools/agreement.py projects/e06-too-close/evidence/run_2026-10-29.csv
    python tools/agreement.py <log.csv> --md > table.md     # Markdown for a chapter

Rows whose model column is empty (the rule ran alone, or the engine failed) are counted
separately and never as agreement.
"""
import argparse
import csv
import json
import sys


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


def load(path):
    with open(path, newline="", encoding="utf-8") as fh:
        reader = csv.DictReader(fh)
        if reader.fieldnames is None or "rule" not in reader.fieldnames or \
                "model" not in reader.fieldnames:
            raise SystemExit("agreement: %s lacks rule and model columns" % path)
        return list(reader)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("log")
    parser.add_argument("--md", action="store_true", help="print Markdown tables")
    args = parser.parse_args()

    rows = load(args.log)
    n_total = len(rows)
    n_empty = 0
    agree = 0
    confusion = {}
    disagreements = []
    for number, row in enumerate(rows, 2):
        rule = row["rule"].strip()
        model = model_decision(row["model"])
        if not model:
            n_empty += 1
            continue
        confusion.setdefault(rule, {}).setdefault(model, 0)
        confusion[rule][model] += 1
        if rule == model:
            agree += 1
        else:
            disagreements.append((number, row.get("t_s", ""), row.get("tag", ""), rule, model))

    n_scored = n_total - n_empty
    rate = (100.0 * agree / n_scored) if n_scored else 0.0
    rules = sorted(confusion)
    models = sorted({m for r in confusion.values() for m in r})

    if args.md:
        print("| N | scored | agreement |")
        print("|---|---|---|")
        print("| %d | %d | %.1f percent |" % (n_total, n_scored, rate))
        print()
        print("| rule \\ model | " + " | ".join(models) + " |")
        print("|---|" + "---|" * len(models))
        for r in rules:
            print("| %s | " % r + " | ".join(str(confusion[r].get(m, 0)) for m in models) + " |")
    else:
        print("N %d, scored %d, empty model column %d, agreement %d (%.1f percent)" %
              (n_total, n_scored, n_empty, agree, rate))
        width = max([len(r) for r in rules] + [12])
        print("rule \\ model".ljust(width) + "".join(m.rjust(12) for m in models))
        for r in rules:
            print(r.ljust(width) + "".join(str(confusion[r].get(m, 0)).rjust(12)
                                           for m in models))
    if disagreements:
        print()
        print("disagreements (log line, t_s, tag, rule, model):")
        for item in disagreements:
            print("  %d, %s, %s, %s, %s" % item)
    return 0


if __name__ == "__main__":
    sys.exit(main())
