// main.rs: the joint monitor of project e14. It watches one classic CAN bus and one
// accelerometer, applies the control rule of src/rules.rs once a minute, asks a
// separate process for the same verdict and one sentence, and writes one log row per
// minute. INSPECT is a recommendation to a person. Nothing here is actuated: there is
// no io_actuate call, no GPIO line and no output of any kind, because a monitor that
// stopped a joint would be a protective function and the model is not allowed near one.
//
// This service is project e13's service with the engine moved out of process. The rule,
// the node table, the grammar, the prompt and the log shape are e13's; the difference is
// that the answer arrives over a Unix domain socket from src/bin/engine_daemon.rs
// instead of from a model loaded here. That one difference is what the chapter measures:
// the resident memory of the pair against one process, the decision latency with the
// socket round trip added, and the time the service needs to answer again after the
// daemon has been stopped with signal 9 while the bus keeps running.
//
// Two tasks, on purpose. The bus reader owns the CAN socket and the accelerometer and
// runs on its own thread; the aggregator owns the engine client and the log and runs on
// the main thread. They meet at one unbounded channel. A socket timeout therefore
// delays a log row and never a frame: at the default timeout of 2000 ms the channel
// holds at most the 2 s of samples the reader produced meanwhile, which at 50 Hz is 100
// samples of 32 bytes.

mod engine_client;
mod rules;

use std::collections::BTreeMap;
use std::fs::{File, OpenOptions};
use std::io::{self, BufRead, BufReader, Write};
use std::os::unix::io::AsRawFd;
use std::path::{Path, PathBuf};
use std::process;
use std::sync::mpsc::{self, RecvTimeoutError, Sender};
use std::thread;
use std::time::Duration;
#[cfg(feature = "live")]
use std::time::Instant;

use engine_client::EngineClient;
use rules::{Decision, Minute, NodeState, NODE_TABLE};

// Read only by iio::Accel, which only the live reader constructs. allow rather than cfg
// throughout this file for the same reason: the stub build is the only build any job
// compiles, since the live feature's socketcan 4.0.0 cannot be parsed by the image's cargo
// 1.75, so gating these out would leave them compiled by nothing.
#[cfg_attr(not(feature = "live"), allow(dead_code))]
const GRAVITY: f64 = 9.80665;

// ---------------------------------------------------------------- options

struct Options {
    replay: Option<PathBuf>,
    live: Option<String>,
    iio: Option<PathBuf>,
    socket: PathBuf,
    log: PathBuf,
    timeout_ms: u64,
    minute_s: f64,
    sample_hz: f64,
    peak_g: f64,
}

fn usage() -> ! {
    eprintln!(
        "edgellm-e14 --replay <csv> | --live <can interface> --iio <iio device dir>\n\
         \u{20}   [--socket /run/edgellm/e14-engine.sock] [--log <csv>]\n\
         \u{20}   [--timeout-ms 2000] [--minute-s 60] [--sample-hz 50] [--peak-g 2.0]\n\
         The interface names of the WS-28164 swap between boots: read dmesg | grep\n\
         mcp25 every session and pass the one you read."
    );
    process::exit(2);
}

fn parse_args() -> Options {
    let mut o = Options {
        replay: None,
        live: None,
        iio: None,
        socket: PathBuf::from("/run/edgellm/e14-engine.sock"),
        log: PathBuf::from("e14_run.csv"),
        timeout_ms: 2000,
        minute_s: 60.0,
        sample_hz: 50.0,
        peak_g: rules::PEAK_G,
    };
    let mut args = std::env::args().skip(1);
    while let Some(a) = args.next() {
        let mut value = || args.next().unwrap_or_else(|| usage());
        match a.as_str() {
            "--replay" => o.replay = Some(PathBuf::from(value())),
            "--live" => o.live = Some(value()),
            "--iio" => o.iio = Some(PathBuf::from(value())),
            "--socket" => o.socket = PathBuf::from(value()),
            "--log" => o.log = PathBuf::from(value()),
            "--timeout-ms" => o.timeout_ms = value().parse().unwrap_or(2000),
            "--minute-s" => o.minute_s = value().parse().unwrap_or(60.0),
            "--sample-hz" => o.sample_hz = value().parse().unwrap_or(50.0),
            "--peak-g" => o.peak_g = value().parse().unwrap_or(rules::PEAK_G),
            "--project" => {
                let _ = value(); /* accepted so that the command lines read like the C projects */
            }
            "-h" | "--help" => usage(),
            other => {
                eprintln!("unknown option {}", other);
                usage();
            }
        }
    }
    if o.replay.is_none() && o.live.is_none() {
        usage();
    }
    if o.live.is_some() && o.iio.is_none() {
        eprintln!("--live needs --iio as well: the rule is about both halves of the minute");
        usage();
    }
    o
}

