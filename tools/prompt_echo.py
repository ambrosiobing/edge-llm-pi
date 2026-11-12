#!/usr/bin/env python3
"""Does a free-text answer carry a span of the project's own prompt, verbatim?

    python3 tools/prompt_echo.py <ledger.csv>
    python3 tools/prompt_echo.py <ledger.csv> --field beam --min-words 5 --coverage 0.70

**What this is for.** Fourteen of the fifteen free-text slots in this volume admit a sentence
of their own prompt as a legal answer, which `tools/free_text_slots.py` measures, and on
hardware the failure is not theoretical: project 6 answered with the prompt's own last clause
on 40 of 40 events across two sittings. A grammar cannot refuse it, because a character class
pins the shape of a field and an instruction has the shape of an answer. So the refusal has to
happen after the answer exists, and this is the instrument that sees it.

**Why it is a shared tool and the no-invented-numbers clause is not.** "A number not in the
event" means verbatim in project 19, rounded in project 3, unit-converted in project 4 and
drawn from a ten-event window in project 6, so one checker would report a defect in three
projects, which is recorded in e20's evidence of Thursday 12 November 2026. "A contiguous run
of the prompt's own words" means the same thing in every project.

**Validated against the two published arms of project 6**, whose counts were measured on
board A on Sunday 1 November 2026:

    arm A, instruction last     20 of 20 echo the instruction      matches the record
    arm B, instruction third     0 of 20 echo the instruction      matches the record
    arm B                        3 of 20 echo some other sentence   the rules section

**Coverage is measured against the PROMPT SENTENCE and not against the answer**, and that is
the one decision in here that had to be corrected by the cases it got wrong. Measured against
the answer, two of arm A's twenty rows escaped: both carried the whole eleven-word instruction
and then appended range numbers, which lengthened the answer and pushed the ratio under the
threshold. An echo with rubbish after it is still an echo. Measured this way the honest answer
stays out: "a wall is in front of the sensor" shares a five-word run with a fourteen-word
instruction and so covers 36 per cent of it.

**The limit, stated because the tool looks more general than it is.** It finds a VERBATIM
span and nothing else. Project 19's note reads "Check the message type, peak value, and time."
and its longest run shared with any prompt sentence is three words out of thirty-four: the
model paraphrased, and no string comparison can see that. So a zero here is not a clean bill,
and a project whose model paraphrases needs a different instrument. Read it beside the shape
count of `tools/answer_shapes.py`, which catches a template the prompt did not supply.

Not in CI: it needs a ledger, and a ledger needs a run. Added Thursday 12 November 2026,
after narrowing the grammar class was measured and refuted as the alternative remedy.
"""
import argparse
import csv
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def words(text):
    return re.findall(r"[a-z0-9]+", text.lower())


def sentences(text):
    """The prompt's sentences, split on a full stop or semicolon followed by space."""
    return [s.strip() for s in re.split(r"(?<=[.;])\s+", text.strip()) if s.strip()]


def longest_run(answer, source):
    """Length in words of the longest contiguous word run shared by the two."""
    a, b = words(answer), words(source)
    best = 0
    for i in range(len(a)):
        for j in range(len(b)):
            k = 0
            while i + k < len(a) and j + k < len(b) and a[i + k] == b[j + k]:
                k += 1
            if k > best:
                best = k
    return best


def echo(answer, prompt_sentences, min_words=5, coverage=0.70):
    """(is_echo, which sentence, run length, coverage of that sentence).

    Coverage is run length over the length of the PROMPT SENTENCE; see the module docstring
    for the two cases that fixed the direction.
    """
    if not words(answer):
        return (False, None, 0, 0.0)
    best = (0, 0.0, None)
    for s in prompt_sentences:
        run = longest_run(answer, s)
        cov = run / float(max(1, len(words(s))))
        if (run, cov) > (best[0], best[1]):
            best = (run, cov, s)
    run, cov, which = best
    return (run >= min_words and cov >= coverage, which, run, cov)


def field_from_grammar(ledger):
    """The free-text rule name for the project this ledger sits under, or None."""
    parts = os.path.abspath(ledger).split(os.sep)
    if "projects" not in parts:
        return None
    stem = parts[parts.index("projects") + 1].split("-")[0]
    sys.path.insert(0, os.path.join(ROOT, "tools"))
    import free_text_slots
    for gram, rule, _lo, _hi, _verdict, _ex in free_text_slots.survey():
        if gram.split("_")[0] == stem:
            return rule
    return None


def prompt_for(ledger):
    """The prompt.txt of the project this ledger sits under, or None."""
    parts = os.path.abspath(ledger).split(os.sep)
    if "projects" not in parts:
        return None
    slug = parts[parts.index("projects") + 1]
    path = os.path.join(ROOT, "projects", slug, "prompt.txt")
    return path if os.path.exists(path) else None


def scan(rows, field, prompt_text, min_words=5, coverage=0.70):
    """(total with a value, echoing any sentence, echoing the last one, examples)."""
    ps = sentences(prompt_text)
    instruction = ps[-1] if ps else ""
    n = n_any = n_instr = 0
    examples = []
    for i, row in enumerate(rows, 1):
        cell = (row.get("model") or "").strip()
        if not cell:
            continue
        try:
            value = str(json.loads(cell).get(field, "")).strip()
        except Exception:
            continue
        if not value:
            continue
        n += 1
        is_echo, which, run, cov = echo(value, ps, min_words, coverage)
        if is_echo:
            n_any += 1
            if which == instruction:
                n_instr += 1
            if len(examples) < 5:
                examples.append((i, run, cov, value[:60]))
    return n, n_any, n_instr, examples


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("ledger")
    ap.add_argument("--field", help="free-text field; default from the grammar")
    ap.add_argument("--prompt", help="prompt file; default the project's prompt.txt")
    ap.add_argument("--min-words", type=int, default=5)
    ap.add_argument("--coverage", type=float, default=0.70)
    args = ap.parse_args(argv)

    field = args.field or field_from_grammar(args.ledger)
    if not field:
        print("prompt_echo: no free-text field known for this ledger; pass --field")
        return 1
    prompt = args.prompt or prompt_for(args.ledger)
    if not prompt:
        print("prompt_echo: no prompt.txt found for this ledger; pass --prompt")
        return 1
    with open(args.ledger, newline="", encoding="utf-8") as fh:
        rows = list(csv.DictReader(fh))
    text = open(prompt, encoding="utf-8").read()
    n, n_any, n_instr, examples = scan(rows, field, text, args.min_words, args.coverage)

    print("ledger   %s" % os.path.relpath(os.path.abspath(args.ledger), ROOT))
    print("field    %s" % field)
    print("prompt   %s, %d sentences" % (os.path.relpath(prompt, ROOT),
                                         len(sentences(text))))
    print("answers with a value   %d" % n)
    print("echo the last sentence %d" % n_instr)
    print("echo any sentence      %d" % n_any)
    for line, run, cov, value in examples:
        print("  row %-5d run %2d words, %.0f%% of that sentence: %r"
              % (line, run, 100.0 * cov, value))
    if n and not n_any:
        print()
        print("NOTE: zero verbatim echoes is not a clean bill. This finds a contiguous span")
        print("and cannot see a paraphrase; read it beside tools/answer_shapes.py.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
