//! The device service of project e18: a mini industrial serial logger.
//!
//! It owns the two RS485 channels of the Waveshare WS-28164 HAT, the RS232 channel on
//! the Pi's own UART, the 64 byte register frame, the control rule and the log. It
//! does NOT own the model: the engine, the grammar and the sampler live in the C half
//! of this project, behind the four declarations of `../c/abi.h`, reached either in
//! process over the FFI binding or out of process over the daemon's Unix domain
//! socket. That split is the whole point of the chapter, and the reason is in the
//! header rather than here: the device logic and the inference engine have different
//! lifetimes, and in a product different authors.
//!
//! WHAT IS REAL AND WHAT IS NOT, said once in the program as well as in the chapter.
//! The HAT's own transceivers are real, and so is the echo between its two RS485
//! channels, which `--echo` measures and which is the one electrical test this project
//! can run. The register stream is modelled: `--replay` reads it from a file, and the
//! file was generated, not captured. The peer device is absent. There is no Modbus
//! slave on this bench and the one serial cable that is here is TTL rather than
//! RS232, so `--live` is written, checked against the frame decoder, and has never
//! been fed by a device. Nothing below reports a measurement that was not taken.
//!
//! WHAT IT EXPECTS OF THE SHARED CRATE. The four modules of `common/rust/edgellm`
//! this service reuses, and the only interface it needs from each. The crate's engine
//! module is deliberately NOT used: the engine is in C here.
//!
//! ```text
//!   edgellm::io        Event { t_s: f64, tag: String, v: Vec<f64> }
//!                      Replay::open(path) -> Result<Replay, edgellm::Error>, MET, at
//!                      io::Replay and not io::replay::Replay
//!                      next(&mut self) -> Result<Option<Event>, edgellm::Error>, MET as
//!                      the IoSource trait method, so IoSource must be in scope, and the
//!                      error is the crate's and not std::io::Error
//!                      Port::open(path, baud) -> io::Result<Port>, MET on Sunday 18
//!                      October 2026 at io::serial, behind the crate's "serial"
//!                      feature, which this project's Cargo.toml enables
//!                      Port::set_rs485(&mut self, bool) -> io::Result<()>, MET, and
//!                      the one piece with no counterpart in common/c: its two flag
//!                      bits are from linux/serial.h and are not yet confirmed
//!                      against the board's own header, so the RS485 echo figure is
//!                      not evidence until that one line has been run
//!                      Port::write_all(&mut self, &[u8]) -> io::Result<()>, MET
//!                      Port::read_exact_timeout(&mut self, &mut [u8], Duration) ->
//!                      io::Result<usize>, MET; a short read is Ok(n), a deadline
//!                      with nothing read is TimedOut
//!   edgellm::rules     Rule (trait), RuleCtx::with_capacity(n) / push(&Event) /
//!                      hist(). MET on Sunday 18 October 2026: the crate's capacity
//!                      became per-context so that WINDOW stays 30 here, and new()
//!                      still keeps RULE_HISTORY for every other project.
//!   edgellm::elog      Elog::open(path, engine) / Elog::row(&Event, &str,
//!                      Option<&Decision>), MET under those names; the answer type is
//!                      edgellm::Decision, which carries those four fields and two more
//!   edgellm::template  wrap_system(Template, &str) and wrap_user(Template, &str), MET
//!                      under those names, taking the template rather than naming it
//! ```
//!
//!     edgellm-e18 (--replay <csv> | --live) --grammar <gbnf> --prompt <txt>
//!                 --log <csv> [--engine stub|socket|ffi] [--socket <path>]
//!                 [--model <gguf>] [--state <path>] [--rs485-a <dev>]
//!                 [--rs485-b <dev>] [--rs232 <dev>] [--baud N] [--addr N]
//!                 [--period S] [--max-events N] [--echo N] [--bench N]

mod abi;
mod registers;
mod rules;

use std::io::Write;
use std::path::PathBuf;
use std::process::ExitCode;
use std::time::{Duration, Instant};

use edgellm::elog::Elog;
use edgellm::Decision;
use edgellm::io::serial::Port;
use edgellm::io::{Event, IoSource, Replay};
use edgellm::rules::{Rule, RuleCtx};
use edgellm::template::{wrap_system, wrap_user, Template};

