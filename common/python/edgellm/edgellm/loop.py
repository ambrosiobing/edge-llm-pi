"""The loop: run_loop() in the order of common/c/src/edgellm_main.c, and main(argv).

For every event: the rule decides on the history (the current event is NOT yet in the
history when the rule sees it; it is pushed after), the project acts through the
source's actuate() (the rule's index, never the model's answer), the project builds
the user text, the loop wraps it in the chat template, asks the engine, logs the row
with the rule column beside the model column, pushes the event, and sleeps the
interval. A project supplies two things, as in edgellm_main.h:

    format_prompt(ev, history) -> str or None   None logs the rule alone
    actuate(source, rule, index, ev) -> int     0, or -1 to count an error

The command line is the C main's, so a project.toml run line reads the same in both
languages; --stub is accepted here always, and the log's engine column then reads
"stub", so a log can never claim an engine it did not run.
"""
import signal
import sys
import time

from .elog import EventLog
from .engine import Config, Result, open_engine
from .io import open_source
from .rules import History, check_index, find
from .template import TEMPLATES, wrap_system, wrap_user

USAGE = ("usage: edgellm-eNN --project eNN (--replay <csv> | --live <backend> <spec>)\n"
         "          --grammar <gbnf> --prompt <txt> --log <csv> [--model <gguf>]\n"
         "          [--n-predict N] [--threads N] [--n-ctx N] [--temperature T]\n"
         "          [--interval S] [--state <path>] [--max-events N] [--stub]\n"
         "          [--template chatml|zephyr|none]\n")


class LoopStats:
    """The one-line summary the C main prints to stderr at the end."""

    def __init__(self):
        self.events = 0
        self.model_answers = 0
        self.model_errors = 0
        self.actuation_errors = 0
        self.sum_prompt_ms = 0.0
        self.sum_generate_ms = 0.0

    def mean_prompt_ms(self):
        return self.sum_prompt_ms / self.model_answers if self.model_answers else 0.0

    def mean_generate_ms(self):
        return self.sum_generate_ms / self.model_answers if self.model_answers else 0.0


class _Stop:
    requested = False


def _on_signal(signum, frame):
    _Stop.requested = True


def read_text(path):
    """prompt.txt without its trailing newlines, as read_text() in the C main."""
    with open(path, "r", encoding="utf-8") as fh:
        return fh.read().rstrip("\r\n")


def run_loop(project, rule, engine, source, log, interval_s=0.0, max_events=0,
             template="chatml", stderr=sys.stderr):
    """The loop proper; the caller opened the engine, the source and the log."""
    history = History()
    stats = LoopStats()
    rc = 0
    for ev in source:
        if _Stop.requested:
            break
        index = check_index(rule, rule(history, ev))
        if project.actuate(source, rule, index, ev) != 0:
            stats.actuation_errors += 1
        user = project.format_prompt(ev, history)
        logged = None
        if user is not None:
            try:
                logged = engine.decide(wrap_user(user, template))
                stats.model_answers += 1
                stats.sum_prompt_ms += logged.prompt_ms
                stats.sum_generate_ms += logged.generate_ms
            except Exception as exc:  # the engine failed; the row is kept, as in C
                stats.model_errors += 1
                print("edgellm: model error: %s" % exc, file=stderr)
                logged = Result()
        log.row(ev, rule.names[index], logged)
        history.push(ev)
        stats.events += 1
        if max_events > 0 and stats.events >= max_events:
            break
        if interval_s > 0.0:
            time.sleep(interval_s)
    print("%s: %d events, %d model answers, %d model errors, %d actuation errors, "
          "mean prompt %.1f ms, mean generate %.1f ms, engine %s, source %s"
          % (rule.project, stats.events, stats.model_answers, stats.model_errors,
             stats.actuation_errors, stats.mean_prompt_ms(), stats.mean_generate_ms(),
             engine.version, source.backend), file=stderr)
    return rc, stats


def parse_args(argv):
    """The C main's options, into a dict; raises SystemExit(2) with the usage on error."""
    opts = {"project": None, "replay": None, "backend": None, "spec": None, "grammar": None,
            "prompt": None, "log": None, "model": None, "state": None, "stub": False,
            "n_predict": 8, "threads": 4, "n_ctx": 512, "temperature": 0.0,
            "interval": 0.0, "max_events": 0, "template": "chatml"}
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--stub":
            opts["stub"] = True
            i += 1
            continue
        if i + 1 >= len(argv):
            raise SystemExit(USAGE)
        v = argv[i + 1]
        if a == "--project":
            opts["project"] = v
        elif a == "--replay":
            opts["replay"] = v
        elif a == "--live":
            if i + 2 >= len(argv):
                raise SystemExit(USAGE)
            opts["backend"], opts["spec"] = v, argv[i + 2]
            i += 1
        elif a == "--grammar":
            opts["grammar"] = v
        elif a == "--prompt":
            opts["prompt"] = v
        elif a == "--log":
            opts["log"] = v
        elif a == "--model":
            opts["model"] = v
        elif a == "--state":
            opts["state"] = v
        elif a == "--n-predict":
            opts["n_predict"] = int(v)
        elif a == "--threads":
            opts["threads"] = int(v)
        elif a == "--n-ctx":
            opts["n_ctx"] = int(v)
        elif a == "--temperature":
            opts["temperature"] = float(v)
        elif a == "--interval":
            opts["interval"] = float(v)
        elif a == "--max-events":
            opts["max_events"] = int(v)
        elif a == "--template":
            opts["template"] = v
        else:
            raise SystemExit(USAGE)
        i += 2
    if opts["template"] not in TEMPLATES:
        raise SystemExit(USAGE)
    if (opts["project"] is None or opts["grammar"] is None or opts["prompt"] is None
            or opts["log"] is None or (opts["replay"] is None) == (opts["backend"] is None)):
        raise SystemExit(USAGE)
    if not opts["stub"] and opts["model"] is None:
        raise SystemExit("edgellm: --model is required without --stub")
    return opts


def main(argv, project, stderr=sys.stderr):
    """edgellm_main(): parse, open the five pieces in the C order, run, close."""
    opts = parse_args(argv)
    rule = find(opts["project"])
    if rule is None:
        print("edgellm: no rule registered for project %s" % opts["project"], file=stderr)
        return 1
    cfg = Config(model_path=opts["model"] or "", grammar_path=opts["grammar"],
                 system_prompt=wrap_system(read_text(opts["prompt"]), opts["template"]),
                 state_path=opts["state"], n_ctx=opts["n_ctx"], n_threads=opts["threads"],
                 n_predict=opts["n_predict"], temperature=opts["temperature"])
    engine = source = log = None
    try:
        engine = open_engine(cfg, stub=opts["stub"])
        source = open_source(opts["replay"], opts["backend"], opts["spec"])
        log = EventLog(opts["log"], engine.version)
        signal.signal(signal.SIGINT, _on_signal)
        signal.signal(signal.SIGTERM, _on_signal)
        rc, _stats = run_loop(project, rule, engine, source, log, opts["interval"],
                              opts["max_events"], opts["template"], stderr)
        return rc
    except (OSError, ValueError, RuntimeError) as exc:
        print("edgellm: %s" % exc, file=stderr)
        return 1
    finally:
        for piece in (log, source, engine):
            if piece is not None:
                piece.close()
