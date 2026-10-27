# e03 at two threads, the throttle clause closed, Tuesday 27 October 2026, 21:40

    board      Raspberry Pi 4 Model B Rev 1.4, 8 GB, Cortex-A72, two threads
    card       eplepi, Raspberry Pi OS, Debian 13 trixie
    HAT        none, bare header
    parts      three Joy-it LK-LED10 modules, green on GPIO17, yellow on GPIO27,
               red on GPIO22, grounds on header pins 14, 20 and 25, V pins
               unconnected on all three, unchanged since the 15:03 run and its
               photograph
    model      SmolLM2-360M-Instruct-Q4_K_M.gguf, 270590880 bytes,
               sha256 2fa3f013dcdd7b99f9b237717fa0b12d75bbb89984cc1274be1471a465bac9c2
    engine     llama.cpp v0.6.0, the pinned tag
    source     replay/state_200.csv, 200 events
    log        evidence/e03_run_threads2.csv, 201 lines, 31569 bytes

The same two hundred events as the 15:03 run of the same day, with the thread count moved
from four to two and nothing else touched. It closes the one clause e03 was failing, and
because it changes exactly one variable it also settles a thread-scaling figure that two
other records had wrong.

## What was measured

    e03: 200 events, 200 model answers, 0 model errors, 0 actuation errors,
         mean prompt 2555.8 ms, mean generate 3923.1 ms, rss 351284 kB,
         engine llama.cpp v0.6.0, source replay

    N 200, scored 200, empty 0, unparsed 0
    colour agreement   200 of 200, 100.0 percent
    reason names field 200 of 200, 100.0 percent

    rule \ model      GREEN   YELLOW      RED
    GREEN               135        0        0
    YELLOW                0       38        0
    RED                   0        0       27

    get_throttled   0x0 before, 0x0 after
    temperature     69.6 C at exit

The matrix is diagonal with no off-diagonal cell, and the class counts are identical to the
four-thread run: 135 GREEN, 38 YELLOW, 27 RED.

## The clauses, all four

    colour agreement over 200 events            PASSES   200 of 200, 100.0 percent
    reason names the field the rule triggered   PASSES   200 of 200, 100.0 percent
    zero rows with no parseable object          PASSES   0 of 200 unparsed, 0 empty
    get_throttled 0x0 before and after          PASSES   0x0 before, 0x0 after
    LED current from the PPK2 figures           already measured Saturday 3 October 2026

**The acceptance was not touched to get here.** e03's clause list says `0x0` before and
after and says nothing about thread count, so a two-thread run answers the clause as
written. What the clause list does not govern, and what this record must therefore state
plainly, is in the deviations below: `wiring.md` prints `--threads 4`.

## The thread count, measured as a control rather than inferred

Both runs are the same binary over the same replay of 200 events on the same board, model
and engine tag, on the same day, with the LEDs attached in the same order. The only
difference is `--threads`.

    threads  prompt      generate    total       tokens  per token   rss         register
    4        1320.7 ms   3448.3 ms   4769.0 ms   28.30   121.9 ms    352896 kB   0xd0000
    2        2555.8 ms   3923.1 ms   6478.9 ms   28.30   138.6 ms    351284 kB   0x0

    prompt      1.94 times faster at four threads
    generate    1.14 times faster at four threads
    end to end  1.36 times faster at four threads

**The two halves of a decision do not scale alike, and that is the finding.** Prompt
processing nearly halves with twice the threads, because it is a batched matmul over the
whole prompt at once and the cores all have work. Generation gains 14 percent, because it
produces one token at a time and is waiting on memory far more than on arithmetic. A single
end-to-end figure averages those two behaviours together and describes neither.

Generated length is identical at **28.30** tokens mean on both runs, and nothing reached the
64-token ceiling in either, so the per-token figures compare directly rather than by
normalising across different budgets.

**The effect is not an average over noise: all 200 rows generated slower at two threads.**
Row by row, not once in two hundred did the two-thread run match or beat the four-thread one
on `generate_ms`. A 14 percent mean that holds unanimously across every event is a property
of the configuration rather than a difference between two samples, which is what lets this
pair carry a ratio at all.

And the comparison is tighter than same-model-same-replay. The `model` column of the two logs
is byte-identical on all 200 rows, so **these are the same two hundred answers timed twice**,
not two runs that happened to agree. See the provenance section for the column-by-column
comparison that establishes it.

## What this withdraws from two other records

On Tuesday 27 October 2026 the e02 and e04 records both claimed **four threads are 1.41
times faster per token**, from e04 at four threads (86.8 ms) against e02 at two (122.1 ms).
That is withdrawn. The controlled figure is **1.14**.

    run   route                   threads  tokens  per token
    e04   llama-server over HTTP        4   64.00     86.8 ms
    e03   in process, 15:03             4   28.30    121.9 ms
    e03   in process, this run          2   28.30    138.6 ms
    e02   in process                    2    9.09    122.1 ms

Read down the middle two rows: e03 at four threads and e02 at two threads differ by a thread
count and land **two tenths of a millisecond apart**. Read the top two: e04 and e03 share a
thread count and sit **29 percent apart**. So the 41 percent was never a thread effect. It
was the route and the generation length, and the thread change happened to be riding along
with them.

