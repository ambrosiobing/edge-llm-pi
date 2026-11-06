# Board A's thermal budget is 234 seconds, and the supply held, Thursday 6 November 2026, 15:10

    board      Raspberry Pi 4 Model B Rev 1.4, 8 GB, hostname eplepi, revision d03114
    supply     black 65W-UC01WAEB USB-C, the unit that measured clean on Tuesday 27
               October 2026; nothing attached to the USB-A ports, confirmed by lsusb
    load       e06 over evidence/e06_descent_50.csv, 50 events, --threads 4,
               --n-predict 64, SmolLM2-360M-Instruct-Q4_K_M.gguf, no sensor
    instrument tools/throttle_log.sh at one sample per second, 305 samples
    trace      evidence/e06_throttle_2026-11-06.csv
    tree       4fd0527

**Two things are measured here and the second was not what the run was for.** The afternoon's
question was whether board A's under-voltage of earlier the same day recurs under load. It
does not. What the trace produced instead is the first timestamped thermal budget this board
has had, replacing a figure that had no source and an estimate of mine that was too generous.

## THE SAMPLER'S OWN COST, ANSWERED BEFORE IT WAS TRUSTED

Its header says the instrument is the first suspect and that its cost is unmeasured, so it
was read alone on an idle board first: 60 samples in 60 seconds, register `0x0` throughout,
clock pinned at 600169920 Hz, and the temperature **fell** from 41.8 to 40.9 C across the
minute, range 40.4 to 42.3.

**It costs nothing measurable**, and that is now on the record rather than assumed.

## NO UNDER-VOLTAGE UNDER LOAD

    register value   samples   meaning
    0x0                 225    clean
    0x80000              68    soft temperature limit has occurred, bit 19
    0x80008              12    soft limit occurred AND active at that instant, bits 19 and 3

**Bit 16 appears in none of the 305 samples.** Every value is thermal. So the supply path, on
the black `65W-UC01WAEB` with nothing on the USB-A ports, held through 50 events at four
threads.

That exonerates the brick, the cable and the socket for this load, and leaves earlier today's
event as a transient or as something not present now. **It does not explain it.** What was
observed earlier stands as observed: bits 16 and 18 were set before a run at 44.8 C, and the
board restarted unasked at 14:26:22 during that run. Neither has a cause.

## THE BUDGET, AS A TIMESTAMP

    15:04:49   39.9 C   0x0        load starts
    15:08:43   79.3 C   0x80000    THE SOFT LIMIT TRIPS

**234 seconds of e06 load at four threads, from 39.9 C.** Then the board sits on the limit
rather than past it, bit 3 going on and off twelve times while the temperature cycles between
80.3 and 82.7 C: clock down, cool, clock up.

    367 s     carried by e06's pre-flight record of Thursday 29 October 2026 and requoted
              in e20's of the same day. It names no load, no thread count and no model.
              WITHDRAWN.
    ~300 s    my own estimate of Thursday 6 November 2026, from the earlier run that
              tripped. Too generous.
    234 s     MEASURED, from 39.9 C, with the trip at 79.3 C and a trace to read it in.

**So the figure is smaller than either guess, and the consequence is firmer.** A
non-degenerate agreement slice is about 50 events, which is roughly 300 seconds of this load.
Board A delivers 234. **e06's first clause cannot be satisfied on this board from any starting
temperature**, and that now rests on a timestamped trace rather than on an inference from
sticky bits.

## TWO CROSS-CHECKS THAT ARRIVED FREE

**The budget-stopped counter agreed with a Python parser that shares nothing with it.** This
run's summary reads `3 stopped at the budget`. The same slice on Sunday 1 November 2026 had
`agreement.py` report "not a decision of the rule 3", because three rows would not parse as
JSON. Those are the same three rows, counted by `n_generated >= n_predict` in C and by a
failed `json.loads` in Python.

**And the throttling the trace shows explains a spread that was previously unexplained.**
`mean generate` has read 3792.0, 3828.6 and 3890.7 ms across three runs of this identical
slice. The trace puts 80 of its 305 samples at or on the soft limit, which is where that
spread comes from. The agreement figure is unaffected, being a count of text.

## WHAT THIS AFTERNOON COST, AND THE LESSON IS MINE

Before this trace existed, the under-voltage question was pursued by reading `get_throttled`
by hand before and after each run, reconstructing a timeline from sticky bits, inferring an
abrupt power loss from a missing journal, and then reconfiguring `systemd-journald` to obtain
a timestamp. The journald change never took effect, and the inference was withdrawn once the
journal directory turned out to be empty with `Storage=auto`.

**`tools/throttle_log.sh` has existed since Sunday 18 October 2026 and its header describes
that exact failure**, in this repository, in the file I should have read first:

> get_throttled's sticky bits (16 to 19) say a limit occurred and never for how long, and its
> live bits (0 to 3) change faster than a person can type... four readings taken by hand
> during it were each reported as a settled value and then withdrawn when the next
> contradicted them.

A project that has already solved an instrumentation problem puts the tool and the reason in
the same file. Reaching past it for a system service was the error, and the trade it implied
was worse than the one it was meant to solve: the existing drop-in sets `Storage=volatile`,
which on a board that boots from an SD card is a deliberate choice about card wear.

## What is established and what is not

    established   the sampler costs nothing measurable: 60 idle samples, temperature fell
    established   zero under-voltage samples in 305 under 50 events at four threads
    established   the soft limit trips at 79.3 C after 234 s of this load from 39.9 C
    established   the board then oscillates on the limit, 80.3 to 82.7 C, bit 3 cycling
    established   the budget-stopped counter and agreement.py independently count 3
    WITHDRAWN     the 367 s thermal budget, and my own 300 s estimate
    NOT shown     any cause for the under-voltage and the restart of earlier today, which
                  did not recur here and which software cannot reach, the 5 V rail having
                  no readable value
    NOT changed   e06's clause one, still blocked on cooling, now on a timestamped trace
