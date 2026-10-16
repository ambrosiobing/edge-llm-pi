SUMMARY = "SmolLM2-360M-Instruct Q4_K_M, the Pi 3 family default"
HOMEPAGE = "https://huggingface.co/bartowski/SmolLM2-360M-Instruct-GGUF"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

EDGELLM_HF_REPO = "bartowski/SmolLM2-360M-Instruct-GGUF"
EDGELLM_GGUF = "SmolLM2-360M-Instruct-Q4_K_M.gguf"
require edgellm-model.inc

# 270590880 bytes, read from the repository tree on Wednesday 14 October 2026.
SRC_URI[sha256sum] = "2fa3f013dcdd7b99f9b237717fa0b12d75bbb89984cc1274be1471a465bac9c2"
