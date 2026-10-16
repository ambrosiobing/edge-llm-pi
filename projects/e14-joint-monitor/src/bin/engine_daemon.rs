// engine_daemon.rs: the second process of project e14. It owns the model and nothing
// else: no bus, no sensor, no rule, no log. It listens on a Unix domain socket, serves
// one connection at a time and answers one request at a time, in order.
//
// That it is single threaded is the design, not a shortcut. Project e13 runs the same
// model in the same way inside the service's own process; if this daemon answered
// several requests at once it would differ from e13 in two ways instead of one, and the
// pair would no longer price the process boundary alone. A second client is accepted
// only after the first has closed, and the chapter says so.
//
// Protocol, one line each way, tab separated (see src/engine_client.rs):
//   decide<TAB>user text      ->  OK<TAB>answer<TAB>prompt_ms<TAB>generate_ms<TAB>
//                                 n_generated<TAB>engine<TAB>rss_kb
//   rss                       ->  the same reply shape with an empty answer
//   anything else             ->  ERR<TAB>reason
//
// Two builds. Without the "engine" feature the answer comes from the stub, which
// expands the grammar's first alternative exactly as common/c/src/edgellm_stub.c does,
// so the oracle file of a Rust project and of a C project mean the same thing and CI
// needs no model file and no llama.cpp build. With --features engine the answer comes
// from llama.cpp through the llama-cpp-2 crate, the system prompt is decoded once at
// start and kept resident, and each decision pays for its own user text only.

use std::fs;
use std::io::{BufRead, BufReader, Write};
use std::os::unix::net::{UnixListener, UnixStream};
use std::path::{Path, PathBuf};
use std::process;

struct Options {
    socket: PathBuf,
    grammar: PathBuf,
    prompt: PathBuf,
    model: Option<PathBuf>,
    n_predict: i32,
    n_ctx: u32,
    threads: i32,
}

fn usage() -> ! {
    eprintln!(
        "edgellm-e14-engine-daemon --socket <path> --grammar <gbnf> --prompt <txt>\n\
         \u{20}   [--model <gguf>] [--n-predict 64] [--n-ctx 512] [--threads 4]\n\
         Without the engine feature the stub answers from the grammar and --model is\n\
         ignored. The socket path is removed and rebound at start."
    );
    process::exit(2);
}

fn parse_args() -> Options {
    let mut o = Options {
        socket: PathBuf::from("/run/edgellm/e14-engine.sock"),
        grammar: PathBuf::new(),
        prompt: PathBuf::new(),
        model: None,
        n_predict: 64,
        n_ctx: 512,
        threads: 4,
    };
    let mut args = std::env::args().skip(1);
    while let Some(a) = args.next() {
        let mut value = || args.next().unwrap_or_else(|| usage());
        match a.as_str() {
            "--socket" => o.socket = PathBuf::from(value()),
            "--grammar" => o.grammar = PathBuf::from(value()),
            "--prompt" => o.prompt = PathBuf::from(value()),
            "--model" => o.model = Some(PathBuf::from(value())),
            "--n-predict" => o.n_predict = value().parse().unwrap_or(64),
            "--n-ctx" => o.n_ctx = value().parse().unwrap_or(512),
            "--threads" => o.threads = value().parse().unwrap_or(4),
            "-h" | "--help" => usage(),
            other => {
                eprintln!("unknown option {}", other);
                usage();
            }
        }
    }
    if o.grammar.as_os_str().is_empty() || o.prompt.as_os_str().is_empty() {
        usage();
    }
    o
}

/// Resident set size of this process in kB, from /proc/self/status, or -1 where that
/// file does not exist. It is the figure the chapter compares against e13's one
/// process, and the service logs it beside its own.
fn rss_kb() -> i64 {
    let text = match fs::read_to_string("/proc/self/status") {
        Ok(t) => t,
        Err(_) => return -1,
    };
    for line in text.lines() {
        if let Some(rest) = line.strip_prefix("VmRSS:") {
            for field in rest.split_whitespace() {
                if let Ok(n) = field.parse::<i64>() {
                    return n;
                }
            }
        }
    }
    -1
}

// ------------------------------------------------------------------ the stub engine
//
// The algorithm is the one documented in common/c/src/edgellm_stub.c, so that the two
// stubs answer the same bytes for the same grammar: start at the rule named root, walk
// the FIRST alternative left to right, append the text of every quoted literal, expand
// a reference to another rule to that rule's first alternative (depth 8), expand a
// parenthesised group to its own first alternative, and append nothing at all for a
// character class or a repetition mark. For grammars/e14_inspect.gbnf that gives
// {"decision":"OK","summary":""}.

