//! The binding to the C ABI of `../c/abi.h`, a safe wrapper over it, and the two
//! other ways the service can reach the same four functions.
//!
//! Three backends, one answer. The engine, the grammar and the sampler live in C in
//! every case; what differs is the boundary.
//!
//! * `Backend::Ffi` calls `libedgellm_abi.so` in this process. Fastest, and the
//!   process dies with the engine. Behind the cargo feature `ffi`, because building
//!   it needs the C half built first and a link path given to the linker, and CI has
//!   neither.
//! * `Backend::Socket` speaks the one line protocol of `../c/daemon.c` to the same
//!   library in another process. One more copy and one more context switch per
//!   decision, and the service survives the engine being stopped, upgraded or
//!   replaced. This is the backend the units install.
//! * `Backend::Stub` answers from the grammar alone, with no C at all, so the whole
//!   service runs on a host with no engine and the oracle test is on the CI path.
//!
//! The stub is the one piece of this project deliberately written twice, here and in
//! `common/c/src/edgellm_stub.c`. The C half of e18 is configured on its own and is
//! not built by `cargo test`, so a Rust service with no Rust stub would have no way to
//! run its own oracle file in CI. The two must agree byte for byte, and the oracle
//! file `tests/oracle/e18.expected.csv` is what holds them to it: it was written from
//! the documented C expansion by `tools/make_replay.py`, and the Rust stub is checked
//! against it on every run of `cargo test`.

use std::fmt;
use std::io::{BufRead, BufReader, Write};
use std::os::unix::net::UnixStream;
use std::path::Path;
#[cfg(feature = "ffi")]
use std::sync::atomic::{AtomicBool, Ordering};

/// The version this service was compiled against, which must equal the value
/// `EDGELLM_ABI_VERSION` carries in `../c/abi.h`. Kept here as a plain integer rather
/// than generated from the header, because a generator would hide exactly the
/// mismatch the number exists to find.
pub const EXPECTED_ABI_VERSION: u32 = 1000;

/// major * 1000 + minor: a different major is refused, a different minor is reported
/// and accepted, which is what the header's own rule says those two mean.
pub const fn abi_major(version: u32) -> u32 {
    version / 1000
}

// Engine and TooLong are constructed only by the ffi backend. allow rather than cfg: the
// ffi feature is compiled by no job, so the default build is this enum's only compile.
#[cfg_attr(not(feature = "ffi"), allow(dead_code))]
#[derive(Debug)]
pub enum EngineError {
    /// The library or the daemon reports a major version this service was not built
    /// against. This is the failure the version field exists to catch.
    Version { want: u32, got: u32 },
    /// No engine is open, or one is already open.
    State(&'static str),
    /// The engine refused the decision.
    Engine(i32),
    /// The answer did not fit the buffer, so nothing was written: a truncated JSON
    /// object parses as nothing and would be logged as a missing literal.
    TooLong,
    /// The socket or a file did not cooperate.
    Io(std::io::Error),
    /// The grammar could not be expanded by the stub's bounded rule.
    Grammar(String),
}

impl fmt::Display for EngineError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            EngineError::Version { want, got } => write!(
                f, "abi version {}, this service was built against {}; refusing to run",
                got, want),
            EngineError::State(s) => write!(f, "{}", s),
            EngineError::Engine(rc) => write!(f, "the engine returned {}", rc),
            EngineError::TooLong => write!(f, "the answer did not fit the buffer"),
            EngineError::Io(e) => write!(f, "{}", e),
            EngineError::Grammar(s) => write!(f, "grammar: {}", s),
        }
    }
}

impl From<std::io::Error> for EngineError {
    fn from(e: std::io::Error) -> Self {
        EngineError::Io(e)
    }
}

/// How long an answer may be. The grammar's longest object is well inside this; the
/// buffer is generous so that a refusal here means the grammar changed, not that the
/// buffer was mean.
#[cfg_attr(not(feature = "ffi"), allow(dead_code))] // read by the ffi backend only
pub const ANSWER_MAX: usize = 1024;

// ---------------------------------------------------------------- the FFI binding

#[cfg(feature = "ffi")]
mod sys {
    use std::os::raw::{c_char, c_float, c_int, c_uint};

