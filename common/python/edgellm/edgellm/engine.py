"""The engine: the mirror of common/c/include/edgellm.h over llama-cpp-python.

LlamaEngine loads the GGUF once, decodes the system prompt once, keeps its state with
Llama.save_state(), and for every decision restores it with Llama.load_state(),
decodes the user text and samples at most n_predict tokens under the GBNF grammar.
The state lives in memory, not in a file: llama-cpp-python's save_state() returns a
LlamaState object, and the C loop's file route (llama_state_save_file) has no
one-call equivalent in the binding (to verify against 0.3.36 on the bench).

StubEngine is the same face with no model behind it: it answers the grammar's first
literal on every path, exactly as common/c/src/edgellm_stub.c does, so the Python
oracle files are built the same way as the C ones. The llama_cpp import is guarded:
the stub, the replay and the logger import on a host with no engine.

Method names verified on the api reference page
https://llama-cpp-python.readthedocs.io/en/latest/api-reference/ on Wednesday
14 October 2026: Llama(model_path, n_ctx, n_threads, n_batch, seed, verbose),
tokenize(text: bytes, add_bos, special), eval(tokens), generate(tokens, ..., reset,
..., grammar), detokenize(tokens, prev_tokens, special), token_eos(), save_state(),
load_state(state), reset(); LlamaGrammar.from_string(grammar, verbose) and
from_file(file, verbose) from llama_cpp/llama_grammar.py on the main branch the same
day. The behaviour of generate() with reset=False after load_state() is the first
thing the bench run confirms (to verify).
"""
import time

try:
    from llama_cpp import Llama, LlamaGrammar
except ImportError:  # the stub path, CI, and any host without the wheel
    Llama = None
    LlamaGrammar = None


class Result:
    """One decision, the edgellm_result of the C core."""

    __slots__ = ("text", "n_prompt_tokens", "n_generated", "prompt_ms", "generate_ms", "stub")

    def __init__(self, text="", n_prompt_tokens=0, n_generated=0, prompt_ms=0.0,
                 generate_ms=0.0, stub=False):
        self.text = text
        self.n_prompt_tokens = n_prompt_tokens
        self.n_generated = n_generated
        self.prompt_ms = prompt_ms
        self.generate_ms = generate_ms
        self.stub = stub


class Config:
    """edgellm_config with the same defaults: n_ctx 512, 4 threads, n_predict 8, greedy."""

    def __init__(self, model_path="", grammar_path="", system_prompt="", state_path=None,
                 n_ctx=512, n_threads=4, n_predict=8, temperature=0.0, seed=1):
        self.model_path = model_path
        self.grammar_path = grammar_path
        self.system_prompt = system_prompt
        self.state_path = state_path
        self.n_ctx = n_ctx
        self.n_threads = n_threads
        self.n_predict = n_predict
        self.temperature = temperature
        self.seed = seed


# ---------------------------------------------------------------- the stub

STUB_DEPTH = 8


def _is_ident(ch):
    return ch.isalnum() or ch in "_-"


def _rules(grammar_text):
    """name -> body text, for the first definition of each name, as find_rule() in C."""
    rules = {}
    for line in grammar_text.splitlines():
        stripped = line.lstrip()
        i = 0
        while i < len(stripped) and _is_ident(stripped[i]):
            i += 1
        name = stripped[:i]
        rest = stripped[i:].lstrip()
        if name and rest.startswith("::="):
            if name not in rules:
                rules[name] = rest[3:]
        elif rules and name == "" and stripped and not stripped.startswith("#"):
            last = list(rules)[-1]
            rules[last] += "\n" + stripped
    return rules


def _skip_item(body, i):
    """Position after the literal, class or group starting at body[i]."""
    depth = 0
    quote = ""
    while i < len(body):
        ch = body[i]
        if quote:
            if ch == "\\":
                i += 1
            elif ch == quote:
                quote = ""
                if depth == 0:
                    return i + 1
        elif ch == '"' or ch == "[":
            quote = '"' if ch == '"' else "]"
        elif ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return i + 1
        elif depth == 0:
            return i + 1
        i += 1
    return len(body)


def _literal(body, i, out):
    """Appends the literal starting at the quote at body[i]; returns the position after."""
    i += 1
    while i < len(body) and body[i] != '"':
        ch = body[i]
        if ch == "\\" and i + 1 < len(body):
            i += 1
            esc = body[i]
            if esc == "n":
                out.append("\n")
            elif esc == "t":
                out.append("\t")
            elif esc == "r":
                out.append("\r")
            elif esc == "x" and i + 2 < len(body):
                out.append(chr(int(body[i + 1:i + 3], 16)))
                i += 2
            else:
                out.append(esc)
        else:
            out.append(ch)
        i += 1
    return i + 1


