# e20: two acceptance clauses swept across every ledger, and one of them is not
# generically measurable, Thursday 12 November 2026, 14:20

    host       aquamarine, the authoring laptop. NO BOARD, no engine, no model
    subject    all 26 ledgers under projects/*/evidence/*.csv that a past run wrote
    clauses    one, zero rows with no parseable object; and the no-invented-numbers
               clause that several chapters carry
    tree       f7967e4

**NO RUN WAS TAKEN.** Both sweeps read files already in the repository. One found nothing,
which is a statement about the volume, and the other found that the clause cannot be checked
by a shared tool at all, which is a statement about the clause.

## CLAUSE ONE: FIVE LEDGERS, ALL FIVE ALREADY DOCUMENTED

    ledger                      rows   objects never closed   where it is recorded
    e02_run.csv                 1000    949,  94.9 percent    published in its record and
                                                              named in agreement.py's own
                                                              comment of Saturday 17
                                                              October 2026
    e04_replay.csv               180    136,  75.6 percent    its record, and the chapter
                                                              as of today
    e06_armB.csv                  20     14,  70.0 percent    its record, under the heading
                                                              "FOURTEEN OF TWENTY ARM B
                                                              ANSWERS NEVER CLOSED"
    e06_descent_sampled.csv       50      3,   6.0 percent    the record of Friday 6
    e06_descent_smol.csv          50      3,   6.0 percent    November 2026, counted twice
                                                              by two independent instruments

Twenty-one of the twenty-six ledgers have no unclosed object at all. **So the sweep found no
undocumented case**, and the one that had been undocumented in a chapter rather than in a
record was corrected earlier today. The practice holds: every run that produced truncated
answers says so where it was written up.

## CLAUSE TWO IS NOT MEASURABLE BY A SHARED TOOL, AND THAT IS THE FINDING

A generic check was written: take the numbers in the free-text answer, and flag any that do
not appear in the event's `raw` column. **It reported 133 of 200 rows in four e03 ledgers, 44
of 44 in e04, and 13 of 20 in one e06 ledger. Every one of those is wrong**, for four separate
reasons that only reading the rows revealed:

    rounding      e03's raw is 36.96 and the answer says "level 37.0". Correct to one
                  decimal, which is what that project asks for
    magnitude     e03's slope is -1.55 and the answer says "slope 1.6 per second". The
                  project asks for the rate, and the sign is carried by the verdict
    unit change   e04's raw is 46260 millidegrees and 1.5e+09 hertz; the answer says
                  "46.3 C" and "1500 MHz". Both are conversions the prompt requires
    history       e06's prompt carries the last ten ranges, so a number absent from this
                  row's raw can still be in the event the model was shown

And a fifth reason was mine: three rows survived the first four and failed on floating point,
the difference 1.6 minus 1.55 being 0.050000000000000044 against a tolerance of 0.05.

**So "a number that is not in the event" means something different in each project**: verbatim
in e19, whose prompt says copy the three fields and do not round them; rounded in e03;
converted in e04; and drawn from a ten-event window in e06. A single checker that enforced any
one of those would report a defect in the other three.

**This justifies the volume's architecture rather than finding a gap in it.** Each project
carries its own checker under `projects/<slug>/tools/`, and the reason is not duplication: the
clause is project-specific in its substance and not only in its thresholds. The one thing a
shared tool can honestly contribute is what `tools/answer_shapes.py` does, counting sentence
forms, because that question has the same meaning everywhere.

## WHAT THIS DOES NOT ESTABLISH

    established   21 of 26 ledgers have zero unclosed objects; the other 5 are documented
    established   no undocumented violation of clause one exists in the tree
    established   the no-invented-numbers clause cannot be checked generically, with four
                  project-specific meanings of "in the event" demonstrated
    withdrawn     the generic check's own output, in full: 133, 44 and 13 rows flagged in
                  six ledgers, every one of them a false positive
    NOT shown     whether each project's own checker is correct; this read the clause and
                  not the implementations
    NOT a gap     the absence of a shared numbers checker, which would be the wrong tool
