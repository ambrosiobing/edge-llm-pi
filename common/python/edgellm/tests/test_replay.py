"""The replay source reads the same CSV form as common/c/src/io_replay.c."""
import pytest

from edgellm.io import Event, ReplaySource, parse_line

CSV = """t_s,tag,v0,v1,v2
0.0,synthetic,37.01,0.00
# a comment line is skipped

1.5,budget,4.0,50,6.34
2.0,short
"""


def test_events_in_order(tmp_path):
    path = tmp_path / "r.csv"
    path.write_text(CSV, encoding="ascii")
    with ReplaySource(path) as src:
        events = list(src)
    assert [e.t_s for e in events] == [0.0, 1.5, 2.0]
    assert events[0].tag == "synthetic" and events[0].values == [37.01, 0.0]
    assert events[1].values == [4.0, 50.0, 6.34] and events[1].n == 3
    assert events[2].values == [] and events[2].v(0) == 0.0 and events[2].v(0, 9.0) == 9.0


def test_header_is_checked(tmp_path):
    path = tmp_path / "bad.csv"
    path.write_text("time,name\n1,2\n", encoding="ascii")
    with pytest.raises(ValueError):
        ReplaySource(path)


def test_bad_number_is_an_error():
    with pytest.raises(ValueError):
        parse_line("1.0,tag,abc", 3)
    with pytest.raises(ValueError):
        parse_line("x,tag,1", 3)


def test_actuate_is_recorded(tmp_path):
    path = tmp_path / "r.csv"
    path.write_text("t_s,tag\n0,a\n", encoding="ascii")
    src = ReplaySource(path)
    assert src.actuate("pattern", 2) == 0
    assert src.actuations == [("pattern", 2)]
    src.close()


def test_event_caps_values_and_tag():
    ev = Event(1, "x" * 40, range(20))
    assert ev.n == 16 and len(ev.tag) == 31
