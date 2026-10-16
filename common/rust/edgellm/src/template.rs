//! template: the chat template, the same three forms as the shared C main. SmolLM2 and
//! Qwen2.5, the ladder's defaults, are ChatML models: a system prompt and a user turn carry
//! the im_start and im_end markers, and the answer begins after the assistant marker.
//! prompt.txt stays plain text and a project's format_prompt returns plain user text; the
//! wrapping happens in run_loop, once, so no project can forget it. TinyLlama uses the
//! Zephyr form. "none" passes text through, which is what a benchmark wants. The markers are
//! parsed as special tokens by the engine (tokenize with parse_special true).

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Template {
    ChatMl,
    Zephyr,
    None,
}

impl Template {
    /// "chatml", "zephyr" or "none", the --template values.
    pub fn parse(name: &str) -> Option<Template> {
        match name {
            "chatml" => Some(Template::ChatMl),
            "zephyr" => Some(Template::Zephyr),
            "none" => Some(Template::None),
            _ => None,
        }
    }

    pub fn name(self) -> &'static str {
        match self {
            Template::ChatMl => "chatml",
            Template::Zephyr => "zephyr",
            Template::None => "none",
        }
    }
}

/// The system prompt in its template, decoded once at start.
pub fn wrap_system(t: Template, prompt: &str) -> String {
    match t {
        Template::ChatMl => format!("<|im_start|>system\n{}<|im_end|>\n", prompt),
        Template::Zephyr => format!("<|system|>\n{}</s>\n", prompt),
        Template::None => prompt.to_string(),
    }
}

/// One user turn, ending where the answer begins.
pub fn wrap_user(t: Template, user: &str) -> String {
    match t {
        Template::ChatMl => {
            format!("<|im_start|>user\n{}<|im_end|>\n<|im_start|>assistant\n", user)
        }
        Template::Zephyr => format!("<|user|>\n{}</s>\n<|assistant|>\n", user),
        Template::None => user.to_string(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn chatml_is_the_form_of_edgellm_main_c() {
        assert_eq!(
            wrap_system(Template::ChatMl, "You are."),
            "<|im_start|>system\nYou are.<|im_end|>\n"
        );
        assert_eq!(
            wrap_user(Template::ChatMl, "peak 2.7 g. Decide."),
            "<|im_start|>user\npeak 2.7 g. Decide.<|im_end|>\n<|im_start|>assistant\n"
        );
    }

    #[test]
    fn zephyr_and_none_follow_the_c_main() {
        assert_eq!(wrap_system(Template::Zephyr, "S"), "<|system|>\nS</s>\n");
        assert_eq!(wrap_user(Template::Zephyr, "U"), "<|user|>\nU</s>\n<|assistant|>\n");
        assert_eq!(wrap_system(Template::None, "S"), "S");
        assert_eq!(wrap_user(Template::None, "U"), "U");
    }

    #[test]
    fn names_round_trip() {
        for t in [Template::ChatMl, Template::Zephyr, Template::None] {
            assert_eq!(Template::parse(t.name()), Some(t));
        }
        assert_eq!(Template::parse("alpaca"), None);
    }
}
