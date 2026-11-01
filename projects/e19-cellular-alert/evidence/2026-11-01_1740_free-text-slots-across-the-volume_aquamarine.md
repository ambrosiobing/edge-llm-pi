# Fourteen of fifteen free-text slots admit their own instruction, Sunday 1 November 2026, 17:40

    host       aquamarine, the authoring laptop; no board, no engine, no model
    subject    all 20 grammars under grammars/, and each project's prompt.txt
    tool       tools/free_text_slots.py, added in the same commit as this record
    tree       004bc9c
    method     static; the only question asked is whether a sentence of a project's own
               prompt is a legal instance of its own free-text rule, verbatim

**This closes no clause of any project.** It sizes a defect that was measured twice this
week and had been treated as two projects' problems. It lives in e19's evidence because
e19's run of the same afternoon is what prompted it and because e19's headline figure is
one of the two the defect invalidates, but its subject is the whole volume.

## The two measured instances

    project 6    40 of 40 answers put the prompt's instruction in `beam`, Friday 30
                 October 2026, real engine, board A at four threads
    project 19   1 of 4 notes was the instruction, 3 of 4 were one identical constant,
                 so 0 of 4 said anything about the event, Sunday 1 November 2026

In project 19 the field checker reported `0 invented a number` on that same run. **That was
not a pass**: it refuses a number absent from the event and none of the four notes contained
a digit, so it had nothing it could refuse.

## The survey

Fifteen of the twenty grammars end a rule with a character class and an explicit repetition
range, which is how this volume lets a model write prose under a grammar.

    grammar            rule        min  max  admits its own prompt sentence
    e03_colour         reason       10  120  yes
    e04_thermal        summary       9  159  yes
    e05_event          sentence      8  120  yes
    e06_range          beam          8  120  yes    MEASURED, 40 of 40
    e07_pressure       sentence     12  130  yes
    e08_answer         answer       19  159  yes
    e11_impact         line          8  120  yes
    e12_band           sentence      8  100  NO
    e13_presence       why           8   96  yes
    e14_inspect        summary       9  159  yes
    e16_interval       reason        9  159  yes
    e17_status         report       39  399  yes
    e18_drift          summary       9  159  yes
    e19_message        note          4   64  yes    MEASURED, 1 of 4 plus 3 of 4 constant
    e20_next           reason       20  200  yes

    14 of 15 admit a sentence of their own prompt verbatim

## THE ONE EXCEPTION IS AN ACCIDENT, WHICH IS WHY IT IS THE USEFUL ROW

    e12_band   sentence ::= [A-Z] [a-z0-9 ,.%-]{8,100} "."

**Project 12 refuses its own instruction because its class admits no capital letter after
the first character.** Any sentence naming "Raspberry Pi", or a unit written "C", or a
literal such as "I60", is outside it. Nothing in the chapter suggests that was the intent;
it reads as a choice about sentence case that happens to exclude most of English prose.

That single false case is what makes this check worth running rather than assuming. A check
that only reported "this class admits English" would fire on all fifteen and discriminate
nothing, which is the rule that an experiment must be able to fail, applied to the check
itself.

## What this does and does not establish

Admitting the instruction is **necessary for the echo and not sufficient.** It says the
failure is available, not that it occurs; only a run with the real engine says that, and two
such runs say yes. The other thirteen slots are **unmeasured, not cleared**, and eleven of
the thirteen belong to projects that have never run on hardware.

The tool also knows nothing about what any project's downstream checker would do with an
echo. Project 19 has one and it was defeated by prose; project 6 has none. That is the
question a real gate would ask, and it cannot be answered statically.

## Why it is not wired into CI

`--check` exits 1 today on 14 of 15 slots. **A gate that fires on almost everything teaches
a reader to ignore it**, which is worse than no gate, and this volume already has a case
where a named pitfall was mistaken for a gate. It belongs in CI once the slots are narrowed
or once each project carries a check that rejects its own instruction, and the tool's
docstring says so in the file rather than only here.

`grammar_pairs.py` remains the only grammar gate, and it pairs 19 grammars **without parsing
either copy**, so no check in this repository has ever looked at what a grammar admits.

## What is established and what is not

    established   15 of 20 grammars carry a free-text slot of the class-plus-range shape
    established   14 of those 15 admit a sentence of their own prompt verbatim
    established   e12 is the only refusal, and its cause is a case restriction
    established   two of the fourteen are measured and both show the echo happening
    NOT shown     anything about the other thirteen beyond availability
    REFUTED       that a reordered prompt removes the echo. Tested on e06 the same
                  evening with one variable, the sentence order, and the model echoed
                  whatever became last instead: the decision literals and the input
                  ranges. See evidence/2026-11-01_1810_prompt-order-is-not-the-fix in
                  project 6. The remedy proposed below is withdrawn.
    NOT a gate    --check fails 14 of 15 and is deliberately outside CI