// ---------------------------------------------------------------- what the reader sends

#[cfg_attr(not(feature = "live"), allow(dead_code))] // constructed by the live reader
enum Sample {
    /// One whole minute, from a replay row: the cumulative error counter, the peak and
    /// how many nodes answered.
    Row { t_s: f64, err_total: u64, peak_g: f64, nodes_present: u32 },
    /// One SocketCAN error frame, with the controller's transmit and receive error
    /// counters where the driver supplies them.
    BusError { t_s: f64, tec: u8, rec: u8 },
    /// One data frame, for the node table.
    Frame { t_s: f64, id: u32 },
    /// One accelerometer reading, as the magnitude of the vector minus 1 g, in g.
    Accel { t_s: f64, g: f64 },
    /// The reader is alive and the clock has moved; a quiet bus still closes its minute.
    Tick { t_s: f64 },
}

// ---------------------------------------------------------------- the IIO source
//
// This note read, until Sunday 18 October 2026: "The shared crate common/rust/edgellm is
// not in this tree on Wednesday 14 October 2026 ... the migration is one use statement:
// read(), nothing else, moves to edgellm::iio::Accel." The crate has been in the tree for
// days, and that migration is not one use statement. Read against
// common/rust/edgellm/src/io.rs on Sunday 18 October 2026:
//
//   - the type is edgellm::io::Iio, an IoSource, not edgellm::iio::Accel;
//   - next() yields an Event carrying every in_*_raw attribute the directory scan found,
//     each multiplied by its own scale, so m/s^2 for this device and not g. It computes no
//     magnitude and takes off no resting 1 g, which is the whole of what read_peak_g below
//     exists to do;
//   - it paces itself, sleeping until its own interval is due, while this project's live
//     reader samples on its own schedule in its own thread and wants a reading when it asks.
//
// So moving to the crate means adding a magnitude-in-g reader beside Iio, or keeping this
// one. That is a decision and not a rename, and it is written down as such. e18's manifest
// carried a note of the same shape, an interface recorded as planned and later read back as
// fact, and it took a compiler to disprove.
//
// The dead-code warnings on Accel, open, read_peak_g and GRAVITY in a default build are
// expected and not a sign that this is unused code: the only caller is live_reader, which
// is behind the "live" feature, and the default feature set does not include it.

#[cfg_attr(not(feature = "live"), allow(dead_code))] // used by the live reader
mod iio {
    use std::fs;
    use std::io;
    use std::path::{Path, PathBuf};

    pub struct Accel {
        dir: PathBuf,
        scale: f64,
    }

    impl Accel {
        /// dir is an IIO device directory, for example
        /// /sys/bus/iio/devices/iio:device0 for the ADXL345 bound by the adxl345 driver
        /// of the Linux volume's project 5. in_accel_scale is in m/s^2 per raw count.
        pub fn open(dir: &Path) -> io::Result<Self> {
            let scale = read_f64(&dir.join("in_accel_scale"))?;
            Ok(Accel { dir: dir.to_path_buf(), scale })
        }

        /// The magnitude of the acceleration vector in g, with the 1 g the module reads
        /// at rest taken off, so a still joint reads near zero and a tap reads its peak.
        pub fn read_peak_g(&self) -> io::Result<f64> {
            let x = read_f64(&self.dir.join("in_accel_x_raw"))? * self.scale;
            let y = read_f64(&self.dir.join("in_accel_y_raw"))? * self.scale;
            let z = read_f64(&self.dir.join("in_accel_z_raw"))? * self.scale;
            let magnitude = (x * x + y * y + z * z).sqrt() / super::GRAVITY;
            Ok((magnitude - 1.0).abs())
        }
    }

