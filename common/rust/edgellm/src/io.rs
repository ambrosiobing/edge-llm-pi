//! io: the one boundary between a project's loop and the hardware, as io.h is for C.
//!
//! A project opens exactly one source: a replayed CSV during development and in CI, or the
//! live device on the bench. The loop is identical in both cases, which is what "ready for
//! board wiring" means. Two backends live here: `Replay`, the same CSV form as
//! common/c/src/io_replay.c, and `Iio`, the sysfs face of an IIO device polled at an
//! interval, the same attribute scan as common/c/src/io_iio.c. e13 adds SocketCAN beside
//! them; the backend names stay the ones the C tree uses.

use std::fs::{self, File};
use std::io::{BufRead, BufReader, Lines};
use std::path::{Path, PathBuf};
use std::thread;
use std::time::{Duration, Instant};

use crate::{err, Error};

/// How many values an event may carry, as IO_MAX_VALUES in io.h.
/// One serial line over termios, behind the "serial" feature so that the projects
/// with no serial line do not take libc. e18 is the only one that enables it.
#[cfg(feature = "serial")]
pub mod serial;

pub const IO_MAX_VALUES: usize = 16;

/// One event: a timestamp, a tag naming the sensor or channel, and up to IO_MAX_VALUES
/// numbers whose meaning the project's README fixes.
#[derive(Debug, Clone, Default, PartialEq)]
pub struct Event {
    /// Seconds: the CSV column on replay, seconds since the source opened when live.
    pub t_s: f64,
    /// "adxl345", "can0", "synthetic", ...
    pub tag: String,
    pub v: Vec<f64>,
}

impl Event {
    pub fn new(t_s: f64, tag: &str, v: &[f64]) -> Event {
        Event { t_s, tag: tag.to_string(), v: v.to_vec() }
    }
}

/// A source of events. `next` blocks until the next event, returns Ok(None) at the end of
/// a replay, and Err on a read error, which is how an open contact on the bench shows up.
pub trait IoSource {
    fn next(&mut self) -> Result<Option<Event>, Error>;

    /// An output the rule, never the model, drives. The default refuses: most backends
    /// have no output, and a refusal is counted by the loop as an actuation error.
    fn actuate(&mut self, what: &str, _value: i32) -> Result<(), Error> {
        err(format!("io: backend {} drives no output ({})", self.backend_name(), what))
    }

    fn backend_name(&self) -> &'static str;
}

/// A CSV stands in for the hardware. The header must start with "t_s,tag" and may
/// continue with v0,v1,...; every later line is one event; blank lines and lines starting
/// with '#' are skipped. `actuate` records the call and succeeds, so a rule's action is
/// accepted here exactly as it is on the bench.
pub struct Replay {
    lines: Lines<BufReader<File>>,
    line_no: u64,
    actuations: u64,
}

impl Replay {
    pub fn open(path: impl AsRef<Path>) -> Result<Replay, Error> {
        let path = path.as_ref();
        let file = File::open(path)
            .map_err(|e| Error(format!("replay: cannot open {}: {}", path.display(), e)))?;
        let mut lines = BufReader::new(file).lines();
        let header = match lines.next() {
            Some(Ok(h)) => h,
            _ => return err(format!("replay: {} is empty", path.display())),
        };
        if !header.trim_end_matches(['\r', '\n']).starts_with("t_s,tag") {
            return err(format!("replay: {} header must start with t_s,tag", path.display()));
        }
        Ok(Replay { lines, line_no: 1, actuations: 0 })
    }

    /// How many times a rule actuated through this replay.
    pub fn actuations(&self) -> u64 {
        self.actuations
    }

