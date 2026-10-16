// main.rs: the e13 bus service. One CAN bus, four nodes, one verdict per node per
// heartbeat period from the three line rule in rules.rs, and one model answer per period
// about the node whose verdict changed.
//
//   edgellm-e13 --project e13 (--replay <csv> | --live) --grammar <gbnf> --prompt <txt>
//               --log <csv> [--model <gguf>] [--state <path>] [--periods N] [--period S]
//               [--threads N] [--n-predict N] [--n-ctx N] [--temperature T]
//               [--template chatml|zephyr|none] [--realtime] [--stub]
//
// What comes from the shared crate common/rust/edgellm, unchanged: the engine behind
// edgellm::open_decider (the llama-cpp-2 binding with the engine feature, the
// grammar-first-expansion stub without it), the nine column log writer edgellm::elog, the
// chat template edgellm::template, the Rule trait and its Registry, and the replay reader
// edgellm::io::Replay, whose CSV form is the same t_s,tag,v0,v1,... as the C tree's.
//
// What this project writes for itself, and why:
//
//   The loop. edgellm::run_loop is one decision per event. Here an event is a frame and a
//   decision is about a node over a period, four nodes at a time, so the two do not line
//   up. The order inside the period is run_loop's order all the same: source, rule, then
//   the model, then the log row.
//
//   The SocketCAN source. The shared crate's io module invites one ("e13 adds SocketCAN
//   beside them"), and this project does not put it there: a periodic reader over three
//   interfaces needs to know which interface each frame arrived on and has to see the
//   error frames, and IoSource::next, one event at a time with no deadline, carries
//   neither. Lifting it into common/rust/edgellm is in Future and extra exercises.
//
//   Two flags. --periods and --period, because the unit of this service is a heartbeat
//   period and not an event. Every other flag is spelled as edgellm::parse_args spells it.
//
// The loop, in this order, every period:
//
//   1. take every frame whose timestamp falls inside the period; a heartbeat credits its
//      node, an error frame credits its interface's health counters, anything else is
//      counted as a stray and credits nothing;
//   2. build one node-period event per node and run the rule on it; the rule's index is
//      what the report shows;
//   3. ask the model about ONE node, the first whose verdict changed since the previous
//      period, else round robin, because one decision is what a one second period affords
//      on a Pi 4 Model B;
//   4. write one log row per node, the rule column and the model column side by side.
//
// The model is never asked before the rule and its answer reaches nothing but the log.
// There is no actuator in this project at all: the service reports, and what it reports is
// which nodes are real.

mod nodes;
mod rules;

use edgellm::elog::Elog;
use edgellm::io::{Event, IoSource, Replay};
use edgellm::rules::{Registry, Rule, RuleCtx};
use edgellm::template::{self, Template};
use edgellm::{open_decider, Decision, EngineConfig};
use nodes::{Node, Source};
use rules::PresenceRule;
use std::fmt::Write as FmtWrite;
use std::process::ExitCode;

const PROJECT: &str = "e13";

// The columns of a frame event, from the replay CSV or from the SocketCAN source: the
// identifier with its flags, the length, the first two data bytes, and the transmit and
// receive error counters an error frame carries in data[6] and data[7].
const F_ID: usize = 0;
const F_D0: usize = 2;
const F_D1: usize = 3;
const F_TX_ERR: usize = 4;
const F_RX_ERR: usize = 5;

// The kernel's error frame encoding, read from the can-utils copy of include/linux/can.h
// and include/linux/can/error.h on Wednesday 14 October 2026. On the bench these bits
// arrive in the identifier of an error frame; in a replay file they are the identifier
// column, so the service reads them the same way in both cases.
const CAN_ERR_FLAG: u32 = 0x2000_0000;
const CAN_ERR_CRTL: u32 = 0x0000_0004;
const CAN_ERR_BUSOFF: u32 = 0x0000_0040;
const CAN_ERR_RESTARTED: u32 = 0x0000_0100;
const CAN_ERR_CRTL_TX_WARNING: u8 = 0x08;
const CAN_ERR_CRTL_TX_PASSIVE: u8 = 0x20;
const CAN_ERR_CRTL_ACTIVE: u8 = 0x40;