This is the second wrong conclusion drawn from that same pair. The first divided by the
token budget instead of the `n_generated` column and reported 6.6 percent; correcting the
arithmetic produced 40.7 percent and a confident claim about cores, which was wrong for an
entirely separate reason. **Fixing the arithmetic of a confounded comparison leaves it
confounded.** The repair was not a better calculation, it was running the one experiment
that moves one variable.

## The supply boundary, which this run redraws

    run   threads  events  duration         register after
    e02         2     100  about 6.5 min    0x0
    e03         2     200  about 21.6 min   0x0
    e03         4     200  about 16 min     0xd0000
    e04         4     180  about 24 min     0xf0000

All four began on a clean boot at `0x0`. **The variable is thread count, not duration.**
This run went about twenty-two minutes at two threads and finished clean, which is longer
than the four-thread run of this same project that dirtied the register at about sixteen.
Three earlier records read the rule as thread count and duration together; duration is not a
necessary term, at least out to twenty-two minutes, and those records have been corrected.

The thermal side agrees. This run exited at **69.6 C with bit 19 clear**, where the
four-thread run set bit 19 along with under-voltage and throttling. Two threads on this
supply is a working configuration and four is not.

So e03's latency figures in this record are the **clean** ones for this project, and the
15:03 figures are upper bounds taken through undervolt. Both are kept: the four-thread run
is the chapter's printed command and its agreement fractions are equally good.

## Resident memory, which the 1 GB rung needs

351284 kB at two threads, which is **343.0 MiB**, against 352896 kB and 344.6 MiB at four.
Thread count does not move the footprint; the 1.6 MiB difference is about what one fewer
worker's stacks and scratch buffers account for.

Against the 3B+ reading of 741548 kB `MemAvailable` taken the same evening, 343.0 MiB is
**47.4 percent** of available memory, with the model file itself at 258.1 MiB being 35.6
percent. Both sit under the 60 percent rule, so the 1 GB rung is expected to run without
paging, and that expectation is written down here before the run rather than after it.

## Deviations from what the chapter prints

1. **`wiring.md` prints `--threads 4` and this run used two.** The clause list does not
   mention threads, so the clause is satisfied as written, but the printed command is not
   what satisfied it. The honest statement of e03's power result is therefore: **the
   throttle clause is closable on this board at two threads and fails at four.** Whether
   the chapter's printed thread count changes is a decision about the chapter, not a
   finding, and it is left open here rather than settled by publishing whichever run passed.
2. **The board is the Pi 4 Model B Rev 1.4, not the Pi 3 Model B the chapter allocates**,
   as in the 15:03 run and for the same reason. The acceptance is board-independent; the
   latency and resident-memory figures belong to a Cortex-A72 with 8 GB.
3. **Run as `bing` from a root login rather than through the systemd unit.**

## How the log reached this repository

Counted on the board after the writer exited: **201 lines, 31569 bytes, zero carriage
returns**.

Those are also, exactly, the counts of `e03_run.csv` from the four-thread run: 201 lines and
31569 bytes, both files. An identical byte count across two runs is the signature of a
duplicated file, so it was checked rather than accepted, and the check shows the equality is
**what this log format guarantees** rather than a coincidence at all.

Counting the bytes each column contributes to the four-thread log:

    t_s            1890      the replay row index, 0.000000 to 199.000000
    tag            1000
    raw            2065
    rule            984
    model         13766
    prompt_ms      1600      width 8 on all 200 rows
    generate_ms    1600      width 8 on all 200 rows
    n_generated     400      width 2 on all 200 rows
    engine         3200
                  -----
                  31569 total, of which 3200 are the two timing columns

**The timing fields are fixed width.** They are written to three decimals, so every value
between 1000.000 and 9999.999 occupies exactly eight characters, and both runs sit inside
that bracket on every row. `n_generated` is two characters on every row, every row having
between 10 and 99 tokens. `t_s` is the replay index read from `state_200.csv` and not a wall
clock, so it is identical between the runs by construction rather than by luck, which is the
column that would otherwise have had to differ between a sixteen-minute and a twenty-two
minute run.

So the byte total cannot move unless the row count changes, a timing value leaves its digit
bracket, or the model's output changes length. It did not move, and comparing the two files
column by column over all 200 rows says exactly which columns moved:

    t_s          identical          prompt_ms     200 of 200 rows differ
    tag          identical          generate_ms   200 of 200 rows differ
    raw          identical          n_generated   identical
    rule         identical          engine        identical
    model        identical

**The model column is byte-identical on 200 of 200 rows.** The two runs produced the same two
hundred answers, character for character, and the only bytes that differ anywhere in the two
files are the two timing columns. `cmp` on the board agrees, reporting its first difference at
byte 176 of line 2, which falls inside row one's `prompt_ms` field at bytes 172 to 181.

That was not guaranteed. Thread count changes the order of floating-point reductions in the
engine, so identical output across it is a result rather than a formality. It means this pair
is not two runs that happened to score alike: **it is one set of answers timed twice.** The
agreement fractions and the confusion matrix could not have differed, because the answers
being scored are the same bytes, and the only thing the thread change moved is how long they
took to produce.

**And the size check has almost no power on this format**, which is worth knowing before it
is relied on again. It exists because e02's register trace was once committed as a 1329-line
prefix of its own 1501-line run, and a prefix is caught by a count. At fixed field width a
count cannot detect a single changed timing value anywhere in the file, and two complete runs
of the same project will always agree on it. It catches truncation and nothing else.