    /// One data line to an event; Ok(None) for a blank line or a comment.
    pub fn parse_line(line: &str, line_no: u64) -> Result<Option<Event>, Error> {
        let line = line.trim_end_matches(['\r', '\n']);
        if line.is_empty() || line.starts_with('#') {
            return Ok(None);
        }
        let mut fields = line.split(',');
        let t_s = match fields.next().and_then(|f| f.parse::<f64>().ok()) {
            Some(t) => t,
            None => return err(format!("replay: line {}: t_s is not a number", line_no)),
        };
        let tag = match fields.next() {
            Some(t) => t.to_string(),
            None => return err(format!("replay: line {}: no tag column", line_no)),
        };
        let mut v = Vec::new();
        for field in fields {
            if field.is_empty() || v.len() >= IO_MAX_VALUES {
                break;
            }
            match field.parse::<f64>() {
                Ok(x) => v.push(x),
                Err(_) => {
                    return err(format!("replay: line {}: v{} is not a number", line_no, v.len()))
                }
            }
        }
        Ok(Some(Event { t_s, tag, v }))
    }
}

impl IoSource for Replay {
    fn next(&mut self) -> Result<Option<Event>, Error> {
        loop {
            let line = match self.lines.next() {
                None => return Ok(None),
                Some(Err(e)) => return err(format!("replay: read error: {}", e)),
                Some(Ok(l)) => l,
            };
            self.line_no += 1;
            if let Some(ev) = Replay::parse_line(&line, self.line_no)? {
                return Ok(Some(ev));
            }
        }
    }

    fn actuate(&mut self, _what: &str, _value: i32) -> Result<(), Error> {
        self.actuations += 1;
        Ok(())
    }

    fn backend_name(&self) -> &'static str {
        "replay"
    }
}

/// Splits "<device dir>[:interval_s]". Because the directory name itself carries a ':'
/// (iio:device0), only a tail that parses as a positive number is taken as the interval.
pub fn split_interval(spec: &str, default_s: f64) -> (String, f64) {
    if let Some((head, tail)) = spec.rsplit_once(':') {
        if let Ok(v) = tail.trim().parse::<f64>() {
            if v > 0.0 {
                return (head.to_string(), v);
            }
        }
    }
    (spec.to_string(), default_s)
}

fn read_number(dir: &Path, attr: &str) -> Option<f64> {
    let text = fs::read_to_string(dir.join(attr)).ok()?;
    let first = text.lines().next()?.trim();
    // strtod takes the leading number; "0.038300\n" and "100.000000" both parse whole.
    first.parse::<f64>().ok()
}

/// "in_accel_x_raw" takes "in_accel_x_scale" when it exists, otherwise "in_accel_scale",
/// otherwise 1.
fn find_scale(dir: &Path, raw: &str) -> f64 {
    let stem = raw.strip_suffix("_raw").unwrap_or(raw);
    if let Some(v) = read_number(dir, &format!("{}_scale", stem)) {
        return v;
    }
    if let Some((shared, _)) = stem.rsplit_once('_') {
        if !shared.is_empty() {
            if let Some(v) = read_number(dir, &format!("{}_scale", shared)) {
                return v;
            }
        }
    }
    1.0
}

/// The sysfs face of an IIO device, polled at an interval. Spec "<device dir>[:interval_s]",
/// for example "/sys/bus/iio/devices/iio:device0:0.02". At open the directory is scanned for
/// every "in_*_raw" attribute, sorted by name, up to IO_MAX_VALUES of them; each read
/// multiplies the raw value by the matching scale, so an accelerometer's values arrive in
/// the kernel's unit, metres per second squared. The tag is the device's "name" attribute
/// or the directory basename. The value order is the sorted attribute order; a project's
/// README prints it. The device's sampling_frequency, when readable, is kept for the sample
/// rate check; the polling here is independent of it.
pub struct Iio {
    dir: PathBuf,
    tag: String,
    raw: Vec<(String, f64)>,
    interval: Duration,
    t0: Instant,
    next_due: Instant,
    sampling_frequency: Option<f64>,
}

impl Iio {
    pub const DEFAULT_INTERVAL_S: f64 = 0.1;

