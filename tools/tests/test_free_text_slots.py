"""The ratchet of tools/free_text_slots.py, including every case that must refuse.

The survey this guards has reported the same number since Sunday 1 November 2026: fourteen
of fifteen free-text slots admit a sentence of their own prompt verbatim. A baseline that
records that number is worth nothing unless it refuses a change away from it, so each test
below drives `ratchet` to one of its verdicts with rows built by hand.

Rows are built rather than surveyed on purpose. `survey()` reads the real grammars and the
real prompts, so a test written against it would assert today's tree and would start failing
the day a grammar is narrowed, which is the outcome the tool exists to encourage. `ratchet`
is pure, takes the rows and the baseline as arguments, and can therefore be shown to refuse
without any grammar being wrong on disk.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import free_text_slots  # noqa: E402


def row(stem, rule, verdict, lo=8, hi=120, example=""):
    """One survey row in the shape survey() emits."""
    return (stem, rule, lo, hi, verdict, example)


BASE = frozenset({("e06_range", "beam"), ("e12_band", "sentence")})


def test_the_baseline_itself_raises_nothing():
    rows = [row("e06_range", "beam", "ADMITS"), row("e12_band", "sentence", "ADMITS")]
    assert free_text_slots.ratchet(rows, BASE) == []


def test_a_slot_that_refuses_and_is_absent_from_the_baseline_is_the_quiet_good_case():
    # The one slot in the real tree that refuses, e12 as it actually is, must not be a
    # problem merely for being measured.
    rows = [row("e06_range", "beam", "ADMITS"), row("e99_new", "note", "refuses")]
    problems = free_text_slots.ratchet(rows, frozenset({("e06_range", "beam")}))
    assert problems == []


def test_a_new_admitting_slot_is_refused():
    rows = [row("e06_range", "beam", "ADMITS"), row("e12_band", "sentence", "ADMITS"),
            row("e99_new", "note", "ADMITS")]
    problems = free_text_slots.ratchet(rows, BASE)
    assert len(problems) == 1
    assert problems[0].startswith("NEW:")
    assert "e99_new" in problems[0] and "note" in problems[0]


def test_a_listed_slot_that_now_refuses_is_refused_because_progress_must_be_recorded():
    rows = [row("e06_range", "beam", "refuses"), row("e12_band", "sentence", "ADMITS")]
    problems = free_text_slots.ratchet(rows, BASE)
    assert len(problems) == 1
    assert problems[0].startswith("NARROWED:")
    assert "remove it from ADMITTING" in problems[0]


def test_a_listed_slot_missing_from_the_survey_is_refused_as_stale():
    rows = [row("e06_range", "beam", "ADMITS")]
    problems = free_text_slots.ratchet(rows, BASE)
    assert len(problems) == 1
    assert problems[0].startswith("STALE:")
    assert "e12_band" in problems[0]


def test_a_slot_with_no_prompt_is_refused_rather_than_counted_as_clear():
    # survey() emits "NO PROMPT" when a grammar has no project prompt beside it. That is
    # neither admitting nor refusing, and the tool must not read it as the latter.
    rows = [row("e06_range", "beam", "ADMITS"), row("e12_band", "sentence", "NO PROMPT")]
    problems = free_text_slots.ratchet(rows, BASE)
    assert len(problems) == 1
    assert problems[0].startswith("UNMEASURED:")
    assert "not thereby cleared" in problems[0]


def test_a_second_slot_in_one_grammar_registers_rather_than_replacing_the_first():
    # Keyed on (grammar, rule). Keyed on the grammar alone, this row would overwrite the
    # first and the new slot would pass unseen.
    rows = [row("e06_range", "beam", "ADMITS"), row("e06_range", "aside", "ADMITS"),
            row("e12_band", "sentence", "ADMITS")]
    problems = free_text_slots.ratchet(rows, BASE)
    assert len(problems) == 1
    assert problems[0].startswith("NEW:")
    assert "aside" in problems[0]


def test_several_failures_are_all_reported_rather_than_the_first():
    rows = [row("e06_range", "beam", "refuses"), row("e99_new", "note", "ADMITS")]
    problems = free_text_slots.ratchet(rows, BASE)
    kinds = sorted(p.split(":")[0] for p in problems)
    assert kinds == ["NARROWED", "NEW", "STALE"]


def test_the_shipped_baseline_matches_the_tree_it_was_recorded_from():
    # The one test here that does read the real tree, and the only assertion it makes is
    # that the shipped baseline and the shipped grammars agree. If this fails, either a
    # grammar changed and ADMITTING was not updated, or ADMITTING was edited without the
    # survey being re-run.
    assert free_text_slots.ratchet(free_text_slots.survey()) == []