// ---------------------------------------------------------------- options

struct Options {
    replay: Option<String>,
    live: bool,
    grammar: String,
    prompt: String,
    log: String,
    model: Option<String>,
    state: Option<String>,
    periods: u64,
    period_s: f64,
    threads: i32,
    n_predict: usize,
    n_ctx: usize,
    temperature: f32,
    template: Template,
    realtime: bool,
    stub: bool,
}

const USAGE: &str = "\
usage: edgellm-e13 --project e13 (--replay <csv> | --live)
          --grammar <gbnf> --prompt <txt> --log <csv> [--model <gguf>]
          [--state <path>] [--periods N] [--period S] [--threads N]
          [--n-predict N] [--n-ctx N] [--temperature T] [--realtime] [--stub]
          [--template chatml|zephyr|none]";

fn parse(args: &[String]) -> Result<Options, String> {
    let mut o = Options {
        replay: None,
        live: false,
        grammar: String::new(),
        prompt: String::new(),
        log: String::new(),
        model: None,
        state: None,
        periods: 0,
        period_s: nodes::HEARTBEAT_PERIOD_S,
        threads: 4,
        n_predict: 64,
        n_ctx: 512,
        temperature: 0.0,
        template: Template::ChatMl,
        realtime: false,
        stub: false,
    };
    let mut i = 1;
    while i < args.len() {
        let a = args[i].as_str();
        if a == "--live" || a == "--realtime" || a == "--stub" {
            match a {
                "--live" => o.live = true,
                "--realtime" => o.realtime = true,
                _ => o.stub = true,
            }
            i += 1;
            continue;
        }
        let v = match args.get(i + 1) {
            Some(v) => v.clone(),
            None => return Err(format!("{} needs a value", a)),
        };
        match a {
            "--project" => {
                if v != PROJECT {
                    return Err(format!("this binary is {}, not {}", PROJECT, v));
                }
            }
            "--replay" => o.replay = Some(v),
            "--grammar" => o.grammar = v,
            "--prompt" => o.prompt = v,
            "--log" => o.log = v,
            "--model" => o.model = Some(v),
            "--state" => o.state = Some(v),
            "--periods" => o.periods = number(a, &v)?,
            "--period" => o.period_s = number(a, &v)?,
            "--threads" => o.threads = number(a, &v)?,
            "--n-predict" => o.n_predict = number(a, &v)?,
            "--n-ctx" => o.n_ctx = number(a, &v)?,
            "--temperature" => o.temperature = number(a, &v)?,
            "--template" => {
                o.template = Template::parse(&v).ok_or(format!("unknown template {}", v))?
            }
            _ => return Err(format!("unknown option {}", a)),
        }
        i += 2;
    }
    if o.grammar.is_empty() || o.prompt.is_empty() || o.log.is_empty() {
        return Err("--grammar, --prompt and --log are required".to_string());
    }
    if o.replay.is_some() == o.live {
        return Err("exactly one of --replay and --live is required".to_string());
    }
    if o.live && o.periods == 0 {
        return Err("--live needs --periods".to_string());
    }
    Ok(o)
}

fn number<T: std::str::FromStr>(flag: &str, v: &str) -> Result<T, String> {
    v.parse::<T>().map_err(|_| format!("{} needs a number, not {}", flag, v))
}

// ---------------------------------------------------------------- bus health

/// What an interface's own controller reports about the medium. The service prints these
/// beside the node's presence because a node can be PRESENT on a bus that is in trouble,
/// and the two are not the same fact.
#[derive(Clone)]
struct Health {
    iface: String,
    error_frames: u64,
    warnings: u64,
    passives: u64,
    busoffs: u64,
    restarts: u64,
    tx_err: u8,
    rx_err: u8,
    state: &'static str,
}

