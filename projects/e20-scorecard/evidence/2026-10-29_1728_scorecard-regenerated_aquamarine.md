# e20: the scorecard regenerated, and the one clause it closes, Thursday 29 October 2026, 17:28

    host       aquamarine, the authoring laptop; Python 3.12, no board involved
    tool       projects/e20-scorecard/tools/make_replay.py, which takes no arguments
    engine     the CI stub, so no model and no server ran
    tree       clean at 0f8cc23 before, three files written, the oracle byte identical

**One half of e20's acceptance is now met and the other half is not.** The acceptance has two
clauses: the model's first identifier is the rule's on at least 40 of the 50 presentations,
and the scorecard has twenty rows with no empty cell, every cell that is not a number reading
"not measured" with its reason. **The second clause is met.** The first needs a model and has
not been run.

## Why this project was the one picked up

Fourteen of the twenty projects carry no dated record. Of those fourteen, **e20 is the only
one that is startable today**, and the reason is in its own key facts: the HAT slot is empty,
the header is empty, there is no sensor and no instrument, and the model is served by
`llama-server` on skyhorizon rather than by the board. So it is the one project that needs
neither the cooling part that four other clauses now wait on, nor the Yocto card that e05 and
e07 wait on.

## THE SNAPSHOT WAS STALE, AND THE TOOL SAYS WHEN TO REGENERATE IT

`make_replay.py` carries its own maintenance rule in its docstring:

    "Regenerate all three together, and the chapter's scorecard table with them, whenever a
     project gains evidence."

Three projects had gained bench evidence since the snapshot was written on Wednesday 14
October 2026, and nobody had run it. Before regenerating, the two modes disagreed, and the
guard caught it rather than letting the disagreement through:

    --stub          the rule's three on the snapshot                  e01, e02, e03
    --live --stub   REFUSED: "the replay row at t_s 0.0 expects e01, e02, e03 and the rule
                    on the repository as it is now answers e01, e05, e06; regenerate the
                    replay and the oracle with tools/make_replay.py"

That refusal is the design working. The snapshot exists so that a replay run is scored
against a fixed table, and the guard is what stops a drifted repository from silently
rebasing the chapter's number.

After regeneration both modes agree: **e01, e05, e06**, 50 of 50, on the snapshot and on the
repository alike.

## What changed, which is three rows and nothing else

    e02,1000,82.3 percent,177,0,0,CI stub    ->  e02,100,2.0 percent,0,98,0,bench
    e03,200,67.5 percent,65,0,0,CI stub      ->  e03,200,100.0 percent,0,0,0,bench
    e04,180,35.0 percent,117,0,0,CI stub     ->  e04,180,17.2 percent,9,4,136,bench

**AND THE ORACLE DID NOT CHANGE AT ALL.** `tests/oracle/e20.expected.csv` is byte identical
across the regeneration, md5 `5a78a7f2e59ed5a6537feed83b7d0cde` before and after, because the
stub still answers `e01` three times and the rule's first pick is still `e01`. So the CI
fixture was not disturbed by a change that moved the snapshot and all fifty presentations.
Only `scorecard_snapshot.csv` and `scorecards_50.csv` differ, 53 insertions and 53 deletions.

## THE STUB FIGURES IT REPLACED WERE WRONG BY UP TO EIGHTY POINTS

This is the chapter's own thesis, that a CI stub run is not evidence, as a measured quantity
rather than an assertion. Where a bench log has now replaced a stub figure:

    project   the stub said   the bench says   error
    e02       82.3 percent    2.0 percent      -80.3 points
    e03       67.5 percent    100.0 percent    +32.5 points
    e04       35.0 percent    17.2 percent     -17.8 points

Wrong in both directions and by a wide margin in both. **This is why `rank.py` sorts on
`rank_n` and `rank_agreement`, which read the bench figures alone and give every stub row a
count of 0 and a rate of minus one.** Had the ranking used the printed figures, the stub's
82.3 percent would have pushed e02 down the list of things to measure next on the strength of
a number that turned out to be 2.0.

## Why the rule's three moved, which is not what it looks like