use abi::{Backend, EngineConfig, EngineError, Socket, Stub, EXPECTED_ABI_VERSION};
use registers::{Frame, FrameError, FRAME_LEN, FUNC_READ_HOLDING, N_REGISTERS};
use rules::DriftRule;

/// One fixed user text for `--bench`, so that the three boundaries are timed on the
/// same prompt and the only difference between the figures is the boundary.
const BENCH_TEXT: &str = "register 0 level 577 counts, span 141 counts over 30 frames, \
temperature 42.4 C, pressure 5.8 bar. Decide.";

#[derive(Debug)]
struct Options {
    replay: Option<PathBuf>,
    live: bool,
    grammar: PathBuf,
    prompt: PathBuf,
    log: PathBuf,
    engine: String,
    socket: PathBuf,
    model: Option<String>,
    state: Option<String>,
    rs485_a: PathBuf,
    rs485_b: PathBuf,
    rs232: Option<PathBuf>,
    baud: u32,
    addr: u8,
    period: f64,
    max_events: usize,
    echo: usize,
    bench: usize,
    n_ctx: i32,
    threads: i32,
    n_predict: i32,
    temperature: f32,
}

fn usage() -> ExitCode {
    eprintln!(
        "usage: edgellm-e18 (--replay <csv> | --live) --grammar <gbnf> --prompt <txt>\n\
         \x20                --log <csv> [--engine stub|socket|ffi] [--socket <path>]\n\
         \x20                [--model <gguf>] [--state <path>] [--rs485-a <dev>]\n\
         \x20                [--rs485-b <dev>] [--rs232 <dev>] [--baud N] [--addr N]\n\
         \x20                [--period S] [--max-events N] [--echo N] [--bench N]"
    );
    ExitCode::from(2)
}

fn parse() -> Option<Options> {
    let mut o = Options {
        replay: None,
        live: false,
        grammar: PathBuf::new(),
        prompt: PathBuf::new(),
        log: PathBuf::new(),
        engine: "stub".to_string(),
        socket: PathBuf::from("/run/edgellm/e18-daemon.sock"),
        model: None,
        state: None,
        rs485_a: PathBuf::from("/dev/ttySC0"),
        rs485_b: PathBuf::from("/dev/ttySC1"),
        rs232: None,
        baud: 115_200,
        addr: 0x11,
        period: 2.0,
        max_events: 0,
        echo: 0,
        bench: 0,
        n_ctx: 512,
        threads: 4,
        n_predict: 64,
        temperature: 0.0,
    };
    let args: Vec<String> = std::env::args().skip(1).collect();
    let mut i = 0usize;
    while i < args.len() {
        let flag = args[i].as_str();
        if flag == "--live" {
            o.live = true;
            i += 1;
            continue;
        }
        let value = args.get(i + 1)?.clone();
        match flag {
            "--replay" => o.replay = Some(PathBuf::from(value)),
            "--grammar" => o.grammar = PathBuf::from(value),
            "--prompt" => o.prompt = PathBuf::from(value),
            "--log" => o.log = PathBuf::from(value),
            "--engine" => o.engine = value,
            "--socket" => o.socket = PathBuf::from(value),
            "--model" => o.model = Some(value),
            "--state" => o.state = Some(value),
            "--rs485-a" => o.rs485_a = PathBuf::from(value),
            "--rs485-b" => o.rs485_b = PathBuf::from(value),
            "--rs232" => o.rs232 = Some(PathBuf::from(value)),
            "--baud" => o.baud = value.parse().ok()?,
            "--addr" => o.addr = value.parse().ok()?,
            "--period" => o.period = value.parse().ok()?,
            "--max-events" => o.max_events = value.parse().ok()?,
            "--echo" => o.echo = value.parse().ok()?,
            "--bench" => o.bench = value.parse().ok()?,
            "--n-ctx" => o.n_ctx = value.parse().ok()?,
            "--threads" => o.threads = value.parse().ok()?,
            "--n-predict" => o.n_predict = value.parse().ok()?,
            "--temperature" => o.temperature = value.parse().ok()?,
            _ => return None,
        }
        i += 2;
    }
    if o.grammar.as_os_str().is_empty() || o.prompt.as_os_str().is_empty() {
        return None;
    }
    if o.echo == 0 && o.bench == 0 {
        if o.log.as_os_str().is_empty() {
            return None;
        }
        if o.replay.is_some() == o.live {
            return None; // exactly one source, never both and never neither
        }
    }
    Some(o)
}

