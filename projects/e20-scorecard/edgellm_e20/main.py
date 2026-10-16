#!/usr/bin/env python3
"""The e20 loop: hand the model a scorecard, ask for the next three measurements.

    python -m edgellm_e20.main --stub                       the CI run, no model
    python -m edgellm_e20.main --server http://skyhorizon.lan:8080
    python -m edgellm_e20.main --live --server <url>         the scorecard as it is now

One event is one presentation of the scorecard. The replay file carries fifty of them:
each row is a seed that reshuffles the twenty rows and the three identifiers the rule
answers, so the fifty differ only in the order the model is shown. The rule's answer
cannot depend on that order, and the acceptance test is exactly whether the model's
does: the fraction of the fifty where the model's first identifier is the rule's.

The order of one event, which is the order of every chapter of this volume:

    1  the rule ranks the scorecard and that ranking is what the chapter publishes
    2  the scorecard is written out in the order this event's seed asks for
    3  the model is asked, under the grammar, for three objects and nothing else
    4  the model's first identifier goes into the log beside the rule's
    5  the three reasons go to the proposals file, which no number is taken from

The model is never asked to change the scorecard, and nothing it answers reaches the
scorecard, the ranking or any file another chapter reads. It proposes; the rule ranks.

The engine is remote: see edgellm_e20/remote.py for why this one project breaks the
volume's own rule that the Pi runs the model, and what the fallbacks are.

Written Wednesday 14 October 2026.
"""
import argparse
import csv
import importlib.util
import json
import random
import sys
import types
from pathlib import Path

from . import rank as rankmod
from .remote import (Answer, LocalEngineMissing, LocalModel, RemoteModel,
                     RemoteUnreachable, StubModel)
from .scorecard import IDS, ScorecardError, build, repo_root, rows_from_csv

SHARED_MODULES = ("io", "elog", "rules", "template")


def load_shared(root):
    """The four shared modules e20 reuses: edgellm.io, elog, rules and template.

    The shared package is installed by path (python -m pip install -e
    common/python/edgellm) and the plain import is tried first. Two fallbacks follow,
    both deliberate: the in-tree copy is put on the path when nothing is installed, and
    if the package's own __init__ imports a module that is not written yet (its engine
    and loop are the e15 author's and were not in the tree on Wednesday 14 October
    2026), the four modules e20 uses are loaded under the package name so that their
    relative imports still resolve. Nothing is copied and nothing is reimplemented."""
    try:
        import edgellm                                           # noqa: F401
        return {name: importlib.import_module("edgellm." + name)
                for name in SHARED_MODULES}
    except ImportError:
        pass
    shared = Path(root) / "common" / "python" / "edgellm"
    if not (shared / "edgellm" / "io.py").exists():
        raise ScorecardError(
            "the shared package is not installed and %s does not exist either; run "
            "python -m pip install -e common/python/edgellm" % shared)
    if str(shared) not in sys.path:
        sys.path.insert(0, str(shared))
    try:
        import edgellm                                           # noqa: F811
        return {name: importlib.import_module("edgellm." + name)
                for name in SHARED_MODULES}
    except ImportError:
        pass
    package = types.ModuleType("edgellm")
    package.__path__ = [str(shared / "edgellm")]
    sys.modules["edgellm"] = package
    loaded = {}
    for name in SHARED_MODULES:
        path = shared / "edgellm" / (name + ".py")
        spec = importlib.util.spec_from_file_location("edgellm." + name, path)
        module = importlib.util.module_from_spec(spec)
        sys.modules["edgellm." + name] = module
        spec.loader.exec_module(module)
        setattr(package, name, module)
        loaded[name] = module
    return loaded


def present(rows, seed):
    """The twenty rows in the order this event's seed asks for. The rule never sees
    this order; the model sees nothing else."""
    order = list(range(len(rows)))
    random.Random(int(seed)).shuffle(order)
    return [rows[index] for index in order]


def scorecard_text(rows):
    """The scorecard as the model reads it: one line per row, the three keys the rule
    sorts on, and the record behind each. A CI figure is shown as what it is."""
    return "\n".join(row.model_line() for row in rows)


