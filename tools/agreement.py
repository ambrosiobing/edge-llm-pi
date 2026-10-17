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
    malformed = []
    # The vocabulary is the set of decisions the rule itself emitted in this log. The rule can
    # only ever emit its own names, so this is the honest list of what a grammar-constrained
    # model was allowed to answer, and anything outside it is a row the grammar did not
    # finish. Taken from the log rather than from the rule's source because this tool reads
    # one CSV and nothing else; the limit of that choice is a log whose rule never used one
    # of its names, where a model answering that name would be reported as malformed. With
    # the thousand-row replays in this volume all three names appear, and a log short enough
    # for the risk to be real is short enough to read by eye.
    vocabulary = {row["rule"].strip() for row in rows if row["rule"].strip()}
    for number, row in enumerate(rows, 2):
        rule = row["rule"].strip()
        model = model_decision(row["model"])
        if not model:
            n_empty += 1
            continue
        if model not in vocabulary:
            malformed.append((number, row.get("t_s", ""), model))
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
        print("| N | scored | not a decision of the rule | agreement |")
        print("|---|---|---|---|")
        print("| %d | %d | **%d** | %.1f percent |"
              % (n_total, n_scored, len(malformed), rate))
        print()
        print("| rule \\ model | " + " | ".join(models) + " |")
        print("|---|" + "---|" * len(models))
        for r in rules:
            print("| %s | " % r + " | ".join(str(confusion[r].get(m, 0)) for m in models) + " |")
    else:
        print("N %d, scored %d, empty model column %d, "
              "not a decision of the rule %d, agreement %d (%.1f percent)" %
              (n_total, n_scored, n_empty, len(malformed), agree, rate))
        width = max([len(r) for r in rules] + [12])
        print("rule \\ model".ljust(width) + "".join(m.rjust(12) for m in models))
        for r in rules:
            print(r.ljust(width) + "".join(str(confusion[r].get(m, 0)).rjust(12)
                                           for m in models))
    if malformed:
        print()
        print("rows whose model column is not a decision of the rule, which every chapter's "
              "first acceptance clause requires to be zero:")
        for number, t_s, model in malformed[:20]:
            print("  %d, %s, %r" % (number, t_s, model))
        if len(malformed) > 20:
            print("  ... and %d more" % (len(malformed) - 20))
    if disagreements:
        print()
        print("disagreements (log line, t_s, tag, rule, model):")
        for item in disagreements:
            print("  %d, %s, %s, %s, %s" % item)
    # Non-zero when the first acceptance clause fails, so a chapter's step cannot report a
    # pass by reading past the number. Until Saturday 17 October 2026 this tool could not
    # fail that clause at all: "empty model column" counts only an empty string, and a
    # grammar-constrained answer truncated by the generation budget is a non-empty string, so
    # it was scored as though it were a decision and appeared as its own column in the
    # confusion table. On e02's thousand-decision run of that day the count read zero while
    # 949 rows of 1000 held an object that never closed. The main loop's "model errors" and
    # the comparison binary's "rows without a literal" both read zero on the same log, for
    # their own reasons. Nothing in the project could fail the clause; a person reading the
    # column header caught it.
    return 1 if malformed else 0


if __name__ == "__main__":
    sys.exit(main())
