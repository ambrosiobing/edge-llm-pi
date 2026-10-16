"""The oracle test: the stub run over the 200 budgets against tests/oracle/e15.expected.csv.

The loop runs exactly as the project.toml stub line does, into a temporary log, and
every rule cell and every model cell must match the oracle row by row; the row count
must match too. A real-engine log is compared on the bench, not here.
"""
import csv

from edgellm_e15.main import E15Project, main


def read_rows(path):
    with open(path, newline="", encoding="ascii") as fh:
        return list(csv.DictReader(fh))


def test_stub_run_matches_the_oracle(tmp_path, project_dir, repo_root):
    log = tmp_path / "e15_stub.csv"
    argv = ["--project", "e15", "--replay", str(project_dir / "replay" / "budgets_200.csv"),
            "--grammar", str(project_dir / "grammar.gbnf"),
            "--prompt", str(project_dir / "prompt.txt"), "--log", str(log), "--stub"]
    assert main(argv) == 0
    got = read_rows(log)
    want = read_rows(repo_root / "tests" / "oracle" / "e15.expected.csv")
    assert len(got) == len(want) == 200
    for n, (g, w) in enumerate(zip(got, want), 2):
        assert g["rule"] == w["rule"], "rule differs at log line %d" % n
        assert g["model"] == w["model"], "model differs at log line %d" % n
        assert g["engine"] == "stub"


def test_prompt_names_the_numbers_and_nothing_else():
    from edgellm.io import Event
    text = E15Project().format_prompt(Event(0.0, "budget", (4.0, 50, 6.34, 3.17, 1.27, 3.17)),
                                      None)
    assert text == ("cost mA: A 6.34, B 3.17, C 1.27, D 3.17. Budget 4.0 mA. "
                    "Minimum on-time 50 percent. Decide.")
    assert E15Project().format_prompt(Event(0.0, "budget", (4.0, 50)), None) is None


def test_actuation_is_recorded_without_a_link(tmp_path, project_dir):
    from edgellm.io import ReplaySource
    src = ReplaySource(project_dir / "replay" / "budgets_200.csv")
    project = E15Project()
    assert project.actuate(src, None, 2, None) == 0
    assert src.actuations == [("pattern", 2)]
    src.close()


def test_serial_option_is_split():
    from edgellm_e15.main import split_serial
    rest, port, baud = split_serial(["--project", "e15", "--serial", "/dev/ttyUSB0:9600",
                                     "--stub"])
    assert rest == ["--project", "e15", "--stub"] and port == "/dev/ttyUSB0" and baud == 9600
    assert split_serial(["--stub"]) == (["--stub"], None, 115200)
