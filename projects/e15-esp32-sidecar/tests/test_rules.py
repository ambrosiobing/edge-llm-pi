"""Eight cases on the e15 rule: each pattern, the two ties, the boundary and a short event."""
import pytest

from edgellm import find
from edgellm.io import Event
from edgellm.rules import History
from edgellm_e15.rules import NAMES, ON_PCT, rule_e15

COST = (6.34, 3.17, 1.27, 3.17)       # the placeholder of tools/make_replay.py


def ev(budget, on_pct, cost=COST):
    return Event(0.0, "budget", (budget, on_pct) + tuple(cost))


@pytest.mark.parametrize("on_pct, expected", [
    (100, "PATTERN_A"),      # only steady is on all the time
    (60, "PATTERN_A"),       # above 50 only steady qualifies
    (50, "PATTERN_B"),       # B and D tie at 50 percent and equal cost; the lower index wins
    (21, "PATTERN_B"),       # just above C's on-time
    (20, "PATTERN_C"),       # exactly C's on-time qualifies, strictness is at or above
    (10, "PATTERN_C"),       # the cheapest of all
])
def test_cheapest_meeting_on_time(on_pct, expected):
    assert NAMES[rule_e15(History(), ev(4.0, on_pct))] == expected


def test_measured_costs_break_the_tie():
    cheaper_d = (6.34, 3.30, 1.27, 3.10)
    assert NAMES[rule_e15(History(), ev(4.0, 50, cheaper_d))] == "PATTERN_D"


def test_budget_does_not_change_the_choice():
    assert rule_e15(History(), ev(0.5, 100)) == rule_e15(History(), ev(8.0, 100))


def test_short_event_is_steady():
    assert rule_e15(History(), Event(0.0, "budget", (4.0, 50))) == 0


def test_registered_and_consistent():
    assert find("e15") is rule_e15
    assert len(NAMES) == len(ON_PCT) == 4
    assert all(0.0 < p <= 100.0 for p in ON_PCT)
