//! engine: the model behind the loop, two implementations behind two cargo features.
//!
//! `stub` (default): the same answer-from-the-grammar rule as common/c/src/edgellm_stub.c,
//! ported line for line so that a Rust project's oracle file and a C project's oracle file
//! mean the same thing. `engine`: llama.cpp through the llama-cpp-2 binding, in the order of
//! common/c/src/edgellm.c: restore the system prompt, reset the sampler, decode the user
//! text, sample under the grammar until n_predict or an end-of-generation token.

/// The first expansion of a grammar, the stub's whole answer. Starting at the rule named
/// `root`, the FIRST alternative is walked left to right: a quoted literal appends its text
/// (escapes \" \\ \n \t \r \xHH resolved), a reference to another rule expands that rule's
/// first alternative in place to a depth of 8, a parenthesised group expands its own first
/// alternative, a character class and every repetition mark append nothing, a comment after
/// '#' is skipped. The answer is capped at 511 bytes like the C stub's buffer.
pub fn first_expansion(grammar: &str) -> Option<String> {
    let g = grammar.as_bytes();
    let body = find_rule(g, b"root")?;
    let mut out = Vec::new();
    expand(g, body, &mut out, 0);
    Some(String::from_utf8_lossy(&out).into_owned())
}

const STUB_DEPTH: usize = 8;
const STUB_CAP: usize = 511;

fn is_ident(c: u8) -> bool {
    c.is_ascii_alphanumeric() || c == b'_' || c == b'-'
}

/// Finds `name ::=` at a line start and returns the body: after "::=" up to the start of
/// the next rule line or the end of the text.
fn find_rule<'a>(g: &'a [u8], name: &[u8]) -> Option<&'a [u8]> {
    let mut found: Option<usize> = None;
    let mut line = 0usize;
    while line < g.len() {
        let mut p = line;
        while p < g.len() && (g[p] == b' ' || g[p] == b'\t') {
            p += 1;
        }
        let id = p;
        while p < g.len() && is_ident(g[p]) {
            p += 1;
        }
        let id_len = p - id;
        while p < g.len() && (g[p] == b' ' || g[p] == b'\t') {
            p += 1;
        }
        if id_len > 0 && g[p..].starts_with(b"::=") {
            if let Some(b) = found {
                return Some(&g[b..line]);
            }
            if &g[id..id + id_len] == name {
                found = Some(p + 3);
            }
        }
        while line < g.len() && g[line] != b'\n' {
            line += 1;
        }
        if line < g.len() {
            line += 1;
        }
    }
    found.map(|b| &g[b..])
}

fn put(out: &mut Vec<u8>, c: u8) {
    if out.len() < STUB_CAP {
        out.push(c);
    }
}

/// Reads a quoted literal starting at the opening quote; returns the position after the
/// closing quote.
fn literal(b: &[u8], mut p: usize, out: &mut Vec<u8>) -> usize {
    p += 1;
    while p < b.len() && b[p] != b'"' {
        if b[p] == b'\\' && p + 1 < b.len() {
            p += 1;
            match b[p] {
                b'n' => put(out, b'\n'),
                b't' => put(out, b'\t'),
                b'r' => put(out, b'\r'),
                b'x' => {
                    if p + 2 < b.len() {
                        if let Ok(hex) = std::str::from_utf8(&b[p + 1..p + 3]) {
                            if let Ok(v) = u8::from_str_radix(hex, 16) {
                                put(out, v);
                            }
                        }
                        p += 2;
                    }
                }
                c => put(out, c),
            }
        } else {
            put(out, b[p]);
        }
        p += 1;
    }
    if p < b.len() { p + 1 } else { p }
}

/// Position after the item that starts at p: a literal, a class, or a group.
fn skip_item(b: &[u8], mut p: usize) -> usize {
    let mut depth = 0i32;
    let mut quote = 0u8;
    while p < b.len() {
        let c = b[p];
        if quote != 0 {
            if c == b'\\' {
                p += 1;
            } else if c == quote {
                quote = 0;
                if depth == 0 {
                    return p + 1;
                }
            }
            p += 1;
            continue;
        }
        if c == b'"' || c == b'[' {
            quote = if c == b'"' { b'"' } else { b']' };
            p += 1;
            continue;
        }
        if c == b'(' {
            depth += 1;
            p += 1;
            continue;
        }
        if c == b')' {
            depth -= 1;
            if depth == 0 {
                return p + 1;
            }
            p += 1;
            continue;
        }
        if depth == 0 {
            return p + 1;
        }
        p += 1;
    }
    b.len()
}

