"""The model lives in ~/models/, and every manifest says so.

The manifests' [run], [build], [oracle] and [comparison] tables are read by people, not by
code: nothing in this repository executes them, which was checked rather than assumed. That
makes them documentation, and documentation whose commands do not run is worse than none,
because it looks authoritative at the bench.

Everything in those commands is relative to the repository root, because that is where they
are run from. The model file is the single exception, because it is the single thing that
does not live in the repository: *.gguf is ignored, one copy in ~/models/ serves every
clone, and a clean that removes ignored files does not take 491 MB with it.

Until Friday 16 October 2026 sixteen manifests disagreed with their own projects' wiring on
this, naming a repo-relative path while all twenty wiring files named the home one. e02's
run command pointed at a file that was not there. This test is what stops that returning.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PROJECTS = os.path.join(ROOT, "projects")


def manifests():
    for slug in sorted(os.listdir(PROJECTS)):
        path = os.path.join(PROJECTS, slug, "project.toml")
        if os.path.isfile(path):
            yield slug, path, open(path, encoding="ascii").read()


def test_there_are_twenty_manifests():
    """A sweep that finds nothing proves nothing unless it looked everywhere."""
    assert len(list(manifests())) == 20


def test_no_manifest_names_a_repo_relative_model():
    offenders = [slug for slug, _, text in manifests() if "--model models/" in text]
    assert offenders == []


def test_the_manifests_that_name_a_model_use_one_of_the_two_real_homes():
    """Two legitimate homes, and the test must be able to tell them from the wrong one.

    A command either runs from a clone, where the model is in ~/models/, or runs from the
    Yocto image, where the recipe installs it under /usr/share/edgellm/models/ beside the
    grammar and the prompt, with the binary on PATH and the log in /var/lib/edgellm/. e13's
    live command is the second kind and is correct. Only a repo-relative models/ is wrong,
    because nothing puts a model there.

    The first draft of this test allowed only the home path and failed on e13. That is the
    shape of mistake this repository has made before, with a guard that refused an oracle
    ARGS entry containing a space and so rejected the one legitimate use of it: a check
    that cannot tell the broken case from the working one is worse than no check.
    """
    allowed = ("~/models/", "/usr/share/edgellm/models/")
    naming = [(slug, text) for slug, _, text in manifests() if "--model " in text]
    assert naming, "no manifest names a model at all, which cannot be right"
    saw_image_form = False
    for slug, text in naming:
        for line in text.splitlines():
            if "--model " in line:
                after = line.split("--model ", 1)[1]
                assert after.startswith(allowed), \
                    "%s: --model is under neither home: %s" % (slug, after[:60])
                saw_image_form |= after.startswith("/usr/share/edgellm/models/")
    assert saw_image_form, \
        "no manifest uses the image path any more; if that is deliberate, delete this line"


def test_the_download_instruction_agrees():
    """MODELS.md is where someone reads how to get the file in the first place."""
    text = open(os.path.join(ROOT, "models", "MODELS.md"), encoding="ascii").read()
    assert "curl -L -o ~/models/" in text
    assert "curl -L -o models/" not in text
