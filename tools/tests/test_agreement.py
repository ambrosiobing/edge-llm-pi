"""The first acceptance clause of every chapter, checked against the log that defeated it.

Each project in this volume carries a clause of the form "the count of rows whose model
column is not one of the rule's decisions
must be zero", and names tools/agreement.py as the thing that measures it. Until Saturday
17 October 2026 the tool could not fail that clause at all. "empty model column" counts only
an empty string; a grammar-constrained answer cut short by the generation budget is a
non-empty string, so it was scored as though it were a decision and showed up as its own
column in the confusion table. On e02's thousand-decision run of that day the count read
zero while 949 rows of 1000 held an object that never closed, and the main loop's "model
errors" and the comparison binary's "rows without a literal" read zero on the same log for
their own separate reasons. Nothing in the project could fail the clause. A person reading a
column header caught it.

The fixtures below are the two shapes that log can take, reduced to three rows each. Nothing
here needs a Raspberry Pi, a model file or a compiler.
"""
import os
import sys

import pytest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import agreement  # noqa: E402

TRUNCATED = (
    't_s,tag,rule,model\n'
    '0.0,synthetic,NORMAL,"{ ""decision"": ""ALERT"""\n'
    '1.0,synthetic,NORMAL,"{ ""decision"": ""NORMAL"" }"\n'
    '2.0,synthetic,ALERT,"{ ""decision"": ""ALERT"""\n'
)

CLEAN = (
    't_s,tag,rule,model\n'
    '0.0,synthetic,NORMAL,"{ ""decision"": ""NORMAL"" }"\n'
    '1.0,synthetic,ALERT,"{ ""decision"": ""ALERT"" }"\n'
    '2.0,synthetic,WATCH,"{ ""decision"": ""NORMAL"" }"\n'
)


def score(tmp_path, monkeypatch, capsys, text, extra=None):
    """Run the tool over one log and return (exit status, what it printed)."""
    path = tmp_path / "log.csv"
    path.write_text(text, encoding="ascii")
    argv = ["agreement.py", str(path)] + (extra or [])
    monkeypatch.setattr(sys, "argv", argv)
    status = agreement.main()
    return status, capsys.readouterr().out


def test_an_object_that_never_closed_is_counted_and_fails_the_clause(tmp_path, monkeypatch,
                                                                    capsys):
    """The defect itself: two of three rows hold a legal prefix that is not a decision."""
    status, out = score(tmp_path, monkeypatch, capsys, TRUNCATED)
    assert "not a decision of the rule 2" in out
    assert status == 1


def test_a_legal_decision_that_differs_from_the_rule_is_not_malformed(tmp_path, monkeypatch,
                                                                     capsys):
    """The discrimination the whole volume rests on.

    A model answering NORMAL where the rule said WATCH is the measurement, not a failure: it
    is the disagreement the agreement table exists to count. Only an answer that is not one
    of the rule's decisions at all fails the clause. If this test and the one above cannot
    both pass, the tool is measuring the wrong thing.
    """
    status, out = score(tmp_path, monkeypatch, capsys, CLEAN)
    assert "not a decision of the rule 0" in out
    assert status == 0
    assert "WATCH, NORMAL" in out      # still reported, as a disagreement


def test_an_empty_model_column_stays_its_own_count(tmp_path, monkeypatch, capsys):
    """The rule ran alone, or the engine failed. That is a third thing, not malformed text."""
    text = ('t_s,tag,rule,model\n'
            '0.0,synthetic,NORMAL,"{ ""decision"": ""NORMAL"" }"\n'
            '1.0,synthetic,ALERT,\n')
    status, out = score(tmp_path, monkeypatch, capsys, text)
    assert "empty model column 1" in out
    assert "not a decision of the rule 0" in out
    assert status == 0


def test_the_markdown_table_carries_the_count_too(tmp_path, monkeypatch, capsys):
    """A chapter pastes the Markdown form, so the clause has to be visible in it."""
    status, out = score(tmp_path, monkeypatch, capsys, TRUNCATED, ["--md"])
    assert "not a decision of the rule" in out
    assert "| 3 | 3 | **2** |" in out
    assert status == 1


def test_the_vocabulary_comes_from_the_rule_column_and_that_limit_is_recorded(tmp_path,
                                                                             monkeypatch,
                                                                             capsys):
    """A known limit, tested so it is recorded rather than discovered.

    The list of legal decisions is taken from the rule column of the log itself, because this
    tool reads one CSV and has no access to the rule's source. So a log in which the rule
    never emitted one of its own names will report a model answering that name as malformed.
    Here the rule only ever says NORMAL, and a correct ALERT from the model is counted
    against the clause. With the thousand-row replays in this volume all three names appear,
    and a log short enough for this to bite is short enough to read by eye.
    """
    text = ('t_s,tag,rule,model\n'
            '0.0,synthetic,NORMAL,"{ ""decision"": ""NORMAL"" }"\n'
            '1.0,synthetic,NORMAL,"{ ""decision"": ""ALERT"" }"\n')
    status, out = score(tmp_path, monkeypatch, capsys, text)
    assert "not a decision of the rule 1" in out
    assert status == 1
