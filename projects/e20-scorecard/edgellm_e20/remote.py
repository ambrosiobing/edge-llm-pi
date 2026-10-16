#!/usr/bin/env python3
"""The three ways e20 can reach a model, and the one it is built for: a remote server.

This is the one project in the volume whose inference is not local. The model it wants
is Qwen2.5-1.5B-Instruct at Q4_K_M, 1.12 GB, which needs about 1.87 GB of MemAvailable
under the volume's 60 percent rung rule: more than the 1 GB of either Pi 3 and more
than the Pi 4 Model B is known to have, because that board's memory has not been read
yet (e01 reads it). So the model is served by llama-server in skyhorizon's WSL and the
Pi 4B runs this client over the bench LAN. The chapter says that plainly, with the
reason, because the volume's own rule is that the Pi runs the model.

    RemoteModel   POST /completion to llama-server, with a timeout and one clear
                  failure when the host does not answer
    LocalModel    the stated fallback: Qwen2.5-0.5B on the Pi itself, through the
                  shared package's engine module, so that a run with no network still
                  produces a log, with its engine column saying which model answered
    StubModel     no model at all: the grammar's first expansion, the rule of
                  common/c/src/edgellm_stub.c mirrored in Python, which is what CI and
                  the oracle file use

The HTTP client is urllib from the standard library on purpose: nothing is added to
requirements.txt for one POST, and a client that cannot reach its server must fail with
a sentence a person can act on rather than a traceback.

The request and response field names of llama-server (prompt, grammar, n_predict,
temperature, cache_prompt; content and timings with prompt_ms, predicted_ms and
predicted_n) are the server's documented form (to verify: the page was not fetched on
the day this file was written). A response without a timings object is accepted and its
two timing cells are written as nought, which the log's engine column explains.

Written Wednesday 14 October 2026.
"""
import json
import os
import socket
import tempfile
import time
import urllib.error
import urllib.request

STUB_DEPTH = 8


class Answer:
    """What a model returned: the text, the two timings, the token count, the engine.

    The four attributes are the ones common/python/edgellm/edgellm/elog.py reads from a
    result when it writes a row, so an Answer goes into the house log unchanged."""

    __slots__ = ("text", "prompt_ms", "generate_ms", "n_generated")

    def __init__(self, text, prompt_ms=0.0, generate_ms=0.0, n_generated=0):
        self.text = text
        self.prompt_ms = float(prompt_ms)
        self.generate_ms = float(generate_ms)
        self.n_generated = int(n_generated)

    def __repr__(self):
        return "Answer(%r, %.3f, %.3f, %d)" % (self.text, self.prompt_ms,
                                               self.generate_ms, self.n_generated)


class RemoteUnreachable(Exception):
    """The server did not answer. The message names the host, the port and the route."""


class LocalEngineMissing(Exception):
    """The local fallback was asked for and the shared engine module is not there."""


# ------------------------------------------------------------------ the stub

def _alternatives(body):
    """The top-level alternatives of a GBNF rule body, quotes and brackets respected."""
    parts, current, depth, quote = [], [], 0, False
    index = 0
    while index < len(body):
        char = body[index]
        if quote:
            current.append(char)
            if char == "\\" and index + 1 < len(body):
                current.append(body[index + 1])
                index += 2
                continue
            if char == '"':
                quote = False
            index += 1
            continue
        if char == '"':
            quote = True
            current.append(char)
        elif char in "([":
            depth += 1
            current.append(char)
        elif char in ")]":
            depth -= 1
            current.append(char)
        elif char == "|" and depth == 0:
            parts.append("".join(current))
            current = []
        else:
            current.append(char)
        index += 1
    parts.append("".join(current))
    return [p.strip() for p in parts]


def _rules(text):
    """Every rule of a grammar as name to body, a body continuing over later lines."""
    rules, order = {}, []
    name = None
    for raw in text.splitlines():
        line = raw.split("#", 1)[0] if raw.lstrip().startswith("#") else raw
        head, sep, tail = line.partition("::=")
        candidate = head.strip()
        if sep and candidate and all(c.isalnum() or c in "_-" for c in candidate):
            name = candidate
            rules[name] = tail.strip()
            order.append(name)
        elif name is not None and line.strip():
            rules[name] += " " + line.strip()
    return rules


