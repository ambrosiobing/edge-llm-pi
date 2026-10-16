SUMMARY = "e01, bring-up: the tokens-per-second harness priced against llama-bench"
DESCRIPTION = "The C++17 harness of project e01, so that the tokens-per-second table \
can be taken on the production image and not only on Raspberry Pi OS. The reference \
instrument, llama-bench, comes from the llama-cpp runtime recipe; this package adds \
only the harness and its prompt."

# e01 does not require recipes-app/edgellm-common.inc on purpose: that include installs
# a project binary named edgellm-eNN with a grammar and a replay folder, and e01 has
# none of those. It decides nothing, so it has no grammar; it reads no events, so it
# has no replay. The source handling below is the include's, line for line, so the
# recipe builds the checked-out tree with no network and no second clone.
HOMEPAGE = "https://github.com/ambrosiobing/edge-llm-pi"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS:prepend := "${EDGELLM_REPO_ROOT}:"

SRC_URI = "\
    file://CMakeLists.txt \
    file://cmake \
    file://common \
    file://grammars \
    file://projects/e01-bringup \
"

# scarthgap unpacks into WORKDIR; walnascar and later move this to ${UNPACKDIR}.
S = "${WORKDIR}"

DEPENDS = "llama-cpp"
RDEPENDS:${PN} = "llama-cpp"

inherit cmake pkgconfig

# The installed libllama from the llama-cpp recipe, not the submodule; no host tests;
# no second copy of llama-bench. The harness target itself is skipped in a stub build,
# so EDGELLM_STUB_MODEL stays at its default OFF here.
EXTRA_OECMAKE = "\
    -DEDGELLM_SYSTEM_LLAMA=ON \
    -DEDGELLM_BUILD_TESTS=OFF \
    -DEDGELLM_BUILD_LLAMA_TOOLS=OFF \
"

do_install() {
    install -Dm0755 ${B}/projects/e01-bringup/e01_bench_harness \
        ${D}${bindir}/edgellm-e01-harness
    install -d ${D}${datadir}/edgellm/e01
    install -m0644 ${S}/projects/e01-bringup/prompt.txt ${D}${datadir}/edgellm/e01/prompt.txt
    install -m0644 ${S}/projects/e01-bringup/wiring.md ${D}${datadir}/edgellm/e01/wiring.md
}

FILES:${PN} = "\
    ${bindir}/edgellm-e01-harness \
    ${datadir}/edgellm/e01 \
"