def format_prompt(rows, seed):
    """The user text of one event: the shuffled scorecard and the one instruction.

    About four hundred tokens, which is ten times what a decision loop in the other
    nineteen chapters sends per event and the reason this one runs against a server
    started with a context of 1024."""
    return ("Scorecard, twenty projects, presentation %d:\n%s\n"
            "Name the three projects to measure next, in the order the rule gives."
            % (int(seed), scorecard_text(rows)))


def parse_answer(text, names=IDS):
    """The model's three identifiers and reasons, or ([], []) when it answered nothing
    the grammar allows. A loop never trusts the grammar it asked for: the answer is
    parsed and checked against the name table, and a row whose model column holds no
    identifier is counted as such instead of being read as a disagreement."""
    try:
        value = json.loads((text or "").strip())
    except ValueError:
        return [], []
    if not isinstance(value, list):
        return [], []
    picks, reasons = [], []
    for item in value:
        if not isinstance(item, dict):
            continue
        project = str(item.get("project", ""))
        if project in names:
            picks.append(project)
            reasons.append(str(item.get("reason", "")))
    return picks, reasons


def open_model(args, grammar_text):
    """The engine for this run, and the reason when the first choice is not available.

    The order is the chapter's: the remote 1.5B, then the local 0.5B if one was named,
    then nothing. The stub is never a fallback; it is asked for."""
    if args.stub:
        return StubModel(grammar_text), "asked for the stub"
    if args.server:
        model = RemoteModel(args.server, grammar_text, args.model_name,
                            n_predict=args.n_predict, temperature=args.temperature,
                            timeout_s=args.timeout)
        try:
            model.health()
            return model, "the remote server answered its readiness route"
        except RemoteUnreachable as exc:
            if not args.fallback_model:
                raise
            print("e20: %s" % exc, file=sys.stderr)
            print("e20: falling back to the local model, as the chapter states",
                  file=sys.stderr)
    if args.fallback_model:
        return (LocalModel(args.fallback_model, grammar_text, args.fallback_name,
                           n_predict=args.n_predict, temperature=args.temperature,
                           threads=args.threads),
                "the remote host did not answer; the local fallback ran")
    raise ScorecardError("no engine: pass --server <url>, or --fallback-model <gguf>, "
                         "or --stub")


