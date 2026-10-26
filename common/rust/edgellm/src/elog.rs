//! elog: the per-event CSV, one row per event. The nine columns are the ones
//! common/c/src/elog.c writes and tools/agreement.py reads, byte for byte:
//!
//!   t_s,tag,raw,rule,model,prompt_ms,generate_ms,n_generated,engine
//!
//! raw joins the values with ';' in C's %.6g; the tag, rule, model and engine columns are
//! quoted CSV-style (doubled inner quotes, newlines replaced by spaces) because a sentence
//! may carry commas.
//!
//! THE SAME SHAPE MEANS THE SAME DURABILITY AND THE SAME EXCLUSIVITY, which this file did
//! not have until Monday 26 October 2026: it flushed after every row and nothing more. The
//! C logger gained both on Tuesday 20 October 2026 and the Python one the same day; this
//! was the last of the four left out, because the obvious tool for it, File::try_lock, is
//! stable only since Rust 1.89 while the image's cargo is scarthgap's 1.75. libc::flock
//! carries no such floor. The reasoning is written out at length in common/c/src/elog.c
//! rather than repeated here, and in short it is:
//!
//! - sync_all and not only flush. A flush moves bytes out of this process into the kernel,
//!   which is enough for kill -9 and not enough for a board that restarts. On Saturday 17
//!   October 2026 that cost about five finished rows out of a log whose recorded size
//!   counted them. The sync is 6.594 ms a row on the bench board's SD card, measured, and
//!   under 0.3 percent of a decision everywhere in this volume.
//!
//! - one writer per log, and LOCK BEFORE TRUNCATE. The file is opened without truncating,
//!   the lock is taken, and only then is it emptied with set_len(0). File::create, which
//!   this module used, empties the first writer's file before this process learns it may
//!   not have it, so the refusal would arrive after the damage rather than instead of it.
//!   A launch line pasted twice once put two engines on one log and cost a whole run.
//!
//! A log left by a run that has FINISHED stays replaceable, so the 16 units in this volume
//! that carry Restart= with a fixed --log path reopen their own log normally.

use std::fs::{File, OpenOptions};
use std::io::Write;
use std::os::unix::io::AsRawFd;
use std::path::Path;

use crate::io::Event;
use crate::{Decision, Error};

pub const HEADER: &str = "t_s,tag,raw,rule,model,prompt_ms,generate_ms,n_generated,engine";

pub struct Elog {
    file: File,
    engine: String,
}

/// An exclusive, non-blocking advisory lock on an open log, or an error naming which of the
/// two cases it is: another live process holding the path, or the lock call itself failing.
fn lock_exclusive(file: &File, path: &Path) -> Result<(), Error> {
    // SAFETY: flock is given a descriptor owned by `file`, which outlives this call, and a
    // flag constant. It writes nothing through the descriptor and returns 0 or -1.
    let rc = unsafe { libc::flock(file.as_raw_fd(), libc::LOCK_EX | libc::LOCK_NB) };
    if rc == 0 {
        return Ok(());
    }
    let err = std::io::Error::last_os_error();
    if err.kind() == std::io::ErrorKind::WouldBlock {
        return Err(Error(format!(
            "{} is being written by another process, so this one will not start: do not run \
             two on one log",
            path.display()
        )));
    }
    Err(Error(format!("cannot lock {}: {}", path.display(), err)))
}

impl Elog {
    pub fn open(path: impl AsRef<Path>, engine_version: &str) -> Result<Elog, Error> {
        let path = path.as_ref();
        // No truncate on the open: the lock comes first and the file is emptied after, which
        // is the order the header explains.
        let mut file = OpenOptions::new()
            .write(true)
            .create(true)
            .open(path)
            .map_err(|e| Error(format!("cannot write {}: {}", path.display(), e)))?;
        lock_exclusive(&file, path)?;
        file.set_len(0)
            .map_err(|e| Error(format!("cannot empty {}: {}", path.display(), e)))?;
        file.write_all(HEADER.as_bytes())
            .and_then(|_| file.write_all(b"\n"))
            .and_then(|_| file.flush())
            .and_then(|_| file.sync_all())
            .map_err(|e| Error(format!("log write failed: {}", e)))?;
        Ok(Elog { file, engine: engine_version.to_string() })
    }