fn expand(g: &[u8], body: &[u8], out: &mut Vec<u8>, depth: usize) {
    let mut p = 0usize;
    while p < body.len() {
        let c = body[p];
        if c == b'#' {
            while p < body.len() && body[p] != b'\n' {
                p += 1;
            }
        } else if c.is_ascii_whitespace() {
            p += 1;
        } else if c == b'"' {
            p = literal(body, p, out);
        } else if c == b'[' {
            p = skip_item(body, p);
        } else if c == b'(' {
            let after = skip_item(body, p);
            if depth < STUB_DEPTH && after > p + 1 {
                expand(g, &body[p + 1..after - 1], out, depth + 1);
            }
            p = after;
        } else if c == b'|' {
            return;
        } else if c == b'{' {
            while p < body.len() && body[p] != b'}' {
                p += 1;
            }
            if p < body.len() {
                p += 1;
            }
        } else if c == b'*' || c == b'+' || c == b'?' {
            p += 1;
        } else if is_ident(c) {
            let id = p;
            while p < body.len() && is_ident(body[p]) {
                p += 1;
            }
            if depth < STUB_DEPTH {
                if let Some(rb) = find_rule(g, &body[id..p]) {
                    expand(g, rb, out, depth + 1);
                }
            }
        } else {
            p += 1;
        }
    }
}

/// The stub: no model, the grammar's first expansion on every call, timings of zero, the
/// engine version "stub".
#[cfg(feature = "stub")]
pub mod stub {
    use super::first_expansion;
    use crate::{err, Decider, Decision, Error};

    pub struct Stub {
        answer: String,
    }

    impl Stub {
        /// Fixes the answer from the grammar text; refuses a grammar without a root rule.
        pub fn open(grammar: &str) -> Result<Stub, Error> {
            match first_expansion(grammar) {
                Some(answer) => Ok(Stub { answer }),
                None => err("stub: the grammar has no root rule"),
            }
        }

        pub fn answer(&self) -> &str {
            &self.answer
        }
    }

    impl Decider for Stub {
        fn decide(&mut self, _user_text: &str) -> Result<Decision, Error> {
            Ok(Decision { text: self.answer.clone(), stub: true, ..Decision::default() })
        }

        fn engine_version(&self) -> String {
            "stub".to_string()
        }
    }
}

/// The real engine over the llama-cpp-2 binding. Type and method names were read from
/// docs.rs for 0.1.159 on Wednesday 14 October 2026: LlamaBackend::init, LlamaModelParams,
/// LlamaModel::load_from_file, LlamaContextParams::with_n_ctx / with_n_batch / with_n_threads
/// / with_n_threads_batch, LlamaModel::new_context, LlamaVocab::tokenize / is_eog /
/// token_to_piece, LlamaBatch::get_one, LlamaContext::decode / clear_kv_cache /
/// kv_cache_seq_rm, LlamaSampler::grammar / greedy / temp / dist / chain_simple / sample /
/// reset. Two names exist on docs.rs but their exact signatures are to verify on the first
/// cargo build in skyhorizon's WSL: LlamaContext::state_save_file and state_load_file; the
/// resident route (kv_cache_seq_rm back to the system prompt) is the one that needs
/// neither and is the default when no --state path is given.
#[cfg(feature = "engine")]
pub mod llama {
    use std::num::NonZeroU32;
    use std::time::Instant;

    use llama_cpp_2::context::params::LlamaContextParams;
    use llama_cpp_2::context::LlamaContext;
    use llama_cpp_2::llama_backend::LlamaBackend;
    use llama_cpp_2::llama_batch::LlamaBatch;
    use llama_cpp_2::model::params::LlamaModelParams;
    use llama_cpp_2::model::LlamaModel;
    use llama_cpp_2::sampling::LlamaSampler;
    use llama_cpp_2::token::LlamaToken;

    use crate::{err, Decider, Decision, EngineConfig, Error};

    /// The binding this crate is pinned to; the bundled llama.cpp commit is recorded in
    /// common/rust/README.md and is not the C projects' tag.
    pub const BINDING_VERSION: &str = "llama-cpp-2 0.1.159";

    const TEXT_CAP: usize = 511;

    fn ms_since(t: Instant) -> f64 {
        t.elapsed().as_secs_f64() * 1e3
    }

