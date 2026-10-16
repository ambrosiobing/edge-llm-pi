#!/usr/bin/env python3
"""The generator: twenty rows, no empty cell, and a loud failure instead of a hole.

    python -m pytest projects/e20-scorecard/tests/test_scorecard.py

Most of these cases are built on a temporary tree of two or three projects, because the
behaviour under test is what happens when something is missing or malformed, and the
repository is not allowed to be either. The last three cases run against the repository
as it stands, which is what the chapter's table is generated from.

Written Wednesday 14 October 2026.
"""
import sys
from pathlib import Path

import pytest

# So that the tests run in a clone where nothing has been installed yet.
PROJECT = Path(__file__).resolve().parents[1]
if str(PROJECT) not in sys.path:
    sys.path.insert(0, str(PROJECT))

from edgellm_e20 import scorecard                                         # noqa: E402
from edgellm_e20.scorecard import (BENCH, IDS, NOT_MEASURED, NOTHING,     # noqa: E402
                                   STUB, ScorecardError)

REPO = Path(__file__).resolve().parents[3]

ENUM_GRAMMAR = (
    'root     ::= "{" ws "\\"decision\\"" ws ":" ws decision ws "}"\n'
    'decision ::= "\\"NORMAL\\"" | "\\"WATCH\\"" | "\\"ALERT\\""\n'
    "ws       ::= [ \\t]*\n")
SENTENCE_GRAMMAR = (
    'root   ::= "{" ws "\\"answer\\"" ws ":" ws "\\"" answer "\\"" ws "}"\n'
    'answer ::= [A-Za-z0-9 ,.]{19,159} "."\n'
    "ws     ::= [ \\t]*\n")
MANIFEST = ('[project]\nid = "%s"\ntitle = "t"\nlanguage = "%s"\nvariant = "%s"\n'
            'board = "%s"\n')
LOG_HEAD = "t_s,tag,raw,rule,model,prompt_ms,generate_ms,n_generated,engine\n"


def make_tree(tmp_path, projects=(("e02", "decision-loop"),)):
    """A repository with the folders named and nothing else in it."""
    (tmp_path / "grammars").mkdir()
    (tmp_path / "tests" / "oracle").mkdir(parents=True)
    for project, slug in projects:
        folder = tmp_path / "projects" / ("%s-%s" % (project, slug))
        (folder / "evidence").mkdir(parents=True)
    return tmp_path


def write_manifest(tmp_path, slug, project, language="C", variant="C", board="a board"):
    path = tmp_path / "projects" / slug / "project.toml"
    path.write_text(MANIFEST % (project, language, variant, board), encoding="ascii")
    return path


def write_log(path, rows):
    """A log in the house shape: (rule, model text) pairs, one row each."""
    lines = [LOG_HEAD]
    for index, (rule, model) in enumerate(rows):
        quoted = model.replace('"', '""')
        lines.append('%.6f,"replay",1;2,"%s","%s",0.000,0.000,0,"stub"\n'
                     % (float(index), rule, quoted))
    path.write_text("".join(lines), encoding="ascii")
    return path


def decision(name):
    return '{"decision":"%s"}' % name


# ------------------------------------------------------------------ the shape

def test_twenty_rows_from_an_almost_empty_tree(tmp_path):
    root = make_tree(tmp_path)
    rows = scorecard.build(root)
    assert len(rows) == 20
    assert [row.project for row in rows] == list(IDS)
    assert scorecard.blank_cells(rows) == []


def test_every_unmeasured_cell_says_so_and_carries_a_reason(tmp_path):
    rows = scorecard.build(make_tree(tmp_path))
    for row in rows:
        cells = row.cells()
        for key in ("agreement", "quieter", "louder", "no_literal"):
            assert cells[key] == NOT_MEASURED
        assert row.note, "%s has no reason for reading not measured" % row.project
        assert cells["note"] != ""


def test_a_missing_folder_is_a_row_and_not_a_gap(tmp_path):
    rows = scorecard.build(make_tree(tmp_path))
    row = rows[IDS.index("e13")]
    assert row.origin == NOTHING
    assert "no projects/e13" in row.note
    assert row.language == "Rust" and row.variant == "R1"


def test_the_manifest_overrides_the_spine_and_a_difference_is_said(tmp_path):
    root = make_tree(tmp_path, (("e02", "decision-loop"),))
    write_manifest(root, "e02-decision-loop", "e02", language="Rust", variant="R9",
                   board="another board")
    row = scorecard.build(root)[IDS.index("e02")]
    assert row.language == "Rust" and row.variant == "R9"
    assert row.board == "another board"
    assert "language differs" in row.note and "variant differs" in row.note


# ------------------------------------------------------------------ loud failures

def test_a_malformed_manifest_stops_the_generator(tmp_path):
    root = make_tree(tmp_path)
    (root / "projects" / "e02-decision-loop" / "project.toml").write_text(
        '[project\nid = "e02"\n', encoding="ascii")
    with pytest.raises(ScorecardError) as caught:
        scorecard.build(root)
    assert "does not parse" in str(caught.value)