    pub fn read_f64(path: &Path) -> io::Result<f64> {
        let text = fs::read_to_string(path)?;
        text.trim()
            .parse()
            .map_err(|_| io::Error::new(io::ErrorKind::InvalidData, format!("{:?}", path)))
    }
}

/// The cumulative error counters of a CAN interface, summed. They are cumulative, which
/// is why rules::err_delta exists: the rule is about a rise inside one minute.
fn interface_err_total(iface: &str) -> io::Result<u64> {
    let base = Path::new("/sys/class/net").join(iface).join("statistics");
    let rx = iio::read_f64(&base.join("rx_errors"))?;
    let tx = iio::read_f64(&base.join("tx_errors"))?;
    Ok(rx.max(0.0) as u64 + tx.max(0.0) as u64)
}

// ---------------------------------------------------------------- the readers

/// The replay reader: one row is one minute, so the file is 100 rows for a 100 minute
/// run and the oracle has one expected row per row of it.
fn replay_reader(path: PathBuf, tx: Sender<Sample>) {
    let file = match File::open(&path) {
        Ok(f) => f,
        Err(e) => {
            eprintln!("e14: {}: {}", path.display(), e);
            process::exit(1);
        }
    };
    for (n, line) in BufReader::new(file).lines().enumerate() {
        let line = match line {
            Ok(l) => l,
            Err(e) => {
                eprintln!("e14: {}: {}", path.display(), e);
                return;
            }
        };
        if n == 0 || line.trim().is_empty() {
            continue; /* the header line, t_s,tag,v0,v1,v2 */
        }
        let f: Vec<&str> = line.split(',').collect();
        if f.len() < 5 {
            eprintln!("e14: {} line {}: {} fields", path.display(), n + 1, f.len());
            continue;
        }
        let row = Sample::Row {
            t_s: f[0].parse().unwrap_or(0.0),
            err_total: f[2].parse().unwrap_or(0),
            peak_g: f[3].parse().unwrap_or(0.0),
            nodes_present: f[4].trim().parse().unwrap_or(0),
        };
        if tx.send(row).is_err() {
            return;
        }
    }
}

/// The bench reader: the CAN socket and the accelerometer, on one thread, with a short
/// socket timeout so that the accelerometer is sampled on time whatever the bus does.
/// It never asks the engine and never touches the log.
#[cfg(feature = "live")]
fn live_reader(iface: String, iio_dir: PathBuf, sample_hz: f64, tx: Sender<Sample>) {
    use socketcan::{CanFrame, CanSocket, EmbeddedFrame, Frame, Socket, SocketOptions};

    let socket = match CanSocket::open(&iface) {
        Ok(s) => s,
        Err(e) => {
            eprintln!("e14: open {}: {}", iface, e);
            process::exit(1);
        }
    };
    if let Err(e) = socket.set_error_filter_accept_all() {
        eprintln!("e14: error frames on {} are not available: {}", iface, e);
    }
    if let Err(e) = socket.set_read_timeout(Duration::from_millis(20)) {
        eprintln!("e14: set the read timeout on {}: {}", iface, e);
    }
    let accel = match iio::Accel::open(&iio_dir) {
        Ok(a) => a,
        Err(e) => {
            eprintln!("e14: {}: {}", iio_dir.display(), e);
            process::exit(1);
        }
    };
    let start = Instant::now();
    let period = Duration::from_secs_f64(1.0 / sample_hz.max(1.0));
    let mut next_sample = start;
    let mut next_tick = start + Duration::from_secs(1);
    loop {
        if let Ok(frame) = socket.read_frame() {
            let t_s = start.elapsed().as_secs_f64();
            let sample = match frame {
                CanFrame::Error(f) => {
                    // linux/can/error.h: with CAN_ERR_CNT set the driver puts the
                    // transmit and receive error counters in data[6] and data[7]. Not
                    // every driver does; whether mcp251x does is to verify on the bench,
                    // which is why the rule reads the interface counters and not these.
                    let d = f.data();
                    Sample::BusError {
                        t_s,
                        tec: *d.get(6).unwrap_or(&0),
                        rec: *d.get(7).unwrap_or(&0),
                    }
                }
                other => Sample::Frame { t_s, id: other.raw_id() },
            };
            if tx.send(sample).is_err() {
                return;
            }
        }
        let now = Instant::now();
        if now >= next_sample {
            next_sample = now + period;
            match accel.read_peak_g() {
                Ok(g) => {
                    if tx.send(Sample::Accel { t_s: start.elapsed().as_secs_f64(), g }).is_err() {
                        return;
                    }
                }
                Err(e) => eprintln!("e14: the accelerometer stopped answering: {}", e),
            }
        }
        if now >= next_tick {
            next_tick = now + Duration::from_secs(1);
            if tx.send(Sample::Tick { t_s: start.elapsed().as_secs_f64() }).is_err() {
                return;
            }
        }
    }
}

