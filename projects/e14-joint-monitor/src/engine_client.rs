// engine_client.rs: the service's side of the process boundary that is the whole point
// of project e14. Project e13 asks the model in its own process; this service asks a
// separate daemon over a Unix domain socket, and nothing else about the two differs.
//
// The protocol is one line of tab separated fields in each direction, because the
// service must never need a parser to read an answer it is going to log verbatim:
//
//   request   decide<TAB><one line of user text, no tab and no newline>
//   request   rss
//   reply     OK<TAB>answer<TAB>prompt_ms<TAB>generate_ms<TAB>n_generated<TAB>engine<TAB>rss_kb
//   reply     ERR<TAB>reason
//
// The answer field holds the grammar constrained JSON object exactly as the engine
// produced it, and the log writes that field unchanged, so the agreement tool reads the
// same bytes it reads for every other project of the volume.
//
// Two properties this file exists to guarantee:
//
//   1. Every read and every write is bounded by the timeout the service was started
//      with, so a daemon that has stopped answering costs one timeout per minute and
//      nothing more. The bus reader runs on its own thread and never touches this file.
//   2. A daemon that was restarted between two minutes is picked up on the next minute
//      without an operator: ask() reconnects once, by itself, before it reports the
//      engine unavailable.

use std::fmt;
use std::io::{BufRead, BufReader, Write};
use std::os::unix::net::UnixStream;
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant};

/// What the daemon answered, with the round trip the service itself timed.
#[derive(Clone, Debug)]
pub struct Reply {
    pub text: String,
    pub prompt_ms: f64,
    pub generate_ms: f64,
    pub n_generated: u32,
    pub engine: String,
    pub daemon_rss_kb: i64,
    pub round_trip_ms: f64,
}

impl Reply {
    /// The cost of the boundary itself: the round trip the service measured minus the
    /// work the daemon reported. It is the one number e13 cannot have.
    pub fn socket_ms(&self) -> f64 {
        let inside = self.prompt_ms + self.generate_ms;
        if self.round_trip_ms > inside {
            self.round_trip_ms - inside
        } else {
            0.0
        }
    }
}

#[derive(Debug)]
pub struct Unavailable {
    pub reason: String,
}

impl fmt::Display for Unavailable {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "engine unavailable: {}", self.reason)
    }
}

pub struct EngineClient {
    path: PathBuf,
    timeout: Duration,
    conn: Option<Conn>,
    reconnects: u64,
    unavailable: u64,
}

struct Conn {
    reader: BufReader<UnixStream>,
    writer: UnixStream,
}

impl EngineClient {
    pub fn new(path: &Path, timeout: Duration) -> Self {
        EngineClient {
            path: path.to_path_buf(),
            timeout,
            conn: None,
            reconnects: 0,
            unavailable: 0,
        }
    }

    /// How many times the service had to open a new connection, and how many minutes it
    /// logged without a model answer. Both go into the recovery measurement.
    pub fn counters(&self) -> (u64, u64) {
        (self.reconnects, self.unavailable)
    }


    fn connect(&mut self) -> Result<(), Unavailable> {
        let stream = UnixStream::connect(&self.path).map_err(|e| Unavailable {
            reason: format!("connect {}: {}", self.path.display(), e),
        })?;
        stream.set_read_timeout(Some(self.timeout)).map_err(|e| Unavailable {
            reason: format!("set read timeout: {}", e),
        })?;
        stream.set_write_timeout(Some(self.timeout)).map_err(|e| Unavailable {
            reason: format!("set write timeout: {}", e),
        })?;
        let writer = stream.try_clone().map_err(|e| Unavailable {
            reason: format!("clone the socket: {}", e),
        })?;
        self.conn = Some(Conn { reader: BufReader::new(stream), writer });
        self.reconnects += 1;
        Ok(())
    }

    /// One request, one reply, bounded by the timeout. On any failure the connection is
    /// dropped and one fresh connection is tried, so a daemon restarted between minutes
    /// costs one minute's answer at most.
    pub fn ask(&mut self, request: &str) -> Result<Reply, Unavailable> {
        // one_line() must not touch the tab that separates the verb from the user text:
        // it is the only thing the daemon splits on. Running it over the whole request
        // turned "decide<TAB>nodes 2 of 3 present..." into one field and the daemon
        // answered "unknown request decide nodes 2 of 3 present...", every minute of
        // every run, so the model column was empty for all one hundred of them and the
        // oracle test failed on the first. Found by that test on Friday 16 October 2026.
        // Sanitise the text, never the frame.
        let line = match request.split_once('\t') {
            Some((op, text)) => format!("{}\t{}", op, one_line(text)),
            None => one_line(request),
        };
        for attempt in 0..2 {
            if self.conn.is_none() {
                if let Err(e) = self.connect() {
                    if attempt == 1 {
                        self.unavailable += 1;
                        return Err(e);
                    }
                    continue;
                }
            }
            match self.round_trip(&line) {
                Ok(reply) => return Ok(reply),
                Err(e) => {
                    self.conn = None; /* the daemon went away or stopped answering */
                    if attempt == 1 {
                        self.unavailable += 1;
                        return Err(e);
                    }
                }
            }
        }
        self.unavailable += 1;
        Err(Unavailable { reason: "two attempts failed".to_string() })
    }