    /// One row. `model` None writes an empty model column and empty timings, which is how
    /// a row where the rule ran alone looks.
    pub fn row(&mut self, ev: &Event, rule: &str, model: Option<&Decision>) -> Result<(), Error> {
        let line = format_row(ev, rule, model, &self.engine);
        self.file
            .write_all(line.as_bytes())
            .and_then(|_| self.file.flush())
            .and_then(|_| self.file.sync_all())
            .map_err(|e| Error(format!("log write failed: {}", e)))
    }
}

/// The row as text, newline included; separated out so a test can read it without a file.
pub fn format_row(ev: &Event, rule: &str, model: Option<&Decision>, engine: &str) -> String {
    let raw: Vec<String> = ev.v.iter().map(|x| format_g6(*x)).collect();
    let mut line =
        format!("{:.6},{},{},{},", ev.t_s, quoted(&ev.tag), raw.join(";"), quoted(rule));
    match model {
        Some(m) => line.push_str(&format!(
            "{},{:.3},{:.3},{},",
            quoted(&m.text),
            m.prompt_ms,
            m.generate_ms,
            m.n_generated
        )),
        None => line.push_str("\"\",,,,"),
    }
    line.push_str(&quoted(engine));
    line.push('\n');
    line
}

/// CSV quoting as elog.c does it: doubled inner quotes, newlines to spaces.
pub fn quoted(text: &str) -> String {
    let mut out = String::with_capacity(text.len() + 2);
    out.push('"');
    for c in text.chars() {
        match c {
            '"' => out.push_str("\"\""),
            '\n' | '\r' => out.push(' '),
            c => out.push(c),
        }
    }
    out.push('"');
    out
}

/// C's "%.6g": six significant digits, trailing zeros dropped, exponent form when the
/// exponent is below -4 or at least 6. The raw column must read the same from a Rust log
/// and a C log.
pub fn format_g6(v: f64) -> String {
    if v == 0.0 {
        return "0".to_string();
    }
    if !v.is_finite() {
        return if v.is_nan() { "nan".to_string() } else if v > 0.0 { "inf".to_string() }
               else { "-inf".to_string() };
    }
    let sci = format!("{:.5e}", v);
    let (mant, exp) = match sci.split_once('e') {
        Some(parts) => parts,
        None => return sci,
    };
    let exp: i32 = exp.parse().unwrap_or(0);
    if exp < -4 || exp >= 6 {
        let sign = if exp < 0 { '-' } else { '+' };
        format!("{}e{}{:02}", trim_zeros(mant), sign, exp.abs())
    } else {
        let decimals = (5 - exp).max(0) as usize;
        trim_zeros(&format!("{:.*}", decimals, v))
    }
}

fn trim_zeros(s: &str) -> String {
    if s.contains('.') {
        s.trim_end_matches('0').trim_end_matches('.').to_string()
    } else {
        s.to_string()
    }
}