    // The four declarations of ../c/abi.h and nothing else. No structure, no enum and
    // no typedef crosses this boundary, which is the discipline the header states, so
    // this block has nothing in it whose layout could drift.
    #[link(name = "edgellm_abi")]
    extern "C" {
        pub fn edgellm_abi_version() -> c_uint;
        pub fn edgellm_abi_open(model_path: *const c_char, grammar_path: *const c_char,
                                prompt_path: *const c_char, state_path: *const c_char,
                                n_ctx: c_int, n_threads: c_int, n_predict: c_int,
                                temperature: c_float) -> c_int;
        pub fn edgellm_abi_decide(user: *const c_char, out: *mut c_char,
                                  out_len: usize) -> c_int;
        pub fn edgellm_abi_close();
    }
}

/// True while an FFI engine is open. The ABI takes no handle on purpose, so one
/// engine per process is the contract; this flag is how the safe wrapper turns that
/// contract into something the type system enforces, by refusing to hand out a second
/// value whose Drop would close the first one's engine.
#[cfg(feature = "ffi")]
static FFI_OPEN: AtomicBool = AtomicBool::new(false);

/// The safe wrapper. Holding one of these means an engine is open; dropping it closes
/// the engine exactly once.
#[cfg(feature = "ffi")]
pub struct Ffi {
    _private: (),
}

#[cfg(feature = "ffi")]
impl Ffi {
    /// Checks the library's version, then opens the engine. Every path through here
    /// leaves either an engine open and a value returned, or nothing open.
    pub fn open(cfg: &EngineConfig) -> Result<Ffi, EngineError> {
        use std::ffi::CString;
        let got = unsafe { sys::edgellm_abi_version() } as u32;
        if abi_major(got) != abi_major(EXPECTED_ABI_VERSION) {
            return Err(EngineError::Version { want: EXPECTED_ABI_VERSION, got });
        }
        if got != EXPECTED_ABI_VERSION {
            eprintln!("e18: abi minor version {} against {}, continuing", got,
                      EXPECTED_ABI_VERSION);
        }
        if FFI_OPEN.swap(true, Ordering::SeqCst) {
            return Err(EngineError::State("an ffi engine is already open"));
        }
        let model = CString::new(cfg.model.clone().unwrap_or_default())
            .map_err(|_| EngineError::State("a NUL in the model path"))?;
        let grammar = CString::new(cfg.grammar.clone())
            .map_err(|_| EngineError::State("a NUL in the grammar path"))?;
        let prompt = CString::new(cfg.prompt.clone())
            .map_err(|_| EngineError::State("a NUL in the prompt path"))?;
        let state = cfg.state.clone().map(|s| CString::new(s).unwrap_or_default());
        let rc = unsafe {
            sys::edgellm_abi_open(model.as_ptr(), grammar.as_ptr(), prompt.as_ptr(),
                                  state.as_ref().map_or(std::ptr::null(), |s| s.as_ptr()),
                                  cfg.n_ctx, cfg.n_threads, cfg.n_predict,
                                  cfg.temperature)
        };
        if rc != 0 {
            FFI_OPEN.store(false, Ordering::SeqCst);
            return Err(EngineError::Engine(rc));
        }
        Ok(Ffi { _private: () })
    }

    pub fn decide(&self, user: &str) -> Result<String, EngineError> {
        use std::ffi::CString;
        let text = CString::new(user).map_err(|_| EngineError::State("a NUL in the prompt"))?;
        let mut out = vec![0u8; ANSWER_MAX];
        let rc = unsafe {
            sys::edgellm_abi_decide(text.as_ptr(), out.as_mut_ptr() as *mut std::os::raw::c_char,
                                    out.len())
        };
        match rc {
            n if n >= 0 => {
                out.truncate(n as usize);
                Ok(String::from_utf8_lossy(&out).into_owned())
            }
            -3 => Err(EngineError::TooLong),
            other => Err(EngineError::Engine(other)),
        }
    }
}

#[cfg(feature = "ffi")]
impl Drop for Ffi {
    fn drop(&mut self) {
        unsafe { sys::edgellm_abi_close() };
        FFI_OPEN.store(false, Ordering::SeqCst);
    }
}

// ---------------------------------------------------------------- the socket client

/// The one line client of ../c/daemon.c. The daemon's version is asked first, over
/// the same connection, so an upgraded daemon under a running service is refused on
/// the first decision rather than producing answers from an engine the service was
/// never tested against.
pub struct Socket {
    writer: UnixStream,
    reader: BufReader<UnixStream>,
}

