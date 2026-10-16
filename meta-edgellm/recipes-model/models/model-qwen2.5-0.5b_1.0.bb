SUMMARY = "Qwen2.5-0.5B-Instruct Q4_K_M, the Pi 4 default for enum decisions"
HOMEPAGE = "https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct-GGUF"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

EDGELLM_HF_REPO = "Qwen/Qwen2.5-0.5B-Instruct-GGUF"
EDGELLM_GGUF = "qwen2.5-0.5b-instruct-q4_k_m.gguf"
require edgellm-model.inc

# 491400032 bytes, read from the repository tree on Wednesday 14 October 2026.
SRC_URI[sha256sum] = "74a4da8c9fdbcd15bd1f6d01d621410d31c6fc00986f5eb687824e7b93d7a9db"
