# e18: the degeneracy guard watches the rule column, and the model column is the degenerate
# one, Monday 9 November 2026, 09:16

    host       aquamarine, the authoring laptop. NO BOARD, no engine, no model, no HAT
    subject    tests/oracle/e18.expected.csv, 600 rows, and the test that guards it,
               projects/e18-industrial-logger/rust/tests/oracle.rs
    method     counted the distinct values in each column of the oracle file, and read
               what the test asserts against what its own comment says it is for
    tree       90b7917 plus the working tree of this session

**THIS CLOSES NO CLAUSE.** It corrects the reading of a figure this volume has carried since
Friday 16 October 2026, and it withdraws part of the provenance of the record written at
08:44 this morning.

## THE MODEL COLUMN HOLDS ONE LITERAL ON ALL 600 ROWS

    column   distinct values   content
    tag                    1   rs485a
    rule                   2   NOMINAL 542, DRIFT 58
    model                  1   {"decision":"NOMINAL","summary":"."}

    the stub's agreement, after parsing the JSON: 542 of 600, 90.3 percent
    the NOMINAL count of the rule column:         542 of 600, 90.3 percent
    the two are the same number, and not by coincidence

The stub answers one byte-identical string on every one of the 600 frames. So its agreement
is the class balance by construction, and it cannot be anything else. **The chapter's
sentence was therefore exactly right and I read it as weaker than it is**: "the NOMINAL count
is the agreement the stub reaches in CI" is not an observation about the stub, it is an
identity.

## THE GUARD IS NAMED FOR THIS FAILURE AND CANNOT CATCH IT

The test is `the_oracle_file_carries_both_verdicts_so_the_comparison_can_fail`, and its
comment states the hazard in as many words:

    A replay whose oracle column held one literal only would pass this test with a
    rule that always answered that literal.

It then asserts `nominal == 542`, `drift == 58`, and that the two sum to the row count. Every
one of those assertions is about the **rule** column, which has two literals and is the column
in no danger. **What no test does is check the model column for more than one distinct
value**, which is the check this comment describes, and the guard's name promises a comparison
that can fail while every assertion inside it is on the column that was never at risk.

**CORRECTED AT 10:40 THE SAME DAY, and the first version of this record overstated it.** It
said no test asserts anything whatever about the model column. Two do.
`the_replayed_register_stream_matches_the_oracle`, in this same file, compares the model column
row by row against the oracle; and `src/abi.rs` pins the exact literal in
`the_stub_answers_the_first_literal_on_every_path`, asserting the stub returns
`{"decision":"NOMINAL","summary":"."}` for any input. **So the project documents and tests the
constant rather than being blind to it**, and the implication that it was blind is withdrawn.
What survives is narrower: a degeneracy check applied to one of the two columns, and a test
name that reads as covering both. The remedy is unchanged, since the cost is a misreading.

This is the third instance of the same class in this volume and the first found in a gate
rather than in a published figure:

    grammar_pairs.py   NOT an instance. Corrected Wednesday 11 November 2026, see below
    free_text_slots.py measured the exposure and stood outside CI, so the count could grow
                       unseen. CLOSED Wednesday 11 November 2026 by a ratchet
    agreement.py       printed a rate with no constant baseline. FIXED Friday 6 November 2026
    datecheck.py       counted weekday names without computing the weekday. FIXED
    oracle.rs          guards the two-literal column, ignores the one-literal column. THIS

**CORRECTED Wednesday 11 November 2026, and the correction removes one row and adds another.**
`grammar_pairs.py` does not belong in this list and was carried in it for days. It promises
one thing in its docstring, that a grammar's two copies are byte identical, and it delivers
exactly that, with CRLF normalised and with a reasoned skip for the one project that decides
nothing. Its not parsing is a stated decision, written down beside the reason: a grammar
differing only in whitespace is still two files somebody edited once and not twice. A tool
that keeps its own promise is not an instance of a gate checking the form and not the fact,
and listing it as one was a claim about the code that reading the code refutes.

**The gap behind the row was real, and it belonged to the other tool.** Nothing in CI could
see that fourteen of fifteen free-text slots admit a sentence of their own prompt verbatim,
not because `grammar_pairs.py` fails to parse, but because `free_text_slots.py`, which does
measure it, was deliberately left out of CI: a gate failing on fourteen of fifteen teaches a
reader to ignore it. That is now closed by a third route its own docstring did not list. A
ratchet passes on exactly the fourteen that admit today and refuses a fifteenth joining them,
one of the fourteen being narrowed without the baseline moving, and a listed slot leaving the
tree. It fires on nothing that stands, so it cannot train anyone to ignore it, and it cannot
be satisfied by making the problem bigger. The fourteen remain fourteen open defects.

## WHAT THIS WITHDRAWS FROM THIS MORNING'S RECORD

The 08:44 record presents 542 NOMINAL, 58 DRIFT and 90.3 percent as computed fresh from the
replay file before any run. The arithmetic was correct and the framing was useful, but **the
two counts were already in the tree and already asserted in CI**, since Friday 16 October
2026, generated by `tools/make_replay.py` on Wednesday 14 October 2026. I computed by hand a
pair of numbers the project's own test suite checks on every push, without reading the test
suite first.

What was genuinely new in that record, and stands:

    new      the reading of 542 as a constant baseline a model has to beat, rather than
             as a count of rows
    new      the four transitions, at frames 180, 209, 400 and 429
    new      the two 29-frame excursions, and the reason: the 30-frame window sliding
             across an injected step
    new      register 0 spanning 410 to 601 counts across the file
    withdrawn the implication that 542 and 58 were established this morning

## WHAT THIS MEANS FOR CLAUSE TWO, WHICH IS NARROWER THAN THE RECORD SAID

Clause two is `python tools/agreement.py .../evidence/e18_replay.csv --md`. It consumes a
ledger that a run writes, and that ledger does not exist yet. **Run with the stub, the clause
can only ever produce 90.3 percent**, because the stub is a constant answerer, so running it
that way would publish the class balance as though it were a result. That is what chapter 20
did at 30.0 against a constant's 100, and what chapter 6 did at 54.0 against a constant's
54.0. The clause needs the real engine or it needs withdrawing.

**And it needs no HAT, which project.toml had wrong.** That file grouped the two acceptance
items together as both needing the WS-28164 on card B. The echo does. The agreement table
does not: the service reads its register stream with `--replay <csv>`, a file, and touches no
serial port on that path. Corrected in project.toml in the same change as this record.

## WHAT THIS DOES NOT ESTABLISH

    established   the model column of the oracle file holds exactly one distinct string
    established   the stub's agreement is 542 of 600 and is the class balance by identity
    established   no test checks the model column for more than one distinct value, which
                  is the degeneracy check the oracle test's comment describes
    established   two tests DO assert over that column: the row by row oracle comparison,
                  and abi.rs pinning the literal the stub returns on every path
    withdrawn     the first version's claim that no test asserts anything about it
    established   clause two needs no HAT and no board; it needs a run with the engine
    NOT shown     anything about a real model's answers on this stream
    NOT DONE      a guard over the model column. The stub answering one literal is its
                  documented behaviour, so demanding variety of the stub would be wrong;
                  what the volume needs is the baseline printed beside every rate, which
                  tools/agreement.py has done since Friday 6 November 2026