impl Socket {
    pub fn connect(path: &Path) -> Result<Socket, EngineError> {
        let stream = UnixStream::connect(path)?;
        let reader = BufReader::new(stream.try_clone()?);
        let mut s = Socket { writer: stream, reader };
        let got = s.version()?;
        if abi_major(got) != abi_major(EXPECTED_ABI_VERSION) {
            return Err(EngineError::Version { want: EXPECTED_ABI_VERSION, got });
        }
        if got != EXPECTED_ABI_VERSION {
            eprintln!("e18: daemon abi minor version {} against {}, continuing", got,
                      EXPECTED_ABI_VERSION);
        }
        Ok(s)
    }

    fn line(&mut self, request: &str) -> Result<String, EngineError> {
        self.writer.write_all(request.as_bytes())?;
        self.writer.write_all(b"\n")?;
        self.writer.flush()?;
        let mut reply = String::new();
        if self.reader.read_line(&mut reply)? == 0 {
            return Err(EngineError::State("the daemon closed the connection"));
        }
        Ok(reply.trim_end().to_string())
    }

    pub fn version(&mut self) -> Result<u32, EngineError> {
        let reply = self.line("V")?;
        match reply.strip_prefix("V ") {
            Some(n) => n.trim().parse::<u32>()
                .map_err(|_| EngineError::State("the daemon's version is not a number")),
            None => Err(EngineError::State("the daemon did not answer V")),
        }
    }

    pub fn decide(&mut self, user: &str) -> Result<String, EngineError> {
        // The user text is one line by construction, so the protocol needs no framing
        // beyond the newline; a newline that did get in would split one request into
        // two and is replaced here rather than trusted.
        let flat: String = user.chars().map(|c| if c == '\n' || c == '\r' { ' ' } else { c })
            .collect();
        let reply = self.line(&format!("D {}", flat))?;
        if let Some(answer) = reply.strip_prefix("A ") {
            Ok(answer.to_string())
        } else if let Some(reason) = reply.strip_prefix("E ") {
            Err(EngineError::Grammar(reason.to_string()))
        } else {
            Err(EngineError::State("the daemon answered neither A nor E"))
        }
    }
}

// ---------------------------------------------------------------- the stub

/// The grammar's first expansion, computed once at open, exactly as
/// common/c/src/edgellm_stub.c documents it: start at the rule named root, walk its
/// FIRST alternative left to right; a quoted literal appends its text with the
/// escapes resolved; a reference to another rule expands that rule's first
/// alternative in place, to a depth of eight; a parenthesised group expands its own
/// first alternative; a character class and every repetition mark append nothing;
/// text after a '#' is a comment.
///
/// For grammars/e18_drift.gbnf that is {"decision":"NOMINAL","summary":"."}, which is
/// what tests/oracle/e18.expected.csv holds in its model column on every row.
pub struct Stub {
    answer: String,
}

impl Stub {
    pub fn open(grammar_path: &Path) -> Result<Stub, EngineError> {
        let text = std::fs::read_to_string(grammar_path)?;
        let answer = expand(&text, "root", 8)?;
        Ok(Stub { answer })
    }

    pub fn decide(&self, _user: &str) -> Result<String, EngineError> {
        Ok(self.answer.clone())
    }
}

/// The head of a rule line, `name` in `name ::= body`, or None when the line is a
/// comment, a blank line or a continuation.
fn rule_head(line: &str) -> Option<(&str, usize)> {
    let at = line.find("::=")?;
    let head = line[..at].trim();
    if head.is_empty() || !head.chars().all(|c| c.is_alphanumeric() || c == '_' || c == '-') {
        return None;
    }
    Some((head, at + 3))
}

/// Finds `name ::=` at the start of a line and returns the rule's body, which runs to
/// the start of the next rule line or the end of the text, so a rule may be continued
/// on the lines below it.
fn rule_body<'a>(grammar: &'a str, name: &str) -> Option<&'a str> {
    let mut begin: Option<usize> = None;
    let mut at = 0usize;
    for raw in grammar.split_inclusive('\n') {
        let line = raw.trim_end_matches(['\n', '\r']);
        if let Some((head, after)) = rule_head(line) {
            if let Some(start) = begin {
                return Some(&grammar[start..at]);
            }
            if head == name {
                begin = Some(at + after);
            }
        }
        at += raw.len();
    }
    begin.map(|start| &grammar[start..])
}

