//! elog: the per-event CSV, one row per event, flushed after every row so that a process
//! stopped with kill -9 leaves every decision it took on disk. The nine columns are the
//! ones common/c/src/elog.c writes and tools/agreement.py reads, byte for byte:
//!
//!   t_s,tag,raw,rule,model,prompt_ms,generate_ms,n_generated,engine
//!
//! raw joins the values with ';' in C's %.6g; the tag, rule, model and engine columns are
//! quoted CSV-style (doubled inner quotes, newlines replaced by spaces) because a sentence
//! may carry commas.

use std::fs::File;
use std::io::Write;
use std::path::Path;

use crate::io::Event;
use crate::{Decision, Error};

pub const HEADER: &str = "t_s,tag,raw,rule,model,prompt_ms,generate_ms,n_generated,engine";

pub struct Elog {
    file: File,
    engine: String,
}

impl Elog {
    pub fn open(path: impl AsRef<Path>, engine_version: &str) -> Result<Elog, Error> {
        let path = path.as_ref();
        let mut file = File::create(path)
            .map_err(|e| Error(format!("cannot write {}: {}", path.display(), e)))?;
        file.write_all(HEADER.as_bytes())
            .and_then(|_| file.write_all(b"\n"))
            .and_then(|_| file.flush())
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
