//! edgellm: the decision loop of the volume, in Rust, in the shape of common/c.
//!
//! One process, one model, one grammar, one system prompt decoded once, one rule, one log.
//! The pieces mirror the C headers one for one so a chapter reads the same against either
//! tree:
//!
//!   common/c/include/edgellm.h    -> Decider, EngineConfig, Decision, engine.rs
//!   common/c/include/io.h         -> io::IoSource, io::Replay, io::Iio, io::open_live
//!   common/c/include/rules.h      -> rules::Rule, rules::RuleCtx, rules::Registry
//!   common/c/include/elog.h       -> elog::Elog, the same nine columns
//!   common/c/src/edgellm_main.c   -> parse_args, Project, run_loop, template.rs
//!
//! Two cargo features choose the engine. "engine" links llama.cpp through the llama-cpp-2
//! binding (0.1.159, pinned in Cargo.toml) into the one executable, which is what catalogue
//! variant R7 means. "stub", the default and what CI builds, answers from the grammar alone
//! exactly as common/c/src/edgellm_stub.c does: the first literal on every path from root. A
//! log written by the stub says "stub" in its engine column and can never be read as a bench
//! log.
//!
//! The safety rule of the volume is visible in the types: the rule's index reaches
//! Project::actuate and the log; the model's text reaches the log and nothing else.
//!
//! Written for e05 on Wednesday 14 October 2026; e13, e14 and e18's service reuse it.

pub mod elog;
pub mod engine;
pub mod io;
pub mod rules;
pub mod template;

use std::fmt;
use std::fs;
use std::path::Path;
use std::time::Duration;

use io::{Event, IoSource};
use rules::{Rule, RuleCtx};
use template::Template;

/// One line of reason, the way the C core writes it into err[].
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Error(pub String);

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.0)
    }
}

impl std::error::Error for Error {}

/// Shorthand for `Err(Error(msg))`.
pub fn err<T>(msg: impl Into<String>) -> Result<T, Error> {
    Err(Error(msg.into()))
}

/// What one decision returns: the grammar-constrained text and its timings, the same
/// fields as `edgellm_result` in edgellm.h.
#[derive(Debug, Clone, Default, PartialEq)]
pub struct Decision {
    pub text: String,
    pub n_prompt_tokens: usize,
    pub n_generated: usize,
    /// Wall clock around the user text decode, milliseconds.
    pub prompt_ms: f64,
    /// Wall clock around the generation loop, milliseconds.
    pub generate_ms: f64,
    /// True when the stub answered.
    pub stub: bool,
}

/// The engine configuration, the same fields as `edgellm_config` in edgellm.h.
#[derive(Debug, Clone)]
pub struct EngineConfig {
    /// GGUF file; ignored by the stub.
    pub model_path: String,
    /// GBNF file, root symbol "root".
    pub grammar_path: String,
    /// Already wrapped in the chat template; decoded once, state saved.
    pub system_prompt: String,
    /// Where the system-prompt state is saved; None keeps it resident and trims.
    pub state_path: Option<String>,
    pub n_ctx: usize,
    pub n_threads: i32,
    pub n_predict: usize,
    /// 0 means greedy; 0.3 for a sentence.
    pub temperature: f32,
    pub seed: u32,
}

impl Default for EngineConfig {
    /// n_ctx 512, 4 threads, n_predict 8, greedy, seed 1: edgellm_config_init().
    fn default() -> EngineConfig {
        EngineConfig {
            model_path: String::new(),
            grammar_path: String::new(),
            system_prompt: String::new(),
            state_path: None,
            n_ctx: 512,
            n_threads: 4,
            n_predict: 8,
            temperature: 0.0,
            seed: 1,
        }
    }
}

/// The one thing the loop asks of an engine: `edgellm_decide` and `edgellm_engine_version`.
pub trait Decider {
    /// Restore the system-prompt state, decode the user text, sample under the grammar
    /// until n_predict or an end-of-generation token.
    fn decide(&mut self, user_text: &str) -> Result<Decision, Error>;
    /// The engine's build identity, or "stub".
    fn engine_version(&self) -> String;
}

#[cfg(feature = "stub")]
fn open_stub(grammar: &str) -> Result<Box<dyn Decider>, Error> {
    Ok(Box::new(engine::stub::Stub::open(grammar)?))
}

#[cfg(not(feature = "stub"))]
fn open_stub(_grammar: &str) -> Result<Box<dyn Decider>, Error> {
    err("edgellm: built without the stub feature; --stub is refused")
}

#[cfg(feature = "engine")]
fn open_engine(cfg: &EngineConfig) -> Result<Box<dyn Decider>, Error> {
    Ok(Box::new(engine::llama::Engine::open(cfg)?))
}