// ---------------------------------------------------------------- the register source

/// Where a frame came from. The two arms are the honesty rule in the type system: a
/// replayed row is re-encoded into a frame and decoded again, so the offsets, the
/// byte order and the checksum are exercised with no wire; a live frame came off
/// port A.
enum Source {
    Replayed(Replay),
    Live { port: Port, addr: u8, timeout: Duration },
}

impl Source {
    fn next(&mut self) -> std::io::Result<Option<(Event, Result<Frame, FrameError>)>> {
        match self {
            Source::Replayed(r) => match r
                .next()
                .map_err(|e| std::io::Error::new(std::io::ErrorKind::Other, e))?
            {
                None => Ok(None),
                Some(ev) => {
                    let frame = encode_row(&ev);
                    let bytes = registers::encode(&frame);
                    Ok(Some((ev, registers::decode(&bytes))))
                }
            },
            Source::Live { port, addr, timeout } => {
                port.write_all(&registers::request(*addr, 0, N_REGISTERS as u16))?;
                let mut bytes = [0u8; FRAME_LEN];
                let n = port.read_exact_timeout(&mut bytes, *timeout)?;
                let decoded = registers::decode(&bytes[..n]);
                let ev = match &decoded {
                    Ok(f) => Event {
                        t_s: now_s(),
                        tag: "rs485a".to_string(),
                        v: vec![f.regs[0] as f64, f.regs[1] as f64, f.regs[2] as f64,
                                f.seq as f64],
                    },
                    Err(_) => Event { t_s: now_s(), tag: "rs485a".to_string(), v: vec![] },
                };
                Ok(Some((ev, decoded)))
            }
        }
    }
}

/// The replay row as the frame the peer would have sent. v0, v1 and v2 are the three
/// meaningful registers and v3 is the sequence number the row carried.
fn encode_row(ev: &Event) -> Frame {
    let mut regs = [0u16; N_REGISTERS];
    for (i, slot) in regs.iter_mut().enumerate().take(3) {
        *slot = ev.v.get(i).copied().unwrap_or(0.0).round().max(0.0).min(65535.0) as u16;
    }
    Frame {
        addr: 0x11,
        func: FUNC_READ_HOLDING,
        seq: (ev.v.get(3).copied().unwrap_or(0.0).round().max(0.0) as u64 % 256) as u8,
        regs,
        flags: 0x0007, // registers 0, 1 and 2 are meaningful; the rest are reserved
    }
}

fn now_s() -> f64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map(|d| d.as_secs_f64())
        .unwrap_or(0.0)
}

// ---------------------------------------------------------------- the prompt

/// The user text for one frame: the register the rule watches, its span over the
/// window, the two other registers and the sequence number. The thresholds are in the
/// system prompt, so they cost no tokens per frame.
fn format_prompt(ev: &Event, span: f64) -> String {
    let level = ev.v.first().copied().unwrap_or(0.0);
    let temp = ev.v.get(1).copied().unwrap_or(0.0) / 10.0;
    let press = ev.v.get(2).copied().unwrap_or(0.0) / 10.0;
    format!(
        "register 0 level {:.0} counts, span {:.0} counts over {} frames, \
         temperature {:.1} C, pressure {:.1} bar. Decide.",
        level, span, rules::WINDOW, temp, press
    )
}

// ---------------------------------------------------------------- the three modes

