"""The shape figure where it is reported, in agreement.py and in e03's own instrument.

The measure itself is tested in test_answer_shapes.py. These tests cover the reporting
decision instead: that a ledger with no free-text field behind it prints nothing rather
than an empty finding, and that e03's instrument counts shapes from the same definition
rather than a second copy of it.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, os.path.join(ROOT, "projects", "e03-led-explainer", "tools"))

import agreement  # noqa: E402
import reason_check  # noqa: E402


def test_a_ledger_outside_projects_gets_no_shape_summary():
    # Nothing names a free-text field for a path with no project behind it, and the caller
    # must print nothing at all rather than a row of zeroes.
    rows = [{"model": '{"decision": "CLEAR", "beam": "level 1.0"}'}]
    assert agreement.shape_summary(rows, "/tmp/somewhere/run.csv") is None


def test_a_decision_only_log_gets_no_shape_summary():
    # e02's logs carry a bare literal and no free text. The field its grammar names is
    # absent from every row, so there is nothing to shape and nothing to print.
    path = os.path.join(ROOT, "projects", "e02-decision-loop", "evidence", "e02_run.csv")
    if not os.path.exists(path):
        return
    rows = [{"model": "ALERT"}, {"model": "NORMAL"}]
    assert agreement.shape_summary(rows, path) is None


def test_a_real_free_text_field_is_summarised_with_its_counts():
    path = os.path.join(ROOT, "projects", "e06-vl53l4cd-too-close", "evidence",
                        "e06_armA.csv")
    if not os.path.exists(path):
        return
    rows = [{"model": '{"decision": "CLEAR", "beam": "level 1.0 and 2.0"}'},
            {"model": '{"decision": "CLEAR", "beam": "level 3.0 and 4.0"}'}]
    summary = agreement.shape_summary(rows, path)
    assert summary is not None
    field, n_values, n_distinct, n_shapes, form, n_top = summary
    assert field == "beam"
    assert (n_values, n_distinct, n_shapes, n_top) == (2, 2, 1, 2)
    assert form == "level # and #"


def test_e03s_instrument_reports_shapes_from_the_same_definition():
    rows = [{"model": '{"colour": "GREEN", "reason": "level 37.0, slope 0.0 per second"}'},
            {"model": '{"colour": "GREEN", "reason": "level 40.9, slope 3.3 per second"}'}]
    lines = reason_check.shape_lines(rows)
    assert lines, "the instrument printed nothing for two parsable rows"
    assert "2 values, 2 distinct, 1 shapes" in lines[0]
    assert "level #, slope # per second" in lines[1]


def test_e03s_instrument_skips_rows_it_could_not_parse():
    # parse_model returns three values, the third its own status word. An earlier version
    # of shape_lines unpacked two and raised on the first real file it was given.
    rows = [{"model": '{"colour": "GREEN", "reason": "level 1.0"}'},
            {"model": "not json"},
            {"model": '{"colour": "GREEN"}'}]
    lines = reason_check.shape_lines(rows)
    assert "1 values, 1 distinct, 1 shapes" in lines[0]


def test_e03s_instrument_prints_nothing_when_no_row_parses():
    assert reason_check.shape_lines([{"model": "not json"}]) == []


def test_the_markdown_form_is_a_table_and_escapes_a_pipe():
    rows = [{"model": '{"colour": "GREEN", "reason": "a | b 1.0"}'}]
    lines = reason_check.shape_lines(rows, md=True)
    assert lines[1].startswith("| free text |")
    assert "\\|" in lines[3]