#[cfg(not(feature = "live"))]
fn live_reader(_iface: String, _iio_dir: PathBuf, _sample_hz: f64, _tx: Sender<Sample>) {
    eprintln!(
        "e14: this binary was built without the live feature, so --live is refused.\n\
         Build the bench binary with: cargo build --release --features live,engine"
    );
    process::exit(2);
}

// ---------------------------------------------------------------- the log

struct Log {
    file: File,
}

impl Log {
    /// One writer per log, and the order of the two calls below is the guard. The file is
    /// opened WITHOUT truncate, the lock is taken, and only then is it emptied: written the
    /// obvious way round, with truncate(true) on the open as this function had until Monday
    /// 26 October 2026, the kernel empties the first writer's file before this process
    /// learns it may not have it, so the refusal arrives after the damage rather than
    /// instead of it. A launch line pasted twice put two engines on one log on Sunday 18
    /// October 2026 and cost a whole run. common/c/src/elog.c carries the full reasoning,
    /// and common/rust/edgellm/src/elog.rs the same code for the shared logger.
    fn create(path: &Path) -> io::Result<Self> {
        let mut file = OpenOptions::new().write(true).create(true).open(path)?;
        // SAFETY: flock is given a descriptor owned by `file`, which outlives this call, and
        // a flag constant. It writes nothing through the descriptor and returns 0 or -1.
        if unsafe { libc::flock(file.as_raw_fd(), libc::LOCK_EX | libc::LOCK_NB) } != 0 {
            let err = io::Error::last_os_error();
            return Err(if err.kind() == io::ErrorKind::WouldBlock {
                io::Error::new(
                    io::ErrorKind::WouldBlock,
                    format!(
                        "{} is being written by another process, so this one will not \
                         start: do not run two on one log",
                        path.display()
                    ),
                )
            } else {
                err
            });
        }
        // The lock is held, so emptying the file now can only discard this caller's own
        // earlier run and never a live one's rows.
        file.set_len(0)?;
        // The nine columns of common/c/include/elog.h, then the four this project adds
        // because they are the whole subject of the chapter.
        writeln!(
            file,
            "t_s,tag,raw,rule,model,prompt_ms,generate_ms,n_generated,engine,\
             round_trip_ms,socket_ms,daemon_rss_kb,service_rss_kb"
        )?;
        file.flush()?;
        file.sync_all()?;
        Ok(Log { file })
    }

    fn row(
        &mut self,
        m: &Minute,
        rule: Decision,
        reply: Option<&engine_client::Reply>,
    ) -> io::Result<()> {
        let raw = format!("{};{};{:.3};{}", m.err_total, m.err_delta, m.peak_g, m.nodes_present);
        write!(self.file, "{:.6},{},{},{},", m.t_s, quoted("minute"), raw, quoted(rule.name()))?;
        match reply {
            Some(r) => writeln!(
                self.file,
                "{},{:.3},{:.3},{},{},{:.3},{:.3},{},{}",
                quoted(&r.text),
                r.prompt_ms,
                r.generate_ms,
                r.n_generated,
                quoted(&r.engine),
                r.round_trip_ms,
                r.socket_ms(),
                r.daemon_rss_kb,
                rss_kb()
            )?,
            None => writeln!(
                self.file,
                "\"\",,,,{},,,,{}",
                quoted("unavailable"),
                rss_kb()
            )?,
        }
        // Out of this process and onto the card. A flush is enough for kill -9 and not for a
        // board that restarts; the sync is after the row, so it cannot move any timing the
        // row carries. Measured at 6.594 ms on the bench board's SD card.
        self.file.flush()?;
        self.file.sync_all()
    }
}

