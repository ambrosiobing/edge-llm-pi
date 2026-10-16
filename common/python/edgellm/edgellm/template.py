"""The chat template, the same three forms as common/c/src/edgellm_main.c.

SmolLM2 and Qwen2.5, the ladder's defaults, are ChatML models: the system prompt and
the user turn carry the im_start and im_end markers and the answer begins after the
assistant marker. prompt.txt stays plain text and a project's format_prompt() returns
plain user text; the wrapping happens here, once, so no project can forget it.
TinyLlama uses the Zephyr form. "none" passes text through, which is what a benchmark
wants. The strings below are byte for byte the C file's, so a state saved by the C
loop and one saved here hold the same tokens.
"""

TEMPLATES = ("chatml", "zephyr", "none")


def wrap_system(prompt, template="chatml"):
    """The system prompt in the template's form; decoded once, its state saved."""
    if template == "zephyr":
        return "<|system|>\n" + prompt + "</s>\n"
    if template == "none":
        return prompt
    if template != "chatml":
        raise ValueError("unknown template %r" % template)
    return "<|im_start|>system\n" + prompt + "<|im_end|>\n"


def wrap_user(user, template="chatml"):
    """The user turn, ending where the assistant's answer begins."""
    if template == "zephyr":
        return "<|user|>\n" + user + "</s>\n<|assistant|>\n"
    if template == "none":
        return user
    if template != "chatml":
        raise ValueError("unknown template %r" % template)
    return "<|im_start|>user\n" + user + "<|im_end|>\n<|im_start|>assistant\n"