fn rule_body<'a>(grammar: &'a str, name: &str) -> Option<&'a str> {
    let mut body: Option<usize> = None;
    let mut end = grammar.len();
    let mut offset = 0usize;
    for line in grammar.split_inclusive('\n') {
        let trimmed = line.trim_start();
        let lead = line.len() - trimmed.len();
        if let Some(pos) = trimmed.find("::=") {
            let ident = trimmed[..pos].trim();
            if body.is_some() {
                end = offset;
                break;
            }
            if ident == name {
                body = Some(offset + lead + pos + 3);
            }
        }
        offset += line.len();
    }
    body.map(|b| &grammar[b..end.max(b)])
}

/// The text of the first alternative of a rule body: everything up to the first '|'
/// that is not inside a quoted literal, a character class or a group.
fn first_alternative(body: &str) -> &str {
    let bytes = body.as_bytes();
    let (mut quoted, mut class, mut depth) = (false, false, 0i32);
    let mut i = 0usize;
    while i < bytes.len() {
        let c = bytes[i] as char;
        match c {
            '\\' if quoted => i += 1,
            '"' => quoted = !quoted,
            '[' if !quoted => class = true,
            ']' if !quoted => class = false,
            '(' if !quoted && !class => depth += 1,
            ')' if !quoted && !class => depth -= 1,
            '#' if !quoted && !class => {
                /* a comment runs to the end of its line and is not an alternative */
                while i < bytes.len() && bytes[i] != b'\n' {
                    i += 1;
                }
            }
            '|' if !quoted && !class && depth == 0 => return &body[..i],
            _ => {}
        }
        i += 1;
    }
    body
}

fn expand(grammar: &str, body: &str, out: &mut String, depth: u32) {
    if depth == 0 {
        return;
    }
    let text = first_alternative(body);
    let bytes = text.as_bytes();
    let mut i = 0usize;
    while i < bytes.len() {
        match bytes[i] {
            b'#' => {
                while i < bytes.len() && bytes[i] != b'\n' {
                    i += 1;
                }
            }
            b'"' => {
                i += 1;
                while i < bytes.len() && bytes[i] != b'"' {
                    if bytes[i] == b'\\' && i + 1 < bytes.len() {
                        out.push(match bytes[i + 1] {
                            b'n' => '\n',
                            b't' => '\t',
                            b'r' => '\r',
                            other => other as char,
                        });
                        i += 2;
                    } else {
                        out.push(bytes[i] as char);
                        i += 1;
                    }
                }
                i += 1;
            }
            b'[' => {
                /* a character class contributes nothing, like every repetition mark */
                while i < bytes.len() && bytes[i] != b']' {
                    i += 1;
                }
                i += 1;
            }
            b'(' => {
                let start = i + 1;
                let mut depth_here = 1i32;
                i += 1;
                while i < bytes.len() && depth_here > 0 {
                    match bytes[i] {
                        b'(' => depth_here += 1,
                        b')' => depth_here -= 1,
                        _ => {}
                    }
                    i += 1;
                }
                let inner = &text[start..(i - 1).max(start)];
                expand(grammar, inner, out, depth - 1);
            }
            c if c.is_ascii_alphanumeric() || c == b'_' || c == b'-' => {
                let start = i;
                while i < bytes.len()
                    && (bytes[i].is_ascii_alphanumeric() || bytes[i] == b'_' || bytes[i] == b'-')
                {
                    i += 1;
                }
                let name = &text[start..i];
                if let Some(inner) = rule_body(grammar, name) {
                    expand(grammar, inner, out, depth - 1);
                }
            }
            _ => i += 1,
        }
    }
}

fn stub_answer(grammar: &str) -> String {
    let mut out = String::new();
    if let Some(body) = rule_body(grammar, "root") {
        expand(grammar, body, &mut out, 8);
    }
    out
}

// ------------------------------------------------------------------ the engine
//
// The engine half is behind the "engine" feature so that CI, which has no model file
// and no reason to compile llama.cpp, builds and tests the boundary in seconds. The
// exact symbol spellings of llama-cpp-2 0.1.159 are to verify at the first compile,
// which is skyhorizon's WSL and never the authoring laptop.

#[cfg(feature = "engine")]
mod engine {
    use llama_cpp_2::context::params::LlamaContextParams;
    use llama_cpp_2::llama_backend::LlamaBackend;
    use llama_cpp_2::llama_batch::LlamaBatch;
    use llama_cpp_2::model::params::LlamaModelParams;
    use llama_cpp_2::model::{AddBos, LlamaModel, Special};
    use llama_cpp_2::sampling::LlamaSampler;
    use std::num::NonZeroU32;
    use std::path::Path;

