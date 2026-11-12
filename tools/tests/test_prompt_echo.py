"""The verbatim echo check of tools/prompt_echo.py, including the cases it must not flag.

Two of these read real ledgers, because the tool's whole claim is that it reproduces counts
measured on board A and published on Sunday 1 November 2026. The rest drive the comparison
with strings built by hand, so the threshold and the direction of the coverage ratio are
pinned without depending on today's prompts.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "tools"))

import prompt_echo as pe  # noqa: E402

INSTRUCTION = "In beam, one short sentence saying what is probably in front of the sensor."
PROMPT = ["You are an edge agent on a Raspberry Pi 3 with one sensor.", INSTRUCTION]


def test_the_instruction_echoed_verbatim_is_flagged():
    ok, which, run, _cov = pe.echo("short sentence saying what is probably in front of "
                                   "the sensor.", PROMPT)
    assert ok and which == INSTRUCTION and run == 11


def test_an_echo_with_rubbish_appended_is_still_an_echo():
    # The case that corrected the coverage direction: two of arm A's twenty rows carried the
    # whole instruction and then appended range numbers. Measured against the answer they
    # escaped; measured against the prompt sentence they do not.
    ok, _which, run, _cov = pe.echo("short sentence saying what is probably in front of "
                                    "the sensor, 902, 902, 896.", PROMPT)
    assert ok and run == 11


def test_the_coverage_direction_is_pinned_by_a_case_where_the_two_disagree():
    """The mutation test of Thursday 12 November 2026 found this gap in the tests.

    Flipping the ratio to divide by the ANSWER's length left eight of nine tests passing,
    because in all of them both ratios clear 0.70. This case separates them: the whole
    eleven-word instruction followed by twenty numbers is 31 words, so coverage against the
    answer is 11/31 = 35 per cent and would not be flagged, while coverage against the
    prompt sentence is 11/14 = 79 per cent and is. An echo with rubbish after it is still
    an echo, so the second is the one the tool must use.
    """
    answer = ("short sentence saying what is probably in front of the sensor, "
              + ", ".join(str(900 + i) for i in range(20)))
    assert len(answer.split()) > 25
    ok, _which, run, cov = pe.echo(answer, PROMPT)
    assert ok, "the instruction is in there verbatim"
    assert run == 11 and cov > 0.70


def test_an_honest_answer_sharing_a_phrase_is_not_flagged():
    # Five shared words out of a fourteen-word sentence is 36 per cent of it.
    ok, _which, run, cov = pe.echo("a wall is in front of the sensor", PROMPT)
    assert not ok
    assert run == 5 and cov < 0.5


def test_an_empty_answer_is_not_an_echo():
    assert pe.echo("", PROMPT)[0] is False
    assert pe.echo("   ", PROMPT)[0] is False


def test_a_short_shared_run_is_below_the_word_floor():
    ok, _which, run, _cov = pe.echo("the sensor", PROMPT)
    assert not ok and run < 5


def test_sentences_split_on_full_stop_and_semicolon():
    text = "One thing. Two things; three things.\nFour."
    assert pe.sentences(text) == ["One thing.", "Two things;", "three things.", "Four."]


def test_longest_run_ignores_case_and_punctuation():
    assert pe.longest_run("In Front, of the SENSOR", "in front of the sensor") == 5


def test_arm_a_reproduces_the_published_twenty_of_twenty():
    import csv
    led = os.path.join(ROOT, "projects", "e06-vl53l4cd-too-close", "evidence",
                       "e06_armA.csv")
    prompt = os.path.join(ROOT, "projects", "e06-vl53l4cd-too-close", "prompt.txt")
    if not (os.path.exists(led) and os.path.exists(prompt)):
        return
    with open(led, newline="", encoding="utf-8") as fh:
        rows = list(csv.DictReader(fh))
    n, n_any, n_instr, _ex = pe.scan(rows, "beam",
                                     open(prompt, encoding="utf-8").read())
    assert (n, n_any, n_instr) == (20, 20, 20)


def test_arm_b_reproduces_the_published_zero_for_the_last_sentence():
    import csv
    base = os.path.join(ROOT, "projects", "e06-vl53l4cd-too-close", "evidence")
    led = os.path.join(base, "e06_armB.csv")
    prompt = os.path.join(base, "e06_prompt_reordered.txt")
    if not (os.path.exists(led) and os.path.exists(prompt)):
        return
    with open(led, newline="", encoding="utf-8") as fh:
        rows = list(csv.DictReader(fh))
    n, n_any, n_instr, _ex = pe.scan(rows, "beam",
                                     open(prompt, encoding="utf-8").read())
    # Six of the twenty answers parse; fourteen were truncated at the token budget and are
    # not judgeable. None of the six echoes the reordered prompt's LAST sentence, which is
    # the published result; three echo some sentence of it, one of them the instruction
    # itself, which no longer sits last.
    assert (n, n_instr) == (6, 0)
    assert n_any == 3