    fn round_trip(&mut self, line: &str) -> Result<Reply, Unavailable> {
        let conn = match self.conn.as_mut() {
            Some(c) => c,
            None => return Err(Unavailable { reason: "no connection".to_string() }),
        };
        let started = Instant::now();
        let mut request = String::with_capacity(line.len() + 1);
        request.push_str(line);
        request.push('\n');
        conn.writer
            .write_all(request.as_bytes())
            .map_err(|e| Unavailable { reason: format!("write: {}", e) })?;
        conn.writer
            .flush()
            .map_err(|e| Unavailable { reason: format!("flush: {}", e) })?;
        let mut answer = String::new();
        let n = conn
            .reader
            .read_line(&mut answer)
            .map_err(|e| Unavailable { reason: format!("read: {}", e) })?;
        if n == 0 {
            return Err(Unavailable { reason: "the daemon closed the connection".to_string() });
        }
        let round_trip_ms = started.elapsed().as_secs_f64() * 1000.0;
        parse_reply(answer.trim_end(), round_trip_ms)
    }
}

/// A request must be one line: a tab would split a field and a newline would end the
/// request, so both become a space before the request is sent.
fn one_line(text: &str) -> String {
    text.chars()
        .map(|c| if c == '\t' || c == '\n' || c == '\r' { ' ' } else { c })
        .collect()
}

fn parse_reply(line: &str, round_trip_ms: f64) -> Result<Reply, Unavailable> {
    let f: Vec<&str> = line.split('\t').collect();
    if f.first() == Some(&"ERR") {
        return Err(Unavailable {
            reason: format!("the daemon reported: {}", f.get(1).unwrap_or(&"no reason")),
        });
    }
    if f.len() < 7 || f[0] != "OK" {
        return Err(Unavailable { reason: format!("malformed reply of {} fields", f.len()) });
    }
    Ok(Reply {
        text: f[1].to_string(),
        prompt_ms: f[2].parse().unwrap_or(0.0),
        generate_ms: f[3].parse().unwrap_or(0.0),
        n_generated: f[4].parse().unwrap_or(0),
        engine: f[5].to_string(),
        daemon_rss_kb: f[6].parse().unwrap_or(-1),
        round_trip_ms,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_reply_parses_into_its_seven_fields() {
        let line = "OK\t{\"decision\":\"OK\"}\t12.5\t37.25\t8\tstub\t41234";
        let r = parse_reply(line, 61.0).unwrap();
        assert_eq!(r.text, "{\"decision\":\"OK\"}");
        assert_eq!(r.n_generated, 8);
        assert_eq!(r.engine, "stub");
        assert_eq!(r.daemon_rss_kb, 41234);
        // 61.0 round trip minus 49.75 inside the daemon is the cost of the boundary.
        assert!((r.socket_ms() - 11.25).abs() < 1e-9);
    }

    #[test]
    fn an_error_reply_is_an_unavailable() {
        assert!(parse_reply("ERR\tno model file", 1.0).is_err());
        assert!(parse_reply("OK\ttoo few fields", 1.0).is_err());
    }

    #[test]
    fn a_request_is_forced_onto_one_line() {
        assert_eq!(one_line("a\tb\nc"), "a b c");
    }

    /// The frame survives sanitising and the text does not keep its tabs. The request
    /// built here is the one main.rs builds, and the split is the one the daemon does.
    #[test]
    fn the_verbs_separator_survives_but_the_text_is_flattened() {
        let built = format!("decide\t{}", "two\tfields\nand a line");
        let sent = match built.split_once('\t') {
            Some((op, text)) => format!("{}\t{}", op, one_line(text)),
            None => one_line(&built),
        };
        let mut fields = sent.splitn(2, '\t');
        assert_eq!(fields.next(), Some("decide"), "the daemon must see the verb alone");
        assert_eq!(fields.next(), Some("two fields and a line"));
        assert_eq!(sent.matches('\t').count(), 1, "exactly one separator leaves here");
    }
}
