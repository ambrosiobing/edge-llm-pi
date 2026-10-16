"""The control rule: the mirror of common/c/include/rules.h.

A rule is what acts; the model only proposes or explains. A rule sees the current event
and a short history (oldest first, the current event not yet in it) and returns an
index into its own name table, so the log and the grammar's literals agree letter for
letter. Rules register by project id and the loop finds them by name, as rule_find()
does in C. A rule never sees the model's answer.
"""
from typing import Protocol, Sequence

from .io import Event

RULE_HISTORY = 16


class History:
    """The last RULE_HISTORY events, oldest first, like rule_ctx in C."""

    def __init__(self, limit=RULE_HISTORY):
        self.limit = limit
        self.events = []

    def push(self, ev):
        self.events.append(ev)
        if len(self.events) > self.limit:
            del self.events[0]

    def __len__(self):
        return len(self.events)

    def __getitem__(self, index):
        return self.events[index]

    def last(self, n):
        """The last n events, oldest first; fewer when the history is shorter."""
        return self.events[-n:] if n > 0 else []


class Rule(Protocol):
    """What a project's rule provides: the id, the name table, the decision."""

    project: str
    names: Sequence[str]

    def __call__(self, history: History, ev: Event) -> int: ...


_REGISTRY = {}


def register(rule):
    """Register a rule under rule.project; the loop finds it with find()."""
    if not rule.names or any(not isinstance(n, str) or not n for n in rule.names):
        raise ValueError("rule %r needs a non-empty name table" % rule.project)
    _REGISTRY[rule.project] = rule
    return rule


def find(project):
    """The registered rule for a project id, or None, like rule_find()."""
    return _REGISTRY.get(project)


def registered():
    """The registered project ids, for a test harness."""
    return sorted(_REGISTRY)


def check_index(rule, index):
    """The loop's guard: an index outside the name table is a fatal rule defect."""
    if not isinstance(index, int) or index < 0 or index >= len(rule.names):
        raise ValueError("rule %s returned %r outside its %d names"
                         % (rule.project, index, len(rule.names)))
    return index