fn health_for<'a>(all: &'a mut Vec<Health>, iface: &str) -> &'a mut Health {
    if let Some(i) = all.iter().position(|h| h.iface == iface) {
        return &mut all[i];
    }
    all.push(Health {
        iface: iface.to_string(),
        error_frames: 0,
        warnings: 0,
        passives: 0,
        busoffs: 0,
        restarts: 0,
        tx_err: 0,
        rx_err: 0,
        state: "error-active",
    });
    let last = all.len() - 1;
    &mut all[last]
}

// ---------------------------------------------------------------- the live source

// The bench half of the service, behind the `live` feature, so that a host build needs no
// SocketCAN at all and the CI manifests job never compiles this module.
//
// The crate socketcan 4.0.0 (MIT) and the type names CanSocket, CanFrame, CanDataFrame,
// CanErrorFrame and CanAnyFrame were read from crates.io and from the crate's own
// documentation on Wednesday 14 October 2026. Four items here were NOT read there and are
// to verify at the first build in skyhorizon's WSL, which is the only place this module is
// ever compiled: the trait names Socket, SocketOptions and Frame, and the accessor
// raw_id(). Nothing in the replay path, the rule, the oracle test or any number this
// chapter publishes depends on them, which is why the project is built this way round.
#[cfg(feature = "live")]
mod live {
    use edgellm::io::Event;
    use edgellm::{err, Error};
    use socketcan::{CanFrame, CanSocket, EmbeddedFrame, Frame as _, Socket, SocketOptions};
    use std::time::{Duration, Instant};

    pub struct CanBus {
        sockets: Vec<(String, CanSocket)>,
        start: Instant,
    }

    impl CanBus {
        /// One raw socket per bound interface, each with a short read timeout so that the
        /// loop returns at the end of every period even on a silent bus, and with the
        /// error mask open so that controller problems, bus-off and the restart arrive as
        /// frames rather than having to be polled out of sysfs.
        pub fn open(ifaces: &[String]) -> Result<CanBus, Error> {
            let mut sockets = Vec::new();
            for iface in ifaces {
                let sock = match CanSocket::open(iface) {
                    Ok(s) => s,
                    Err(e) => return err(format!("cannot open {}: {}", iface, e)),
                };
                if let Err(e) = sock.set_read_timeout(Duration::from_millis(20)) {
                    return err(format!("{}: read timeout: {}", iface, e));
                }
                if let Err(e) = sock.set_error_filter_accept_all() {
                    return err(format!("{}: error filter: {}", iface, e));
                }
                sockets.push((iface.clone(), sock));
            }
            Ok(CanBus { sockets, start: Instant::now() })
        }

        /// Every frame that arrives before the period ends, in the same event shape the
        /// replay reader produces: tag the interface, then the identifier with its flags,
        /// the length, two data bytes and the two error counters.
        pub fn drain_until(&mut self, until_s: f64) -> Vec<Event> {
            let mut out = Vec::new();
            while self.start.elapsed().as_secs_f64() < until_s {
                for (iface, sock) in self.sockets.iter() {
                    let t_s = self.start.elapsed().as_secs_f64();
                    let frame = match sock.read_frame() {
                        Ok(f) => f,
                        Err(_) => continue,
                    };
                    let (id, data) = match &frame {
                        CanFrame::Data(f) => (f.raw_id(), f.data().to_vec()),
                        CanFrame::Error(f) => (f.raw_id(), f.data().to_vec()),
                        CanFrame::Remote(f) => (f.raw_id(), f.data().to_vec()),
                    };
                    let at = |i: usize| -> f64 {
                        data.get(i).map(|b| *b as f64).unwrap_or(0.0)
                    };
                    out.push(Event::new(t_s, iface, &[id as f64, data.len() as f64, at(0),
                                                      at(1), at(6), at(7)]));
                }
            }
            out
        }
    }
}