The three moved from `e01, e02, e03` to `e01, e05, e06`, and a reader could take that for e05
and e06 having got worse. They have not. **Seventeen of the twenty projects tie at exactly
the same rank key**, `(0, -1.0)`, because they have no bench run log, and the identifier is
the third and last key, so it breaks the tie alphabetically. e02, e03 and e04 left the tie by
landing bench logs, which moved them from the front of the ranking to the back of it. e05 and
e06 did not move at all; the two projects that had been standing in front of them stepped out.

    e01  rank key (0, -1.0)  no record at all, and no oracle file either
    e05  rank key (0, -1.0)  2000 CI stub rows, which the ranking counts as nothing
    e06  rank key (0, -1.0)  2825 CI stub rows, which the ranking counts as nothing

**e05 and e06 both carry dated records written this week, and the ranking is still right to
call them unmeasured.** The generator counts a run log, `evidence/*.csv`, and both of their
records say in their own first paragraph that they close no clause. A narrative record is not
a measurement, and the scorecard and the records agree about that.

## A ROW THAT IS READING A LOG FROM BEFORE A FIX LANDED

e04's row says 17.2 percent, and the figure is arithmetically sound but it is not a fact about
the model. The row reads:

    e04,180,17.2 percent,9,4,136,bench

136 of the 180 rows carried no literal at all. Those are the unclosed objects found and fixed
in `projects/e04-thermal-narrator/src/e04_client.c` on Tuesday 27 October 2026, where the
client printed `no literal 0` and counted nothing. The arithmetic closes exactly:

    agreed                 31   = 17.2 percent of 180
    carried a literal      44   = 180 - 136
    31 agreed + 9 quieter + 4 louder = 44, which is every row that carried a literal

**So 17.2 percent is 31 of 180 and the rate over the rows that actually carried an answer is
70.5 percent.** The scorecard is reporting a number that a since-fixed defect produced, and
e04's row will move when e04 is re-run with the fixed client. The row is left as it is,
because the snapshot is a dated record of what the repository holds and the repository holds
the old log; what is wrong is not the row but the absence of a new run.

## ONE CLAIM IN THE CHAPTER IS NOW OBSOLETE

e20's prose says the engine is not on a board because the 1.5B rung cannot be shown to fit:

    "1.12 GB under 60 percent of MemAvailable needs about 1.87 GB available after boot, which
     neither 1 GB Pi 3 has and which the Pi 4 Model B is not known to have, because that
     board's memory has never been read. Project 1 reads it with free -m, and until then the
     1.5B rung on the Pi 4B is read it and not yes."

**That board's memory has been read.** e01's evidence of Friday 16 October 2026 records
`MemTotal 8,007,396 kB, MemAvailable 7,789,840 kB`, two days after e20 was written. The rung
arithmetic now resolves:

    MemAvailable       7,789,840 kB  =  7.98 GB
    the rung needs     1.87 GB for a 1.12 GB model at 60 percent
    headroom           4.3 times, the model being 14.0 percent of MemAvailable

So the 1.5B rung on board A is **yes**, not "read it", and the stated fallback of Qwen2.5-0.5B
on the board is a weaker fallback than the board can now carry.

**This does not change the recommendation, and the reason is heat rather than memory.** Fifty
presentations at `n_predict` 192 is a long stretch of near-continuous generation, and board A
trips its thermal limit after 367 seconds of load. e20's acceptance happens to carry no
`get_throttled` clause, unlike e06's, so a throttled run would not be discarded, only slowed.
Remote inference on skyhorizon remains the better sitting and remains what the chapter
describes. What needs correcting is the sentence, which gives a reason that is no longer true.

## What is established and what is not

    established   clause two: twenty rows, no empty cell, 20 of 20 carrying a note,
                  17 carrying numbers and 3 reading "not measured" with a reason
    established   the snapshot, the fifty presentations and the repository now agree
    established   the oracle is byte identical, so CI is undisturbed
    established   the stub figures were wrong by -80.3, +32.5 and -17.8 points
    established   e04's 17.2 percent is 31 of 180, and 70.5 percent over carried rows
    NOT shown     clause one, the model's first pick against the rule's over 50 presentations
    NOT shown     anything about the real model, since only the stub ran
    NOT shown     any timing figure, since no engine answered

## Gates

    line_check             524 files, 0 problems
    no_trace_scan          670 files, 0 hits
    no_authoring_files     0 refused
    grammar_pairs          19 grammars paired, 0 problems
    verify_e02_figures     43 claims checked, 0 confirmed figures do not match their column
    pytest                 46 passed, e20's own suite
