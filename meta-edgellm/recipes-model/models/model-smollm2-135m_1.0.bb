SUMMARY = "SmolLM2-135M-Instruct Q4_K_M, the NanoPi floor of the model ladder"
HOMEPAGE = "https://huggingface.co/bartowski/SmolLM2-135M-Instruct-GGUF"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

EDGELLM_HF_REPO = "bartowski/SmolLM2-135M-Instruct-GGUF"
EDGELLM_GGUF = "SmolLM2-135M-Instruct-Q4_K_M.gguf"
require edgellm-model.inc

# 105454432 bytes, read from the repository tree on Wednesday 14 October 2026.
SRC_URI[sha256sum] = "2e8040ceae7815abe0dcb3540b9995eaa1fa0d2ca9e797d0a635ae4433c68c2d"