    pub struct Engine {
        backend: LlamaBackend,
        model: LlamaModel,
        grammar: String,
        system: String,
        n_predict: i32,
        n_ctx: u32,
        threads: i32,
    }

    pub struct Answer {
        pub text: String,
        pub prompt_ms: f64,
        pub generate_ms: f64,
        pub n_generated: u32,
    }

    impl Engine {
        pub fn open(
            model_path: &Path,
            grammar: &str,
            system: &str,
            n_predict: i32,
            n_ctx: u32,
            threads: i32,
        ) -> Result<Self, String> {
            let backend = LlamaBackend::init().map_err(|e| format!("backend: {}", e))?;
            let params = LlamaModelParams::default();
            let model = LlamaModel::load_from_file(&backend, model_path, &params)
                .map_err(|e| format!("load {}: {}", model_path.display(), e))?;
            Ok(Engine {
                backend,
                model,
                grammar: grammar.to_string(),
                system: system.to_string(),
                n_predict,
                n_ctx,
                threads,
            })
        }

        /// One decision. The prompt is the ChatML form both SmolLM2 and Qwen2.5 expect;
        /// prompt.txt is plain text and carries no markers, as every project of this
        /// volume keeps it.
        pub fn decide(&mut self, user: &str) -> Result<Answer, String> {
            let ctx_params = LlamaContextParams::default()
                .with_n_ctx(NonZeroU32::new(self.n_ctx))
                .with_n_threads(self.threads);
            let mut ctx = self
                .model
                .new_context(&self.backend, ctx_params)
                .map_err(|e| format!("context: {}", e))?;
            let prompt = format!(
                "<|im_start|>system\n{}<|im_end|>\n<|im_start|>user\n{}<|im_end|>\n\
                 <|im_start|>assistant\n",
                self.system.trim(),
                user
            );
            let tokens = self
                .model
                .str_to_token(&prompt, AddBos::Always)
                .map_err(|e| format!("tokenize: {}", e))?;
            let mut batch = LlamaBatch::new(tokens.len().max(16), 1);
            batch
                .add_sequence(&tokens, 0, false)
                .map_err(|e| format!("batch: {}", e))?;
            let t0 = std::time::Instant::now();
            ctx.decode(&mut batch).map_err(|e| format!("decode: {}", e))?;
            let prompt_ms = t0.elapsed().as_secs_f64() * 1000.0;

            let mut sampler = LlamaSampler::chain_simple([
                LlamaSampler::grammar(&self.model, &self.grammar, "root"),
                LlamaSampler::greedy(),
            ]);
            let mut out = String::new();
            let mut n_generated = 0u32;
            let mut n_cur = tokens.len() as i32;
            let t1 = std::time::Instant::now();
            while n_generated < self.n_predict as u32 {
                let token = sampler.sample(&ctx, -1);
                sampler.accept(token);
                if self.model.is_eog_token(token) {
                    break;
                }
                out.push_str(
                    &self
                        .model
                        .token_to_str(token, Special::Tokenize)
                        .map_err(|e| format!("detokenize: {}", e))?,
                );
                n_generated += 1;
                batch.clear();
                batch
                    .add(token, n_cur, &[0], true)
                    .map_err(|e| format!("batch add: {}", e))?;
                ctx.decode(&mut batch).map_err(|e| format!("decode: {}", e))?;
                n_cur += 1;
            }
            Ok(Answer {
                text: out,
                prompt_ms,
                generate_ms: t1.elapsed().as_secs_f64() * 1000.0,
                n_generated,
            })
        }

        pub fn version(&self) -> String {
            format!("llama-cpp-2 0.1.159, {}", self.model.n_params())
        }
    }
}

// ------------------------------------------------------------------ the server

enum Answerer {
    Stub { text: String },
    #[cfg(feature = "engine")]
    Real(engine::Engine),
}

impl Answerer {
    fn version(&self) -> String {
        match self {
            Answerer::Stub { .. } => "stub".to_string(),
            #[cfg(feature = "engine")]
            Answerer::Real(e) => e.version(),
        }
    }

    fn answer(&mut self, user: &str) -> (String, f64, f64, u32) {
        match self {
            Answerer::Stub { text } => (text.clone(), 0.0, 0.0, 0),
            #[cfg(feature = "engine")]
            Answerer::Real(e) => match e.decide(user) {
                Ok(a) => (a.text, a.prompt_ms, a.generate_ms, a.n_generated),
                Err(reason) => (format!("ERROR {}", reason), 0.0, 0.0, 0),
            },
        }
    }
}