/// `--echo N`: the one real electrical test. Port A writes a frame, port B reads it
/// and writes it back, port A reads the echo. Every refusal is counted by its own
/// reason, because "the echo failed" is not a diagnosis.
fn echo_test(o: &Options) -> Result<(), String> {
    let timeout = Duration::from_millis(200);
    let mut a = Port::open(&o.rs485_a, o.baud).map_err(|e| format!("{:?}: {}", o.rs485_a, e))?;
    let mut b = Port::open(&o.rs485_b, o.baud).map_err(|e| format!("{:?}: {}", o.rs485_b, e))?;
    // Half duplex: the transceiver's driver enable comes from the port's RTS line, so
    // the driver must own the direction, and the receiver must stay enabled while the
    // port transmits or the echo arrives in a window the port is not listening in.
    a.set_rs485(true).map_err(|e| format!("rs485 on port A: {}", e))?;
    b.set_rs485(true).map_err(|e| format!("rs485 on port B: {}", e))?;

    let mut sent = 0usize;
    let mut ok = 0usize;
    let mut short_reads = 0usize;
    let mut crc_errors = 0usize;
    let mut other_errors = 0usize;
    let mut gaps = 0u32;
    let mut previous: Option<u8> = None;
    let mut worst_us = 0.0f64;
    let mut sum_us = 0.0f64;

    let mut regs = [0u16; N_REGISTERS];
    for i in 0..o.echo {
        regs[0] = (420 + (i % 400)) as u16;
        regs[1] = 424;
        regs[2] = 58;
        let frame = Frame { addr: o.addr, func: FUNC_READ_HOLDING, seq: (i % 256) as u8,
                            regs, flags: 0x0007 };
        let bytes = registers::encode(&frame);
        let start = Instant::now();
        a.write_all(&bytes).map_err(|e| format!("port A write: {}", e))?;
        let mut on_b = [0u8; FRAME_LEN];
        match b.read_exact_timeout(&mut on_b, timeout) {
            Ok(FRAME_LEN) => {
                b.write_all(&on_b).map_err(|e| format!("port B write: {}", e))?;
            }
            Ok(n) => {
                short_reads += 1;
                sent += 1;
                eprintln!("frame {}: port B read {} of {} bytes", i, n, FRAME_LEN);
                continue;
            }
            Err(e) => {
                other_errors += 1;
                sent += 1;
                eprintln!("frame {}: port B: {}", i, e);
                continue;
            }
        }
        let mut back = [0u8; FRAME_LEN];
        let n = match a.read_exact_timeout(&mut back, timeout) {
            Ok(n) => n,
            Err(e) => {
                other_errors += 1;
                sent += 1;
                eprintln!("frame {}: port A: {}", i, e);
                continue;
            }
        };
        let round_us = start.elapsed().as_secs_f64() * 1e6;
        sum_us += round_us;
        if round_us > worst_us {
            worst_us = round_us;
        }
        sent += 1;
        match registers::decode(&back[..n]) {
            Ok(got) => {
                if got != frame {
                    other_errors += 1;
                    eprintln!("frame {}: the echo decoded but differs", i);
                    continue;
                }
                if let Some(p) = previous {
                    gaps += registers::seq_gap(p, got.seq);
                }
                previous = Some(got.seq);
                ok += 1;
            }
            Err(FrameError::Short(k)) => {
                short_reads += 1;
                eprintln!("frame {}: port A read {} of {} bytes", i, k, FRAME_LEN);
            }
            Err(FrameError::Crc { want, got }) => {
                crc_errors += 1;
                eprintln!("frame {}: checksum 0x{:04x}, computed 0x{:04x}", i, got, want);
            }
            Err(e) => {
                other_errors += 1;
                eprintln!("frame {}: {}", i, e);
            }
        }
    }
    let errors = short_reads + crc_errors + other_errors;
    println!("echo: {} frames at {} baud on {:?} and {:?}", sent, o.baud, o.rs485_a,
             o.rs485_b);
    println!("echo: {} good, {} short reads, {} checksum failures, {} other, {} sequence gaps",
             ok, short_reads, crc_errors, other_errors, gaps);
    if ok > 0 {
        println!("echo: round trip mean {:.0} us, worst {:.0} us", sum_us / ok as f64,
                 worst_us);
    }
    println!("echo: the pass condition is zero errors and zero gaps over all {} frames; \
              this run has {} errors and {} gaps", o.echo, errors, gaps);
    if errors == 0 && gaps == 0 && ok == o.echo {
        Ok(())
    } else {
        Err("the echo did not pass".to_string())
    }
}

