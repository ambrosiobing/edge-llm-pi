# common/python/edgellm

The shared Python package of the volume, written for e15 and reused by e20. It is the
Python mirror of `common/c`, piece for piece, so that a log from a Python project and a
log from a C project have the same columns and `tools/agreement.py` reads both:

| Module | C counterpart | What it holds |
|---|---|---|
| `edgellm/engine.py` | `edgellm.h`, `edgellm.c`, `edgellm_stub.c` | `LlamaEngine` over llama-cpp-python (grammar, system prompt decoded once, `save_state` and `load_state` per decision), `StubEngine` answering the grammar's first literal, `Config`, `Result` |
| `edgellm/io.py` | `io.h`, `io_replay.c`, `io_serial.c` | `Event`, `ReplaySource` (the same CSV form), `SerialLink` and `SerialSource` over pyserial |
| `edgellm/rules.py` | `rules.h`, `rules.c` | the `Rule` protocol, `History`, `register` and `find` |
| `edgellm/elog.py` | `elog.h`, `elog.c` | `EventLog`: exactly the C columns and quoting |
| `edgellm/template.py` | `edgellm_main.c` | the ChatML, Zephyr and none wrappings |
| `edgellm/loop.py` | `edgellm_main.c` | `run_loop` in the C order, `main(argv, project)` with the C options |

Install by path, then the project; the tests need no model and no engine:

    python -m pip install -e common/python/edgellm
    python -m pytest common/python/edgellm

The engine is an extra (`pip install -e "common/python/edgellm[engine]"`, or
`requirements.txt` at the root), because `llama-cpp-python` builds llama.cpp from
source at install time; how long that takes on a Pi 4B is not measured yet. The
`llama_cpp` import is guarded, so the stub path, the replay and the logger import
without it.

A project supplies an object with `format_prompt(ev, history)` and
`actuate(source, rule, index, ev)`, registers its rule, and calls `edgellm.main(argv,
project)`; `projects/e15-esp32-sidecar/edgellm_e15/main.py` is the worked example. The
rule acts through the source's `actuate()`; the model's answer only reaches the log.

Method names of llama-cpp-python used here were read from its API reference on
Wednesday 14 October 2026; the behaviour of `generate(reset=False)` after
`load_state()` is confirmed by the first bench run (to verify). Written Wednesday
14 October 2026.
