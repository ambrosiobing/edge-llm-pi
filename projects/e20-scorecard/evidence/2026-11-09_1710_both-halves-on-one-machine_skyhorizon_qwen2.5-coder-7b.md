# e20 search: both halves measured on one machine, Monday 9 November 2026, 17:10

    machine        skyhorizon, WSL, 8 cores
    under test     qwen2.5-1.5b-instruct-q4_k_m.gguf, served by llama-server built from
                   the repository's own submodule at d81235049 (v0.6.0), target
                   llama-server, CPU backend at -march=native
                   127.0.0.1:8080, -c 1024, -t 8, n_slots 4, n_ctx_slot 1024
    mutation       qwen2.5-coder:7b through Ollama 0.40.5, 127.0.0.1:11434, weights read
                   in place from the Extreme Pro, which mounts as D: on this machine
    tree           worktree at ~/src/edge-llm-pi-main, detached at 2764c4c
    prompt         the project's own prompt.txt, unmodified

**This is a cost measurement and an integration check. No search has been run, no candidate
has been evolved, and nothing about the prompt has changed.**

## The chain works end to end

First time both halves have been on one machine. Twelve presentations, twelve answered,
exit code 0, no error tail. The server reported `n_ctx_slot = 1024`, so the context asked
for took effect and no prompt was silently truncated, which matters because a truncation
would have shown up as a bad score rather than as an error.

## The score behaved the way noise should

    presentations   triple agreement
    5               0.067
    12              0.139
    50              0.147   the published run of Thursday 29 October 2026

Converging on the published figure rather than drifting from it. That is a second check on
the evaluator, independent of the first: the first reproduced the record's per-position
counts arithmetically, this one reproduces its figure by running the model again a fortnight
later against a rebuilt server.

The 0.067 at five presentations is one position out of fifteen and is not a finding. It is
recorded here only so nobody later reads it as one.

## What a candidate costs

    evaluation   86.6 s for 12 presentations        7.2 s a presentation
    mutation     200 tokens at 6.3 and 6.1 tok/s    about 6.2 tok/s, two runs agreeing
    cold load    9.9 s for 4.7 GB off the drive     once per model per server lifetime

The mutation figure was also taken on aquamarine before the drive moved, where the same
model gave 5.6 tok/s and a 21.1 s cold load. The machines differ by about ten per cent on
generation and by a factor of two on the load, which is the USB path and not the model.

## What a forty-candidate run costs, computed and not guessed

    mutation     40 x 1024 tokens at 6.2 tok/s      1.8 h   a ceiling: a candidate that
                                                            stops early costs less
    evaluation   40 x 12 presentations at 7.2 s     58 min  measured
    sum                                             2.8 h

**That sum is optimistic and should not be quoted on its own.** Both halves now share eight
cores. `llama-server` is holding eight threads and Ollama wants cores for the 7B, and every
figure above was taken with the other side idle. Run concurrently they will contend, so the
honest range for forty candidates at `--limit 12` is **about three to four hours**, and the
upper end assumes forty per cent contention, which is itself an estimate and not a
measurement.

## Not measured

- **The contention itself.** Nobody has run both engines at once yet, so the multiplier
  above is a guess with a label on it. The first real search will measure it for free.
- **The 1.5B as the mutation model on this machine.** It gave 24.5 tok/s on aquamarine,
  which would cut the mutation half from 1.8 h to about 28 minutes, but that figure is from
  the other laptop and is not quoted as if it were this one.
- **Anything about whether a searched prompt is better.** That needs a search.