def _literal(text, index):
    """The text of the quoted literal starting at index, and the position after it."""
    out = []
    index += 1
    while index < len(text) and text[index] != '"':
        if text[index] == "\\" and index + 1 < len(text):
            index += 1
            char = text[index]
            if char == "n":
                out.append("\n")
            elif char == "t":
                out.append("\t")
            elif char == "r":
                out.append("\r")
            elif char == "x" and index + 2 < len(text):
                out.append(chr(int(text[index + 1:index + 3], 16)))
                index += 2
            else:
                out.append(char)
        else:
            out.append(text[index])
        index += 1
    return "".join(out), index + 1


def _skip_bracket(text, index, open_char, close_char):
    depth = 0
    while index < len(text):
        if text[index] == '"':
            _, index = _literal(text, index)
            continue
        if text[index] == open_char:
            depth += 1
        elif text[index] == close_char:
            depth -= 1
            if depth == 0:
                return index + 1
        index += 1
    return index


def first_expansion(grammar_text, start="root", depth=STUB_DEPTH):
    """The answer the stub gives for a grammar, by the rule of edgellm_stub.c.

    Starting at the named rule, the FIRST alternative is walked left to right. A quoted
    literal appends its text with the escapes resolved; a reference to another rule
    expands that rule's first alternative in place; a parenthesised group expands its
    own first alternative; a character class and every repetition mark append nothing.
    So the house decision grammar yields its first literal, and a bounded sentence of
    character classes yields an empty string."""
    rules = _rules(grammar_text)
    if start not in rules:
        raise ValueError("the grammar has no rule named %r" % start)

    def walk(body, left):
        if left <= 0:
            return ""
        first = _alternatives(body)[0]
        out, index = [], 0
        while index < len(first):
            char = first[index]
            if char == '"':
                text, index = _literal(first, index)
                out.append(text)
            elif char == "[":
                index = _skip_bracket(first, index, "[", "]")
            elif char == "(":
                end = _skip_bracket(first, index, "(", ")")
                out.append(walk(first[index + 1:end - 1], left - 1))
                index = end
            elif char.isalnum() or char == "_":
                begin = index
                while index < len(first) and (first[index].isalnum()
                                              or first[index] in "_-"):
                    index += 1
                reference = first[begin:index]
                if reference in rules:
                    out.append(walk(rules[reference], left - 1))
            else:
                index += 1
        return "".join(out)

    return walk(rules[start], depth)


class StubModel:
    """No model: the grammar's first expansion, every time, with timings of nought."""

    def __init__(self, grammar_text):
        self.grammar_text = grammar_text
        self.answer_text = first_expansion(grammar_text)
        self.engine = "stub"

    def health(self):
        return True

    def ask(self, user_text):
        del user_text
        return Answer(self.answer_text, 0.0, 0.0, 0)

    def close(self):
        pass


# ------------------------------------------------------------------ the remote server

class RemoteModel:
    """A client of llama-server over the bench LAN. One POST per decision.

    url is the server's base, for example http://skyhorizon.lan:8080; the completion
    route is url + "/completion" and the readiness route url + "/health". Every call
    carries the timeout, and a host that does not answer raises RemoteUnreachable with
    the host, the port, the timeout and what to do next in the message. The model name
    is not discovered from the server: it is passed in, so that the log's engine column
    names the model the operator believes is loaded and a mismatch is an operator
    error that can be read in the log rather than a silent substitution."""

    def __init__(self, url, grammar_text, model_name, n_predict=192, temperature=0.3,
                 timeout_s=20.0):
        self.url = url.rstrip("/")
        self.grammar_text = grammar_text
        self.model_name = model_name
        self.n_predict = int(n_predict)
        self.temperature = float(temperature)
        self.timeout_s = float(timeout_s)
        self.engine = "remote %s at %s" % (model_name, self.url)
        self.requests = 0

    def _unreachable(self, route, exc):
        return RemoteUnreachable(
            "e20 could not reach llama-server at %s after %.0f s (%s: %s). Start the "
            "server in skyhorizon's WSL, check that it listens on 0.0.0.0 and not on "
            "127.0.0.1, and that the Pi can reach that host; or run this project with "
            "--fallback-model for the local 0.5B, or with --stub for no model at all."
            % (route, self.timeout_s, type(exc).__name__, exc))

    def health(self):
        """True when the server answers its readiness route inside the timeout."""
        route = self.url + "/health"
        request = urllib.request.Request(route, method="GET")
        try:
            with urllib.request.urlopen(request, timeout=self.timeout_s) as response:
                return 200 <= response.status < 300
        except (urllib.error.URLError, socket.timeout, OSError) as exc:
            raise self._unreachable(route, exc) from None

    def ask(self, user_text):
        """One completion under the grammar. The timings come from the server."""
        route = self.url + "/completion"
        payload = {
            "prompt": user_text,
            "grammar": self.grammar_text,
            "n_predict": self.n_predict,
            "temperature": self.temperature,
            "cache_prompt": True,
        }
        data = json.dumps(payload).encode("ascii")
        request = urllib.request.Request(
            route, data=data, method="POST",
            headers={"Content-Type": "application/json"})
        started = time.monotonic()
        try:
            with urllib.request.urlopen(request, timeout=self.timeout_s) as response:
                body = json.loads(response.read().decode("utf-8", "replace"))
        except (urllib.error.URLError, socket.timeout, OSError) as exc:
            raise self._unreachable(route, exc) from None
        except ValueError as exc:
            raise RemoteUnreachable(
                "llama-server at %s answered something that is not JSON (%s); the "
                "route or the server version is not what this client expects"
                % (route, exc)) from None
        self.requests += 1
        round_trip_ms = 1000.0 * (time.monotonic() - started)
        timings = body.get("timings") or {}
        return Answer(
            str(body.get("content", "")),
            float(timings.get("prompt_ms", 0.0)),
            float(timings.get("predicted_ms", round_trip_ms)),
            int(timings.get("predicted_n", 0)))

    def close(self):
        pass