def run(args):
    root = repo_root(args.repo)
    shared = load_shared(root)
    grammar_text = Path(args.grammar).read_text(encoding="ascii")
    system_prompt = Path(args.prompt).read_text(encoding="ascii").strip()

    if args.live:
        rows = build(root)
        origin = "the repository as it is now"
    else:
        rows = rows_from_csv(args.snapshot)
        origin = "the snapshot %s" % Path(args.snapshot).name
    rule = rankmod.NextMeasurement(rows)
    shared["rules"].register(rule)
    if shared["rules"].find("e20") is not rule:
        raise ScorecardError("the rule did not register under e20")
    rule_three = [row.project for row in rule.three()]

    model, why = open_model(args, grammar_text)
    # The system prompt is wrapped once, in the shared template, and is sent with every
    # request: a completion over HTTP is stateless, so there is no state file to restore
    # as in e02, and what stands in for it is the server's own prefix cache
    # (cache_prompt). prompt.txt itself stays plain text, with no markers.
    system_text = shared["template"].wrap_system(system_prompt, args.template)

    history = shared["rules"].History()
    log = shared["elog"].EventLog(args.log, model.engine)
    proposals = None
    writer = None
    if args.proposals:
        proposals = open(args.proposals, "w", newline="", encoding="ascii")
        writer = csv.writer(proposals, lineterminator="\n")
        writer.writerow(("t_s", "seed", "rank", "project", "reason"))

    matched = events = empty = 0
    try:
        with shared["io"].ReplaySource(args.replay) as source:
            for event in source:
                if args.limit and events >= args.limit:
                    break
                seed = int(event.v(0))
                expected = [IDS[int(event.v(index)) - 1] for index in (1, 2, 3)
                            if 1 <= int(event.v(index)) <= len(IDS)]
                if expected and expected != rule_three:
                    raise ScorecardError(
                        "the replay row at t_s %.1f expects %s and the rule on %s "
                        "answers %s; regenerate the replay and the oracle with "
                        "tools/make_replay.py"
                        % (event.t_s, ", ".join(expected), origin,
                           ", ".join(rule_three)))
                index = shared["rules"].check_index(rule, rule(history, event))
                shown = present(rows, seed)
                user_text = shared["template"].wrap_user(format_prompt(shown, seed),
                                                         args.template)
                answer = model.ask(system_text + user_text)
                picks, reasons = parse_answer(answer.text)
                events += 1
                if not picks:
                    empty += 1
                elif picks[0] == rule.names[index]:
                    matched += 1
                log.row(event, rule.names[index],
                        Answer(picks[0] if picks else "", answer.prompt_ms,
                               answer.generate_ms, answer.n_generated))
                history.push(event)
                if writer is not None:
                    for position, project in enumerate(picks, 1):
                        writer.writerow((("%.6f" % event.t_s), seed, position, project,
                                         reasons[position - 1]))
    finally:
        log.close()
        if proposals is not None:
            proposals.close()
        model.close()

    print("e20: engine %s (%s)" % (model.engine, why))
    print("e20: the rule's three on %s: %s" % (origin, ", ".join(rule_three)))
    print("e20: %d events, top pick matched the rule in %d (%.1f percent), %d answered "
          "no identifier" % (events, matched,
                             (100.0 * matched / events) if events else 0.0, empty))
    print("e20: log %s" % args.log)
    if args.require is not None and events:
        share = 1.0 * matched / events
        if share < args.require:
            print("e20: the acceptance condition was at least %.0f percent and the run "
                  "reached %.1f percent" % (100.0 * args.require, 100.0 * share),
                  file=sys.stderr)
            return 1
    return 0


def parse_args(argv):
    here = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--project", default="e20", help="the rule to find, always e20")
    parser.add_argument("--repo", help="the repository root; found from this file else")
    parser.add_argument("--replay", default=str(here / "replay" / "scorecards_50.csv"))
    parser.add_argument("--snapshot",
                        default=str(here / "replay" / "scorecard_snapshot.csv"),
                        help="the dated scorecard a replay run is scored against")
    parser.add_argument("--live", action="store_true",
                        help="rank the repository as it is now, not the snapshot")
    parser.add_argument("--grammar", default=str(here / "grammar.gbnf"))
    parser.add_argument("--prompt", default=str(here / "prompt.txt"))
    parser.add_argument("--log", default="e20_run.csv")
    parser.add_argument("--proposals", help="the three reasons per event, as CSV")
    parser.add_argument("--server", help="llama-server base URL on skyhorizon's WSL")
    parser.add_argument("--model-name", default="qwen2.5-1.5b-instruct-q4_k_m.gguf")
    parser.add_argument("--fallback-model", help="a local GGUF for the stated fallback")
    parser.add_argument("--fallback-name", default="qwen2.5-0.5b-instruct-q4_k_m.gguf")
    parser.add_argument("--stub", action="store_true", help="no model: the grammar")
    parser.add_argument("--template", default="chatml", choices=("chatml", "zephyr",
                                                                "none"))
    parser.add_argument("--n-predict", type=int, default=192)
    parser.add_argument("--temperature", type=float, default=0.3)
    parser.add_argument("--threads", type=int, default=4)
    parser.add_argument("--timeout", type=float, default=20.0)
    parser.add_argument("--limit", type=int, default=0, help="stop after N events")
    parser.add_argument("--require", type=float,
                        help="exit 1 below this share of matched top picks")
    args = parser.parse_args(argv)
    if args.project != "e20":
        parser.error("this binary carries the e20 rule only")
    return args


def main(argv=None):
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        return run(args)
    except (ScorecardError, RemoteUnreachable, LocalEngineMissing) as exc:
        print("e20: %s" % exc, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