def test_a_manifest_without_a_language_stops_the_generator(tmp_path):
    root = make_tree(tmp_path)
    (root / "projects" / "e02-decision-loop" / "project.toml").write_text(
        '[project]\nid = "e02"\nvariant = "C"\n', encoding="ascii")
    with pytest.raises(ScorecardError) as caught:
        scorecard.build(root)
    assert "language" in str(caught.value)


def test_a_folder_id_outside_the_twenty_stops_the_generator(tmp_path):
    root = make_tree(tmp_path, (("e02", "decision-loop"),))
    (root / "projects" / "e21-invented").mkdir()
    with pytest.raises(ScorecardError) as caught:
        scorecard.build(root)
    assert "not one of the twenty" in str(caught.value)


def test_two_folders_claiming_one_id_stop_the_generator(tmp_path):
    root = make_tree(tmp_path, (("e02", "decision-loop"), ("e02", "second-copy")))
    with pytest.raises(ScorecardError) as caught:
        scorecard.build(root)
    assert "two folders claim e02" in str(caught.value)


def test_a_manifest_id_against_its_folder_stops_the_generator(tmp_path):
    root = make_tree(tmp_path, (("e02", "decision-loop"),))
    write_manifest(root, "e02-decision-loop", "e03")
    with pytest.raises(ScorecardError) as caught:
        scorecard.build(root)
    assert "but the folder is e02" in str(caught.value)


def test_an_oracle_without_the_columns_stops_the_generator(tmp_path):
    root = make_tree(tmp_path, (("e02", "decision-loop"),))
    (root / "grammars" / "e02_decision.gbnf").write_text(ENUM_GRAMMAR, encoding="ascii")
    (root / "tests" / "oracle" / "e02.expected.csv").write_text(
        "t_s,tag,verdict\n0.0,x,NORMAL\n", encoding="ascii")
    with pytest.raises(ScorecardError) as caught:
        scorecard.build(root)
    assert "no rule and model columns" in str(caught.value)


# ------------------------------------------------------------------ the counting

def test_the_three_classes_and_the_rate(tmp_path):
    root = make_tree(tmp_path, (("e02", "decision-loop"),))
    (root / "grammars" / "e02_decision.gbnf").write_text(ENUM_GRAMMAR, encoding="ascii")
    write_log(root / "projects" / "e02-decision-loop" / "evidence" / "run.csv", [
        ("NORMAL", decision("NORMAL")),          # agreement
        ("WATCH", decision("WATCH")),            # agreement
        ("ALERT", decision("WATCH")),            # quieter
        ("ALERT", decision("NORMAL")),           # quieter
        ("NORMAL", decision("ALERT")),           # louder
        ("WATCH", "sorry, I cannot"),            # no literal
        ("WATCH", ""),                           # the model was not asked
    ])
    row = scorecard.build(root)[IDS.index("e02")]
    assert row.origin == BENCH and row.n == 7
    assert (row.quieter, row.louder, row.no_literal) == (2, 1, 1)
    assert row.agreement == pytest.approx(100.0 * 2 / 6)
    assert "not asked on 1 of the 7 rows" in row.note


def test_the_direction_follows_the_grammar_order(tmp_path):
    """quieter means earlier in the name table, which grammars/README.md puts the quiet
    literal first of. Reverse the grammar and the same log reads the other way."""
    root = make_tree(tmp_path, (("e02", "decision-loop"),))
    (root / "grammars" / "e02_decision.gbnf").write_text(
        ENUM_GRAMMAR.replace('"\\"NORMAL\\"" | "\\"WATCH\\"" | "\\"ALERT\\""',
                             '"\\"ALERT\\"" | "\\"WATCH\\"" | "\\"NORMAL\\""'),
        encoding="ascii")
    write_log(root / "projects" / "e02-decision-loop" / "evidence" / "run.csv",
              [("ALERT", decision("NORMAL"))])
    row = scorecard.build(root)[IDS.index("e02")]
    assert (row.quieter, row.louder) == (0, 1)


def test_an_object_keyed_on_something_else_is_still_read(tmp_path):
    """e03 keys its object on "colour" and e07 on "trend"; both must be counted."""
    root = make_tree(tmp_path, (("e03", "led-explainer"),))
    (root / "grammars" / "e03_colour.gbnf").write_text(
        'root   ::= "{" ws colour ws "}"\n'
        'colour ::= "\\"GREEN\\"" | "\\"YELLOW\\"" | "\\"RED\\""\n'
        "ws     ::= [ \\t]*\n", encoding="ascii")
    write_log(root / "projects" / "e03-led-explainer" / "evidence" / "run.csv",
              [("GREEN", '{"colour":"GREEN","reason":"the level is low"}'),
               ("RED", '{"colour":"GREEN","reason":"the level is low"}')])
    row = scorecard.build(root)[IDS.index("e03")]
    assert row.n == 2 and row.quieter == 1
    assert row.agreement == pytest.approx(50.0)


