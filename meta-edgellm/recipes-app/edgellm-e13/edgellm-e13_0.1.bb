SUMMARY = "e13, the CAN honesty agent: per node PRESENT, MODELLED or ABSENT"
DESCRIPTION = "A Rust service that watches the heartbeats of four nodes on one CAN bus, \
declares each node present, modelled or absent from a three line rule, and asks a local \
language model for the same verdict from the same counters. The rule is what the report \
shows; the model's answer goes only to the log."

HOMEPAGE = "https://github.com/ambrosiobing/edge-llm-pi"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# A Rust project does not include recipes-app/edgellm-common.inc: that file inherits cmake
# and installs a C binary. This recipe inherits the cargo class instead, and is the layer's
# worked example for the four Rust projects (e05, e13, e14 and e18's service).
#
# The sources are the tree this layer sits in, not a remote: FILESEXTRAPATHS points at the
# repository root, exported by conf/layer.conf, and SRC_URI names the directories cargo
# needs, so the recipe builds exactly the checked out revision with no network and no
# second clone.
FILESEXTRAPATHS:prepend := "${EDGELLM_REPO_ROOT}:"

SRC_URI = "\
    file://common/rust \
    file://projects/e13-can-honesty \
    file://grammars \
    file://edgellm-e13.service \
"

# scarthgap unpacks into WORKDIR; walnascar and later move this to ${UNPACKDIR}.
S = "${WORKDIR}/projects/e13-can-honesty"
CARGO_SRC_DIR = ""

inherit cargo systemd

# The crate list is NOT hand written. `cargo bitbake` in skyhorizon's WSL reads Cargo.lock
# and writes edgellm-e13-crates.inc beside this file, carrying one SRC_URI crate:// line
# per dependency with its checksum; the command is in the chapter. include, not require,
# so that a layer parse before that file exists reports a missing fetch rather than a
# parse error.
include edgellm-e13-crates.inc

# The bench build takes the engine from the shared crate and the SocketCAN backend, and
# drops the stub. The exact variable the cargo class appends to the build command on the
# bench's scarthgap release is to verify at the first layer parse: CARGO_BUILD_FLAGS is
# the name in the class as read, and a release that renamed it would show here first.
CARGO_BUILD_FLAGS:append = " --no-default-features --features engine,live"

DEPENDS = "llama-cpp clang-native"
RDEPENDS:${PN} = "llama-cpp model-qwen2.5-0.5b can-utils"

# can-utils is a runtime dependency rather than a convenience: cangen is what produces the
# virtual node's heartbeat, and ip -details -statistics link show is the instrument the
# acceptance test reads the bus off counters from.

do_install:append() {
    install -d ${D}${datadir}/edgellm/e13
    install -m0644 ${S}/prompt.txt ${D}${datadir}/edgellm/e13/prompt.txt
    install -m0644 ${S}/grammar.gbnf ${D}${datadir}/edgellm/e13/grammar.gbnf
    install -d ${D}${datadir}/edgellm/e13/replay
    install -m0644 ${S}/replay/heartbeats_1000.csv \
        ${D}${datadir}/edgellm/e13/replay/heartbeats_1000.csv
    install -Dm0644 ${WORKDIR}/edgellm-e13.service \
        ${D}${systemd_system_unitdir}/edgellm-e13.service
}

SYSTEMD_SERVICE:${PN} = "edgellm-e13.service"
SYSTEMD_AUTO_ENABLE = "enable"

FILES:${PN} += "\
    ${datadir}/edgellm/e13 \
    ${systemd_system_unitdir}/edgellm-e13.service \
"