/// `--bench N`: N decisions on one fixed prompt through the chosen boundary. The
/// figure this prints is the Rust side of the chapter's comparison; the C side comes
/// from `e18_abi_test`, which times the same engine with no boundary at all.
fn bench(engine: &mut Backend, n: usize) -> Result<(), String> {
    let user = wrap_user(Template::ChatMl, BENCH_TEXT);
    let mut us = Vec::with_capacity(n);
    for i in 0..n {
        let start = Instant::now();
        engine.decide(&user).map_err(|e| format!("call {}: {}", i, e))?;
        us.push(start.elapsed().as_secs_f64() * 1e6);
    }
    let mean = us.iter().sum::<f64>() / n as f64;
    let sd = (us.iter().map(|v| (v - mean) * (v - mean)).sum::<f64>()
              / (n as f64 - 1.0).max(1.0)).sqrt();
    println!("bench: backend {}, abi {}, {} calls", engine.name(), EXPECTED_ABI_VERSION, n);
    println!("bench: mean {:.3} us, sd {:.3} us", mean, sd);
    Ok(())
}

/// The logging loop. One frame in, one rule verdict, one line on RS232, one model
/// sentence beside it in the log, in that order; the verdict is written before the
/// model is asked, which is what makes the safety rule a property of the code rather
/// than of the prose.
fn run(o: &Options, engine: &mut Backend) -> Result<(), String> {
    let rule = DriftRule;
    let mut ctx = RuleCtx::with_capacity(rules::WINDOW);
    let mut source = if let Some(path) = &o.replay {
        Source::Replayed(Replay::open(path).map_err(|e| format!("{:?}: {}", path, e))?)
    } else {
        let mut port = Port::open(&o.rs485_a, o.baud)
            .map_err(|e| format!("{:?}: {}", o.rs485_a, e))?;
        port.set_rs485(true).map_err(|e| format!("rs485 on port A: {}", e))?;
        Source::Live { port, addr: o.addr, timeout: Duration::from_millis(200) }
    };
    let mut rs232 = match &o.rs232 {
        Some(path) => Some(Port::open(path, o.baud).map_err(|e| format!("{:?}: {}", path, e))?),
        None => None,
    };
    let engine_id = format!("e18-{}-abi{}", engine.name(), EXPECTED_ABI_VERSION);
    let mut log = Elog::open(&o.log, &engine_id)
        .map_err(|e| format!("{:?}: {}", o.log, e))?;

    let mut frames = 0usize;
    let mut bad_frames = 0usize;
    let mut gaps = 0u32;
    let mut previous_seq: Option<u8> = None;
    let mut answers = 0usize;
    let mut engine_errors = 0usize;
    let mut overruns = 0usize;
    let mut sum_us = 0.0f64;

    while let Some((ev, decoded)) = source.next().map_err(|e| e.to_string())? {
        match &decoded {
            Ok(frame) => {
                if let Some(p) = previous_seq {
                    gaps += registers::seq_gap(p, frame.seq);
                }
                previous_seq = Some(frame.seq);
            }
            Err(e) => {
                bad_frames += 1;
                eprintln!("frame {}: {}", frames, e);
            }
        }
        let index = rule.decide(&ctx, &ev);
        let verdict = rule.names()[index];
        let span = rules::window_span(&ctx, &ev);
        // The rule acts here, and this is the only output in the program: one line on
        // the RS232 channel. The model's sentence never reaches it.
        if let Some(port) = rs232.as_mut() {
            let line = format!("{:.1} {} span {:.0} counts of {:.0}\r\n", ev.t_s, verdict,
                               span, rules::drift_threshold());
            if let Err(e) = port.write_all(line.as_bytes()) {
                eprintln!("rs232: {}", e);
            }
        }
        let answer = if decoded.is_ok() {
            let user = wrap_user(Template::ChatMl, &format_prompt(&ev, span));
            let start = Instant::now();
            match engine.decide(&user) {
                Ok(text) => {
                    let call_us = start.elapsed().as_secs_f64() * 1e6;
                    sum_us += call_us;
                    answers += 1;
                    if call_us > o.period * 1e6 {
                        overruns += 1;
                    }
                    // prompt_ms stays 0 and n_generated stays 0 on purpose: both are
                    // fields of a structure, no structure crosses this ABI, and the
                    // service will not invent a split it cannot see. generate_ms is
                    // the whole call as the caller paid for it, which is the figure
                    // worth publishing anyway.
                    // Decision carries two fields beyond the four this service can
                    // fill, n_prompt_tokens and stub, and both are left at their defaults
                    // for the same reason the two below are zero: nothing crosses the ABI
                    // that would let this side invent them.
                    Some(Decision { text, prompt_ms: 0.0, generate_ms: call_us / 1000.0,
                                    n_generated: 0, ..Decision::default() })
                }
                Err(EngineError::Version { want, got }) => {
                    return Err(format!("abi version {} against {}", got, want));
                }
                Err(e) => {
                    engine_errors += 1;
                    eprintln!("frame {}: {}", frames, e);
                    None
                }
            }
        } else {
            None // a frame that did not decode is logged with the rule alone
        };
        log.row(&ev, verdict, answer.as_ref()).map_err(|e| format!("log: {}", e))?;
        ctx.push(&ev);
        frames += 1;
        if o.max_events > 0 && frames >= o.max_events {
            break;
        }
    }
    let _ = std::io::stdout().flush();
    eprintln!(
        "e18: {} frames, {} refused, {} sequence gaps, {} answers, {} engine errors, \
         {} over the {:.1} s period, mean call {:.1} ms, backend {}, abi {}",
        frames, bad_frames, gaps, answers, engine_errors, overruns, o.period,
        if answers > 0 { sum_us / answers as f64 / 1000.0 } else { 0.0 },
        engine.name(), EXPECTED_ABI_VERSION
    );
    Ok(())
}