    pub fn open(spec: &str) -> Result<Iio, Error> {
        let (dir, interval_s) = split_interval(spec, Iio::DEFAULT_INTERVAL_S);
        let dir = PathBuf::from(dir);
        let entries = fs::read_dir(&dir).map_err(|_| Error(format!("iio: cannot scan {}", spec)))?;
        let mut names: Vec<String> = entries
            .filter_map(|e| e.ok())
            .map(|e| e.file_name().to_string_lossy().into_owned())
            .filter(|n| n.starts_with("in_") && n.len() > 7 && n.ends_with("_raw"))
            .collect();
        names.sort();
        names.truncate(IO_MAX_VALUES);
        if names.is_empty() {
            return err(format!("iio: no in_*_raw attribute under {}", dir.display()));
        }
        let raw: Vec<(String, f64)> =
            names.into_iter().map(|n| { let s = find_scale(&dir, &n); (n, s) }).collect();
        let tag = match fs::read_to_string(dir.join("name")) {
            Ok(t) if !t.trim().is_empty() => t.trim().to_string(),
            _ => dir
                .file_name()
                .map(|n| n.to_string_lossy().into_owned())
                .unwrap_or_else(|| dir.display().to_string()),
        };
        let sampling_frequency = read_number(&dir, "sampling_frequency")
            .or_else(|| read_number(&dir, "in_accel_sampling_frequency"));
        let now = Instant::now();
        Ok(Iio {
            dir,
            tag,
            raw,
            interval: Duration::from_secs_f64(interval_s),
            t0: now,
            next_due: now,
            sampling_frequency,
        })
    }

    /// The attribute names in the order the values are delivered.
    pub fn attributes(&self) -> Vec<&str> {
        self.raw.iter().map(|(n, _)| n.as_str()).collect()
    }

    /// The scale applied to each attribute, in the same order.
    pub fn scales(&self) -> Vec<f64> {
        self.raw.iter().map(|(_, s)| *s).collect()
    }

    /// The device's own sampling_frequency attribute, when it exists and parses.
    pub fn sampling_frequency(&self) -> Option<f64> {
        self.sampling_frequency
    }

    pub fn interval(&self) -> Duration {
        self.interval
    }

    pub fn tag(&self) -> &str {
        &self.tag
    }
}

impl IoSource for Iio {
    fn next(&mut self) -> Result<Option<Event>, Error> {
        let now = Instant::now();
        if now < self.next_due {
            thread::sleep(self.next_due - now);
        }
        let now = Instant::now();
        self.next_due = now + self.interval;
        let mut v = Vec::with_capacity(self.raw.len());
        for (attr, scale) in &self.raw {
            match read_number(&self.dir, attr) {
                Some(raw) => v.push(raw * scale),
                None => {
                    return err(format!("iio: cannot read {}/{}", self.dir.display(), attr));
                }
            }
        }
        Ok(Some(Event { t_s: (now - self.t0).as_secs_f64(), tag: self.tag.clone(), v }))
    }

    fn backend_name(&self) -> &'static str {
        "iio"
    }
}