    fn fail<E: std::fmt::Display>(what: &'static str) -> impl FnOnce(E) -> Error {
        move |e| Error(format!("edgellm: {}: {}", what, e))
    }

    /// One model, one context, one sampler chain, the system prompt decoded once at open.
    /// The model and the backend live as long as the process (one Box::leak each, once):
    /// a service loads one model and never unloads it, and the context type borrows the
    /// model for its whole life.
    pub struct Engine {
        model: &'static LlamaModel,
        ctx: LlamaContext<'static>,
        sampler: LlamaSampler,
        sys_tokens: Vec<LlamaToken>,
        state_path: Option<String>,
        n_ctx: usize,
        n_predict: usize,
    }

    impl Engine {
        /// Loads the model, decodes the system prompt once and saves its state when a path
        /// is given, as edgellm_open() does; the grammar is read from cfg.grammar_path.
        pub fn open(cfg: &EngineConfig) -> Result<Engine, Error> {
            let grammar = crate::read_text_raw(&cfg.grammar_path)?;
            let backend = LlamaBackend::init().map_err(fail("backend init"))?;
            let backend: &'static LlamaBackend = Box::leak(Box::new(backend));
            let params = LlamaModelParams::default();
            let model = LlamaModel::load_from_file(backend, &cfg.model_path, &params)
                .map_err(fail("cannot load the model file"))?;
            let model: &'static LlamaModel = Box::leak(Box::new(model));
            let n_ctx = if cfg.n_ctx > 0 { cfg.n_ctx } else { 512 };
            let n_predict = if cfg.n_predict > 0 { cfg.n_predict } else { 8 };
            let threads = if cfg.n_threads > 0 { cfg.n_threads } else { 4 };
            let ctx_params = LlamaContextParams::default()
                .with_n_ctx(NonZeroU32::new(n_ctx as u32))
                .with_n_batch(n_ctx as u32)
                .with_n_threads(threads)
                .with_n_threads_batch(threads);
            let mut ctx = model
                .new_context(backend, ctx_params)
                .map_err(fail("cannot create the context"))?;
            let sys_tokens = model.vocab().tokenize(cfg.system_prompt.as_bytes(), true, true);
            if sys_tokens.len() + n_predict + 16 > n_ctx {
                return err("edgellm: system prompt leaves no room in n_ctx");
            }
            let mut batch = LlamaBatch::get_one(&sys_tokens).map_err(fail("system batch"))?;
            ctx.decode(&mut batch).map_err(fail("system prompt decode failed"))?;
            if let Some(path) = &cfg.state_path {
                ctx.state_save_file(path, &sys_tokens)
                    .map_err(fail("cannot save the system prompt state"))?;
            }
            let grammar = LlamaSampler::grammar(model, &grammar, "root")
                .map_err(fail("grammar rejected; is the root rule named root?"))?;
            let sampler = if cfg.temperature <= 0.0 {
                LlamaSampler::chain_simple([grammar, LlamaSampler::greedy()])
            } else {
                LlamaSampler::chain_simple([
                    grammar,
                    LlamaSampler::temp(cfg.temperature),
                    LlamaSampler::dist(cfg.seed),
                ])
            };
            Ok(Engine {
                model,
                ctx,
                sampler,
                sys_tokens,
                state_path: cfg.state_path.clone(),
                n_ctx,
                n_predict,
            })
        }

        /// Brings the context back to the end of the system prompt: reload the saved
        /// state when a path was given, otherwise remove every position past it.
        fn restore(&mut self) -> Result<(), Error> {
            let n_sys = self.sys_tokens.len();
            match &self.state_path {
                Some(path) => {
                    self.ctx.clear_kv_cache();
                    let loaded = self
                        .ctx
                        .state_load_file(path, n_sys)
                        .map_err(fail("state restore failed"))?;
                    if loaded.len() != n_sys {
                        return err("edgellm: state restore returned a different token count");
                    }
                    Ok(())
                }
                None => self
                    .ctx
                    .kv_cache_seq_rm(0, Some(n_sys as u32), None)
                    .map_err(fail("cannot trim the context back to the system prompt")),
            }
        }
    }

