#!/bin/sh
# Points this clone at the repository's own hooks. Run once after cloning:
#     sh tools/install-hooks.sh
# The commit-msg hook refuses attribution words (tools/no_trace_words.txt) and the
# pre-commit hook refuses the authoring files (tools/no_authoring_files.py).
set -e
root=$(git rev-parse --show-toplevel)
git -C "$root" config core.hooksPath .githooks
chmod +x "$root/.githooks/commit-msg" "$root/.githooks/pre-commit" 2>/dev/null || true
echo "hooks installed: $(git -C "$root" config --get core.hooksPath)"
