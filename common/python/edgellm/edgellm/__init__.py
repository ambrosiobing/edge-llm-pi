"""edgellm: the shared decision loop in Python, the mirror of common/c.

One model, one grammar, one saved system-prompt state, one rule, one log. The pieces
map one to one onto the C core so that a log from a Python project and a log from a C
project are the same shape and tools/agreement.py reads both:

    engine.py    edgellm.h     LlamaEngine over llama-cpp-python; StubEngine for CI
    io.py        io.h          Event, ReplaySource, SerialLink and SerialSource
    rules.py     rules.h       the Rule protocol, History, the name table registry
    elog.py      elog.h        EventLog, exactly the C columns and quoting
    template.py  edgellm_main.c   the ChatML (and Zephyr) wrapping
    loop.py      edgellm_main.c   run_loop in the same order, and main(argv)

Python is the language of e15 and e20 only; everything else on the bench is C, C++ or
Rust (CONTRIBUTING.md). Written Wednesday 14 October 2026.
"""
from .engine import Result, StubEngine, open_engine
from .elog import EventLog
from .io import Event, ReplaySource, SerialLink, SerialSource
from .loop import LoopStats, main, run_loop
from .rules import History, Rule, find, register
from .template import wrap_system, wrap_user

__all__ = [
    "Event", "EventLog", "History", "LoopStats", "ReplaySource", "Result", "Rule",
    "SerialLink", "SerialSource", "StubEngine", "find", "main", "open_engine", "register",
    "run_loop", "wrap_system", "wrap_user",
]
__version__ = "0.1.0"
