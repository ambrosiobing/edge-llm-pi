// tests/oracle.rs: the oracle test of project e14, and the only test in the volume that
// exercises a process boundary rather than a function.
//
// It does what tests/oracle_check does for a C project, in Rust and across two
// processes: it starts the engine daemon on a socket of its own under the temporary
// directory, runs the service over the hundred minute replay file against that socket,
// and compares the log's rule and model columns with tests/oracle/e14.expected.csv row
// by row. Neither the socketcan nor the llama-cpp-2 feature is needed, so CI runs it in
// seconds with no CAN interface and no model file, and what it proves is exactly the
// thing e14 adds to e13: the service decides, the answer comes from another process,
// and the log is the same shape either way.
//
// The second test is the safety property of the chapter: with no daemon at all, every
// minute still carries the rule's decision, and the engine column reads unavailable.
// A monitor that stopped monitoring because a language model was not there would have
// failed the volume's rule, and this is the test that would catch it.

use std::fs;
use std::path::{Path, PathBuf};
use std::process::{Child, Command};
use std::thread::sleep;
use std::time::{Duration, Instant};

const EXPECTED_MINUTES: usize = 100;
const EXPECTED_INSPECT: usize = 8;
const STUB_ANSWER: &str = "{\"decision\":\"OK\",\"summary\":\".\"}";

fn project_dir() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
}

fn repo_root() -> PathBuf {
    let p = project_dir();
    p.parent()
        .and_then(Path::parent)
        .expect("the project sits two levels under the repository root")
        .to_path_buf()
}

fn scratch(name: &str) -> PathBuf {
    let dir = std::env::temp_dir().join(format!("edgellm-e14-{}-{}", std::process::id(), name));
    fs::create_dir_all(&dir).expect("create the scratch directory");
    dir
}

/// One CSV line into fields, with the quoting common/c/src/elog.c writes: a quoted
/// field may hold commas, and an inner quote is doubled.
fn split_csv(line: &str) -> Vec<String> {
    let mut fields = Vec::new();
    let mut field = String::new();
    let mut quoted = false;
    let mut chars = line.chars().peekable();
    while let Some(c) = chars.next() {
        match c {
            '"' if quoted && chars.peek() == Some(&'"') => {
                field.push('"');
                chars.next();
            }
            '"' => quoted = !quoted,
            ',' if !quoted => {
                fields.push(std::mem::take(&mut field));
            }
            other => field.push(other),
        }
    }
    fields.push(field);
    fields
}

fn read_rows(path: &Path) -> Vec<Vec<String>> {
    let text = fs::read_to_string(path).unwrap_or_else(|e| panic!("{}: {}", path.display(), e));
    text.lines()
        .skip(1)
        .filter(|l| !l.trim().is_empty())
        .map(split_csv)
        .collect()
}

/// The daemon, stopped when the test ends whichever way it ends.
struct Daemon(Child);

impl Drop for Daemon {
    fn drop(&mut self) {
        let _ = self.0.kill();
        let _ = self.0.wait();
    }
}

fn start_daemon(socket: &Path) -> Daemon {
    let project = project_dir();
    let child = Command::new(env!("CARGO_BIN_EXE_edgellm-e14-engine-daemon"))
        .arg("--socket")
        .arg(socket)
        .arg("--grammar")
        .arg(project.join("grammar.gbnf"))
        .arg("--prompt")
        .arg(project.join("prompt.txt"))
        .spawn()
        .expect("start the engine daemon");
    let deadline = Instant::now() + Duration::from_secs(10);
    while !socket.exists() {
        assert!(Instant::now() < deadline, "the daemon did not bind {}", socket.display());
        sleep(Duration::from_millis(20));
    }
    Daemon(child)
}

fn run_service(socket: &Path, log: &Path, timeout_ms: u64) {
    let project = project_dir();
    let status = Command::new(env!("CARGO_BIN_EXE_edgellm-e14"))
        .arg("--project")
        .arg("e14")
        .arg("--replay")
        .arg(project.join("replay").join("joint_100min.csv"))
        .arg("--socket")
        .arg(socket)
        .arg("--log")
        .arg(log)
        .arg("--timeout-ms")
        .arg(timeout_ms.to_string())
        .status()
        .expect("run the service over the replay file");
    assert!(status.success(), "the service exited with {:?}", status.code());
}