/// Opens a live backend by name. Only the backends compiled into this crate are accepted;
/// the names are the ones the C tree uses, so a project.toml reads the same in either.
pub fn open_live(backend: &str, spec: &str) -> Result<Box<dyn IoSource>, Error> {
    match backend {
        "iio" => Ok(Box::new(Iio::open(spec)?)),
        other => err(format!("io: backend {} is not in this crate (iio is)", other)),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;

    fn scratch(name: &str) -> PathBuf {
        let dir = std::env::temp_dir().join(format!("edgellm_io_{}_{}", std::process::id(), name));
        let _ = fs::remove_dir_all(&dir);
        fs::create_dir_all(&dir).unwrap();
        dir
    }

    #[test]
    fn split_interval_keeps_the_colon_of_the_device_name() {
        assert_eq!(
            split_interval("/sys/bus/iio/devices/iio:device0:0.02", 0.1),
            ("/sys/bus/iio/devices/iio:device0".to_string(), 0.02)
        );
        assert_eq!(
            split_interval("/sys/bus/iio/devices/iio:device0", 0.1),
            ("/sys/bus/iio/devices/iio:device0".to_string(), 0.1)
        );
    }

    #[test]
    fn a_replay_line_parses_like_the_c_backend() {
        let ev = Replay::parse_line("1.5,adxl345,0.1,-0.2,9.81", 2).unwrap().unwrap();
        assert_eq!(ev, Event::new(1.5, "adxl345", &[0.1, -0.2, 9.81]));
        assert!(Replay::parse_line("# comment", 3).unwrap().is_none());
        assert!(Replay::parse_line("", 4).unwrap().is_none());
        assert!(Replay::parse_line("x,adxl345,1", 5).is_err());
        assert!(Replay::parse_line("1.0,adxl345,1,abc", 6).is_err());
        let short = Replay::parse_line("2.0,adxl345,1,,3", 7).unwrap().unwrap();
        assert_eq!(short.v, vec![1.0]);
    }

    #[test]
    fn a_replay_file_is_read_to_its_end() {
        let dir = scratch("replay");
        let path = dir.join("r.csv");
        let mut f = File::create(&path).unwrap();
        writeln!(f, "t_s,tag,v0,v1,v2").unwrap();
        writeln!(f, "0.0,adxl345,0,0,9.8").unwrap();
        writeln!(f, "").unwrap();
        writeln!(f, "0.02,adxl345,0,0,9.9").unwrap();
        drop(f);
        let mut r = Replay::open(&path).unwrap();
        assert_eq!(r.next().unwrap().unwrap().t_s, 0.0);
        assert_eq!(r.next().unwrap().unwrap().v[2], 9.9);
        assert!(r.next().unwrap().is_none());
        assert!(r.actuate("led:0", 1).is_ok());
        assert_eq!(r.actuations(), 1);
        assert_eq!(r.backend_name(), "replay");
        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_replay_with_the_wrong_header_is_refused() {
        let dir = scratch("header");
        let path = dir.join("bad.csv");
        fs::write(&path, "time,value\n1,2\n").unwrap();
        assert!(Replay::open(&path).is_err());
        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_fake_sysfs_device_is_scanned_scaled_and_read() {
        let dir = scratch("iio");
        let dev = dir.join("iio:device0");
        fs::create_dir_all(&dev).unwrap();
        fs::write(dev.join("name"), "adxl345\n").unwrap();
        fs::write(dev.join("in_accel_x_raw"), "0\n").unwrap();
        fs::write(dev.join("in_accel_y_raw"), "-26\n").unwrap();
        fs::write(dev.join("in_accel_z_raw"), "256\n").unwrap();
        fs::write(dev.join("in_accel_scale"), "0.038300\n").unwrap();
        fs::write(dev.join("sampling_frequency"), "100.000000\n").unwrap();
        fs::write(dev.join("in_voltage0_raw"), "5\n").unwrap();
        let spec = format!("{}:0.001", dev.display());
        let mut iio = Iio::open(&spec).unwrap();
        assert_eq!(iio.tag(), "adxl345");
        assert_eq!(iio.attributes(),
                   vec!["in_accel_x_raw", "in_accel_y_raw", "in_accel_z_raw", "in_voltage0_raw"]);
        assert_eq!(iio.scales(), vec![0.0383, 0.0383, 0.0383, 1.0]);
        assert_eq!(iio.sampling_frequency(), Some(100.0));
        let ev = iio.next().unwrap().unwrap();
        assert_eq!(ev.tag, "adxl345");
        assert_eq!(ev.v.len(), 4);
        assert!((ev.v[2] - 256.0 * 0.0383).abs() < 1e-9);
        assert_eq!(ev.v[3], 5.0);
        let ev2 = iio.next().unwrap().unwrap();
        assert!(ev2.t_s >= ev.t_s + 0.001);
        assert!(iio.actuate("led:0", 1).is_err());
        fs::remove_file(dev.join("in_accel_y_raw")).unwrap();
        assert!(iio.next().is_err(), "a missing attribute is a read error, never a wrong value");
        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn an_unknown_backend_is_refused() {
        assert!(open_live("can", "can0").is_err());
    }
}
