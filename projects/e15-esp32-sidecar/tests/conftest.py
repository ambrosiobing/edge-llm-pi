"""The shared package by path when it is not installed, and the project folders.

A PEP 508 dependency cannot name a relative path, so pyproject.toml lists only the
PyPI pins and the shared package is installed with its own pip command; when it is
not, this file puts common/python/edgellm on the path so that pytest runs either way.
"""
import sys
from pathlib import Path

import pytest

PROJECT = Path(__file__).resolve().parents[1]
REPO = PROJECT.parents[1]
SHARED = REPO / "common" / "python" / "edgellm"

try:
    import edgellm  # noqa: F401
except ImportError:
    sys.path.insert(0, str(SHARED))
if str(PROJECT) not in sys.path:
    sys.path.insert(0, str(PROJECT))


@pytest.fixture
def project_dir():
    return PROJECT


@pytest.fixture
def repo_root():
    return REPO
