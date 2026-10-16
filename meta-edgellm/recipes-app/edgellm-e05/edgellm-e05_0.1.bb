SUMMARY = "e05, the ADXL345 event reporter: one Rust executable with the engine inside"
DESCRIPTION = "The decision loop in Rust over the llama-cpp-2 binding, reading the ADXL345 \
through the kernel's IIO driver on the Pi 3 Model B and logging the rule beside the model. \
The first Rust recipe of the layer: the cargo class instead of the shared C include."
HOMEPAGE = "https://github.com/ambrosiobing/edge-llm-pi"

LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# The sources are the tree this layer sits in, as in recipes-app/edgellm-common.inc: the
# project folder, the shared crate it depends on by path, and the unit from files/.
FILESEXTRAPATHS:prepend := "${EDGELLM_REPO_ROOT}:"
SRC_URI = "\
    file://common/rust/edgellm \
    file://projects/e05-adxl345-events \
    file://edgellm-e05.service \
"

# cargo.bbclass fetches every crate of the lock file through crate:// entries with their
# checksums. That list is written by the class itself on the first build host and is not
# written by hand:
#     bitbake -c update_crates edgellm-e05
# then commit the edgellm-e05-crates.inc it produces beside this file and uncomment:
# require edgellm-e05-crates.inc

S = "${WORKDIR}"
CARGO_SRC_DIR = "projects/e05-adxl345-events"

inherit cargo systemd

# The binding builds llama.cpp inside the crate at the commit it bundles, so the engine
# package is not linked; it stays a dependency so the image carries llama-bench, the
# instrument every tokens per second figure is published beside, and so the dynamic-link
# feature of the binding (the extra exercise) has a library to find. cmake-native builds the
# bundled engine; bindgen needs libclang at build time, which on scarthgap is the meta-clang
# layer's clang-native, not yet in the kas files (to verify on the first parse).
DEPENDS = "llama-cpp cmake-native clang-native"
RDEPENDS:${PN} = "llama-cpp model-smollm2-360m"

# The engine, not the stub: a bench image must never carry a binary that answers from the
# grammar alone. Whether the class accepts the two flags through this variable is to
# verify on the first build; the fallback is EXTRA_CARGO_FLAGS or a do_compile:prepend.
CARGO_BUILD_FLAGS:append = " --no-default-features --features engine"

do_install:append() {
    install -d ${D}${datadir}/edgellm/e05
    install -m0644 ${S}/projects/e05-adxl345-events/prompt.txt \
        ${D}${datadir}/edgellm/e05/prompt.txt
    install -m0644 ${S}/projects/e05-adxl345-events/grammar.gbnf \
        ${D}${datadir}/edgellm/e05/grammar.gbnf
    install -d ${D}${datadir}/edgellm/e05/replay
    install -m0644 ${S}/projects/e05-adxl345-events/replay/*.csv \
        ${D}${datadir}/edgellm/e05/replay/
    install -Dm0644 ${WORKDIR}/edgellm-e05.service \
        ${D}${systemd_system_unitdir}/edgellm-e05.service
}

SYSTEMD_SERVICE:${PN} = "edgellm-e05.service"
SYSTEMD_AUTO_ENABLE = "enable"

FILES:${PN} += "\
    ${datadir}/edgellm/e05 \
    ${systemd_system_unitdir}/edgellm-e05.service \
"
