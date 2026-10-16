"""The logger writes exactly the C columns and the C quoting, byte for byte."""
from edgellm.elog import COLUMNS, EventLog, format_row, quoted
from edgellm.engine import Result
from edgellm.io import Event


def test_columns_match_elog_h():
    assert COLUMNS == "t_s,tag,raw,rule,model,prompt_ms,generate_ms,n_generated,engine"


def test_quoting_doubles_quotes_and_flattens_newlines():
    assert quoted('{"decision":"NORMAL"}') == '"{""decision"":""NORMAL""}"'
    assert quoted("a\nb\r") == '"a b "'


def test_row_with_a_model_answer():
    ev = Event(3.0, "synthetic", [40.87, 3.34])
    res = Result(text='{"decision":"NORMAL"}', n_generated=0, prompt_ms=0.0, generate_ms=0.0)
    row = format_row(ev, "NORMAL", res, "stub")
    assert row == ('3.000000,"synthetic",40.87;3.34,"NORMAL","{""decision"":""NORMAL""}",'
                   '0.000,0.000,0,"stub"')


def test_row_without_a_model_answer():
    ev = Event(0.5, "budget", [4.0, 50, 6.34, 3.17, 1.27, 3.17])
    row = format_row(ev, "PATTERN_B", None, "stub")
    assert row == '0.500000,"budget",4;50;6.34;3.17;1.27;3.17,"PATTERN_B","",,,,"stub"'


def test_raw_uses_six_significant_digits():
    ev = Event(0.0, "t", [1234567.0, 0.000012345])
    assert format_row(ev, "", None, "").startswith('0.000000,"t",1.23457e+06;1.2345e-05,')


def test_file_has_header_and_rows(tmp_path):
    path = tmp_path / "log.csv"
    with EventLog(path, "stub") as log:
        assert log.row(Event(0, "a", [1]), "X", None) == 0
        assert log.rows == 1
    lines = path.read_text(encoding="ascii").split("\n")
    assert lines[0] == COLUMNS
    assert lines[1] == '0.000000,"a",1,"X","",,,,"stub"'
    assert lines[2] == ""
