SUMMARY = "e11, the Nucleo CDC sensor hub narrator"
DESCRIPTION = "The loop of project e11 over the sensor hub of the Linux volume's project \
12: a NUCLEO-H7A3ZI-Q with the X-NUCLEO-IKS5A1 streaming framed CBOR over USB CDC. The \
frame layer decodes the frames with tinycbor; the rule decides QUIET, LOW_G_EVENT or \
HIGH_G_EVENT per frame; the model writes the decision and one line into the log."

EDGELLM_PROJECT = "e11"
EDGELLM_PROJECT_DIR = "e11-nucleo-hub-narrator"
EDGELLM_MODEL_PACKAGE = "model-qwen2.5-0.5b"

# No live backend of common/c is compiled in: the frame layer owns the port itself.
EDGELLM_IO_OPTIONS = ""

require recipes-app/edgellm-common.inc

# The decoder. tinycbor is MIT; the project's CMakeLists.txt finds it through pkg-config
# (the include already inherits pkgconfig), so the tinycbor recipe must install its
# tinycbor.pc. The provider of the recipe named here is to verify on the first layer
# parse: the layer index could not be read on Wednesday 14 October 2026, and the three
# usual meta-oe paths (recipes-devtools, recipes-support, recipes-connectivity) did not
# hold one. If no layer provides it, a four-line recipe over the v0.6.1 tarball with the
# cmake class, or EXTRA_OECMAKE += "-DEDGELLM_E11_TINYCBOR_SRC=..." against a checkout,
# are the two routes, and the chapter names both.
DEPENDS += "tinycbor"

# The hub's ST-LINK virtual COM port gets a stable name and a device unit the service
# binds to, and ModemManager is told to leave it alone. The product id 374e is the
# ST-LINK V3 with the VCP, as project 12 read it with lsusb; confirm it on the first plug.
SRC_URI += "file://80-edgellm-e11.rules"

do_install:append() {
    install -Dm0644 ${WORKDIR}/80-edgellm-e11.rules \
        ${D}${sysconfdir}/udev/rules.d/80-edgellm-e11.rules
}

FILES:${PN} += "${sysconfdir}/udev/rules.d/80-edgellm-e11.rules"
