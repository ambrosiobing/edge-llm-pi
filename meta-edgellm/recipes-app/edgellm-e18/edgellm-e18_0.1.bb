SUMMARY = "e18, a mini industrial serial logger: the Rust device service"
DESCRIPTION = "The Rust half of project e18: it owns the two RS485 channels of the \
WS-28164 HAT, the RS232 channel on the Pi's own UART, the 64 byte register frame, the \
span rule and the log, and reaches the engine over the Unix domain socket of \
edgellm-e18-daemon. It links no engine of its own."

# A Rust project, so the cargo class of meta-rust rather than
# recipes-app/edgellm-common.inc, which is for the C and C++ projects built from the
# repository's top level CMakeLists.txt. The layer's README names meta-rust as the
# dependency this recipe adds to the image; whether the bench's kas setup already
# carries that layer at the scarthgap release is to verify on the first parse, and the
# parse, not the image build, is the check that runs first.

HOMEPAGE = "https://github.com/ambrosiobing/edge-llm-pi"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS:prepend := "${EDGELLM_REPO_ROOT}:"

# common/rust carries the shared crate the service's four modules come from; it is the
# e05 author's and is not in the tree yet, so this recipe cannot build until it is.
# That is stated here rather than discovered at build time.
SRC_URI = "\
    file://common/rust \
    file://grammars \
    file://projects/e18-industrial-logger \
"

S = "${WORKDIR}/projects/e18-industrial-logger/rust"

inherit cargo systemd

# The default feature set: no ffi. The service reaches the daemon over its socket, so
# it links neither libedgellm_abi.so nor an engine, and the two packages have no build
# time relationship at all. Only a run time one, and only a Wants=.
CARGO_BUILD_FLAGS += "--no-default-features"

RDEPENDS:${PN} = "edgellm-e18-daemon"

do_install:append() {
    install -d ${D}${datadir}/edgellm/e18
    install -m0644 ${S}/../replay/registers_600.csv \
        ${D}${datadir}/edgellm/e18/registers_600.csv
    install -Dm0644 ${S}/../files/edgellm-e18.service \
        ${D}${systemd_system_unitdir}/edgellm-e18.service
}

SYSTEMD_SERVICE:${PN} = "edgellm-e18.service"
SYSTEMD_AUTO_ENABLE = "enable"

FILES:${PN} += "\
    ${datadir}/edgellm/e18/registers_600.csv \
    ${systemd_system_unitdir}/edgellm-e18.service \
"

# The overlay this project needs is not in this recipe. The two RS485 channels come
# from dtoverlay=sc16is752-spi1,int_pin=25 with dtparam=spi=on and
# dtoverlay=spi1-3cs, and the RS232 channel from enable_uart=1 with the console
# released; those are config.txt lines for the whole card, which e13 and e14 share, so
# they belong in the layer's single
# recipes-bsp/bootfiles/rpi-config_%.bbappend rather than in one project's recipe.
# projects/e18-industrial-logger/wiring.md carries the block and the order to apply it
# in; the chapter says why half applying it fails silently.