enum Bus {
    /// The shared crate's replay reader plus the one event already read past the period
    /// boundary, because a period needs to know where it ends and IoSource cannot peek.
    File { src: Replay, pending: Option<Event>, done: bool },
    #[cfg(feature = "live")]
    Live(live::CanBus),
}

impl Bus {
    fn drain_until(&mut self, until_s: f64) -> Result<Vec<Event>, edgellm::Error> {
        match self {
            Bus::File { src, pending, done } => {
                let mut out = Vec::new();
                loop {
                    if pending.is_none() && !*done {
                        match src.next()? {
                            Some(ev) => *pending = Some(ev),
                            None => *done = true,
                        }
                    }
                    match pending {
                        Some(ev) if ev.t_s < until_s => out.push(pending.take().unwrap()),
                        _ => return Ok(out),
                    }
                }
            }
            #[cfg(feature = "live")]
            Bus::Live(b) => Ok(b.drain_until(until_s)),
        }
    }

    /// True when a replay file has been read to its end.
    fn is_done(&self) -> bool {
        match self {
            Bus::File { pending, done, .. } => *done && pending.is_none(),
            #[cfg(feature = "live")]
            Bus::Live(_) => false,
        }
    }

    fn backend_name(&self) -> &'static str {
        match self {
            Bus::File { src, .. } => src.backend_name(),
            #[cfg(feature = "live")]
            Bus::Live(_) => "can",
        }
    }
}

// ---------------------------------------------------------------- the tracker

/// One frame into the node table and the health counters. A heartbeat identifier that
/// arrives on an interface its node is not bound to is refused and counted: without that
/// check a generator on a virtual interface could replay a hardware node's identifier and
/// a MODELLED node would read PRESENT, which is the one failure this project is about.
fn ingest(nodes: &mut [Node], health: &mut Vec<Health>, ev: &Event, strays: &mut u64) {
    let at = |i: usize| -> f64 { ev.v.get(i).copied().unwrap_or(0.0) };
    let id = at(F_ID) as u32;
    if id & CAN_ERR_FLAG != 0 {
        let h = health_for(health, &ev.tag);
        h.error_frames += 1;
        h.tx_err = at(F_TX_ERR) as u8;
        h.rx_err = at(F_RX_ERR) as u8;
        let data1 = at(F_D1) as u8;
        if id & CAN_ERR_BUSOFF != 0 {
            h.busoffs += 1;
            h.state = "bus-off";
        } else if id & CAN_ERR_RESTARTED != 0 {
            h.restarts += 1;
            h.state = "error-active";
        } else if id & CAN_ERR_CRTL != 0 {
            if data1 & CAN_ERR_CRTL_TX_PASSIVE != 0 {
                h.passives += 1;
                h.state = "error-passive";
            } else if data1 & CAN_ERR_CRTL_TX_WARNING != 0 {
                h.warnings += 1;
                h.state = "error-warning";
            } else if data1 & CAN_ERR_CRTL_ACTIVE != 0 {
                h.state = "error-active";
            }
        }
        return;
    }
    match nodes::index_of_id(nodes, id) {
        Some(i) if nodes[i].iface.as_deref() == Some(ev.tag.as_str()) => {
            nodes[i].frames += 1;
            nodes[i].frames_this_period += 1;
            nodes[i].last_heard_s = Some(ev.t_s);
            nodes[i].state_byte = at(F_D0) as u8;
        }
        _ => *strays += 1,
    }
}

