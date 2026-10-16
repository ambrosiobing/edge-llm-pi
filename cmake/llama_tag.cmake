# The one llama.cpp pin. Read by CMakeLists.txt (compiled into edgellm_engine_version),
# by .github/workflows/ci.yml (the submodule must sit at this tag) and by
# meta-edgellm/recipes-runtime/llama-cpp/llama-cpp_git.bb (SRCREV is this tag's commit).
#
# Fetched from https://api.github.com/repos/ggml-org/llama.cpp/releases/latest on
# Wednesday 14 October 2026: tag v0.6.0, published Sunday 5 October 2026. The newest build
# tag the same day was b11617; the release tag is pinned, not the build tag.
#
# Three ids belong to one tag and only one of them is the pin. Corrected Friday 16 October
# 2026, after the first workflow run failed its own tag check on all of the jobs that read
# the submodule:
#
#   69ecae922d2b60c1fa8c590279f4e2099af75111  the annotated tag object
#   d81235049384534c167caea52b85a694f6103d14  THE COMMIT v0.6.0 POINTS AT, which is the pin
#   8345f333951c661d166b00e6f9362e553768f292  target_commitish from the releases API
#
# The last of those was recorded here as "the commit the tag points at" and it is not that.
# target_commitish is the branch or commit a release was cut from; for this release it is
# a different commit altogether, "hexagon: matmul and flash-atten scalability updates
# (#29974)", while the tag is on "llama.cpp : bump version to 0.6.0 (#29997)". The field
# was quoted accurately and read wrongly, which is the failure that leaves no trace until
# something resolves the id: here, git describe --tags --exact-match returning nothing.
#
# The authority is the repository, never the releases API:
#     git rev-parse v0.6.0^{commit}
set(LLAMA_CPP_TAG "v0.6.0")
set(LLAMA_CPP_COMMIT "d81235049384534c167caea52b85a694f6103d14")
