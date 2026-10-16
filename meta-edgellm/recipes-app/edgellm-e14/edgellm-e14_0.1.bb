# edgellm-e14_0.1.bb: the joint monitor and its engine daemon, the layer's first Rust
# recipe. It does NOT include recipes-app/edgellm-common.inc: that file is the shared
# pattern for a C or C++ project built with the cmake class against the installed
# libllama, and this project is cargo with two binaries and no CMake at all.
#
# Written Wednesday 14 October 2026. Two things about this recipe are honest limits
# rather than decisions, and the chapter says both:
#
#   1. It builds with the crate's DEFAULT features, which pull in no dependency at all,
#      so BitBake needs no crate vendoring, no network and no crate:// list to parse and
#      build what is here. The bench build is
#          cargo build --release --features live,engine
#      and the two crates that brings in, socketcan 4.0.0 and llama-cpp-2 0.1.159, need
#      their dependency tree written into a generated include before the image can carry
#      them:
#          bitbake -c update_crates edgellm-e14
#      writes it, and the line below is commented until that file is in the tree. So the
#      image installs the service without its SocketCAN backend and the daemon with the
#      stub, which is a package that runs and answers and watches nothing; the first
#      real run is scp plus the two units on Raspberry Pi OS, which is the volume's rule
#      for every project anyway.
#   2. The engine feature compiles llama.cpp a second time inside cargo, through
#      llama-cpp-sys-2, instead of linking the layer's llama-cpp recipe at the pinned
#      tag. That contradicts the layer's own rule of one engine at one revision. Whether
#      llama-cpp-sys-2 can be told to link an installed libllama is to verify; until it
#      can, the C ABI daemon of project e18 is the route that keeps one engine in the
#      image, and this recipe's daemon stays the stub there.
#
# require edgellm-e14-crates.inc

SUMMARY = "Joint monitor: one classic CAN bus, one ADXL345, a rule, and a model daemon"
DESCRIPTION = "The e13 service with the language model moved into a second process. The \
rule decides and logs; the daemon answers over a Unix domain socket. INSPECT is a \
recommendation to a person and nothing is actuated."
HOMEPAGE = "https://github.com/ambrosiobing/edge-llm-pi"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# The sources are the tree this layer sits in, exported by conf/layer.conf, so the
# recipe builds exactly the checked out revision with no second clone.
FILESEXTRAPATHS:prepend := "${EDGELLM_REPO_ROOT}:"

SRC_URI = "\
    file://projects/e14-joint-monitor \
    file://grammars/e14_inspect.gbnf \
"

S = "${WORKDIR}/projects/e14-joint-monitor"

inherit cargo systemd

# scarthgap unpacks into WORKDIR; walnascar and later move this to UNPACKDIR, and the
# two lines below are what has to change then.
CARGO_SRC_DIR = ""
CARGO_MANIFEST_PATH = "${S}/Cargo.toml"

# Empty on purpose; the bench build sets "live,engine". See the note at the top.
EDGELLM_E14_FEATURES ?= ""
CARGO_BUILD_FLAGS:append = " ${@bb.utils.contains('EDGELLM_E14_FEATURES', '', '', \
    '--features ' + d.getVar('EDGELLM_E14_FEATURES'), d)}"

RDEPENDS:${PN} = "model-qwen2.5-0.5b"

do_install() {
    install -Dm0755 ${B}/${BUILD_DIR}/edgellm-e14 ${D}${bindir}/edgellm-e14
    install -Dm0755 ${B}/${BUILD_DIR}/edgellm-e14-engine-daemon \
        ${D}${bindir}/edgellm-e14-engine-daemon
    install -d ${D}${datadir}/edgellm/e14
    install -m0644 ${S}/prompt.txt ${D}${datadir}/edgellm/e14/prompt.txt
    install -m0644 ${S}/grammar.gbnf ${D}${datadir}/edgellm/e14/grammar.gbnf
    install -d ${D}${datadir}/edgellm/e14/replay
    install -m0644 ${S}/replay/joint_100min.csv ${D}${datadir}/edgellm/e14/replay/
    install -Dm0644 ${S}/files/edgellm-e14.service \
        ${D}${systemd_system_unitdir}/edgellm-e14.service
    install -Dm0644 ${S}/files/edgellm-e14-daemon.service \
        ${D}${systemd_system_unitdir}/edgellm-e14-daemon.service
}

# Both units are enabled, and the direction of the dependency between them is in the
# unit files: the daemon BindsTo the monitor, the monitor only Wants the daemon, so a
# stopped engine never stops the monitoring.
SYSTEMD_SERVICE:${PN} = "edgellm-e14.service edgellm-e14-daemon.service"
SYSTEMD_AUTO_ENABLE = "enable"

FILES:${PN} = "\
    ${bindir}/edgellm-e14 \
    ${bindir}/edgellm-e14-engine-daemon \
    ${datadir}/edgellm/e14 \
    ${systemd_system_unitdir}/edgellm-e14.service \
    ${systemd_system_unitdir}/edgellm-e14-daemon.service \
"

# The overlay fragment is not a kernel fragment: the two CAN controllers and the ADXL345
# are bound by three config.txt lines on card B, listed in projects/e14-joint-monitor/
# wiring.md, and the I2C bus must stay at 100 kHz:
#   dtoverlay=mcp2515,spi0-0,oscillator=16000000,interrupt=23
#   dtoverlay=mcp251xfd,spi0-1,interrupt=24
#   dtparam=i2c_arm_baudrate=100000
