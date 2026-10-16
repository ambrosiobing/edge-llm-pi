#!/usr/bin/env python3
"""The oracle: the stub loop over the fifty presentations, row for row.

    python -m pytest projects/e20-scorecard/tests/test_oracle.py

This is the Python form of what tests/oracle_check does for a C project. The loop is
run with the stub engine over projects/e20-scorecard/replay/scorecards_50.csv and every
row of the log it writes is compared with tests/oracle/e20.expected.csv. Nothing here
needs a model file, a server or llama-cpp-python, which is the point: the one thing CI
can prove is that the loop, the grammar, the rule, the log and the comparison are
whole.

The stub's answer is not written down twice. It is the first expansion of
grammars/e20_next.gbnf under the rule of common/c/src/edgellm_stub.c, which this
project mirrors in Python, and the first case below is that the two agree.

Written Wednesday 14 October 2026.
"""
import csv
import sys
from pathlib import Path

import pytest

# So that the tests run in a clone where nothing has been installed yet.
PROJECT = Path(__file__).resolve().parents[1]
if str(PROJECT) not in sys.path:
    sys.path.insert(0, str(PROJECT))

from edgellm_e20 import main as mainmod                                   # noqa: E402
from edgellm_e20.remote import StubModel, first_expansion                 # noqa: E402
from edgellm_e20.scorecard import IDS, rows_from_csv                      # noqa: E402

REPO = Path(__file__).resolve().parents[3]
GRAMMAR = PROJECT / "grammar.gbnf"
REPLAY = PROJECT / "replay" / "scorecards_50.csv"
SNAPSHOT = PROJECT / "replay" / "scorecard_snapshot.csv"
ORACLE = REPO / "tests" / "oracle" / "e20.expected.csv"
STUB_ANSWER = ('[{"project":"e01","reason":""},{"project":"e01","reason":""},'
               '{"project":"e01","reason":""}]')


def read_csv(path):
    with Path(path).open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


# ------------------------------------------------------------------ the grammar

def test_the_project_grammar_is_the_one_in_grammars():
    assert GRAMMAR.read_text(encoding="ascii") == \
        (REPO / "grammars" / "e20_next.gbnf").read_text(encoding="ascii")


def test_the_stub_answer_is_the_grammars_first_expansion():
    assert first_expansion(GRAMMAR.read_text(encoding="ascii")) == STUB_ANSWER


def test_the_stub_answer_is_three_objects_and_parses():
    picks, reasons = mainmod.parse_answer(StubModel(
        GRAMMAR.read_text(encoding="ascii")).answer_text)
    assert picks == ["e01", "e01", "e01"]
    assert reasons == ["", "", ""]


def test_an_answer_outside_the_name_table_is_no_answer():
    assert mainmod.parse_answer('[{"project":"e99","reason":"x"}]') == ([], [])
    assert mainmod.parse_answer("I would measure e01 first") == ([], [])
    assert mainmod.parse_answer("") == ([], [])
    assert mainmod.parse_answer('{"project":"e01"}') == ([], [])


# ------------------------------------------------------------------ the files

def test_the_three_generated_files_exist_and_agree():
    assert SNAPSHOT.exists() and REPLAY.exists() and ORACLE.exists()
    replay = read_csv(REPLAY)
    oracle = read_csv(ORACLE)
    assert len(replay) == 50 and len(oracle) == 50
    snapshot = rows_from_csv(SNAPSHOT)
    assert len(snapshot) == 20
    expected = [IDS[int(float(replay[0][name])) - 1] for name in ("v1", "v2", "v3")]
    from edgellm_e20 import rank
    assert [row.project for row in rank.top(snapshot, 3)] == expected
    assert {row["rule"] for row in oracle} == {expected[0]}


def test_the_fifty_presentations_differ_only_in_the_seed():
    replay = read_csv(REPLAY)
    seeds = [row["v0"] for row in replay]
    assert len(set(seeds)) == 50
    assert len({(row["v1"], row["v2"], row["v3"]) for row in replay}) == 1


def test_a_presentation_reorders_the_rows_and_keeps_all_twenty():
    snapshot = rows_from_csv(SNAPSHOT)
    first = mainmod.present(snapshot, 2026)
    second = mainmod.present(snapshot, 2027)
    assert {row.project for row in first} == set(IDS)
    assert [row.project for row in first] != [row.project for row in second]
    assert mainmod.present(snapshot, 2026)[0].project == first[0].project


def test_the_prompt_shows_every_row_and_no_stub_rate_as_evidence():
    snapshot = rows_from_csv(SNAPSHOT)
    text = mainmod.format_prompt(mainmod.present(snapshot, 2026), 2026)
    for project in IDS:
        assert ("\n%s evidence " % project) in text or text.startswith(project)
    assert text.count(" evidence ") == 20
    assert "which is not evidence" in text


# ------------------------------------------------------------------ the loop

def test_the_stub_loop_matches_the_oracle_row_for_row(tmp_path):
    log = tmp_path / "e20_stub.csv"
    proposals = tmp_path / "e20_proposals.csv"
    status = mainmod.main(["--stub", "--log", str(log), "--proposals", str(proposals),
                           "--repo", str(REPO), "--require", "0.0"])
    assert status == 0
    written = read_csv(log)
    expected = read_csv(ORACLE)
    assert len(written) == len(expected) == 50
    for number, (got, want) in enumerate(zip(written, expected), 2):
        assert float(got["t_s"]) == pytest.approx(float(want["t_s"])), number
        assert got["tag"] == want["tag"], number
        assert got["rule"] == want["rule"], number
        assert got["model"] == want["model"], number
        assert got["engine"] == "stub", number
    rows = read_csv(proposals)
    assert len(rows) == 150
    assert {row["project"] for row in rows} == {"e01"}


def test_the_acceptance_condition_can_fail(tmp_path):
    """The stub's top pick is the grammar's first literal and the rule's top pick is
    the first identifier with no evidence, so in CI the two agree on all fifty. That
    is a coincidence of two first literals and not a result, so the gate is checked
    the other way round too: a condition above what the run reached must fail."""
    log = tmp_path / "e20_stub.csv"
    assert mainmod.main(["--stub", "--log", str(log), "--repo", str(REPO),
                         "--require", "1.01"]) == 1


def test_a_replay_row_from_another_scorecard_stops_the_run(tmp_path):
    """The guard that keeps a stale replay from being scored against a moved rule."""
    stale = tmp_path / "stale.csv"
    stale.write_text("t_s,tag,v0,v1,v2,v3\n0.0,scorecard,2026,20,19,18\n",
                     encoding="ascii")
    status = mainmod.main(["--stub", "--log", str(tmp_path / "log.csv"),
                           "--repo", str(REPO), "--replay", str(stale)])
    assert status == 1


def test_the_loop_refuses_without_an_engine(tmp_path):
    assert mainmod.main(["--log", str(tmp_path / "log.csv"), "--repo", str(REPO)]) == 1
