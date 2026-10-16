"""Shared fixtures: the repository root, found from this file, for the grammars."""
from pathlib import Path

import pytest


@pytest.fixture
def repo_root():
    return Path(__file__).resolve().parents[4]