/// The user text for one node, one period. It names the node, where its frames come from
/// as the service bound it, how many frames arrived in this period, how many periods have
/// been missed, how long it has been silent, and, for a hardware node, what its own
/// controller says about the medium. It never names the rule's verdict: the question of
/// this chapter is whether the model reaches the same verdict from the same counters.
fn format_prompt(node: &Node, now_s: f64, health: &[Health]) -> String {
    let silent = node.silent_for(now_s);
    let silent_text = if silent < 0.0 {
        "never heard".to_string()
    } else {
        format!("{:.1} s", silent)
    };
    let mut bus = String::new();
    if node.source.is_hardware() {
        if let Some(iface) = node.iface.as_deref() {
            if let Some(h) = health.iter().find(|h| h.iface == iface) {
                let _ = write!(bus, " Its controller is {}, tx errors {}, rx errors {}, \
                                      {} bus-off events.", h.state, h.tx_err, h.rx_err,
                               h.busoffs);
            }
        }
    }
    format!("Node {}, source {}. Frames this period {}, missed periods {}, silent {}.{} \
             Report this node.", node.name, node.describe_source(),
            node.frames_this_period, node.missed_periods, silent_text, bus)
}

/// The node to ask the model about this period: the first whose verdict changed since the
/// previous period, else round robin. One decision per period is what a one second period
/// affords on a Pi 4 Model B, and a node whose verdict just changed is the one worth it.
fn select(nodes: &[Node], verdicts: &[usize], period: u64) -> usize {
    for (i, n) in nodes.iter().enumerate() {
        if let Some(prev) = n.last_verdict {
            if prev != verdicts[i] {
                return i;
            }
        }
    }
    (period as usize) % nodes.len()
}

