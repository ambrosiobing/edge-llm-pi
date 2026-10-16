# The shared pattern for a C or C++ project recipe. A project recipe sets three
# variables and requires this file; recipes-app/edgellm-e02/edgellm-e02_0.1.bb is the
# worked example.
#
#   EDGELLM_PROJECT       "e02"                        the short id, rule_find() name
#   EDGELLM_PROJECT_DIR   "e02-decision-loop"          the folder under projects/
#   EDGELLM_MODEL_PACKAGE "model-qwen2.5-0.5b"         the model recipe it runs on
#   EDGELLM_IO_OPTIONS    "-DEDGELLM_IO_GPIO=ON ..."   the backends the bench run needs
#
# The sources are not fetched from a remote: they are the tree this layer sits in.
# FILESEXTRAPATHS points at the repository root (exported by conf/layer.conf) and
# SRC_URI names the directories the top-level CMakeLists.txt needs, so the recipe
# builds exactly the checked-out revision with no network and no second clone. The
# systemd unit comes from the recipe's own files/ directory.
#
# A Rust project (e05, e13, e14, e18's service) uses meta-rust's cargo class and a
# Python project (e15, e20) setuptools3; they do not include this file.

HOMEPAGE = "https://github.com/ambrosiobing/edge-llm-pi"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

EDGELLM_PROJECT ?= ""
EDGELLM_PROJECT_DIR ?= ""
EDGELLM_MODEL_PACKAGE ?= "model-smollm2-360m"
EDGELLM_IO_OPTIONS ?= ""

FILESEXTRAPATHS:prepend := "${EDGELLM_REPO_ROOT}:"

SRC_URI = "\
    file://CMakeLists.txt \
    file://cmake \
    file://common \
    file://grammars \
    file://projects/${EDGELLM_PROJECT_DIR} \
    file://edgellm-${EDGELLM_PROJECT}.service \
"

# scarthgap unpacks into WORKDIR; walnascar and later move this to ${UNPACKDIR}.
S = "${WORKDIR}"

DEPENDS = "llama-cpp"
RDEPENDS:${PN} = "llama-cpp ${EDGELLM_MODEL_PACKAGE}"

inherit cmake pkgconfig systemd

# The installed libllama from the llama-cpp recipe, not the submodule; no host tests;
# no second copy of llama-bench.
EXTRA_OECMAKE = "\
    -DEDGELLM_SYSTEM_LLAMA=ON \
    -DEDGELLM_BUILD_TESTS=OFF \
    -DEDGELLM_BUILD_LLAMA_TOOLS=OFF \
    ${EDGELLM_IO_OPTIONS} \
"

do_install() {
    install -Dm0755 ${B}/projects/${EDGELLM_PROJECT_DIR}/edgellm-${EDGELLM_PROJECT} \
        ${D}${bindir}/edgellm-${EDGELLM_PROJECT}
    install -d ${D}${datadir}/edgellm/${EDGELLM_PROJECT}
    install -m0644 ${S}/projects/${EDGELLM_PROJECT_DIR}/prompt.txt \
        ${D}${datadir}/edgellm/${EDGELLM_PROJECT}/prompt.txt
    install -m0644 ${S}/projects/${EDGELLM_PROJECT_DIR}/grammar.gbnf \
        ${D}${datadir}/edgellm/${EDGELLM_PROJECT}/grammar.gbnf
    if [ -d ${S}/projects/${EDGELLM_PROJECT_DIR}/replay ]; then
        install -d ${D}${datadir}/edgellm/${EDGELLM_PROJECT}/replay
        install -m0644 ${S}/projects/${EDGELLM_PROJECT_DIR}/replay/*.csv \
            ${D}${datadir}/edgellm/${EDGELLM_PROJECT}/replay/
    fi
    install -Dm0644 ${WORKDIR}/edgellm-${EDGELLM_PROJECT}.service \
        ${D}${systemd_system_unitdir}/edgellm-${EDGELLM_PROJECT}.service
}

SYSTEMD_SERVICE:${PN} = "edgellm-${EDGELLM_PROJECT}.service"
SYSTEMD_AUTO_ENABLE = "enable"

FILES:${PN} = "\
    ${bindir}/edgellm-${EDGELLM_PROJECT} \
    ${datadir}/edgellm/${EDGELLM_PROJECT} \
    ${systemd_system_unitdir}/edgellm-${EDGELLM_PROJECT}.service \
"
