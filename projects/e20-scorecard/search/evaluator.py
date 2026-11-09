#!/usr/bin/env python3
"""Score one candidate system prompt for e20, by running e20.

The thing being searched is `prompt.txt`, the system prompt this project sends with every
presentation. `main.py` already loads it from a path given by `--prompt`, so a candidate is
a file and nothing in the project has to change for one to be tried.

**This drives e20's own command line rather than reimplementing it.** A scorer that
reimplements the pipeline measures the reimplementation. The only thing computed here is
the score.

    python search/evaluator.py prompt.txt --server http://skyhorizon.lan:8080 --limit 12

## The score is the ordered triple, and deliberately not the published figure

e20's published figure is the share of presentations on which the model's **first**
identifier is the rule's, `main.py` line 226. On Thursday 29 October 2026 a stub answering
the same thing three times scored 100 percent on it, so it is a criterion a constant
satisfies and it must not become the objective of a search: a search handed that metric
would find a constant and report a triumph.

What is scored instead is positional agreement over the whole ordered triple, which is what
`--proposals` has always written and what the register says to publish: for each
presentation, how many of the three positions hold the identifier the rule puts there,
divided by three, averaged over presentations that were answered.

## The floor is printed with every score, never on request

The rule's triple is fixed for a pinned snapshot, so the best constant answer is the rule's
own triple and it scores 1.0. That makes the floor for a *constant* uninteresting and the
floor for a *guesser* the one that matters, so what is reported beside the score is
`random_floor`: the expected positional agreement of three identifiers drawn without
replacement from the twenty, which is 3/20 of the positions, 0.15. A candidate at or below
that has learned nothing about the scorecard. This follows `tools/agreement.py`, which has
printed its baseline unconditionally since Sunday 1 November 2026.

## What is held fixed, and why it matters

The model under test is e20's own, served by llama-server under e20's grammar, reached at
`--server`. It is **not** the model that writes the candidates, which OpenEvolve reaches
over a separate /v1 endpoint. If the two were the same, or if either moved between
candidates, the result would be about the model and not about the prompt.

The replay is pinned too. `--replay` defaults to `replay/scorecards_50.csv`, fifty events
each carrying its own seed, so every candidate faces the identical presentations in the
identical order. `main.py` raises if that file's expected triple disagrees with the rule on
the snapshot, so the ground truth is checked rather than assumed.
"""
import argparse
import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent

REPLAY = PROJECT / "replay" / "scorecards_50.csv"
# Twenty identifiers, three positions: a guesser drawing three of twenty without
# replacement expects 3/20 of its positions right.
RANDOM_FLOOR = 3.0 / 20.0


def expected_triple(replay=REPLAY):
    """The rule's ordered three, read from the replay file's own expectation columns.

    The replay carries v1, v2 and v3 as one-based indices into the identifier table, and
    `main.py` refuses to run when they disagree with the rule computed on the snapshot. So
    reading them here is reading a checked fact and not a second opinion.
    """
    sys.path.insert(0, str(PROJECT))
    from edgellm_e20.scorecard import IDS                      # noqa: E402
    with open(replay, newline="", encoding="ascii") as handle:
        first = next(csv.DictReader(handle))
    return [IDS[int(first["v%d" % i]) - 1] for i in (1, 2, 3)]


def run_e20(prompt_path, server, limit, timeout_s):
    """One run of e20 over `limit` presentations, returning its proposals rows."""
    with tempfile.TemporaryDirectory() as tmp:
        proposals = Path(tmp) / "proposals.csv"
        cmd = [sys.executable, "-m", "edgellm_e20.main",
               "--prompt", str(Path(prompt_path).resolve()),
               "--server", server,
               "--limit", str(limit),
               "--proposals", str(proposals),
               "--log", str(Path(tmp) / "run.csv")]
        try:
            done = subprocess.run(cmd, cwd=str(PROJECT), capture_output=True, text=True,
                                  timeout=timeout_s)
        except subprocess.TimeoutExpired:
            return [], 124, "timed out after %d s" % timeout_s
        if not proposals.exists():
            return [], done.returncode, (done.stderr or "")[-400:]
        with open(proposals, newline="", encoding="ascii") as handle:
            return list(csv.DictReader(handle)), done.returncode, ""


def triples_by_seed(rows):
    """{seed: [rank1, rank2, rank3]}, a position the model did not fill left as None."""
    out = {}
    for row in rows:
        try:
            rank = int(row["rank"])
        except (KeyError, ValueError):
            continue
        if 1 <= rank <= 3:
            out.setdefault(row["seed"], [None, None, None])[rank - 1] = row["project"]
    return out


def positional_score(triples, target):
    """Mean over answered presentations of how many of three positions match the rule's."""
    answered = {s: p for s, p in triples.items() if any(p)}
    if not answered:
        return 0.0, 0
    total = sum(sum(1 for i in range(3) if p[i] == target[i]) for p in answered.values())
    return total / (3.0 * len(answered)), len(answered)


def evaluate(program_path, server=None, limit=12, timeout_s=1800):
    """The contract OpenEvolve calls: a dict of metrics carrying a combined_score."""
    server = server or "http://localhost:8080"
    target = expected_triple()
    rows, code, err = run_e20(program_path, server, limit, timeout_s)
    triples = triples_by_seed(rows)
    score, answered = positional_score(triples, target)
    return {
        "combined_score": score,
        "triple_agreement": score,
        "random_floor": RANDOM_FLOOR,
        "beats_floor": 1.0 if score > RANDOM_FLOOR else 0.0,
        "presentations": float(len(triples)),
        "answered": float(answered),
        "exit_code": float(code),
        "rule_triple": ",".join(target),
        "error_tail": err,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("prompt", help="the candidate system prompt to score")
    parser.add_argument("--server", default="http://localhost:8080",
                        help="llama-server serving e20's own model, not the mutation model")
    parser.add_argument("--limit", type=int, default=12,
                        help="presentations per candidate; the published run used 50")
    parser.add_argument("--timeout-s", type=int, default=1800)
    args = parser.parse_args()
    result = evaluate(args.prompt, args.server, args.limit, args.timeout_s)
    print(json.dumps(result, indent=2, sort_keys=True))
    # A score is never printed without its floor, so a reader cannot quote one alone.
    print("\n  triple agreement %.3f against a random floor of %.3f, on %d of %d answered"
          % (result["triple_agreement"], result["random_floor"],
             int(result["answered"]), int(result["presentations"])), file=sys.stderr)
    return 0 if result["answered"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