fn serve(stream: UnixStream, answerer: &mut Answerer, version: &str) {
    let mut writer = match stream.try_clone() {
        Ok(w) => w,
        Err(e) => {
            eprintln!("e14 daemon: clone the socket: {}", e);
            return;
        }
    };
    let reader = BufReader::new(stream);
    for line in reader.lines() {
        let line = match line {
            Ok(l) => l,
            Err(e) => {
                eprintln!("e14 daemon: read: {}", e);
                return;
            }
        };
        let mut fields = line.splitn(2, '\t');
        let op = fields.next().unwrap_or("");
        let user = fields.next().unwrap_or("");
        let reply = match op {
            "decide" => {
                /* the service times the round trip; the two figures below are the work
                 * inside this process, and their difference is the cost of the socket */
                let (text, prompt_ms, generate_ms, n) = answerer.answer(user);
                format!(
                    "OK\t{}\t{:.3}\t{:.3}\t{}\t{}\t{}",
                    text.replace('\t', " ").replace('\n', " "),
                    prompt_ms,
                    generate_ms,
                    n,
                    version,
                    rss_kb()
                )
            }
            "rss" => format!("OK\t\t0.000\t0.000\t0\t{}\t{}", version, rss_kb()),
            other => format!("ERR\tunknown request {}", other),
        };
        if writeln!(writer, "{}", reply).is_err() || writer.flush().is_err() {
            return; /* the service went away; the next accept() picks up its successor */
        }
    }
}

fn main() {
    let o = parse_args();
    let grammar = match fs::read_to_string(&o.grammar) {
        Ok(g) => g,
        Err(e) => {
            eprintln!("e14 daemon: {}: {}", o.grammar.display(), e);
            process::exit(1);
        }
    };
    let system = match fs::read_to_string(&o.prompt) {
        Ok(p) => p,
        Err(e) => {
            eprintln!("e14 daemon: {}: {}", o.prompt.display(), e);
            process::exit(1);
        }
    };

    #[cfg(feature = "engine")]
    let mut answerer = match o.model.as_deref() {
        Some(path) => match engine::Engine::open(
            path,
            &grammar,
            &system,
            o.n_predict,
            o.n_ctx,
            o.threads,
        ) {
            Ok(e) => Answerer::Real(e),
            Err(reason) => {
                eprintln!("e14 daemon: {}", reason);
                process::exit(1);
            }
        },
        None => Answerer::Stub { text: stub_answer(&grammar) },
    };
    #[cfg(not(feature = "engine"))]
    let mut answerer = {
        let _ = (&o.model, &system, o.n_predict, o.n_ctx, o.threads);
        Answerer::Stub { text: stub_answer(&grammar) }
    };

    let version = answerer.version();
    bind_and_listen(&o.socket, &mut answerer, &version);
}

fn bind_and_listen(socket: &Path, answerer: &mut Answerer, version: &str) {
    if socket.exists() {
        /* a socket file left behind by a process that was stopped with signal 9 is not
         * a running daemon: bind() would refuse it, so it is removed first */
        if let Err(e) = fs::remove_file(socket) {
            eprintln!("e14 daemon: remove the stale socket {}: {}", socket.display(), e);
        }
    }
    let listener = match UnixListener::bind(socket) {
        Ok(l) => l,
        Err(e) => {
            eprintln!("e14 daemon: bind {}: {}", socket.display(), e);
            process::exit(1);
        }
    };
    eprintln!(
        "e14 daemon: listening on {}, engine {}, one request at a time",
        socket.display(),
        version
    );
    for stream in listener.incoming() {
        match stream {
            Ok(s) => serve(s, answerer, version),
            Err(e) => eprintln!("e14 daemon: accept: {}", e),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const GRAMMAR: &str = concat!(
        "# a comment line\n",
        "root     ::= \"{\" ws \"\\\"decision\\\"\" ws \":\" ws decision ws \",\" ws",
        " \"\\\"summary\\\"\" ws \":\" ws summary ws \"}\"\n",
        "decision ::= \"\\\"OK\\\"\" | \"\\\"INSPECT\\\"\"\n",
        "summary  ::= \"\\\"\" [A-Za-z0-9 ,.]{9,159} \".\" \"\\\"\"\n",
        "ws       ::= [ \\t]*\n"
    );

    #[test]
    fn the_stub_answers_the_first_literal_on_every_path() {
        assert_eq!(stub_answer(GRAMMAR), "{\"decision\":\"OK\",\"summary\":\".\"}");
    }

    #[test]
    fn a_missing_root_answers_nothing() {
        assert_eq!(stub_answer("decision ::= \"\\\"OK\\\"\"\n"), "");
    }

    #[test]
    fn rss_is_a_number_or_minus_one() {
        let n = rss_kb();
        assert!(n == -1 || n > 0);
    }
}