def test_a_sentence_grammar_has_no_direction(tmp_path):
    root = make_tree(tmp_path, (("e08", "explorer700-console"),))
    (root / "grammars" / "e08_answer.gbnf").write_text(SENTENCE_GRAMMAR,
                                                       encoding="ascii")
    write_log(root / "projects" / "e08-explorer700-console" / "evidence" / "run.csv",
              [("IN_SYNC", '{"answer":"the clock agrees."}')])
    row = scorecard.build(root)[IDS.index("e08")]
    assert row.n == 1 and row.origin == BENCH
    assert row.quieter is None and row.agreement is None
    assert "a sentence and not an enum" in row.note


def test_bench_evidence_is_preferred_to_the_oracle(tmp_path):
    root = make_tree(tmp_path, (("e02", "decision-loop"),))
    (root / "grammars" / "e02_decision.gbnf").write_text(ENUM_GRAMMAR, encoding="ascii")
    write_log(root / "tests" / "oracle" / "e02.expected.csv",
              [("NORMAL", decision("NORMAL"))] * 9)
    write_log(root / "projects" / "e02-decision-loop" / "evidence" / "run.csv",
              [("NORMAL", decision("NORMAL"))] * 3)
    row = scorecard.build(root)[IDS.index("e02")]
    assert row.origin == BENCH and row.n == 3


def test_the_oracle_is_used_when_there_is_no_bench_log_and_is_named_a_stub(tmp_path):
    root = make_tree(tmp_path, (("e02", "decision-loop"),))
    (root / "grammars" / "e02_decision.gbnf").write_text(ENUM_GRAMMAR, encoding="ascii")
    write_log(root / "tests" / "oracle" / "e02.expected.csv",
              [("NORMAL", decision("NORMAL")), ("ALERT", decision("NORMAL"))])
    row = scorecard.build(root)[IDS.index("e02")]
    assert row.origin == STUB and row.n == 2
    assert "not a measurement" in row.note
    assert row.rank_n == 0 and row.rank_agreement == -1.0
    assert "which is not evidence" in row.model_line()
    assert "evidence 0" in row.model_line()


def test_a_second_artefact_in_evidence_is_passed_over_with_a_note(tmp_path):
    root = make_tree(tmp_path, (("e20", "scorecard"),))
    (root / "grammars" / "e20_next.gbnf").write_text(ENUM_GRAMMAR, encoding="ascii")
    evidence = root / "projects" / "e20-scorecard" / "evidence"
    (evidence / "proposals.csv").write_text("t_s,seed,rank,project,reason\n0.0,1,1,e01,x\n",
                                            encoding="ascii")
    write_log(evidence / "run.csv", [("NORMAL", decision("NORMAL"))])
    row = scorecard.build(root)[IDS.index("e20")]
    assert row.origin == BENCH and row.n == 1
    assert "passed over" in row.note


# ------------------------------------------------------------------ reading it back

def test_the_csv_round_trips(tmp_path):
    root = make_tree(tmp_path, (("e02", "decision-loop"),))
    (root / "grammars" / "e02_decision.gbnf").write_text(ENUM_GRAMMAR, encoding="ascii")
    write_log(root / "projects" / "e02-decision-loop" / "evidence" / "run.csv",
              [("NORMAL", decision("NORMAL")), ("ALERT", decision("NORMAL"))])
    rows = scorecard.build(root)
    path = tmp_path / "snapshot.csv"
    with path.open("w", newline="", encoding="ascii") as handle:
        scorecard.as_csv(rows, handle)
    back = scorecard.rows_from_csv(path)
    assert len(back) == 20
    for before, after in zip(rows, back):
        assert before.project == after.project
        assert before.n == after.n and before.origin == after.origin
        assert before.rank_n == after.rank_n
        if before.measured:
            assert after.agreement == pytest.approx(before.agreement, abs=0.05)
        else:
            assert after.agreement is None


def test_a_snapshot_of_nineteen_rows_is_refused(tmp_path):
    path = tmp_path / "short.csv"
    path.write_text(",".join(scorecard.CSV_COLUMNS) + "\n", encoding="ascii")
    with pytest.raises(ScorecardError):
        scorecard.rows_from_csv(path)


# ------------------------------------------------------------------ the repository

def test_the_name_table_of_the_e20_grammar_is_the_twenty_identifiers():
    text = (REPO / "grammars" / "e20_next.gbnf").read_text(encoding="ascii")
    assert scorecard.read_name_table(text) == IDS


def test_the_repository_builds_twenty_full_rows():
    rows = scorecard.build(REPO)
    assert len(rows) == 20
    assert scorecard.blank_cells(rows) == []
    for row in rows:
        cells = row.cells()
        if cells["agreement"] == NOT_MEASURED:
            assert row.note, "%s reads not measured with no reason" % row.project


def test_every_e20_cell_of_the_repository_agrees_with_the_manifest():
    row = scorecard.build(REPO)[IDS.index("e20")]
    assert row.language == "Python" and row.variant == "P and R8"
    assert "language differs" not in row.note
    assert "variant differs" not in row.note
