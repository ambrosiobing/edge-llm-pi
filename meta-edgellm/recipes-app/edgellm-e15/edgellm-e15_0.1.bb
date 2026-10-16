SUMMARY = "e15, the ESP32 sidecar with a bounded pattern proposal (Python)"
DESCRIPTION = "The e15 loop on the shared Python package: the replayed budgets, the \
rule that picks the cheapest LED pattern meeting the on-time, the one-letter command \
to the ESP32 over the serial cable, and the model's proposal logged beside the rule's."
HOMEPAGE = "https://github.com/ambrosiobing/edge-llm-pi"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# A Python project: the setuptools3 class, not recipes-app/edgellm-common.inc, which is
# the C and C++ pattern. The sources are the tree this layer sits in, as for every
# project recipe: FILESEXTRAPATHS is the repository root from conf/layer.conf.
FILESEXTRAPATHS:prepend := "${EDGELLM_REPO_ROOT}:"

SRC_URI = "\
    file://projects/e15-esp32-sidecar \
    file://edgellm-e15.service \
"

S = "${WORKDIR}/projects/e15-esp32-sidecar"

inherit setuptools3 systemd

# The shared package (python3-edgellm, this folder) and pyserial have recipes;
# python3-pyserial is in meta-python (to verify the recipe name on scarthgap).
# llama-cpp-python has NO recipe in poky or meta-openembedded: the wheel builds
# llama.cpp from source at install time, and a recipe for it would have to carry that
# CMake build against the layer's llama-cpp runtime recipe. Until that recipe exists
# the image installs this package without the engine and the service runs with --stub;
# the dependency below is named so that the parse fails loudly rather than the run
# failing quietly on the board (to verify: write python3-llama-cpp-python in
# recipes-runtime/ against the pinned tag, or drop the line and keep --stub).
RDEPENDS:${PN} = "\
    python3-edgellm \
    python3-pyserial \
    python3-llama-cpp-python \
    ${EDGELLM_MODEL_PACKAGE} \
"
EDGELLM_MODEL_PACKAGE = "model-qwen2.5-0.5b"

do_install:append() {
    install -d ${D}${datadir}/edgellm/e15/replay
    install -m0644 ${S}/prompt.txt ${D}${datadir}/edgellm/e15/prompt.txt
    install -m0644 ${S}/grammar.gbnf ${D}${datadir}/edgellm/e15/grammar.gbnf
    install -m0644 ${S}/replay/*.csv ${D}${datadir}/edgellm/e15/replay/
    install -Dm0644 ${WORKDIR}/edgellm-e15.service \
        ${D}${systemd_system_unitdir}/edgellm-e15.service
}

SYSTEMD_SERVICE:${PN} = "edgellm-e15.service"
SYSTEMD_AUTO_ENABLE = "disable"

FILES:${PN} += "\
    ${datadir}/edgellm/e15 \
    ${systemd_system_unitdir}/edgellm-e15.service \
"
