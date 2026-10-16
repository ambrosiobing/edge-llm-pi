#!/usr/bin/env python3
"""The ranking rule: the order it produces, and that the order is total.

    python -m pytest projects/e20-scorecard/tests/test_rank.py

The rule is the control rule of this project, so these cases are what rules_eNN_test.c
is for a C project: the boundaries of the three keys, and the guarantee that no two
rows can swap between runs.

Written Wednesday 14 October 2026.
"""
import sys
from pathlib import Path

import pytest

# So that the tests run in a clone where nothing has been installed yet.
PROJECT = Path(__file__).resolve().parents[1]
if str(PROJECT) not in sys.path:
    sys.path.insert(0, str(PROJECT))

from edgellm_e20 import rank                                              # noqa: E402
from edgellm_e20.scorecard import (BENCH, IDS, NOTHING, STUB, Row,        # noqa: E402
                                   ScorecardError)


def make_row(project, n=0, agreement=None, origin=NOTHING, quieter=0, louder=0,
             no_literal=0):
    row = Row(project, "title of " + project, "C", "C", "a board", "decision")
    row.n = n
    row.agreement = agreement
    row.origin = origin
    if agreement is not None:
        row.quieter, row.louder, row.no_literal = quieter, louder, no_literal
    return row


def twenty(**overrides):
    """The twenty rows with nothing measured, then the overrides applied by id."""
    rows = [make_row(project) for project in IDS]
    for project, row in overrides.items():
        rows[IDS.index(project)] = row
    return rows


def test_least_evidence_first():
    rows = [make_row("e05", n=1000, agreement=90.0, origin=BENCH),
            make_row("e06", n=100, agreement=90.0, origin=BENCH),
            make_row("e07", n=0)]
    assert [row.project for row in rank.rank(rows)] == ["e07", "e06", "e05"]


def test_worst_agreement_breaks_an_equal_n():
    rows = [make_row("e05", n=100, agreement=90.0, origin=BENCH),
            make_row("e06", n=100, agreement=12.5, origin=BENCH),
            make_row("e07", n=100, agreement=50.0, origin=BENCH)]
    assert [row.project for row in rank.rank(rows)] == ["e06", "e07", "e05"]


def test_not_measured_sorts_below_nought_percent():
    """An unknown rate is a reason to measure; a known bad one is a reason to think."""
    unknown = make_row("e06", n=100, agreement=None, origin=BENCH)
    zero = make_row("e05", n=100, agreement=0.0, origin=BENCH)
    assert [row.project for row in rank.rank([zero, unknown])] == ["e06", "e05"]
    assert unknown.rank_agreement == rank.RANK_NOT_MEASURED


def test_a_stub_run_is_not_evidence():
    """A thousand stub rows rank with the projects that have nothing at all, and the
    identifier decides between them."""
    stub = make_row("e02", n=1000, agreement=82.3, origin=STUB)
    nothing = make_row("e19", n=0)
    bench = make_row("e03", n=10, agreement=100.0, origin=BENCH)
    assert stub.rank_n == 0 and stub.rank_agreement == rank.RANK_NOT_MEASURED
    assert [row.project for row in rank.rank([bench, stub, nothing])] == \
        ["e02", "e19", "e03"]


def test_the_identifier_is_the_last_key_so_the_order_is_total():
    rows = twenty()
    keys = [rank.sort_key(row) for row in rows]
    assert len(set(keys)) == len(keys)
    assert [row.project for row in rank.rank(rows)] == list(IDS)


def test_top_three_and_its_guards():
    rows = twenty()
    assert [row.project for row in rank.top(rows, 3)] == ["e01", "e02", "e03"]
    with pytest.raises(ValueError):
        rank.top(rows, 0)
    with pytest.raises(ScorecardError):
        rank.top(rows[:2], 3)


def test_rank_leaves_its_input_alone():
    rows = twenty(e01=make_row("e01", n=500, agreement=99.0, origin=BENCH))
    before = [row.project for row in rows]
    rank.rank(rows)
    assert [row.project for row in rows] == before


def test_the_rule_answers_an_index_into_its_own_name_table():
    rows = twenty()
    rule = rank.NextMeasurement(rows)
    assert rule.project == "e20"
    assert rule.names == IDS and len(rule.names) == 20
    index = rule(None, None)
    assert rule.names[index] == "e01"
    assert rank.index_of("e20") == 19


def test_the_rule_needs_all_twenty_rows():
    with pytest.raises(ScorecardError):
        rank.NextMeasurement([make_row("e01")])


def test_the_rule_ignores_the_presentation():
    """The rule's answer must not depend on the order the rows arrive in, because the
    acceptance test is exactly whether the model's answer does."""
    rows = twenty(e04=make_row("e04", n=7, agreement=10.0, origin=BENCH))
    forwards = rank.NextMeasurement(rows).three()
    backwards = rank.NextMeasurement(list(reversed(rows))).three()
    assert [row.project for row in forwards] == [row.project for row in backwards]


def test_a_reason_quotes_only_the_cells_of_its_row():
    bench = make_row("e05", n=120, agreement=75.0, origin=BENCH, quieter=20, louder=10)
    text = rank.reason(bench)
    assert "120 bench events" in text and "75.0 percent" in text
    assert "20 quieter" in text and "10 louder" in text
    stub = make_row("e02", n=1000, agreement=82.3, origin=STUB)
    assert "not evidence" in rank.reason(stub) and "1000 rows" in rank.reason(stub)
    assert "no record at all" in rank.reason(make_row("e19"))


def test_the_proposal_is_three_identifiers_with_three_reasons():
    rule = rank.NextMeasurement(twenty())
    proposal = rule.proposal()
    assert len(proposal) == 3
    for project, why in proposal:
        assert project in IDS
        assert 20 <= len(why) <= 200
