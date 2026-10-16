SUMMARY = "Qwen2.5-1.5B-Instruct Q4_K_M, Pi 4 with 4 GB or more, the text projects"
HOMEPAGE = "https://huggingface.co/Qwen/Qwen2.5-1.5B-Instruct-GGUF"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

EDGELLM_HF_REPO = "Qwen/Qwen2.5-1.5B-Instruct-GGUF"
EDGELLM_GGUF = "qwen2.5-1.5b-instruct-q4_k_m.gguf"
require edgellm-model.inc

# 1117320736 bytes, read from the repository tree on Wednesday 14 October 2026.
SRC_URI[sha256sum] = "6a1a2eb6d15622bf3c96857206351ba97e1af16c30d7a74ee38970e434e9407e"
