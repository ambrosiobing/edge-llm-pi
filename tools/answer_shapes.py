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

**It also prints the LOOKUP-TABLE baseline, added Thursday 12 November 2026, which is the
constant baseline's counterpart for free text.** The rule's verdict is known without any
model, so the question is what a table from that verdict to a fixed sentence would score. The
figure is the share of rows whose shape is the commonest shape within their own verdict class:

    e19_run.csv     4 notes, 2 shapes, table 100.0 percent over 2 classes
    e20_run.csv    50 rows,  1 shape,  table 100.0 percent over 1 class
    e03_run.csv   200 rows,  4 shapes, table  93.5 percent over 3 classes

**This is the figure that catches a paraphrase**, which no text comparison can. Project 19's
four notes are three of one sentence and one of another, and the one that differs is the one
FREEFALL row among three TAPs, so a two-entry table reproduces every note exactly. Calling
that note "the instruction" was a reading of its meaning; calling it a lookup of its verdict
is a measurement.

**What NONE of these three figures measures on its own, and why there are three.** Each catches
a different way of contributing nothing, and each is blind to the others:

    tools/prompt_echo.py    the instruction echoed verbatim. Catches e06 arm A at 20 of 20,
                            where the shape count reads 5 shapes because the same echo
                            carries different rubbish after it
    the shape count here    a template the prompt did not supply. Catches e03 at 187 of 200,
                            which no echo check sees, the template being the model's own
    the lookup baseline     a sentence predictable from the verdict alone. Catches e19 and
                            e20 at 100 per cent, which neither of the others sees

And masking digits still cannot see an answer that varies its words while saying nothing. A
low shape count is not by itself a defect either: a decision enum SHOULD have few shapes, and
a project whose answer is a fixed report line honestly has one. All of this is evidence about a
free-text slot, to be read beside the agreement rate and its constant baseline, never alone.

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


def lookup_baseline(rows, field):
    """(share, hits, n, classes) for a table from the rule's verdict to one fixed shape.

    The rule column is what is known without a model, so a table keyed on it is the free-text
    counterpart of the constant baseline that tools/agreement.py prints beside every rate.
    Rows with no verdict or no value are skipped; None when fewer than two rows remain, a
    baseline over one row being meaningless.
    """
    import collections
    pairs = []
    for row in rows:
        verdict = (row.get("rule") or "").strip()
        cell = (row.get("model") or "").strip()
        if not verdict or not cell:
            continue
        if cell.startswith("{"):
            try:
                value = str(json.loads(cell).get(field, "")).strip()
            except Exception:
                continue
        else:
            # e19's answer is an SMS body with no braces, so the cell IS the free text.
            value = cell
        if value:
            pairs.append((verdict, shape(value)))
    if len(pairs) < 2:
        return None
    by = collections.defaultdict(collections.Counter)
    for verdict, form in pairs:
        by[verdict][form] += 1
    hits = sum(c.most_common(1)[0][1] for c in by.values())
    return (hits / float(len(pairs)), hits, len(pairs), len(by))


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
        if not cell.startswith(("{", "[")):
            # e19's answer is an SMS body with no braces, so the cell IS the free text.
            # Tested on the first character and not by trying to parse, so that a
            # truncated object, which starts with a brace and does not parse, still
            # counts as broken rather than being read as a sentence.
            values.append(cell)
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
    base = lookup_baseline(rows, field)
    if base:
        share, hits, n, classes = base
        print()
        print("a table from the rule's verdict to one fixed shape reproduces %d of %d rows,"
              % (hits, n))
        print("  %.1f percent, over %d verdict class(es). That is what no model is needed for."
              % (100.0 * share, classes))

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
