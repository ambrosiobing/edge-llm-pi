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

**It also prints what a constant predictor scores on the same rows, and that is the figure
an agreement rate is meaningless without.** Added Sunday 1 November 2026, after the same
defect was measured in two projects on two days:

    e20, Thursday 29 October 2026   the model scored 30.0 percent where a constant
                                    scored 100 on the published figure
    e06, Sunday 1 November 2026     the model answered APPROACHING on 50 of 50 events and
                                    scored 27 of 50, 54.0 percent. A constant APPROACHING
                                    predictor scores 27 of 50 on the same rows. It did not
                                    beat the best constant, it equalled it, being one.

**And the same constant output scored 0.0 percent on a different slice of the same replay**,
because the first 20 rows of e06's file are all CLEAR while the 50-row descent slice is 27
APPROACHING, 12 CLEAR and 11 TOO_CLOSE. A rate of 54.0 percent therefore says more about the
class balance of the rows than about the model, and a reader given only the rate cannot tell
the difference. The baseline is printed unconditionally for that reason.

`--require-beat-constant` makes failing to beat the best constant a non-zero exit. It is NOT
the default, because today it would fail e06 and e20, and a gate that fires on the projects
it was written for teaches a reader to ignore it before it has ever caught anything new.
Turn it on per project as each one is shown to clear its own baseline.
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
    parser.add_argument("--require-beat-constant", action="store_true",
                        help="exit non-zero if the model does not beat the best constant")
    args = parser.parse_args()

    rows = load(args.log)
    n_total = len(rows)
    n_empty = 0
    agree = 0
    confusion = {}
    disagreements = []
    malformed = []
    # The rule's label counts over SCORED rows, which is what a constant predictor is
    # measured on. Counting them over all rows would credit a constant for rows the model
    # never answered, and the comparison has to be over the same rows or it is not one.
    rule_counts = {}
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
        rule_counts[rule] = rule_counts.get(rule, 0) + 1
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
    # A constant predictor that always answers L is right exactly as often as the rule said
    # L. The best of those is the number the model has to beat to have added anything.
    constants = sorted(((100.0 * c / n_scored) if n_scored else 0.0, lab)
                       for lab, c in rule_counts.items())
    best_rate, best_label = (constants[-1] if constants else (0.0, ""))
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
    # The baseline, printed in both modes and unconditionally. An agreement rate without it
    # cannot be read: the same constant output scored 0.0 percent and 54.0 percent on two
    # slices of one replay, and the difference was the class balance of the rows.
    beats = agree > 0 and rate > best_rate
    if args.md:
        print()
        print("| constant predictor | scores |")
        print("|---|---|")
        for c_rate, label in reversed(constants):
            print("| always %s | %.1f percent |" % (label, c_rate))
        print()
        print("**The model %s the best constant**, %.1f percent against always %s at "
              "%.1f percent." % ("beats" if beats else "does NOT beat", rate, best_label,
                                 best_rate))
    else:
        print()
        print("what a constant predictor scores on these same rows:")
        for c_rate, label in reversed(constants):
            print("  always %-14s %5.1f percent" % (label, c_rate))
        print("THE MODEL %s THE BEST CONSTANT: %.1f percent against always %s at %.1f"
              % ("BEATS" if beats else "DOES NOT BEAT", rate, best_label, best_rate))
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
    if malformed:
        return 1
    # Opt in per project. Not the default: see the module docstring for why a gate that
    # fires on the two projects it was written for is worse than no gate.
    if args.require_beat_constant and not beats:
        print()
        print("agreement: the model does not beat a constant on these rows, so the rate "
              "above is the class balance and not a result")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