fn run(o: Options) -> Result<(), edgellm::Error> {
    let plain = edgellm::read_text(&o.prompt)?;
    let ecfg = EngineConfig {
        model_path: o.model.clone().unwrap_or_default(),
        grammar_path: o.grammar.clone(),
        system_prompt: template::wrap_system(o.template, &plain),
        state_path: o.state.clone(),
        n_ctx: o.n_ctx,
        n_threads: o.threads,
        n_predict: o.n_predict,
        temperature: o.temperature,
        seed: 1,
    };
    let mut decider = open_decider(&ecfg, o.stub)?;
    let mut registry = Registry::new();
    registry.register(Box::new(PresenceRule));
    let rule: &dyn Rule = match registry.find(PROJECT) {
        Some(r) => r,
        None => return edgellm::err("no rule registered for e13"),
    };
    let mut table = nodes::table();

    let mut bus = match &o.replay {
        Some(path) => {
            nodes::bind_for_replay(&mut table);
            Bus::File { src: Replay::open(path)?, pending: None, done: false }
        }
        None => {
            #[cfg(feature = "live")]
            {
                for line in nodes::bind_by_driver(&mut table).map_err(edgellm::Error)? {
                    eprintln!("{}: {}", PROJECT, line);
                }
                let ifaces: Vec<String> = table
                    .iter()
                    .filter(|n| !n.source.is_absent())
                    .filter_map(|n| n.iface.clone())
                    .collect();
                Bus::Live(live::CanBus::open(&ifaces)?)
            }
            #[cfg(not(feature = "live"))]
            {
                return edgellm::err("this build has no live backend; build with the live \
                                     feature");
            }
        }
    };

    let mut health: Vec<Health> = Vec::new();
    let mut log = Elog::open(&o.log, &decider.engine_version())?;
    let ctx = RuleCtx::new();
    let mut strays = 0u64;
    let mut asked = 0u64;
    let mut agreed = 0u64;
    let mut model_errors = 0u64;
    let mut sum_prompt_ms = 0.0;
    let mut sum_generate_ms = 0.0;
    let start = std::time::Instant::now();
    let periods = if o.periods > 0 { o.periods } else { u64::MAX };

    for period in 0..periods {
        let end = (period + 1) as f64 * o.period_s;
        let frames = bus.drain_until(end)?;
        if o.periods == 0 && frames.is_empty() && bus.is_done() {
            break;
        }
        for ev in frames.iter() {
            ingest(&mut table, &mut health, ev, &mut strays);
        }
        for n in table.iter_mut() {
            if n.frames_this_period == 0 {
                n.missed_periods += 1;
            } else {
                n.missed_periods = 0;
            }
        }
        let events: Vec<Event> = table
            .iter()
            .map(|n| rules::node_event(end, n.name, n.frames_this_period, n.missed_periods,
                                       n.silent_for(end), n.source.code()))
            .collect();
        let verdicts: Vec<usize> =
            events.iter().map(|ev| rule.decide(&ctx, ev)).collect();
        for (i, v) in verdicts.iter().enumerate() {
            if *v >= rule.names().len() {
                return edgellm::err(format!("rule e13 returned {} for {} outside its {} \
                                             names", v, table[i].name, rule.names().len()));
            }
        }
        let pick = select(&table, &verdicts, period);
        let user = format_prompt(&table[pick], end, &health);
        let answer: Option<Decision> = match decider.decide(&template::wrap_user(o.template,
                                                                                 &user)) {
            Ok(d) => {
                asked += 1;
                sum_prompt_ms += d.prompt_ms;
                sum_generate_ms += d.generate_ms;
                if d.text.contains(rule.names()[verdicts[pick]]) {
                    agreed += 1;
                }
                Some(d)
            }
            Err(e) => {
                eprintln!("{}: {}", PROJECT, e);
                model_errors += 1;
                Some(Decision::default())
            }
        };
        for i in 0..table.len() {
            let model = if i == pick { answer.as_ref() } else { None };
            log.row(&events[i], rule.names()[verdicts[i]], model)?;
        }
        for (i, n) in table.iter_mut().enumerate() {
            n.last_verdict = Some(verdicts[i]);
            n.frames_this_period = 0;
        }
        if o.realtime {
            let elapsed = start.elapsed().as_secs_f64();
            if end > elapsed {
                std::thread::sleep(std::time::Duration::from_secs_f64(end - elapsed));
            }
        }
    }

    for n in table.iter() {
        let where_from = match n.source {
            Source::Hardware { controller, .. } => format!(
                "{} on {}", controller, n.iface.as_deref().unwrap_or("unbound")),
            Source::Vcan { interface } => format!("vcan {}", interface),
            Source::Absent { .. } => "absent, not connected".to_string(),
        };
        eprintln!("{}: {} ({}): {} frames, last verdict {}", PROJECT, n.name, where_from,
                  n.frames,
                  n.last_verdict.map(|i| rule.names()[i]).unwrap_or("none"));
    }
    for h in health.iter() {
        eprintln!("{}: {}: {} error frames, {} warning, {} passive, {} bus-off, {} \
                   restarts, tx errors {}, rx errors {}, state {}", PROJECT, h.iface,
                  h.error_frames, h.warnings, h.passives, h.busoffs, h.restarts, h.tx_err,
                  h.rx_err, h.state);
    }
    eprintln!("{}: {} model answers, {} agreed, {} model errors, {} stray frames, mean \
               prompt {:.1} ms, mean generate {:.1} ms, rss {} kB, engine {}, source {}",
              PROJECT, asked, agreed, model_errors, strays,
              if asked > 0 { sum_prompt_ms / asked as f64 } else { 0.0 },
              if asked > 0 { sum_generate_ms / asked as f64 } else { 0.0 },
              edgellm::rss_kb(), decider.engine_version(), bus.backend_name());
    Ok(())
}

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().collect();
    let options = match parse(&args) {
        Ok(o) => o,
        Err(message) => {
            eprintln!("edgellm-e13: {}\n{}", message, USAGE);
            return ExitCode::from(2);
        }
    };
    match run(options) {
        Ok(()) => ExitCode::SUCCESS,
        Err(e) => {
            eprintln!("edgellm-e13: {}", e);
            ExitCode::from(1)
        }
    }
}
