# Searching for e20's system prompt

A search over `prompt.txt`, the one file that tells this project's model what to do, driven
by OpenEvolve with a local code model as the mutation operator.

**Nothing here has been run.** The evaluator has been checked against a record, which is a
different and smaller thing, and that check is below.

## Why this project and not another

e20 is the only project in the volume whose inference is not local, so it already talks to
an engine over HTTP, already carries the override that lets it be written in Python, and
already uses the largest model on the bench. The other nineteen are C on a board, and
putting a search loop inside one of those would mean arguing the house rule rather than
running an experiment.

## The two models are different, on purpose

| Role | Model | Served by | Reached at |
| --- | --- | --- | --- |
| Writes candidate prompts | `qwen2.5-coder:7b` | Ollama | `http://localhost:11434/v1` |
| Under test, follows the prompt | e20's own Qwen2.5-1.5B-Instruct | llama-server, with e20's grammar | `--server` |

If these were the same model, or if either moved between candidates, the result would be
about the model and not about the prompt. The model under test is configured nowhere in
`config.yaml`, so one edit cannot quietly change the subject of the experiment.

## What is scored

Positional agreement over the ordered triple: for each presentation, how many of the three
positions hold the identifier the rule puts there, divided by three, averaged over the
presentations that were answered.

**Not the published figure.** e20's published figure is whether the model's first identifier
is the rule's, and on Thursday 29 October 2026 a stub answering the same thing three times
scored 100 percent on it. A search handed that objective would find a constant and report a
triumph.

**Not the strict ordered triple either.** The strict triple scores 0 of 50 for the model and
0 of 50 for the stub, which is the right thing to publish and a useless thing to optimise,
because a metric that is zero everywhere has no gradient for a search to climb.

The floor printed beside every score is `random_floor`, 0.15, which is what three
identifiers drawn from twenty expect to score. The rule's own triple is fixed for a pinned
snapshot, so the best *constant* scores 1.0 and tells you nothing; the guesser is the
comparison that bites.

## What the evaluator reproduces, which is how it was checked

Run against the fifty presentations already in `evidence/e20_proposals.csv`:

    this evaluator, per position   [15, 5, 2] of 50
    the evidence record of
    Thursday 29 October 2026       15, 5 and 2 of 50
    strict ordered triple          0 of 50, both

It agrees exactly with a record written a week earlier by a different route, which is the
only reason to believe it. A self-test would not have shown this.

## The starting point, which is below chance

    positional agreement of the published run   0.147
    random floor                                0.150

**The prompt in the repository today does not beat guessing on the ordered triple.** That is
not a new run and not a claim about a new experiment: it is the arithmetic of the numbers
already in the evidence file, under the metric the register already said to publish. It
gives the search real headroom and an honest baseline, and anything at or below 0.15 at the
end of a run is a null to be reported as one.

## Running it

Not yet possible on the authoring laptop: Ollama is not installed there, verified on Monday
9 November 2026. The weights are present, 59.1 GB of them, on the Extreme Pro, which mounts
as `E:` and not `D:`.

    OLLAMA_MODELS=E:\OllamaModels        so no weights are copied or downloaded
    llama-server                         serving e20's own model, as e20 already expects

Then the evaluator alone, which is worth doing before any search, because it says whether
the engine and the project are talking at all:

    python search/evaluator.py prompt.txt --server http://localhost:8080 --limit 5

## What this is not

It is not a result. It is not a claim about evolutionary search in general, and every figure
it can produce is bounded by one 1.5B model under test and one 7B model proposing. The
chapter's acceptance table is untouched and stays untouched until a run exists to report.