#[cfg(not(feature = "engine"))]
fn open_engine(_cfg: &EngineConfig) -> Result<Box<dyn Decider>, Error> {
    err("edgellm: built without the engine feature; pass --stub, or build with --features engine")
}

/// Opens the stub when asked for, otherwise the real engine. Either refusal names the
/// feature that is missing, so a log can never claim an engine it did not run.
pub fn open_decider(cfg: &EngineConfig, want_stub: bool) -> Result<Box<dyn Decider>, Error> {
    if want_stub {
        let grammar = read_text_raw(&cfg.grammar_path)?;
        open_stub(&grammar)
    } else {
        open_engine(cfg)
    }
}

/// The identity of the engine this build carries, before any model is loaded: the binding
/// and its version with the engine feature, "stub" without it. The same string the log's
/// engine column carries.
pub fn engine_version() -> String {
    #[cfg(feature = "engine")]
    {
        engine::llama::BINDING_VERSION.to_string()
    }
    #[cfg(not(feature = "engine"))]
    {
        "stub".to_string()
    }
}

/// The C name of the configuration, for a project written against edgellm.h.
pub type Config = EngineConfig;

#[cfg(feature = "engine")]
pub use engine::llama::Engine;

/// Reads a whole file; the system prompt and the grammar come through here.
pub fn read_text_raw(path: impl AsRef<Path>) -> Result<String, Error> {
    let path = path.as_ref();
    fs::read_to_string(path).map_err(|e| Error(format!("cannot read {}: {}", path.display(), e)))
}

/// Reads a file and drops its trailing newlines, as the shared C main does for prompt.txt.
pub fn read_text(path: impl AsRef<Path>) -> Result<String, Error> {
    let text = read_text_raw(path)?;
    Ok(text.trim_end_matches(['\n', '\r']).to_string())
}

/// Resident set size of this process in kB from /proc/self/status, or -1.
pub fn rss_kb() -> i64 {
    let status = match fs::read_to_string("/proc/self/status") {
        Ok(s) => s,
        Err(_) => return -1,
    };
    for line in status.lines() {
        if let Some(rest) = line.strip_prefix("VmRSS:") {
            if let Some(num) = rest.split_whitespace().next() {
                if let Ok(kb) = num.parse::<i64>() {
                    return kb;
                }
            }
        }
    }
    -1
}

/// The command line of every Rust project, the same flags as the C main.
pub const USAGE: &str = "\
usage: edgellm-eNN --project eNN (--replay <csv> | --live <backend> <spec>)
          --grammar <gbnf> --prompt <txt> --log <csv> [--model <gguf>]
          [--n-predict N] [--threads N] [--n-ctx N] [--temperature T]
          [--interval S] [--state <path>] [--max-events N] [--stub]
          [--template chatml|zephyr|none]";

/// The parsed command line.
#[derive(Debug, Clone)]
pub struct RunConfig {
    pub project: String,
    pub replay: Option<String>,
    /// Backend name and its spec, for example ("iio", "/sys/bus/iio/devices/iio:device0:0.02").
    pub live: Option<(String, String)>,
    pub grammar: String,
    pub prompt: String,
    pub log: String,
    pub model: Option<String>,
    pub state: Option<String>,
    pub n_predict: usize,
    pub threads: i32,
    pub n_ctx: usize,
    pub temperature: f32,
    /// Seconds to sleep after each event; 0 lets the source set the pace.
    pub interval: f64,
    /// Stop after this many events; 0 runs to the end of the source.
    pub max_events: u64,
    pub stub: bool,
    pub template: Template,
}

