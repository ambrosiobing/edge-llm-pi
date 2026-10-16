#!/usr/bin/env python3
"""A stand-in for llama-server, for CI and for a host with no engine. Python is allowed
here as a test harness.

    python projects/e04-thermal-narrator/tools/mock_server.py --port 8080
    python projects/e04-thermal-narrator/tools/mock_server.py --run \
        [--check <oracle_check> <expected.csv> <log.csv>] -- <command with {PORT}>

The first form serves until stopped. The second form, the one ctest uses, binds an
ephemeral port, replaces every {PORT} in the command with it, runs the command, and,
when --check is given and the command exited 0, runs oracle_check on the log; the exit
status is the command's, or the checker's.

Three endpoints, the three the client touches:

    GET  /health      {"status":"ok"} with 200, as llama-server answers once loaded
    GET  /props       {"model_path":"mock","build_info":"mock"}
    POST /completion  the body's "grammar" expanded by its first alternative on every
                      path, exactly as common/c/src/edgellm_stub.c does for the
                      in-process projects; the answer goes back in "content" with
                      zero timings and "model":"mock"

So the mock is the stub of this project: what it answers is fixed by the grammar the
client sends, never by the prompt, and the oracle file is written from the same
expansion by make_replay.py, which imports first_expansion() from this file.
"""
import argparse
import json
import socket
import subprocess
import sys
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer

STUB_DEPTH = 8


def _rules(grammar):
    """name -> body text, from every line of the form `name ::= body`; a body runs to
    the next rule line, as in the stub."""
    rules = {}
    current = None
    for line in grammar.splitlines():
        stripped = line.lstrip()
        head, sep, tail = stripped.partition("::=")
        name = head.strip()
        if sep and name and all(c.isalnum() or c in "_-" for c in name):
            current = name
            rules[current] = tail
        elif current is not None:
            rules[current] += "\n" + line
    return rules


def _skip_item(body, i):
    """Index after the literal, class or group starting at body[i]."""
    depth = 0
    quote = ""
    n = len(body)
    while i < n:
        c = body[i]
        if quote:
            if c == "\\":
                i += 1
            elif c == quote:
                quote = ""
                if depth == 0:
                    return i + 1
        elif c in '"[':
            quote = '"' if c == '"' else "]"
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i + 1
        elif depth == 0:
            return i + 1
        i += 1
    return n


def _literal(body, i, out):
    """Appends the quoted literal starting at body[i] (the opening quote) to out and
    returns the index after the closing quote."""
    i += 1
    n = len(body)
    while i < n and body[i] != '"':
        c = body[i]
        if c == "\\" and i + 1 < n:
            i += 1
            c = body[i]
            if c == "n":
                out.append("\n")
            elif c == "t":
                out.append("\t")
            elif c == "r":
                out.append("\r")
            elif c == "x" and i + 2 < n:
                out.append(chr(int(body[i + 1:i + 3], 16)))
                i += 2
            else:
                out.append(c)
        else:
            out.append(c)
        i += 1
    return i + 1 if i < n else n


def _expand(rules, body, out, depth):
    i = 0
    n = len(body)
    while i < n:
        c = body[i]
        if c == "#":
            while i < n and body[i] != "\n":
                i += 1
        elif c.isspace():
            i += 1
        elif c == '"':
            i = _literal(body, i, out)
        elif c == "[":
            i = _skip_item(body, i)
        elif c == "(":
            after = _skip_item(body, i)
            if depth < STUB_DEPTH:
                _expand(rules, body[i + 1:after - 1], out, depth + 1)
            i = after
        elif c == "|":
            return
        elif c == "{":
            while i < n and body[i] != "}":
                i += 1
            i += 1
        elif c in "*+?":
            i += 1
        elif c.isalnum() or c in "_-":
            j = i
            while j < n and (body[j].isalnum() or body[j] in "_-"):
                j += 1
            name = body[i:j]
            if depth < STUB_DEPTH and name in rules:
                _expand(rules, rules[name], out, depth + 1)
            i = j
        else:
            i += 1


def first_expansion(grammar, root="root"):
    """The text the stub answers for this grammar: the first alternative of every rule,
    walked from root. Raises KeyError when the root rule is missing."""
    rules = _rules(grammar)
    out = []
    _expand(rules, rules[root], out, 0)
    return "".join(out)


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        if self.server.verbose:
            sys.stderr.write("mock: " + fmt % args + "\n")

    def _send(self, code, payload):
        body = json.dumps(payload).encode("ascii")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/health":
            self._send(200, {"status": "ok"})
        elif self.path == "/props":
            self._send(200, {"model_path": "mock", "build_info": "mock"})
        else:
            self._send(404, {"error": "no such endpoint"})

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        try:
            request = json.loads(self.rfile.read(length).decode("utf-8"))
        except ValueError:
            self._send(400, {"error": "body is not JSON"})
            return
        if self.path != "/completion":
            self._send(404, {"error": "no such endpoint"})
            return
        grammar = request.get("grammar")
        if not isinstance(grammar, str):
            self._send(400, {"error": "no grammar in the request"})
            return
        try:
            answer = first_expansion(grammar)
        except KeyError:
            self._send(400, {"error": "the grammar has no root rule"})
            return
        self.server.answered += 1
        self._send(200, {
            "content": answer,
            "model": "mock",
            "stop": True,
            "tokens_predicted": 0,
            "tokens_evaluated": 0,
            "timings": {"prompt_n": 0, "prompt_ms": 0.0, "predicted_n": 0,
                        "predicted_ms": 0.0},
        })


class MockServer(HTTPServer):
    def __init__(self, port, verbose):
        super().__init__(("127.0.0.1", port), Handler)
        self.verbose = verbose
        self.answered = 0


def run_with(port_server, argv, check):
    port = port_server.server_address[1]
    thread = threading.Thread(target=port_server.serve_forever, daemon=True)
    thread.start()
    command = [a.replace("{PORT}", str(port)) for a in argv]
    print("mock: port %d, running: %s" % (port, " ".join(command)))
    rc = subprocess.call(command)
    print("mock: the command exited %d after %d completions" % (rc, port_server.answered))
    if rc == 0 and check:
        checker, expected, log = check
        rc = subprocess.call([checker, log, expected])
        print("mock: oracle_check exited %d" % rc)
    port_server.shutdown()
    return rc


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--run", action="store_true",
                        help="bind an ephemeral port, run the command after --, exit "
                             "with its status")
    parser.add_argument("--check", nargs=3, metavar=("ORACLE_CHECK", "EXPECTED", "LOG"),
                        help="after a --run command exits 0, compare LOG with EXPECTED")
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("command", nargs="*")
    args = parser.parse_args()
    if args.run:
        if not args.command:
            parser.error("--run needs a command after --")
        server = MockServer(0, args.verbose)
        return run_with(server, args.command, args.check)
    server = MockServer(args.port, True)
    print("mock: serving on 127.0.0.1:%d, Ctrl-C stops it" % server.server_address[1])
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