/// Splits one CSV line into fields the way tests/oracle_check.c does: a field may be
/// quoted, with doubled inner quotes; the trailing newline is dropped.
pub fn split_csv_line(line: &str) -> Vec<String> {
    let line = line.trim_end_matches(['\r', '\n']);
    let mut fields = Vec::new();
    let mut cur = String::new();
    let mut in_quotes = false;
    let mut chars = line.chars().peekable();
    while let Some(c) = chars.next() {
        if in_quotes {
            if c == '"' && chars.peek() == Some(&'"') {
                cur.push('"');
                chars.next();
            } else if c == '"' {
                in_quotes = false;
            } else {
                cur.push(c);
            }
        } else if c == '"' {
            in_quotes = true;
        } else if c == ',' {
            fields.push(std::mem::take(&mut cur));
        } else {
            cur.push(c);
        }
    }
    fields.push(cur);
    fields
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn g6_matches_the_c_library() {
        let cases: &[(f64, &str)] = &[
            (0.0, "0"),
            (37.01, "37.01"),
            (-0.05, "-0.05"),
            (100.0, "100"),
            (9.80665, "9.80665"),
            (9.806651, "9.80665"),
            (0.0383, "0.0383"),
            (0.0001234, "0.0001234"),
            (0.00001234, "1.234e-05"),
            (123456.0, "123456"),
            (1234567.0, "1.23457e+06"),
            (999999.5, "1e+06"),
            (-26.0, "-26"),
        ];
        for (v, want) in cases {
            assert_eq!(format_g6(*v), *want, "%.6g of {}", v);
        }
    }

    #[test]
    fn a_row_has_the_nine_columns_of_elog_c() {
        let ev = Event::new(1.5, "adxl345", &[0.0, -0.26, 9.81]);
        let d = Decision {
            text: "{\"decision\":\"NONE\",\"why\":\"steady, 1 g.\"}".to_string(),
            n_prompt_tokens: 100,
            n_generated: 12,
            prompt_ms: 812.5,
            generate_ms: 1300.25,
            stub: false,
        };
        let row = format_row(&ev, "NONE", Some(&d), "llama-cpp-2 0.1.159");
        assert_eq!(
            row,
            "1.500000,\"adxl345\",0;-0.26;9.81,\"NONE\",\
             \"{\"\"decision\"\":\"\"NONE\"\",\"\"why\"\":\"\"steady, 1 g.\"\"}\",\
             812.500,1300.250,12,\"llama-cpp-2 0.1.159\"\n"
        );
        let alone = format_row(&ev, "TAP", None, "stub");
        assert_eq!(alone, "1.500000,\"adxl345\",0;-0.26;9.81,\"TAP\",\"\",,,,\"stub\"\n");
        let fields = split_csv_line(&row);
        assert_eq!(fields.len(), 9);
        assert_eq!(fields[3], "NONE");
        assert_eq!(fields[4], d.text);
        assert_eq!(fields[8], "llama-cpp-2 0.1.159");
        assert_eq!(split_csv_line(HEADER)[4], "model");
    }

    #[test]
    fn quoting_doubles_quotes_and_flattens_newlines() {
        assert_eq!(quoted("a\"b\nc"), "\"a\"\"b c\"");
    }

    #[test]
    fn a_second_writer_on_one_log_is_refused_and_the_first_keeps_its_rows() {
        // Made to refuse rather than assumed to work, which is this volume's rule and the
        // reason the C and Python loggers carry the same three cases. The middle assertion
        // is the regression: the refusal must cost the first writer nothing, which holds
        // only because the lock comes before set_len(0). File::create, which this module
        // used until Monday 26 October 2026, empties first and refuses afterwards.
        let path =
            std::env::temp_dir().join(format!("edgellm_elog_guard_{}.csv", std::process::id()));
        let _ = std::fs::remove_file(&path);
        let mut first = Elog::open(&path, "stub").unwrap();
        first.row(&Event::new(0.0, "t", &[1.0]), "A", None).unwrap();
        let before = std::fs::read(&path).unwrap();

        // Matched rather than unwrap_err()'d: that method panics by printing the Ok value,
        // so it requires Elog: Debug, which an open file handle has no business deriving.
        let message = match Elog::open(&path, "stub") {
            Ok(_) => panic!("a second writer on one log must be refused"),
            Err(e) => e.0,
        };
        assert!(
            message.contains("do not run two on one log"),
            "the refusal must name the live writer, got: {}",
            message
        );
        assert_eq!(
            std::fs::read(&path).unwrap(),
            before,
            "the refusal must not touch the first writer's rows"
        );

        // A finished log stays replaceable: 16 units in this volume carry Restart= with a
        // fixed --log path and have to reopen their own log. Asserted so that narrowing the
        // guard later fails here first.
        drop(first);
        let reopened = Elog::open(&path, "stub");
        assert!(reopened.is_ok(), "a closed log must be reopenable");
        drop(reopened);
        let _ = std::fs::remove_file(&path);
    }

    #[test]
    fn a_log_file_is_written_and_flushed_per_row() {
        let path = std::env::temp_dir().join(format!("edgellm_elog_{}.csv", std::process::id()));
        {
            let mut log = Elog::open(&path, "stub").unwrap();
            log.row(&Event::new(0.0, "t", &[1.0]), "A", None).unwrap();
            let text = std::fs::read_to_string(&path).unwrap();
            assert_eq!(text.lines().count(), 2, "the row is on disk before the log is closed");
        }
        let text = std::fs::read_to_string(&path).unwrap();
        assert!(text.starts_with(HEADER));
        let _ = std::fs::remove_file(&path);
    }
}