#[test]
fn the_replay_run_matches_the_oracle_across_the_socket() {
    let dir = scratch("oracle");
    let socket = dir.join("e14-engine.sock");
    let log = dir.join("e14_oracle.csv");
    let _daemon = start_daemon(&socket);
    run_service(&socket, &log, 2000);

    let got = read_rows(&log);
    let want = read_rows(&repo_root().join("tests").join("oracle").join("e14.expected.csv"));
    assert_eq!(want.len(), EXPECTED_MINUTES, "the oracle file is not 100 minutes long");
    assert_eq!(got.len(), want.len(), "the log has {} rows, the oracle {}", got.len(), want.len());

    let mut inspect = 0usize;
    for (n, (g, w)) in got.iter().zip(want.iter()).enumerate() {
        assert_eq!(g[0], w[0], "minute {}: the timestamp column", n);
        assert_eq!(g[1], w[1], "minute {}: the tag column", n);
        assert_eq!(g[3], w[2], "minute {}: the rule column", n);
        assert_eq!(g[4], w[3], "minute {}: the model column", n);
        assert_eq!(g[4], STUB_ANSWER, "minute {}: the stub answered something else", n);
        assert_eq!(g[8], "stub", "minute {}: the engine column", n);
        if g[3] == "INSPECT" {
            inspect += 1;
        }
    }
    assert_eq!(inspect, EXPECTED_INSPECT, "the rule's INSPECT count moved");

    // The agreement the chapter prints for CI: the stub answers OK on every row, so the
    // agreement is the OK fraction of the rule column.
    let agreement = 100.0 * (EXPECTED_MINUTES - inspect) as f64 / EXPECTED_MINUTES as f64;
    assert!((agreement - 92.0).abs() < 1e-9, "the CI agreement is {} percent", agreement);
    let _ = fs::remove_dir_all(&dir);
}

#[test]
fn a_missing_daemon_still_logs_every_rule_decision() {
    let dir = scratch("no-daemon");
    let socket = dir.join("there-is-no-daemon-here.sock");
    let log = dir.join("e14_no_engine.csv");
    // No daemon is started. Two connect attempts per minute fail at once, so a short
    // timeout keeps the test quick; the service must still write all hundred rows.
    run_service(&socket, &log, 200);

    let got = read_rows(&log);
    assert_eq!(got.len(), EXPECTED_MINUTES);
    let mut inspect = 0usize;
    for (n, row) in got.iter().enumerate() {
        assert!(row[3] == "OK" || row[3] == "INSPECT", "minute {}: rule {:?}", n, row[3]);
        assert_eq!(row[4], "", "minute {}: there was no model answer to log", n);
        assert_eq!(row[8], "unavailable", "minute {}: the engine column", n);
        if row[3] == "INSPECT" {
            inspect += 1;
        }
    }
    assert_eq!(inspect, EXPECTED_INSPECT, "the rule changed when the engine went away");
    let _ = fs::remove_dir_all(&dir);
}

#[test]
fn the_grammar_and_the_rule_names_are_the_same_bytes() {
    // The literals of grammars/e14_inspect.gbnf, the project's copy of it, and the name
    // table of src/rules.rs are one thing in three files; the agreement table means
    // nothing if they drift. lint.py checks the chapter against the files; this checks
    // the two copies of the grammar against each other.
    let project = project_dir();
    let mine = fs::read_to_string(project.join("grammar.gbnf")).expect("the project grammar");
    let shared = fs::read_to_string(repo_root().join("grammars").join("e14_inspect.gbnf"))
        .expect("the shared grammar");
    assert_eq!(mine, shared, "projects/e14-joint-monitor/grammar.gbnf has drifted");
    assert!(mine.contains("\"\\\"OK\\\"\" | \"\\\"INSPECT\\\"\""), "the two literals, in order");
}
