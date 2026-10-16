SUMMARY = "TinyLlama-1.1B-Chat-v1.0 Q4_K_M, the pi-llm-bench reference point"
HOMEPAGE = "https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

EDGELLM_HF_REPO = "TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF"
EDGELLM_GGUF = "tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf"
require edgellm-model.inc

# 668788096 bytes, read from the repository tree on Wednesday 14 October 2026.
SRC_URI[sha256sum] = "9fecc3b3cd76bba89d504f29b616eedf7da85b96540e490ca5824d3f7d2776a0"