/// Parses the command line. Err carries the one-line reason; the caller prints USAGE and
/// exits with 2, as the C main does. The two build-feature checks live here so that a
/// binary without the engine refuses a bench run before anything is opened.
pub fn parse_args(args: &[String]) -> Result<RunConfig, String> {
    let defaults = EngineConfig::default();
    let mut cfg = RunConfig {
        project: String::new(),
        replay: None,
        live: None,
        grammar: String::new(),
        prompt: String::new(),
        log: String::new(),
        model: None,
        state: None,
        n_predict: defaults.n_predict,
        threads: defaults.n_threads,
        n_ctx: defaults.n_ctx,
        temperature: defaults.temperature,
        interval: 0.0,
        max_events: 0,
        stub: false,
        template: Template::ChatMl,
    };
    let mut i = 1;
    while i < args.len() {
        let a = args[i].as_str();
        if a == "--stub" {
            cfg.stub = true;
            i += 1;
            continue;
        }
        let v = match args.get(i + 1) {
            Some(v) => v.clone(),
            None => return Err(format!("{} needs a value", a)),
        };
        match a {
            "--project" => cfg.project = v,
            "--replay" => cfg.replay = Some(v),
            "--live" => {
                let spec = match args.get(i + 2) {
                    Some(s) => s.clone(),
                    None => return Err("--live needs a backend and a spec".to_string()),
                };
                cfg.live = Some((v, spec));
                i += 1;
            }
            "--grammar" => cfg.grammar = v,
            "--prompt" => cfg.prompt = v,
            "--log" => cfg.log = v,
            "--model" => cfg.model = Some(v),
            "--state" => cfg.state = Some(v),
            "--n-predict" => cfg.n_predict = number(a, &v)?,
            "--threads" => cfg.threads = number(a, &v)?,
            "--n-ctx" => cfg.n_ctx = number(a, &v)?,
            "--temperature" => cfg.temperature = number(a, &v)?,
            "--interval" => cfg.interval = number(a, &v)?,
            "--max-events" => cfg.max_events = number(a, &v)?,
            "--template" => {
                cfg.template = match Template::parse(&v) {
                    Some(t) => t,
                    None => return Err(format!("unknown template {}", v)),
                }
            }
            _ => return Err(format!("unknown option {}", a)),
        }
        i += 2;
    }
    if cfg.project.is_empty() || cfg.grammar.is_empty() || cfg.prompt.is_empty()
        || cfg.log.is_empty()
    {
        return Err("--project, --grammar, --prompt and --log are required".to_string());
    }
    if cfg.replay.is_none() == cfg.live.is_none() {
        return Err("exactly one of --replay and --live is required".to_string());
    }
    if cfg.stub && !cfg!(feature = "stub") {
        return Err("built with the real engine only; enable the stub feature for --stub".into());
    }
    if !cfg.stub && !cfg!(feature = "engine") {
        return Err("built with the stub only; pass --stub, or build with --features engine"
            .into());
    }
    if !cfg.stub && cfg.model.is_none() {
        return Err("--model is required with the real engine".to_string());
    }
    Ok(cfg)
}

fn number<T: std::str::FromStr>(flag: &str, v: &str) -> Result<T, String> {
    v.parse::<T>().map_err(|_| format!("{} needs a number, not {}", flag, v))
}

/// What a project supplies to the loop: the two hooks of edgellm_main.h.
pub trait Project {
    /// The user text for this event, or None to log the rule alone without asking the
    /// model. The current event is not yet in `ctx` when this is called.
    fn format_prompt(&mut self, ev: &Event, ctx: &RuleCtx) -> Option<String>;

    /// Called after the rule, before the model. Drive an output through the source here,
    /// never from the model's answer. The default does nothing.
    fn actuate(
        &mut self,
        _src: &mut dyn IoSource,
        _rule: &dyn Rule,
        _index: usize,
        _ev: &Event,
    ) -> Result<(), Error> {
        Ok(())
    }
}

/// The counters the loop prints at the end, one line on stderr like the C main.
#[derive(Debug, Clone, Default, PartialEq)]
pub struct RunSummary {
    pub n_events: u64,
    pub n_model: u64,
    pub n_model_errors: u64,
    pub n_act_errors: u64,
    pub sum_prompt_ms: f64,
    pub sum_generate_ms: f64,
    pub rss_kb: i64,
    pub engine: String,
    pub source: String,
}

impl RunSummary {
    pub fn mean_prompt_ms(&self) -> f64 {
        if self.n_model > 0 { self.sum_prompt_ms / self.n_model as f64 } else { 0.0 }
    }

    pub fn mean_generate_ms(&self) -> f64 {
        if self.n_model > 0 { self.sum_generate_ms / self.n_model as f64 } else { 0.0 }
    }
}