/// The CSV quoting of common/c/src/elog.c: every text field is quoted, an inner quote is
/// doubled and a newline becomes a space, so a sentence with a comma is one field.
fn quoted(text: &str) -> String {
    let mut out = String::with_capacity(text.len() + 2);
    out.push('"');
    for c in text.chars() {
        match c {
            '"' => out.push_str("\"\""),
            '\n' | '\r' => out.push(' '),
            other => out.push(other),
        }
    }
    out.push('"');
    out
}

fn rss_kb() -> i64 {
    let text = match std::fs::read_to_string("/proc/self/status") {
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

// ---------------------------------------------------------------- the aggregator

/// The user text one minute becomes. It names the node table, the counter and its rise,
/// and the peak against its threshold, and nothing else: the thresholds are in the
/// system prompt the daemon decoded once, so they cost no tokens per minute.
fn format_user_text(m: &Minute, nodes: &str) -> String {
    format!(
        "nodes {}. CAN error counter {}, up {} this minute. vibration peak {:.2} g, \
         threshold {:.2} g. Decide.",
        nodes, m.err_total, m.err_delta, m.peak_g, rules::PEAK_G
    )
}

struct Open {
    start_t: f64,
    peak_g: f64,
    last_seen: BTreeMap<u32, f64>,
    last_tec: u8,
    last_rec: u8,
    error_frames: u64,
}

impl Open {
    fn new(start_t: f64) -> Self {
        Open {
            start_t,
            peak_g: 0.0,
            last_seen: BTreeMap::new(),
            last_tec: 0,
            last_rec: 0,
            error_frames: 0,
        }
    }

    fn nodes_present(&self, now: f64, on_vcan: bool) -> u32 {
        NODE_TABLE
            .iter()
            .filter(|row| {
                let period = row.period_ms as f64 / 1000.0;
                let missed = match self.last_seen.get(&row.id) {
                    Some(seen) => ((now - seen) / period).floor().max(0.0) as u32,
                    None => u32::MAX,
                };
                rules::node_state(missed, on_vcan) == NodeState::Present
            })
            .count() as u32
    }
}

fn main() {
    let o = parse_args();
    let (tx, rx) = mpsc::channel::<Sample>();
    let live_iface = o.live.clone();
    let on_vcan = live_iface.as_deref().map(|n| n.starts_with("vcan")).unwrap_or(false);

    match (&o.replay, &o.live) {
        (Some(path), _) => {
            let path = path.clone();
            thread::spawn(move || replay_reader(path, tx));
        }
        (None, Some(iface)) => {
            let iface = iface.clone();
            let dir = o.iio.clone().unwrap_or_default();
            let hz = o.sample_hz;
            thread::spawn(move || live_reader(iface, dir, hz, tx));
        }
        _ => usage(),
    }

    let mut log = match Log::create(&o.log) {
        Ok(l) => l,
        Err(e) => {
            eprintln!("e14: {}: {}", o.log.display(), e);
            process::exit(1);
        }
    };
    let mut client = EngineClient::new(&o.socket, Duration::from_millis(o.timeout_ms));
    let mut previous_total: Option<u64> = None;
    let mut open = Open::new(0.0);
    let (mut minutes, mut inspect) = (0u64, 0u64);

    loop {
        match rx.recv_timeout(Duration::from_millis(500)) {
            Ok(Sample::Row { t_s, err_total, peak_g, nodes_present }) => {
                let m = close_minute(&mut previous_total, t_s, err_total, peak_g, nodes_present);
                let nodes = format!("{} of {} present", nodes_present, NODE_TABLE.len());
                decide_and_log(&mut log, &mut client, &m, &nodes, &mut inspect);
                minutes += 1;
            }
            Ok(Sample::BusError { t_s, tec, rec }) => {
                open.error_frames += 1;
                open.last_tec = tec;
                open.last_rec = rec;
                let _ = t_s;
            }
            Ok(Sample::Frame { t_s, id }) => {
                open.last_seen.insert(id, t_s);
            }
            Ok(Sample::Accel { t_s, g }) => {
                let _ = t_s;
                if g > open.peak_g {
                    open.peak_g = g;
                }
            }
            Ok(Sample::Tick { t_s }) => {
                if t_s - open.start_t >= o.minute_s {
                    let iface = live_iface.clone().unwrap_or_default();
                    let total = interface_err_total(&iface).unwrap_or_else(|e| {
                        eprintln!("e14: the counters of {} are unreadable: {}", iface, e);
                        previous_total.unwrap_or(0)
                    });
                    let present = open.nodes_present(t_s, on_vcan);
                    let start_t = open.start_t;
                    let peak = open.peak_g;
                    let m = close_minute(&mut previous_total, start_t, total, peak, present);
                    let nodes = format!(
                        "{} of {} present, TEC {}, REC {}, {} error frames",
                        present,
                        NODE_TABLE.len(),
                        open.last_tec,
                        open.last_rec,
                        open.error_frames
                    );
                    decide_and_log(&mut log, &mut client, &m, &nodes, &mut inspect);
                    minutes += 1;
                    open = Open::new(t_s);
                }
            }
            Err(RecvTimeoutError::Timeout) => continue,
            Err(RecvTimeoutError::Disconnected) => break,
        }
    }

    let (reconnects, unavailable) = client.counters();
    println!(
        "e14: {} minutes, {} INSPECT, {} OK; engine reconnects {}, minutes without an \
         answer {}; log {}",
        minutes,
        inspect,
        minutes - inspect,
        reconnects,
        unavailable,
        o.log.display()
    );
}

/// The cumulative counter becomes a rise here, in one place, for the replay path and the
/// bench path alike.
fn close_minute(
    previous_total: &mut Option<u64>,
    t_s: f64,
    err_total: u64,
    peak_g: f64,
    nodes_present: u32,
) -> Minute {
    let delta = match *previous_total {
        Some(p) => rules::err_delta(p, err_total),
        None => 0, /* the first minute has nothing to be a rise over */
    };
    *previous_total = Some(err_total);
    Minute { t_s, err_total, err_delta: delta, peak_g, nodes_present }
}

fn decide_and_log(
    log: &mut Log,
    client: &mut EngineClient,
    m: &Minute,
    nodes: &str,
    inspect: &mut u64,
) {
    let rule = rules::decide(m);
    if rule == Decision::Inspect {
        *inspect += 1;
    }
    let request = format!("decide\t{}", format_user_text(m, nodes));
    let outcome = client.ask(&request);
    let written = match &outcome {
        Ok(reply) => log.row(m, rule, Some(reply)),
        Err(e) => {
            eprintln!("e14: minute {:.0}: {}", m.t_s, e);
            log.row(m, rule, None)
        }
    };
    if let Err(e) = written {
        eprintln!("e14: the log refused a row: {}", e);
    }
}

#[cfg(test)]
mod tests {
    use super::Log;

    /// The one-writer guard, made to refuse rather than assumed to work. This project keeps
    /// its own Log rather than using the shared crate's Elog, because it writes four columns
    /// the shared one does not, so the guard is duplicated and therefore has to be tested
    /// twice. The middle assertion is the regression: the refusal must cost the first writer
    /// nothing, which holds only because the lock is taken before set_len(0).
    #[test]
    fn a_second_writer_on_one_log_is_refused_and_the_first_keeps_its_rows() {
        let path = std::env::temp_dir().join(format!("e14_log_guard_{}.csv", std::process::id()));
        let _ = std::fs::remove_file(&path);

        let first = Log::create(&path).expect("a fresh path opens");
        let before = std::fs::read(&path).expect("the header reached the file");
        assert!(!before.is_empty(), "the header reached the file");

        // Matched rather than unwrap_err()'d: that method panics by printing the Ok value,
        // so it requires Log: Debug, which a struct holding an open file has no business
        // deriving for the sake of one test.
        let message = match Log::create(&path) {
            Ok(_) => panic!("a second writer on one log must be refused"),
            Err(e) => e.to_string(),
        };
        assert!(
            message.contains("do not run two on one log"),
            "the refusal must name the live writer, got: {}",
            message
        );
        assert_eq!(
            std::fs::read(&path).expect("the first log is still readable"),
            before,
            "the refusal must not touch the first writer's rows"
        );

        // A finished log stays replaceable, which the units that carry Restart= depend on.
        drop(first);
        assert!(Log::create(&path).is_ok(), "a closed log must be reopenable");
        let _ = std::fs::remove_file(&path);
    }
}
