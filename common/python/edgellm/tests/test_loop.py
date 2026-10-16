"""The loop in the C order: rule, act, prompt, decide, log, push; and the C options."""
import csv

import pytest

from edgellm import loop as loop_mod
from edgellm.loop import main, parse_args
from edgellm.template import wrap_system, wrap_user

GRAMMAR = ('root ::= "{" ws "\\"decision\\"" ws ":" ws d ws "}"\n'
           'd ::= "\\"LOW\\"" | "\\"HIGH\\""\nws ::= [ \\t]*\n')


class ThresholdRule:
    project = "e00"
    names = ("LOW", "HIGH")

    def __call__(self, history, ev):
        return 1 if ev.v(0) > 50.0 else 0


class Project:
    def __init__(self):
        self.acted = []
        self.seen_history = []

    def format_prompt(self, ev, history):
        self.seen_history.append(len(history))
        return "value %.1f. Decide." % ev.v(0)

    def actuate(self, source, rule, index, ev):
        self.acted.append(rule.names[index])
        return source.actuate("out", index)


def test_templates_match_the_c_main():
    assert wrap_system("S") == "<|im_start|>system\nS<|im_end|>\n"
    assert wrap_user("U") == "<|im_start|>user\nU<|im_end|>\n<|im_start|>assistant\n"
    assert wrap_system("S", "zephyr") == "<|system|>\nS</s>\n"
    assert wrap_user("U", "zephyr") == "<|user|>\nU</s>\n<|assistant|>\n"
    assert wrap_user("U", "none") == "U"


def test_stub_run_end_to_end(tmp_path, monkeypatch):
    (tmp_path / "g.gbnf").write_text(GRAMMAR, encoding="ascii")
    (tmp_path / "p.txt").write_text("Say LOW or HIGH.\n", encoding="ascii")
    (tmp_path / "r.csv").write_text("t_s,tag,v0\n0,t,10\n1,t,60\n2,t,51\n", encoding="ascii")
    log = tmp_path / "log.csv"
    rule = ThresholdRule()
    monkeypatch.setattr(loop_mod, "find", lambda project: rule if project == "e00" else None)
    project = Project()
    argv = ["--project", "e00", "--replay", str(tmp_path / "r.csv"), "--grammar",
            str(tmp_path / "g.gbnf"), "--prompt", str(tmp_path / "p.txt"), "--log", str(log),
            "--stub"]
    assert main(argv, project) == 0
    # the rule acted before the prompt was built, and the history excluded the event
    assert project.acted == ["LOW", "HIGH", "HIGH"]
    assert project.seen_history == [0, 1, 2]
    with open(log, newline="", encoding="ascii") as fh:
        rows = list(csv.DictReader(fh))
    assert [r["rule"] for r in rows] == ["LOW", "HIGH", "HIGH"]
    assert {r["model"] for r in rows} == {'{"decision":"LOW"}'}
    assert {r["engine"] for r in rows} == {"stub"}


def test_max_events_and_unknown_project(tmp_path, monkeypatch):
    (tmp_path / "g.gbnf").write_text(GRAMMAR, encoding="ascii")
    (tmp_path / "p.txt").write_text("x", encoding="ascii")
    (tmp_path / "r.csv").write_text("t_s,tag,v0\n0,t,10\n1,t,60\n", encoding="ascii")
    rule = ThresholdRule()
    monkeypatch.setattr(loop_mod, "find", lambda project: rule if project == "e00" else None)
    base = ["--replay", str(tmp_path / "r.csv"), "--grammar", str(tmp_path / "g.gbnf"),
            "--prompt", str(tmp_path / "p.txt"), "--log", str(tmp_path / "l.csv"), "--stub"]
    assert main(["--project", "e99"] + base, Project()) == 1
    assert main(["--project", "e00", "--max-events", "1"] + base, Project()) == 0
    assert len((tmp_path / "l.csv").read_text(encoding="ascii").splitlines()) == 2


def test_option_errors():
    with pytest.raises(SystemExit):
        parse_args(["--project", "e00"])                       # no source
    with pytest.raises(SystemExit):
        parse_args(["--project", "e00", "--replay", "a", "--live", "serial", "p",
                    "--grammar", "g", "--prompt", "p", "--log", "l"])   # both sources
    with pytest.raises(SystemExit):
        parse_args(["--project", "e00", "--replay", "a", "--grammar", "g", "--prompt", "p",
                    "--log", "l"])                                     # no model, no stub
    opts = parse_args(["--project", "e00", "--replay", "a", "--grammar", "g", "--prompt",
                       "p", "--log", "l", "--stub", "--n-predict", "6", "--template", "none"])
    assert opts["n_predict"] == 6 and opts["template"] == "none"