/// The loop, in the order of common/c/src/edgellm_main.c: open the engine with the wrapped
/// system prompt, open the source, open the log; then per event run the rule on the history,
/// check its index, actuate, build the user text, wrap it, ask the engine, write the row,
/// push the event. A read error from the source ends the run with Err, which is the right
/// outcome on a bench where an open contact shows as a read error and never as wrong data.
/// There is no signal handler: the log is flushed after every row, so a SIGTERM from
/// systemd loses nothing.
pub fn run_loop(
    cfg: &RunConfig,
    rule: &dyn Rule,
    project: &mut dyn Project,
) -> Result<RunSummary, Error> {
    let plain = read_text(&cfg.prompt)?;
    let ecfg = EngineConfig {
        model_path: cfg.model.clone().unwrap_or_default(),
        grammar_path: cfg.grammar.clone(),
        system_prompt: template::wrap_system(cfg.template, &plain),
        state_path: cfg.state.clone(),
        n_ctx: cfg.n_ctx,
        n_threads: cfg.threads,
        n_predict: cfg.n_predict,
        temperature: cfg.temperature,
        seed: 1,
    };
    let mut engine = open_decider(&ecfg, cfg.stub)?;
    let mut src: Box<dyn IoSource> = match (&cfg.replay, &cfg.live) {
        (Some(path), _) => Box::new(io::Replay::open(path)?),
        (None, Some((backend, spec))) => io::open_live(backend, spec)?,
        (None, None) => return err("edgellm: no source"),
    };
    let mut log = elog::Elog::open(&cfg.log, &engine.engine_version())?;
    let mut ctx = RuleCtx::new();
    let mut sum = RunSummary::default();
    let n_names = rule.names().len();

    while let Some(ev) = src.next()? {
        let idx = rule.decide(&ctx, &ev);
        if idx >= n_names {
            return err(format!(
                "{}: rule {} returned {} outside its {} names",
                cfg.project,
                rule.project(),
                idx,
                n_names
            ));
        }
        if project.actuate(src.as_mut(), rule, idx, &ev).is_err() {
            sum.n_act_errors += 1;
        }
        let mut logged: Option<Decision> = None;
        if let Some(user) = project.format_prompt(&ev, &ctx) {
            let wrapped = template::wrap_user(cfg.template, &user);
            match engine.decide(&wrapped) {
                Ok(d) => {
                    sum.n_model += 1;
                    sum.sum_prompt_ms += d.prompt_ms;
                    sum.sum_generate_ms += d.generate_ms;
                    logged = Some(d);
                }
                Err(e) => {
                    eprintln!("{}: {}", cfg.project, e);
                    sum.n_model_errors += 1;
                    logged = Some(Decision::default());
                }
            }
        }
        log.row(&ev, rule.names()[idx], logged.as_ref())?;
        ctx.push(&ev);
        sum.n_events += 1;
        if cfg.max_events > 0 && sum.n_events >= cfg.max_events {
            break;
        }
        if cfg.interval > 0.0 {
            std::thread::sleep(Duration::from_secs_f64(cfg.interval));
        }
    }
    sum.rss_kb = rss_kb();
    sum.engine = engine.engine_version();
    sum.source = src.backend_name().to_string();
    eprintln!(
        "{}: {} events, {} model answers, {} model errors, {} actuation errors, mean prompt \
         {:.1} ms, mean generate {:.1} ms, rss {} kB, engine {}, source {}",
        cfg.project,
        sum.n_events,
        sum.n_model,
        sum.n_model_errors,
        sum.n_act_errors,
        sum.mean_prompt_ms(),
        sum.mean_generate_ms(),
        sum.rss_kb,
        sum.engine,
        sum.source
    );
    Ok(sum)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn args(list: &[&str]) -> Vec<String> {
        let mut v = vec!["edgellm-e00".to_string()];
        v.extend(list.iter().map(|s| s.to_string()));
        v
    }

    #[test]
    fn a_replay_command_line_parses() {
        let a = args(&["--project", "e05", "--replay", "r.csv", "--grammar", "g", "--prompt", "p",
                       "--log", "l", "--stub", "--n-predict", "64", "--interval", "0.5"]);
        let cfg = parse_args(&a).unwrap();
        assert_eq!(cfg.project, "e05");
        assert_eq!(cfg.replay.as_deref(), Some("r.csv"));
        assert!(cfg.stub);
        assert_eq!(cfg.n_predict, 64);
        assert_eq!(cfg.interval, 0.5);
        assert_eq!(cfg.template, Template::ChatMl);
    }

    #[test]
    fn live_takes_a_backend_and_a_spec() {
        let a = args(&["--project", "e05", "--live", "iio",
                       "/sys/bus/iio/devices/iio:device0:0.02", "--grammar", "g", "--prompt",
                       "p", "--log", "l", "--stub"]);
        let cfg = parse_args(&a).unwrap();
        let (b, s) = cfg.live.unwrap();
        assert_eq!(b, "iio");
        assert_eq!(s, "/sys/bus/iio/devices/iio:device0:0.02");
    }

    #[test]
    fn replay_and_live_together_are_refused() {
        let a = args(&["--project", "e05", "--replay", "r", "--live", "iio", "x", "--grammar", "g",
                       "--prompt", "p", "--log", "l", "--stub"]);
        assert!(parse_args(&a).is_err());
    }

    #[test]
    fn a_missing_required_flag_is_refused() {
        let a = args(&["--project", "e05", "--replay", "r", "--grammar", "g", "--stub"]);
        assert!(parse_args(&a).is_err());
    }

    #[test]
    fn an_unknown_template_is_refused() {
        let a = args(&["--project", "e05", "--replay", "r", "--grammar", "g", "--prompt", "p",
                       "--log", "l", "--stub", "--template", "alpaca"]);
        assert!(parse_args(&a).is_err());
    }
}
