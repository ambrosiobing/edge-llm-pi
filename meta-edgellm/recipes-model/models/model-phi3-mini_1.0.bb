SUMMARY = "Phi-3-mini-4k-instruct q4, the ceiling, for a Pi 4 with 8 GB only"
HOMEPAGE = "https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-gguf"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

EDGELLM_HF_REPO = "microsoft/Phi-3-mini-4k-instruct-gguf"
EDGELLM_GGUF = "Phi-3-mini-4k-instruct-q4.gguf"
require edgellm-model.inc

# 2393231072 bytes, read from the repository tree on Wednesday 14 October 2026. Not in
# any default image: e01 reads the Pi 4's RAM first, and the rung rule (under 60
# percent of MemAvailable) admits this file only on an 8 GB board.
SRC_URI[sha256sum] = "8a83c7fb9049a9b2e92266fa7ad04933bb53aa1e85136b7b30f1b8000ff2edef"
