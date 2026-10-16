SUMMARY = "e02, the decision loop: grammar, state reuse, control harness"
DESCRIPTION = "The reference decision library every later chapter links, run over its \
own replay file on the Pi 4B with no HAT. The worked example of a project recipe: \
three variables and the shared include."

# The project folder is written by the e02 chapter's author; the path is the
# repository convention projects/eNN-slug. If the slug differs, this line is the one
# to change.
EDGELLM_PROJECT = "e02"
EDGELLM_PROJECT_DIR = "e02-decision-loop"
EDGELLM_MODEL_PACKAGE = "model-qwen2.5-0.5b"

# e02 replays a CSV; no live backend is compiled in.
EDGELLM_IO_OPTIONS = ""

require recipes-app/edgellm-common.inc
