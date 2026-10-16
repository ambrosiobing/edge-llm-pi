"""The stub answers the grammar's first literal on every path, as edgellm_stub.c does."""
import pytest

from edgellm.engine import Config, StubEngine, open_engine, stub_answer

HOUSE = '''# the house shape
root     ::= "{" ws "\\"decision\\"" ws ":" ws decision ws "}"
decision ::= "\\"NORMAL\\"" | "\\"WATCH\\"" | "\\"ALERT\\""
ws       ::= [ \\t]*
'''


def test_house_grammar_first_literal():
    assert stub_answer(HOUSE) == '{"decision":"NORMAL"}'


def test_e15_grammar_first_literal(repo_root):
    text = (repo_root / "grammars" / "e15_pattern.gbnf").read_text(encoding="ascii")
    assert stub_answer(text) == '{"decision":"PATTERN_A"}'


def test_group_and_class_and_escape():
    g = 'root ::= ("a" | "b") [0-9]+ "x\\n" rest\nrest ::= "y" | "z"\n'
    assert stub_answer(g) == "ax\ny"


def test_no_root_is_refused():
    with pytest.raises(ValueError):
        stub_answer('other ::= "a"\n')


def test_stub_engine_result(tmp_path):
    path = tmp_path / "g.gbnf"
    path.write_text(HOUSE, encoding="ascii")
    engine = open_engine(Config(grammar_path=str(path)), stub=True)
    assert isinstance(engine, StubEngine)
    res = engine.decide("<|im_start|>user\nanything<|im_end|>\n<|im_start|>assistant\n")
    assert res.text == '{"decision":"NORMAL"}'
    assert res.stub and res.prompt_ms == 0.0 and res.n_generated == 0
    assert engine.version == "stub"
