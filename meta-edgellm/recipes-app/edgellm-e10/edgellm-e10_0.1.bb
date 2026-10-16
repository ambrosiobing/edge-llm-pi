SUMMARY = "e10, the MCC 118 voltage event labeller"
DESCRIPTION = "The decision loop over 2 ms windows of one MCC 118 channel at 100 kS/s on \
the Pi 4B, card A, labelling each window NOISE, RAMP or STEP by a hysteresis band and a \
slope test, with the model proposing the same label from a 13-point description."

EDGELLM_PROJECT = "e10"
EDGELLM_PROJECT_DIR = "e10-mcc118-labeller"
EDGELLM_MODEL_PACKAGE = "model-qwen2.5-0.5b"

# The live source is project-local over libdaqhats; this option compiles it in and
# links the library. No shared io backend is needed.
EDGELLM_IO_OPTIONS = "-DEDGELLM_DAQHATS=ON"

require recipes-app/edgellm-common.inc

# The HAT library at build time (daqhats/daqhats.h, libdaqhats.so) and at run time.
# There is no daqhats recipe in this layer yet. The stub below is what it would be,
# to verify on the first layer parse that includes it:
#
#   SUMMARY = "MCC DAQ HAT library"
#   LICENSE = "MIT"
#   LIC_FILES_CHKSUM = "file://LICENSE.md;md5=<read it>"
#   SRC_URI = "git://github.com/mccdaq/daqhats.git;protocol=https;branch=master"
#   SRCREV = "<the commit tag v1.5.0.1 points at, published Tuesday 31 March 2026>"
#   DEPENDS = "libgpiod"            # the 1.5.0.1 release note names libgpiod under trixie
#   S = "${WORKDIR}/git"
#   do_compile() { oe_runmake -C lib }
#   do_install() {
#       install -d ${D}${libdir} ${D}${includedir}/daqhats
#       install -m0755 ${S}/lib/build/libdaqhats.so* ${D}${libdir}/      # path to verify
#       install -m0644 ${S}/include/*.h ${D}${includedir}/daqhats/
#   }
#   The EEPROM read (daqhats_read_eeproms into /etc/mcc/hats) is a first-boot job on
#   the target, not a build step: the calibration belongs to the seated board.
DEPENDS += "daqhats"
RDEPENDS:${PN} += "daqhats"