fn open_engine(o: &Options) -> Result<Backend, String> {
    let cfg = EngineConfig {
        model: o.model.clone(),
        grammar: o.grammar.to_string_lossy().into_owned(),
        prompt: o.prompt.to_string_lossy().into_owned(),
        state: o.state.clone(),
        n_ctx: o.n_ctx,
        n_threads: o.threads,
        n_predict: o.n_predict,
        temperature: o.temperature,
    };
    match o.engine.as_str() {
        "stub" => Stub::open(&o.grammar).map(Backend::Stub).map_err(|e| e.to_string()),
        "socket" => Socket::connect(&o.socket).map(Backend::Socket).map_err(|e| e.to_string()),
        #[cfg(feature = "ffi")]
        "ffi" => abi::Ffi::open(&cfg).map(Backend::Ffi).map_err(|e| e.to_string()),
        #[cfg(not(feature = "ffi"))]
        "ffi" => {
            let _ = cfg;
            Err("this binary was built without the ffi feature; build it with \
                 --features ffi and a link path to libedgellm_abi.so, or use \
                 --engine socket".to_string())
        }
        other => Err(format!("unknown engine {}, expected stub, socket or ffi", other)),
    }
}

fn main() -> ExitCode {
    let o = match parse() {
        Some(o) => o,
        None => return usage(),
    };
    // The system prompt is read here only to fail early and to say its length: the
    // engine reads the file itself, on whichever side of the boundary it sits.
    match std::fs::read_to_string(&o.prompt) {
        Ok(text) => {
            let _ = wrap_system(Template::ChatMl, text.trim_end());
            eprintln!("e18: system prompt {} words from {:?}",
                      text.split_whitespace().count(), o.prompt);
        }
        Err(e) => {
            eprintln!("e18: {:?}: {}", o.prompt, e);
            return ExitCode::from(1);
        }
    }
    if o.echo > 0 {
        return match echo_test(&o) {
            Ok(()) => ExitCode::SUCCESS,
            Err(e) => {
                eprintln!("e18: {}", e);
                ExitCode::from(1)
            }
        };
    }
    let mut engine = match open_engine(&o) {
        Ok(e) => e,
        Err(e) => {
            eprintln!("e18: {}", e);
            return ExitCode::from(1);
        }
    };
    let outcome = if o.bench > 0 {
        bench(&mut engine, o.bench)
    } else {
        run(&o, &mut engine)
    };
    match outcome {
        Ok(()) => ExitCode::SUCCESS,
        Err(e) => {
            eprintln!("e18: {}", e);
            ExitCode::from(1)
        }
    }
}
