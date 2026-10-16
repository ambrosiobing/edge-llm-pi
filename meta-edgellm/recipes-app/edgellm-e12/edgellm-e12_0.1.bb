SUMMARY = "e12, the STWIN.box vibration band reporter"
DESCRIPTION = "Reads 6 kHz vibration frames from the STWIN.box over USB CDC-ACM, windows \
them, computes four band energies with a C++ FFT, applies the band-shift rule and asks the \
model for a two-sentence report on the DSI panel. Pi 4B, no HAT."

EDGELLM_PROJECT = "e12"
EDGELLM_PROJECT_DIR = "e12-stwin-band-reporter"
EDGELLM_MODEL_PACKAGE = "model-qwen2.5-0.5b"

# The CDC port is read through the shared serial backend.
EDGELLM_IO_OPTIONS = "-DEDGELLM_IO_SERIAL=ON"

require recipes-app/edgellm-common.inc

# The FFT: pocketfft (BSD 3-clause), header only, checked out as the submodule
# third_party/pocketfft on its cpp branch. This recipe requires the submodule: do_fetch
# fails without it, on purpose, because the image must carry the transform the chapter
# names. The project's CMakeLists.txt falls back to a built-in radix-2 transform only on a
# host clone without the submodule, for the CI tests. The md5 below is a placeholder that
# do_populate_lic refuses; replace it with the checksum of the submodule's LICENSE.md on
# the first build (to verify).
SRC_URI += "file://third_party/pocketfft"
LICENSE = "MIT & BSD-3-Clause"
LIC_FILES_CHKSUM += "file://third_party/pocketfft/LICENSE.md;md5=00000000000000000000000000000000"

# The CDC-ACM port must be in the image's kernel: CONFIG_USB_ACM, which the Raspberry Pi
# defconfig carries. The unit waits for the device node.