    impl Decider for Engine {
        fn decide(&mut self, user_text: &str) -> Result<Decision, Error> {
            self.restore()?;
            self.sampler.reset();
            let user = self.model.vocab().tokenize(user_text.as_bytes(), false, true);
            if self.sys_tokens.len() + user.len() + self.n_predict > self.n_ctx {
                return err("edgellm: prompt plus n_predict does not fit n_ctx");
            }
            let t0 = Instant::now();
            let mut batch = LlamaBatch::get_one(&user).map_err(fail("user batch"))?;
            self.ctx.decode(&mut batch).map_err(fail("prompt decode failed"))?;
            let mut out = Decision {
                prompt_ms: ms_since(t0),
                n_prompt_tokens: self.sys_tokens.len() + user.len(),
                ..Decision::default()
            };
            let t1 = Instant::now();
            let mut text: Vec<u8> = Vec::new();
            for i in 0..self.n_predict {
                let tok = self.sampler.sample(&self.ctx, -1);
                if self.model.vocab().is_eog(tok) {
                    break;
                }
                let piece = self.model.vocab().token_to_piece(tok, true, None);
                if text.len() + piece.len() >= TEXT_CAP {
                    break;
                }
                text.extend_from_slice(&piece);
                out.n_generated += 1;
                if i + 1 < self.n_predict {
                    let one = [tok];
                    let mut next = LlamaBatch::get_one(&one).map_err(fail("token batch"))?;
                    self.ctx.decode(&mut next).map_err(fail("decode failed during generation"))?;
                }
            }
            out.generate_ms = ms_since(t1);
            out.text = String::from_utf8_lossy(&text).into_owned();
            Ok(out)
        }

        fn engine_version(&self) -> String {
            BINDING_VERSION.to_string()
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    // Decider, for s.decide() below. It sat at the top of this file until Saturday
    // 17 October 2026, unused in every build except this one: both feature modules
    // import what they need for themselves, and running this crate's tests for the
    // first time that evening printed the difference, all four names unused in the
    // library build and only the other three unused in the test build.
    use crate::Decider;

    const HOUSE: &str = "# the house shape\n\
root     ::= \"{\" ws \"\\\"decision\\\"\" ws \":\" ws decision ws \"}\"\n\
decision ::= \"\\\"NORMAL\\\"\" | \"\\\"WATCH\\\"\" | \"\\\"ALERT\\\"\"\n\
ws       ::= [ \\t]*\n";

    const E05: &str = "root     ::= \"{\" ws \"\\\"decision\\\"\" ws \":\" ws decision ws \",\" \
ws why ws \"}\"\n\
why      ::= \"\\\"why\\\"\" ws \":\" ws sentence\n\
decision ::= \"\\\"NONE\\\"\" | \"\\\"TAP\\\"\" | \"\\\"TILT\\\"\" | \"\\\"FREEFALL\\\"\"\n\
sentence ::= \"\\\"\" [A-Za-z0-9 ,%-]{8,120} \".\" \"\\\"\"\n\
ws       ::= [ \\t]*\n";

    #[test]
    fn the_house_grammar_expands_to_its_first_literal() {
        assert_eq!(first_expansion(HOUSE).unwrap(), "{\"decision\":\"NORMAL\"}");
    }

    #[test]
    fn the_e05_grammar_expands_to_none_and_an_empty_sentence() {
        assert_eq!(first_expansion(E05).unwrap(), "{\"decision\":\"NONE\",\"why\":\".\"}");
    }

    #[test]
    fn a_grammar_without_root_is_refused() {
        assert!(first_expansion("start ::= \"a\"\n").is_none());
    }

    #[test]
    fn escapes_groups_and_repetitions_follow_the_c_stub() {
        let g = "root ::= (\"a\" | \"b\") \"\\t\" [x-z]+ \"\\x41\"? tail{2,3}\n\
                 tail ::= \"c\" | \"d\"\n";
        assert_eq!(first_expansion(g).unwrap(), "a\tAc");
    }

    #[test]
    fn a_rule_reference_follows_the_first_alternative_only() {
        let g = "root ::= one | two\none ::= \"1\" | \"x\"\ntwo ::= \"2\"\n";
        assert_eq!(first_expansion(g).unwrap(), "1");
    }

    #[cfg(feature = "stub")]
    #[test]
    fn the_stub_answers_the_same_on_every_call() {
        let mut s = stub::Stub::open(HOUSE).unwrap();
        let a = s.decide("anything").unwrap();
        let b = s.decide("else").unwrap();
        assert_eq!(a, b);
        assert!(a.stub);
        assert_eq!(a.prompt_ms, 0.0);
        assert_eq!(s.engine_version(), "stub");
    }
}
