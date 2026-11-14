# e08: the recovery after signal 9 is 36.1 seconds, and the chapter's bound of 30 was
# replaced by its first measurement. Saturday 14 November 2026, 17:39 to 17:59

    board      Raspberry Pi 3 Model B Plus Rev 1.3, card rpi3BV1p2
    layout     the two published units, installed as written: binaries in /usr/bin,
               grammar and prompt in /usr/share/edgellm/e08, the model symlinked into
               /usr/share/edgellm/models, socket /run/edgellm/e08.sock, log
               /var/lib/edgellm/e08.csv
    logs       evidence/e08_units_2026-11-14.csv, evidence/e08_restart_2026-11-14.txt
    before     throttled 0x0 at 17:39
    after      throttled 0x80000 at 56.9 C

## THE CLAUSE: THE RECOVERY AFTER SIGNAL 9

Two clean measurements, by the console's own `CLOCK_MONOTONIC` from the first refused
connection to the first good reply:

    17:44:00 away  ->  17:44:36 back    36.110 s
    17:45:06 away  ->  17:45:42 back    36.181 s

**Seventy milliseconds apart on two independent kills.** The chapter says of its own
placeholder: "it is under 30 s on this board until the first measurement replaces that
bound". **The first measurement replaces it: 36.1 s, not under 30.**

The rest of the clause is met. **The console is the same process throughout**: every line
of the journal from the start at 17:39:15 to the stop at 17:59:17 is tagged
`edgellm-e08-console[859]`. It survived four disappearances of the daemon without being
restarted, which is what the unit's `Wants=` rather than `BindsTo=` was written for, and
the unit file says so in its own comment.

**What the 36 seconds are made of**: one second of `RestartSec` and the rest the model
load. Loading by hand earlier the same day took about a hundred seconds; here it is
thirty-five, because the 258 MiB file was already in the page cache. A recovery from a cold
cache would be longer and has not been measured.

**A prediction of mine was wrong in magnitude.** Before the run I wrote that I expected
"clearly more than 30 s", meaning the hundred seconds the cold load had taken. It is 36.
Right in direction, wrong by a factor of three, and the reason is the page cache.

## TWO READINGS THAT ARE NOT RECOVERIES

The log also holds `365.270 s` and `357.635 s`. **Those are not recovery times.** The
daemon was killed thirty-eight times between 17:43:06 and 17:58:53, and every kill during
the model load restarts the load, so the daemon never reached the point of answering. The
figure is the accumulated time across many kills.

That is itself worth recording: **repeated kills inside the load window postpone recovery
without limit**, and neither unit has anything that would notice. `Restart=on-failure` with
`RestartSec=1` has no start-limit interval configured, so systemd restarts forever and the
console waits forever.

## THE ANSWERS FROM THE SAME SITTING, AND A DEFECT UNDER THEM

Ten questions were asked, six answered, four fell inside a window where the daemon was
dead, and the checker counts those four as failures because the model column is empty.
`tools/answer_check.py` scores **1 of 10**.

**But three of those failures cannot be judged, and the reason is in the logger.**
`common/c/src/elog.c` wrote every raw value with `%.6g`. An epoch second has ten digits,
so both clocks were stored as `1.79467e+09`, which is the true value rounded to the nearest
ten thousand seconds, two hours and forty-seven minutes.

    checker expected   RTC time 15:26 UTC
    model answered     16:42:43 UTC
    the board's clock  17:42 local, which is 16:42 UTC

**The model was right and the expectation was an artefact.** The same rounding makes the
difference between the two clocks exactly zero in every row, whatever it was, so question 1
cannot be scored either. Two of the five questions were unscoreable from this log and
nothing said so.

The format is `%.13g` from this day: thirteen digits hold an epoch second to the
millisecond and print 97.2953 and 27.74 unchanged. No oracle or replay file in the tree
contains a value that changes, e08 being the only project that logs a number that large.

## WHAT THIS DOES NOT ESTABLISH

    established   the recovery after signal 9 is 36.110 s and 36.181 s, two kills
    established   the console survives as one process, PID 859 across four outages
    established   the chapter's bound of under 30 s is replaced by its first measurement
    established   repeated kills inside the load window postpone recovery without limit
    established   elog.c wrote raw values with %.6g and destroyed the clock columns
    established   the model's time of day was right where the checker's expectation was
                  a rounding artefact
    NOT A RECOVERY  the 365 s and 358 s figures; they span many kills
    NOT MEASURED  a recovery from a cold page cache
    NOT ACCEPTED  the thermal condition: 0x0 at 17:39 and 0x80000 at 17:59
    established   the corrected format works on the board, verified at 18:40 with one
                  replayed event through the real engine: the raw column reads
                  0;1760440028.35;1760440028.34;98.08;24, twelve digits with hundredths
                  of a second, and the two clocks differ by 0.01 s where %.6g would have
                  written 1.76044e+09 twice and a difference of zero
    NOT RESCORED  the ten answers under the corrected format; that needs a new run