# ------------------------------------------------------------------ the local fallback

class LocalModel:
    """Qwen2.5-0.5B on the Pi itself, through the shared package's engine module.

    llama-cpp-python is reached only through common/python/edgellm, which is where this
    volume's Python engine lives, and the import is guarded: a host with neither the
    package nor the library still runs the stub path, which is how CI works. On the day
    this file was written the shared package's engine module had not been written yet,
    so this class raises LocalEngineMissing with the module named, and the chapter's
    figures say which engine actually answered."""

    def __init__(self, model_path, grammar_text, model_name, n_predict=192,
                 temperature=0.3, threads=4, n_ctx=512):
        self.model_path = str(model_path)
        self.model_name = model_name
        self.engine = "local %s on the board" % model_name
        try:
            from edgellm.engine import open_engine
        except ImportError as exc:
            raise LocalEngineMissing(
                "the local fallback needs common/python/edgellm/edgellm/engine.py and "
                "llama-cpp-python; neither is reachable here (%s). Run with --server "
                "for the remote 1.5B or --stub for no model." % exc) from None
        # open_engine takes a Config and a grammar PATH, so the grammar text this class is
        # given is written to a temporary file for the engine to read. The system prompt is
        # left empty on purpose: e20's loop sends the wrapped system text with every request,
        # because the remote path is a stateless completion over HTTP and the two paths must
        # answer the same prompt. The cost is that the local path re-decodes the system text
        # on every decision instead of restoring a saved prefix state as e02 does,
        # chapter prices the two separately.
        from edgellm.engine import Config
        fh = tempfile.NamedTemporaryFile("w", suffix=".gbnf", delete=False, encoding="ascii")
        try:
            fh.write(grammar_text)
        finally:
            fh.close()
        self._grammar_file = fh.name
        cfg = Config(model_path=self.model_path, grammar_path=self._grammar_file,
                     system_prompt="", n_ctx=n_ctx, n_threads=threads,
                     n_predict=n_predict, temperature=temperature)
        try:
            self._engine = open_engine(cfg)
        except (TypeError, RuntimeError, OSError) as exc:
            self._cleanup_grammar()
            raise LocalEngineMissing(
                "the local engine would not open (%s); run with --server for the remote "
                "model or --stub for no model" % exc) from None

    def health(self):
        return True

    def ask(self, user_text):
        try:
            result = self._engine.decide(user_text)
        except AttributeError as exc:
            raise LocalEngineMissing(
                "the shared package's engine has no decide() for e20 to call (%s)"
                % exc) from None
        return Answer(getattr(result, "text", str(result)),
                      getattr(result, "prompt_ms", 0.0),
                      getattr(result, "generate_ms", 0.0),
                      getattr(result, "n_generated", 0))

    def _cleanup_grammar(self):
        name = getattr(self, "_grammar_file", None)
        if name:
            try:
                os.unlink(name)
            except OSError:
                pass
            self._grammar_file = None

    def close(self):
        close = getattr(getattr(self, "_engine", None), "close", None)
        if close is not None:
            close()
        self._cleanup_grammar()
