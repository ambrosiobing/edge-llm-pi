# e02's loop on SmolLM2-360M, the in-process arm of e04's comparison, Tuesday 27 October 2026, 16:26

    board      Raspberry Pi 4 Model B Rev 1.4, 8 GB, Cortex-A72, two threads
    card       eplepi, Raspberry Pi OS, Debian 13 trixie
    model      SmolLM2-360M-Instruct-Q4_K_M.gguf, 270590880 bytes,
               sha256 2fa3f013dcdd7b99f9b237717fa0b12d75bbb89984cc1274be1471a465bac9c2
    engine     llama.cpp v0.6.0, in process through the C API
    source     replay/synthetic_1000.csv, first 100 events via --max-events 100
    settings   --threads 2 --n-predict 12, which are e02's own protocol
    log        evidence/e02_inprocess_360m_2026-10-27.csv, 101 lines, 11188 bytes

**This is not a re-run of e02.** e02 is finished: both clauses passed on Monday 19 October
2026 and its published figures are on Qwen2.5-0.5B and stay that way. This run exists to
fill one row of **e04's** comparison table, which had read "not measured" since that chapter
was written, and which e04's own text says is like for like only on the same model, the same
board and the same sitting.

## What was measured

    e02: 100 events, 100 model answers, 0 model errors, 0 actuation errors,
         mean prompt 2771.2 ms, mean generate 1110.1 ms, rss 327024 kB,
         engine llama.cpp v0.6.0, source replay

    100 rows
    mean prompt     2771.2 ms
    mean generate   1110.1 ms
    total           3881.3 ms
    mean tokens        9.09
    per token        122.1 ms
    at the ceiling     0 of 100

    get_throttled   0x0 before, 0x0 after

## The comparison, with what it can and cannot say

    route                     threads  budget  prompt   generate  tokens  per token  total
    D, llama-server, HTTP           4      64  2124.8    5555.1   64.0      86.8    7683.4
    C, in process, e02's loop       2      12  2771.2    1110.1    9.09    122.1    3881.3

**The wire is free, and that is the row the chapter wanted.** e04's HTTP cost is 3.5 ms,
taken as the client's clock minus the server's own two timings on the same rows, against a
decision that costs between 3.9 and 7.7 seconds by either route. Catalogue variant D costs
nothing at this interval.

**The totals cannot be compared, and the two chapters' own protocols are why.** e04 fixes
four threads and a 64-token budget; e02 fixes two threads and 12. e04's table asks for
"e02's loop on SmolLM2-360M with no other change", and honouring that guarantees two
variables move at once. The comparison as specified cannot be made cleanly. That is a defect
in the specification, found by carrying it out, and it is recorded here rather than hidden
behind a side-by-side table that implies more than it knows.

**Per token the generate figures do compare, and they overturn a claim made before the log
was read.** 86.8 ms per token at four threads against 122.1 at two: **four threads are 1.41
times faster per token**. An earlier reading of this run divided 1110.1 ms by the budget of
12 and reported a 6.6 percent difference, concluding that generation on this board is bound
by memory bandwidth rather than by cores. The `n_generated` column says the run averaged
**9.09** tokens, not 12, and the correct figure is 40.7 percent. Cores buy real speed here.
Divide by what was generated, never by what was permitted.

**The budget behaves as the task dictates, for the third time.** e02 asks for a bare enum
and closed every one of 100 objects inside a budget of 12, averaging 9.09 tokens with
**nothing at the ceiling**. e04 asks for up to 160 characters of prose and hit its 64-token
ceiling on 159 of 180 rows with 136 objects never closing. Same model, same engine, same
board, same week. The difference is the question asked.

## The supply has a boundary now, and this run is the clean side of it

    run   threads  events  duration        register after
    e02         2     100  about 6.5 min   0x0
    e03         4     200  about 16 min    0xd0000
    e04         4     180  about 24 min    0xf0000

All three began on a clean boot at `0x0`. So the brownout on this board is a function of
thread count and duration rather than something inherent to running a model on it. **Two
threads for six minutes is inside what the supply delivers.** That makes the figures in this
record clean, where e03's and e04's latencies are upper bounds taken through undervolt and
throttling, and it gives the remaining projects a usable rule rather than a warning.

Resident memory 327024 kB, which is 319.4 MiB, against e03's 344.6 MiB at a 64-token budget
on the same board and model. Both are the figure to compare against when this is repeated on
the 1 GB rung, where the model at 258.1 MiB against 905 MB of total memory is the whole
question rather than a footnote.

## A note on reading this log, which cost a wrong number once already

The header is `t_s,tag,raw,rule,model,prompt_ms,generate_ms,n_generated,engine`, and the
`model` field holds JSON containing commas. So `$5` and `$6` are **not** `prompt_ms` and
`generate_ms`; the quoted field shifts everything after it. Count from the end instead:

    $(NF-3) prompt_ms   $(NF-2) generate_ms   $(NF-1) n_generated   $NF engine

The same header is used by e03 and e04. An awk written from the left on this file returned a
mean prompt of 0.0 ms and silently moved the prompt figure into the generate column.

## How the log reached this repository

`scp` from the board **after the run had exited**, then checked against the board's own
count before anything was written from it: 101 lines and 11188 bytes on both sides. Every
figure above re-derives from the committed copy.
