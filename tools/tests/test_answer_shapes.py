"""The shape count of tools/answer_shapes.py, including the cases that must not merge.

The figure this tool exists for is the gap between 191 distinct values and 4 shapes in
project 3's 200-row run. The tests below build that gap synthetically, so they say whether
the arithmetic is right rather than asserting today's ledgers, and they pin the two
distinctions that make the number honest: a number is masked and a word is not, and a run
whose answers did not parse is not a run with a tidy shape count.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import answer_shapes  # noqa: E402


def test_the_project_3_gap_is_what_the_tool_reports():
    # One sentence form with a different measurement on every row: the case the distinct
    # count cannot see.
    values = ["level %d.0, slope %d.1 per second" % (i, i) for i in range(200)]
    assert len(set(values)) == 200
    counted = answer_shapes.shapes(values)
    assert len(counted) == 1
    assert counted.most_common(1)[0] == ("level #, slope # per second", 200)


def test_a_decimal_a_sign_a_comma_and_a_hex_word_are_all_masked():
    assert answer_shapes.shape("peak 2.33 g") == "peak # g"
    assert answer_shapes.shape("offset -17") == "offset #"
    assert answer_shapes.shape("level 1,5 per second") == "level # per second"
    assert answer_shapes.shape("throttle bits 0x50005") == "throttle bits #"


def test_words_are_not_masked_so_two_real_sentences_stay_two_shapes():
    # The limit stated in the docstring: masking digits cannot see an answer that varies its
    # words. This test is here so that the limit is a decision and not an accident.
    values = ["a wall is in front of the sensor", "a hand is in front of the sensor"]
    assert len(answer_shapes.shapes(values)) == 2


def test_an_unparsable_model_cell_is_counted_and_not_shaped():
    rows = [{"model": '{"beam": "level 1.0"}'}, {"model": "not json at all"}]
    values, unparsable, empty = answer_shapes.field_values(rows, "beam")
    assert values == ["level 1.0"]
    assert unparsable == 1 and empty == 0


def test_json_that_is_not_an_object_counts_as_unparsable():
    # A bare array parses as JSON and has no fields. Treated as a value, it would silently
    # become the empty string and inflate the empty count instead of the broken one.
    rows = [{"model": '["ALERT", "CLEAR"]'}]
    values, unparsable, empty = answer_shapes.field_values(rows, "beam")
    assert values == [] and unparsable == 1 and empty == 0


def test_a_missing_field_is_empty_rather_than_unparsable():
    rows = [{"model": '{"decision": "CLEAR"}'}]
    values, unparsable, empty = answer_shapes.field_values(rows, "beam")
    assert values == [] and unparsable == 0 and empty == 1


def test_a_blank_model_cell_is_empty():
    rows = [{"model": ""}, {"model": "   "}]
    values, unparsable, empty = answer_shapes.field_values(rows, "beam")
    assert values == [] and unparsable == 0 and empty == 2


def test_the_shipped_ledger_of_project_3_still_shows_one_dominant_shape():
    # The one test that reads a real file. It asserts the finding the tool was written for,
    # so if a future run of project 3 answers differently this fails and the chapter's
    # figure has to be re-read rather than quietly kept.
    here = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    path = os.path.join(here, "projects", "e03-led-explainer", "evidence", "e03_run.csv")
    if not os.path.exists(path):
        return
    import csv
    with open(path, newline="", encoding="utf-8") as fh:
        rows = list(csv.DictReader(fh))
    values, _unparsable, _empty = answer_shapes.field_values(rows, "reason")
    counted = answer_shapes.shapes(values)
    form, n = counted.most_common(1)[0]
    assert form == "level #, slope # per second"
    assert n == 187 and len(values) == 200 and len(set(values)) == 191
