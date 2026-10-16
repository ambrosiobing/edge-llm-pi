# The one llama.cpp pin. Read by CMakeLists.txt (compiled into edgellm_engine_version),
# by .github/workflows/ci.yml (the submodule must sit at this tag) and by
# meta-edgellm/recipes-runtime/llama-cpp/llama-cpp_git.bb (SRCREV is this tag's commit).
#
# Fetched from https://api.github.com/repos/ggml-org/llama.cpp/releases/latest on
# Wednesday 14 October 2026: tag v0.6.0, published Sunday 5 October 2026, commit
# 8345f333951c661d166b00e6f9362e553768f292 (an annotated tag, object
# 69ecae922d2b60c1fa8c590279f4e2099af75111). The newest build tag the same day was
# b11617 (Wednesday 14 October 2026); the release tag is pinned, not the build tag.
set(LLAMA_CPP_TAG "v0.6.0")
set(LLAMA_CPP_COMMIT "8345f333951c661d166b00e6f9362e553768f292")