def _expand(rules, body, out, depth):
    """The first alternative of body, left to right, as expand() in edgellm_stub.c."""
    i = 0
    while i < len(body):
        ch = body[i]
        if ch == "#":
            while i < len(body) and body[i] != "\n":
                i += 1
        elif ch.isspace():
            i += 1
        elif ch == '"':
            i = _literal(body, i, out)
        elif ch == "[":
            i = _skip_item(body, i)
        elif ch == "(":
            after = _skip_item(body, i)
            if depth < STUB_DEPTH:
                _expand(rules, body[i + 1:after - 1], out, depth + 1)
            i = after
        elif ch == "|":
            return
        elif ch == "{":
            while i < len(body) and body[i] != "}":
                i += 1
            i += 1
        elif ch in "*+?":
            i += 1
        elif _is_ident(ch):
            j = i
            while j < len(body) and _is_ident(body[j]):
                j += 1
            name = body[i:j]
            if depth < STUB_DEPTH and name in rules:
                _expand(rules, rules[name], out, depth + 1)
            i = j
        else:
            i += 1


def stub_answer(grammar_text):
    """The grammar's first literal on every path from root, or ValueError without root."""
    rules = _rules(grammar_text)
    if "root" not in rules:
        raise ValueError("grammar has no root rule")
    out = []
    _expand(rules, rules["root"], out, 0)
    return "".join(out)


class StubEngine:
    """The same face as LlamaEngine with no model: timings 0, engine 'stub'."""

    version = "stub"

    def __init__(self, cfg):
        with open(cfg.grammar_path, "r", encoding="ascii") as fh:
            self.answer = stub_answer(fh.read())
        self.n_predict = cfg.n_predict

    def decide(self, user_text):
        return Result(text=self.answer, stub=True)

    def close(self):
        pass


# ---------------------------------------------------------------- the engine

class LlamaEngine:
    """edgellm_open, edgellm_decide, edgellm_close over llama-cpp-python."""

    def __init__(self, cfg):
        if Llama is None:
            raise RuntimeError("llama-cpp-python is not installed; use --stub or "
                               "pip install -r requirements.txt (builds the engine)")
        with open(cfg.grammar_path, "r", encoding="ascii") as fh:
            grammar_text = fh.read()
        self.grammar = LlamaGrammar.from_string(grammar_text, verbose=False)
        self.n_predict = cfg.n_predict
        self.temperature = cfg.temperature
        self.llm = Llama(model_path=cfg.model_path, n_ctx=cfg.n_ctx, n_threads=cfg.n_threads,
                         n_batch=cfg.n_ctx, seed=cfg.seed, verbose=False)
        self.eos = self.llm.token_eos()
        # The system prompt, decoded once; its state kept for every decision.
        self.llm.reset()
        system_tokens = self.llm.tokenize(cfg.system_prompt.encode("utf-8"), add_bos=True,
                                          special=True)
        self.llm.eval(system_tokens)
        self.n_system = len(system_tokens)
        self.state = self.llm.save_state()
        self.version = "llama-cpp-python %s" % _binding_version()

    def decide(self, user_text):
        t0 = time.perf_counter()
        self.llm.load_state(self.state)
        user_tokens = self.llm.tokenize(user_text.encode("utf-8"), add_bos=False, special=True)
        if len(user_tokens) > 1:
            self.llm.eval(user_tokens[:-1])
        t1 = time.perf_counter()
        out = []
        n = 0
        # generate() evaluates the tokens it is given, then samples; with reset=False it
        # continues from the context just restored and evaluated, so the last user token
        # is handed to it and decoded there, inside generate_ms. The grammar sampler
        # admits only tokens some continuation of root allows.
        for tok in self.llm.generate(user_tokens[-1:], temp=self.temperature, reset=False,
                                     grammar=self.grammar):
            if tok == self.eos:
                break
            out.append(tok)
            n += 1
            if n >= self.n_predict:
                break
        t2 = time.perf_counter()
        text = self.llm.detokenize(out).decode("utf-8", "replace")
        return Result(text=text, n_prompt_tokens=self.n_system + len(user_tokens),
                      n_generated=n, prompt_ms=(t1 - t0) * 1000.0,
                      generate_ms=(t2 - t1) * 1000.0, stub=False)

    def close(self):
        self.llm = None


def _binding_version():
    try:
        from importlib.metadata import version
        return version("llama-cpp-python")
    except Exception:  # a source checkout without metadata
        return "unknown"


def open_engine(cfg, stub=False):
    """edgellm_open: the stub when asked for, else the real engine."""
    return StubEngine(cfg) if stub else LlamaEngine(cfg)
