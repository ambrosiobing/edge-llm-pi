#!/usr/bin/env python3
"""How many SHAPES are in a run's free-text answers, not how many distinct strings?

    python3 tools/answer_shapes.py <ledger.csv> --field beam
    python3 tools/answer_shapes.py <ledger.csv>            field taken from the grammar

**Why a count of distinct answers is as misleading as a rate with no baseline.** Project 3's
200-row run on board A, with the real engine, produced 191 distinct values in its `reason`
field. That reads as a model writing something different almost every time. Masking the
numbers says what it actually did:

    191 distinct values   4 shapes   187 of 200 are "level #, slope # per second"

So 93.5 per cent of the answers are one sentence form with the event's own measurements
substituted in, and the model contributed the form once. The distinct count cannot see that,
because the event supplies a fresh number on every row and the number is what differs.

**The three ways a free-text answer carries nothing, which are separate defects and were
treated as one until Thursday 12 November 2026.** All three were already in this
repository's ledgers:

    the instruction, verbatim   project 6: 20 of 20 rows answered "short sentence saying
                                what is probably in front of the sensor", the prompt's own
                                last clause. One shape
    the event, re-spelled       project 3: "level 37.0, slope 0.0 per second". 187 of 200
                                rows, one shape, numbers from the event
    an invented constant        project 19: 3 of 4 notes were one identical sentence that
                                is in neither the prompt nor the event

A checker that refuses the instruction catches the first and neither of the others. A shape
count catches the first and the second, and catches the third as a count of one.

**What this does NOT measure, stated because the number looks stronger than it is.** Masking
digits cannot see an answer that varies its words while saying nothing, and a low shape count
is not by itself a defect: a decision enum SHOULD have few shapes, and a project whose answer
is a fixed report line honestly has one. The figure is evidence about a free-text slot, to be
read beside the agreement rate and its constant baseline, never on its own.

Added Thursday 12 November 2026, from ledgers that had been in the tree since Tuesday
27 October 2026. Nothing here needs a board: every input is a file a past run already wrote.
"""
import argparse
import collections
import csv
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# A number with an optional sign and an optional decimal part, decimal point or comma, so
# that "37.0" and "1 103,85" both mask. Hex is masked by the second pattern because a
# throttle word such as 0x50005 differs between rows for the same reason a level does.
NUMBER = re.compile(r"0[xX][0-9a-fA-F]+|-?\d+(?:[.,]\d+)?")


def shape(text):
    """The answer with every number replaced by '#', which is its sentence form."""
    return NUMBER.sub("#", text)


def shapes(values):
    """Counter of shapes, commonest first when read through most_common()."""
    return collections.Counter(shape(v) for v in values)


def field_values(rows, field):
    """The free-text field of every row whose model cell is parsable and non-empty.

    Rows whose model cell does not parse are counted separately rather than skipped
    silently: a run that failed to produce JSON is a different fact from a run that
    produced the same sentence every time, and a tool that merges them would report a
    tidy shape count for a broken run.
    """
    values, unparsable, empty = [], 0, 0
    for row in rows:
        cell = (row.get("model") or "").strip()
        if not cell:
            empty += 1
            continue
        try:
            obj = json.loads(cell)
        except Exception:
            unparsable += 1
            continue
        if not isinstance(obj, dict):
            unparsable += 1
            continue
        value = str(obj.get(field, "")).strip()
        if value:
            values.append(value)
        else:
            empty += 1
    return values, unparsable, empty


def field_from_grammar(ledger):
    """The free-text rule name for the project this ledger sits under, or None.

    Read from tools/free_text_slots.py so that the field is the one the grammar actually
    names, rather than the first string in the answer, whose position in the JSON is not
    guaranteed and was wrong for two projects when this was first written by hand.
    """
    parts = os.path.abspath(ledger).split(os.sep)
    if "projects" not in parts:
        return None
    slug = parts[parts.index("projects") + 1]
    stem = slug.split("-")[0]
    sys.path.insert(0, os.path.join(ROOT, "tools"))
    import free_text_slots  # noqa: E402
    for gram, rule, _lo, _hi, _verdict, _example in free_text_slots.survey():
        if gram.split("_")[0] == stem:
            return rule
    return None


def engines(rows):
    return sorted({(r.get("engine") or "").strip() for r in rows if r.get("engine")})


def report(ledger, field=None, top=5):
    with open(ledger, newline="", encoding="utf-8") as fh:
        rows = list(csv.DictReader(fh))
    if not rows:
        print("answer_shapes: %s has no rows" % ledger)
        return 1
    if field is None:
        field = field_from_grammar(ledger)
        if field is None:
            print("answer_shapes: no free-text field known for %s; pass --field" % ledger)
            return 1
    values, unparsable, empty = field_values(rows, field)
    eng = ",".join(engines(rows)) or "unrecorded"
    print("ledger   %s" % os.path.relpath(os.path.abspath(ledger), ROOT))
    print("field    %s" % field)
    print("engine   %s" % eng)
    print("rows     %d, with a value %d, empty %d, model cell unparsable %d"
          % (len(rows), len(values), empty, unparsable))
    if not values:
        print("answer_shapes: nothing to shape")
        return 0
    counted = shapes(values)
    print("distinct %d values, %d shapes" % (len(set(values)), len(counted)))
    print()
    print("%6s  %6s  %s" % ("rows", "share", "shape"))
    for form, n in counted.most_common(top):
        print("%6d  %5.1f%%  %s" % (n, 100.0 * n / len(values), form[:84]))
    if "stub" in eng:
        print()
        print("NOTE: this run used the stub engine, whose expansion of the grammar is fixed,")
        print("so one shape is its documented behaviour and says nothing about any model.")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("ledger")
    ap.add_argument("--field", help="free-text field name; default from the grammar")
    ap.add_argument("--top", type=int, default=5, help="how many shapes to print")
    args = ap.parse_args(argv)
    return report(args.ledger, args.field, args.top)


if __name__ == "__main__":
    sys.exit(main())
