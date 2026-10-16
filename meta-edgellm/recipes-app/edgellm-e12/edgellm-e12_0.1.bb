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

# The FFT: pocketfft, header only, checked out as the submodule third_party/pocketfft on
# its cpp branch. This recipe requires the submodule: do_fetch fails without it, on
# purpose, because the image must carry the transform the chapter names. The project's
# CMakeLists.txt falls back to a built-in radix-2 transform only on a host clone without
# the submodule, which is how the CI tests run.
#
# The licence is BSD-3-Clause: Max-Planck-Society, the two redistribution clauses and the
# "Neither the name" clause. The md5 below is of LICENSE.md at commit c90e55b3 on the cpp
# branch, computed on Friday 16 October 2026 from
# raw.githubusercontent.com/mreineck/pocketfft/c90e55b3.../LICENSE.md, and it replaces a
# placeholder of thirty-two zeros that do_populate_lic would have refused. If BitBake
# reports a mismatch, the upstream file changed: read the new one before editing this
# line, because the checksum exists to make that change visible.
SRC_URI += "file://third_party/pocketfft"
LICENSE = "MIT & BSD-3-Clause"
LIC_FILES_CHKSUM += "file://third_party/pocketfft/LICENSE.md;md5=bb0e49a83ba802d32f3a05a7b8b2180b"

# The CDC-ACM port must be in the image's kernel: CONFIG_USB_ACM, which the Raspberry Pi
# defconfig carries. The unit waits for the device node.
