# e20 search: what the mutation model costs on the authoring laptop, Monday 9 November 2026, 16:36

    machine    aquamarine, 12th Gen Intel Core i7-1255U, 10 cores, 15.7 GB, Iris Xe
               graphics and so no usable acceleration: this is processor-only
    engine     Ollama 0.40.5, served in the foreground with OLLAMA_MODELS set
    store      E:\OllamaModels on the Extreme Pro, 65 blobs, 59.1 GB, read in place
    prompt     one fixed request for a merge sort with a docstring, seed 1
    settings   num_predict 200, stream false
    measured   through the engine's own eval_count and eval_duration, not by a
               stopwatch around the call

**This is a throughput measurement and not an acceptance run.** It prices the mutation half
of the e20 prompt search. It says nothing about any score, and no candidate was evolved.

## What was measured

    model                  tokens   generation   cold load from the drive
    qwen2.5-coder:1.5b       200     24.5 tok/s   under 1 s
    qwen2.5-coder:7b         200      5.6 tok/s   21.1 s

Two runs of the 1.5B agreed at 24.3 and 24.7 tokens per second, so the figure is stable to
about one per cent over the pair. The 7B was measured cold and then warm, 5.8 and 5.6, and
the difference between those two is the drive, not the model.

**The first call of the day pays 21 seconds** to pull 4.7 GB off a USB drive. That cost is
paid once per model per server lifetime, not once per candidate, and it is the reason the
server is left running rather than started per run.

## What it implies for a run, computed rather than guessed

The configuration asks for at most 1024 tokens per candidate.

    1024 tokens at 5.6 tok/s        about 183 s, so roughly 3 minutes a candidate
    40 candidates, mutation only    about 2 hours
    the same with the 1.5B          about 28 minutes

Those are ceilings: a candidate that stops early costs less. The evaluation half is extra
and is not priced here, because it runs against a different machine.

## What this changes about the plan

**Shake the wiring out with the 1.5B and then answer the question with the 7B.** The
control asks whether the loop can improve anything at all, and a flat curve from a weak
mutation model would not distinguish a broken loop from a weak proposer. So the 1.5B is for
proving the integration cheaply, at about five minutes for five candidates, and the 7B is
for the verdict.

**Leave the server up and the drive attached.** The weights are read in place, so the
Extreme Pro has to stay mounted for as long as anything is serving, and restarting the
server re-pays the 21 seconds.

## Not measured here

The evaluation half. Each candidate is scored by running e20 against llama-server on
skyhorizon over `--limit` presentations, and that machine was not exercised for this
record. Until it is, the cost of a full search is the mutation figure above plus an unknown,
and no total should be quoted.
