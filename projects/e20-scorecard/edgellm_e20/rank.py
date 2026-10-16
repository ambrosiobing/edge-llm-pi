#!/usr/bin/env python3
"""The control rule of e20: the deterministic ranking of the scorecard.

    python -m edgellm_e20.rank            the ranked twenty, worst evidence first

The rule is three lines of comparison and nothing else. It sorts the scorecard by the
number of bench events ascending, then by the agreement rate ascending, then by the
project identifier ascending, and the first three of that order are what to measure
next. N comes first on purpose: an agreement rate over a handful of events is not a
rate, and a project with no evidence at all must outrank one with a bad rate over a
thousand events. A row that reads "not measured" sorts as an agreement of minus one,
which is worse than nought percent, because an unknown rate is a reason to measure and
a known bad one is a reason to think.

The two keys are the row's bench figures and not its printed ones: a CI stub run is not
evidence, so a project with a thousand stub rows and no bench run sorts with the
projects that have nothing at all, and the rule's own reason says which it is. That is
also what keeps the ranking still when a chapter lands its replay file and its oracle:
a generated oracle changes the printed N of a row and never its place in this order.

The identifier is the last key so that the order is total: two rows with the same N and
the same rate cannot swap between runs, which is what lets an oracle file hold the
rule's answer and a test compare it row for row.

The rule acts and the model proposes: the ranking is what the chapter publishes and
what the acceptance test scores the model against. The model's three identifiers reach
the log and the proposals file, never the scorecard.

Written Wednesday 14 October 2026.
"""
import sys

from .scorecard import BENCH, IDS, STUB, ScorecardError, build

RANK_NOT_MEASURED = -1.0


def sort_key(row):
    """The whole rule: least evidence first, then worst agreement, then the id."""
    return (row.rank_n, row.rank_agreement, row.project)


def rank(rows):
    """The scorecard in measure-next order. The input list is left as it was."""
    return sorted(rows, key=sort_key)


def top(rows, count=3):
    """The first count rows of the ranking: what to measure next, in order."""
    if count < 1:
        raise ValueError("top() needs at least one row, not %r" % count)
    ordered = rank(rows)
    if len(ordered) < count:
        raise ScorecardError("the scorecard has %d rows and %d were asked for"
                             % (len(ordered), count))
    return ordered[:count]


def index_of(project):
    """The rule's decision as an index into its name table, as rules.h asks for."""
    return IDS.index(project)


def reason(row):
    """The rule's own one-line reason for putting this row where it is.

    This is the sentence the model is asked to write for itself, and the one a reader
    compares its answer against. It never invents a number: every figure in it is a
    cell of the row."""
    if row.origin == BENCH and row.measured:
        return ("%s has %d bench events at %.1f percent agreement, with %d quieter, %d "
                "louder and %d without a literal"
                % (row.project, row.n, row.agreement, row.quieter, row.louder,
                   row.no_literal))
    if row.origin == BENCH:
        return ("%s has %d bench events and no rate, because no row carried an answer"
                % (row.project, row.n))
    if row.origin == STUB:
        rate = ("%.1f percent" % row.agreement) if row.measured else "no rate"
        return ("%s has no bench event at all: its %d rows are a CI stub run at %s, "
                "which is not evidence" % (row.project, row.n, rate))
    return ("%s has no record at all, so nothing is known about whether a model adds "
            "anything there" % row.project)


class NextMeasurement:
    """The rule as the shared package asks for one: an id, a name table, a decision.

    The name table is the twenty project identifiers in reading order, the same order
    and the same letters as the project rule of grammars/e20_next.gbnf, so that the
    log's rule column and its model column are compared letter for letter. The rule is
    memoryless: it reads the scorecard it was built with, never the history, and never
    the model's answer. The event is not read at all, which is why its values carry the
    presentation seed rather than a measurement: the rule's answer must not depend on
    the order the rows were shown in, and the acceptance test is exactly whether the
    model's answer does.
    """

    project = "e20"
    names = IDS

    def __init__(self, rows):
        if len(rows) != len(IDS):
            raise ScorecardError("the rule needs all %d rows, not %d"
                                 % (len(IDS), len(rows)))
        self.rows = list(rows)

    def __call__(self, history, event):
        del history, event
        return index_of(top(self.rows, 1)[0].project)

    def three(self):
        """The three rows the rule proposes, in order."""
        return top(self.rows, 3)

    def proposal(self):
        """The three identifiers with the rule's reason for each."""
        return [(row.project, reason(row)) for row in self.three()]


def main(argv=None):
    del argv
    try:
        rows = build()
    except ScorecardError as exc:
        print("rank: %s" % exc, file=sys.stderr)
        return 1
    ordered = rank(rows)
    print("%-4s %6s %13s  %s" % ("No.", "benchN", "bench rate", "the rule's reason"))
    print("-" * 95)
    for position, row in enumerate(ordered, 1):
        mark = " <- measure next" if position <= 3 else ""
        print("%-4s %6d %13s  %s%s"
              % (row.project, row.rank_n,
                 ("%.1f percent" % row.rank_agreement) if row.rank_agreement >= 0.0
                 else "not measured", reason(row), mark))
    print("-" * 95)
    print("the rule's three: %s" % ", ".join(r.project for r in top(rows, 3)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
