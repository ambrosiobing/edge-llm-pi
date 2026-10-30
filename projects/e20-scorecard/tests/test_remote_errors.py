#!/usr/bin/env python3
"""What the client says when the server refuses, as against when it cannot be reached.

    python -m pytest projects/e20-scorecard/tests/test_remote_errors.py

These cases exist because of a defect measured on Thursday 29 October 2026. The first
real run of this project returned HTTP 400, because llama-server could not parse the
grammar, and the client reported "could not reach llama-server" and advised checking
that the server listens on 0.0.0.0 and that the Pi can reach the host. The server had
been reached. The cause is that urllib.error.HTTPError is a subclass of URLError, so
the refusal fell into the unreachable branch, and the response body, which carried the
server's own explanation, was never read.

So the thing under test is the ORDER of the except clauses and not the wording of a
message: a test that only called _refused directly would pass with the clauses in the
wrong order, which is exactly the arrangement that shipped. Both cases therefore drive
RemoteModel.ask() with urlopen replaced.

Written Friday 30 October 2026.
"""
import io
import sys
import urllib.error
from pathlib import Path

import pytest

# So that the tests run in a clone where nothing has been installed yet.
PROJECT = Path(__file__).resolve().parents[1]
if str(PROJECT) not in sys.path:
    sys.path.insert(0, str(PROJECT))

from edgellm_e20 import remote                                           # noqa: E402

GRAMMAR = 'root ::= "yes"\n'
SERVER_SAID = "Failed to initialize samplers: failed to parse grammar"


def _model():
    return remote.RemoteModel("http://10.0.0.1:8080", GRAMMAR, "test.gguf", timeout_s=1.0)


def _http_error(code=400, body=SERVER_SAID):
    return urllib.error.HTTPError("http://10.0.0.1:8080/completion", code, "Bad Request",
                                  {}, io.BytesIO(body.encode("utf-8")))


def test_a_refusal_is_not_reported_as_unreachable(monkeypatch):
    """A 400 must not print the remedy for a network fault, which cannot clear it."""
    monkeypatch.setattr(remote.urllib.request, "urlopen",
                        lambda *a, **k: (_ for _ in ()).throw(_http_error()))
    with pytest.raises(remote.RemoteUnreachable) as caught:
        _model().ask("anything")
    message = str(caught.value)
    assert "refused" in message
    assert "could not reach" not in message
    assert "0.0.0.0" not in message


def test_the_server_s_own_reason_reaches_the_message(monkeypatch):
    """The body is a readable file and it says why. Reading it is the point."""
    monkeypatch.setattr(remote.urllib.request, "urlopen",
                        lambda *a, **k: (_ for _ in ()).throw(_http_error()))
    with pytest.raises(remote.RemoteUnreachable) as caught:
        _model().ask("anything")
    assert SERVER_SAID in str(caught.value)
    assert "400" in str(caught.value)


def test_a_refusal_is_still_caught_by_the_fallback(monkeypatch):
    """RemoteRefused subclasses RemoteUnreachable so open_model's fallback is unchanged."""
    assert issubclass(remote.RemoteRefused, remote.RemoteUnreachable)
    monkeypatch.setattr(remote.urllib.request, "urlopen",
                        lambda *a, **k: (_ for _ in ()).throw(_http_error(503, "loading")))
    with pytest.raises(remote.RemoteUnreachable):
        _model().ask("anything")


def test_a_real_network_fault_still_reads_as_unreachable(monkeypatch):
    """The other half of the distinction: a URLError that is not an HTTPError."""
    def boom(*a, **k):
        raise urllib.error.URLError("connection refused")
    monkeypatch.setattr(remote.urllib.request, "urlopen", boom)
    with pytest.raises(remote.RemoteUnreachable) as caught:
        _model().ask("anything")
    message = str(caught.value)
    assert "could not reach" in message
    assert not isinstance(caught.value, remote.RemoteRefused)


def test_health_makes_the_same_distinction(monkeypatch):
    """health() had the identical clause order and the identical defect."""
    monkeypatch.setattr(remote.urllib.request, "urlopen",
                        lambda *a, **k: (_ for _ in ()).throw(_http_error(404, "no route")))
    with pytest.raises(remote.RemoteRefused) as caught:
        _model().health()
    assert "404" in str(caught.value)