/// Expands the first alternative of one rule. Returns an error rather than a wrong
/// answer when the grammar is not of the house shape, because a stub that quietly
/// answers the empty string would be read as a model that said nothing.
fn expand(grammar: &str, name: &str, depth: u32) -> Result<String, EngineError> {
    if depth == 0 {
        return Err(EngineError::Grammar(format!("{} nests deeper than eight rules", name)));
    }
    let body = rule_body(grammar, name)
        .ok_or_else(|| EngineError::Grammar(format!("no rule named {}", name)))?;
    let mut out = String::new();
    let mut chars = body.chars().peekable();
    let mut alternatives_done = false;
    while let Some(c) = chars.next() {
        if alternatives_done {
            break;
        }
        match c {
            '#' => {
                while let Some(&n) = chars.peek() {
                    chars.next();
                    if n == '\n' {
                        break;
                    }
                }
            }
            '|' => alternatives_done = true,
            '"' => {
                while let Some(q) = chars.next() {
                    match q {
                        '"' => break,
                        '\\' => match chars.next() {
                            Some('n') => out.push('\n'),
                            Some('t') => out.push('\t'),
                            Some('r') => out.push('\r'),
                            Some(other) => out.push(other),
                            None => break,
                        },
                        other => out.push(other),
                    }
                }
            }
            '[' => {
                // A character class appends nothing, like the C stub.
                for q in chars.by_ref() {
                    if q == ']' {
                        break;
                    }
                }
            }
            '{' => {
                // A repetition mark appends nothing either.
                for q in chars.by_ref() {
                    if q == '}' {
                        break;
                    }
                }
            }
            c if c.is_alphanumeric() || c == '_' || c == '-' => {
                let mut ident = String::new();
                ident.push(c);
                while let Some(&n) = chars.peek() {
                    if n.is_alphanumeric() || n == '_' || n == '-' {
                        ident.push(n);
                        chars.next();
                    } else {
                        break;
                    }
                }
                out.push_str(&expand(grammar, &ident, depth - 1)?);
            }
            _ => {}
        }
    }
    Ok(out)
}

// ---------------------------------------------------------------- the three together

/// What an engine needs to open, whichever boundary it sits behind.
#[derive(Debug, Clone)]
pub struct EngineConfig {
    pub model: Option<String>,
    pub grammar: String,
    pub prompt: String,
    pub state: Option<String>,
    pub n_ctx: i32,
    pub n_threads: i32,
    pub n_predict: i32,
    pub temperature: f32,
}

/// The engine behind one of the three boundaries. The service holds one of these and
/// does not otherwise know which it has, so the three latency figures the chapter
/// publishes differ in the boundary and in nothing else.
pub enum Backend {
    Stub(Stub),
    Socket(Socket),
    #[cfg(feature = "ffi")]
    Ffi(Ffi),
}

impl Backend {
    pub fn name(&self) -> &'static str {
        match self {
            Backend::Stub(_) => "stub",
            Backend::Socket(_) => "socket",
            #[cfg(feature = "ffi")]
            Backend::Ffi(_) => "ffi",
        }
    }

    pub fn decide(&mut self, user: &str) -> Result<String, EngineError> {
        match self {
            Backend::Stub(s) => s.decide(user),
            Backend::Socket(s) => s.decide(user),
            #[cfg(feature = "ffi")]
            Backend::Ffi(s) => s.decide(user),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path::PathBuf;

    fn grammar_path() -> PathBuf {
        PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("..").join("grammar.gbnf")
    }

    #[test]
    fn the_stub_answers_the_first_literal_on_every_path() {
        let stub = Stub::open(&grammar_path()).expect("the project's own grammar");
        assert_eq!(stub.decide("anything").unwrap(),
                   "{\"decision\":\"NOMINAL\",\"summary\":\".\"}");
    }

    #[test]
    fn the_stub_refuses_a_grammar_with_no_root() {
        let text = "start ::= \"x\"\n";
        match expand(text, "root", 8) {
            Err(EngineError::Grammar(_)) => {}
            other => panic!("expected a grammar error, got {:?}", other),
        }
    }

    #[test]
    fn a_major_version_difference_is_refused_and_a_minor_one_is_not() {
        assert_ne!(abi_major(2000), abi_major(EXPECTED_ABI_VERSION));
        assert_eq!(abi_major(1007), abi_major(EXPECTED_ABI_VERSION));
    }
}
