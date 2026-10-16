//! The oracle test of project e18: the service over the replayed register stream with
//! the Rust stub, compared row by row against tests/oracle/e18.expected.csv.
//!
//! It is the same comparison tests/oracle_check.c makes for the C projects, written
//! here in Rust because the C half of e18 is configured on its own and `cargo test`
//! has no C build to call. Only the rule column and the model column are compared,
//! located by name, so the expected file may hold the four column short shape or the
//! full log shape.
//!
//! What this test can fail on, which is the reason it exists:
//!
//!   * the rule column, if the span test of src/rules.rs and the one in
//!     tools/make_replay.py ever disagree, including by one count at the boundary;
//!   * the model column, if the Rust stub of src/abi.rs stops agreeing byte for byte
//!     with the documented expansion of common/c/src/edgellm_stub.c on this grammar;
//!   * the row count, if make_replay.py writes the replay file at a different length;
//!   * the frame decoder, because every replayed row is encoded into the 64 byte frame
//!     and decoded again before the rule sees it, so a wrong offset, a flipped byte
//!     order or a broken checksum stops the run rather than the comparison.

use std::path::{Path, PathBuf};
use std::process::Command;

fn project_dir() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("..")
}

fn repo_root() -> PathBuf {
    project_dir().join("..").join("..")
}

/// Splits one CSV line, resolving the quoting common/c/src/elog.c writes: a field may
/// be quoted and an inner quote is doubled.
fn fields(line: &str) -> Vec<String> {
    let mut out = vec![String::new()];
    let mut in_quotes = false;
    let mut chars = line.chars().peekable();
    while let Some(c) = chars.next() {
        match c {
            '"' if in_quotes && chars.peek() == Some(&'"') => {
                chars.next();
                out.last_mut().unwrap().push('"');
            }
            '"' => in_quotes = !in_quotes,
            ',' if !in_quotes => out.push(String::new()),
            other => out.last_mut().unwrap().push(other),
        }
    }
    out
}

fn column(header: &str, name: &str) -> usize {
    fields(header)
        .iter()
        .position(|f| f == name)
        .unwrap_or_else(|| panic!("no column named {} in {}", name, header))
}

fn read_csv(path: &Path) -> (String, Vec<String>) {
    let text = std::fs::read_to_string(path)
        .unwrap_or_else(|e| panic!("{}: {}", path.display(), e));
    let mut lines = text.lines().filter(|l| !l.is_empty());
    let header = lines.next().unwrap_or_else(|| panic!("{} has no header", path.display()));
    (header.to_string(), lines.map(|l| l.to_string()).collect())
}

#[test]
fn the_replayed_register_stream_matches_the_oracle() {
    let project = project_dir();
    let replay = project.join("replay").join("registers_600.csv");
    let grammar = project.join("grammar.gbnf");
    let prompt = project.join("prompt.txt");
    let expected = repo_root().join("tests").join("oracle").join("e18.expected.csv");
    let log = std::env::temp_dir().join("e18_oracle_run.csv");

    let status = Command::new(env!("CARGO_BIN_EXE_edgellm-e18"))
        .arg("--replay").arg(&replay)
        .arg("--grammar").arg(&grammar)
        .arg("--prompt").arg(&prompt)
        .arg("--engine").arg("stub")
        .arg("--log").arg(&log)
        .status()
        .expect("the service did not start");
    assert!(status.success(), "the service exited with {:?}", status.code());

    let (got_header, got_rows) = read_csv(&log);
    let (want_header, want_rows) = read_csv(&expected);
    assert_eq!(got_rows.len(), want_rows.len(),
               "the log has {} rows, the oracle {}", got_rows.len(), want_rows.len());

    let (g_rule, g_model) = (column(&got_header, "rule"), column(&got_header, "model"));
    let (w_rule, w_model) = (column(&want_header, "rule"), column(&want_header, "model"));
    let mut differences = Vec::new();
    for (i, (got, want)) in got_rows.iter().zip(want_rows.iter()).enumerate() {
        let g = fields(got);
        let w = fields(want);
        if g[g_rule] != w[w_rule] {
            differences.push(format!("row {}: rule expected {:?} got {:?}", i + 1,
                                     w[w_rule], g[g_rule]));
        }
        if g[g_model] != w[w_model] {
            differences.push(format!("row {}: model expected {:?} got {:?}", i + 1,
                                     w[w_model], g[g_model]));
        }
    }
    assert!(differences.is_empty(), "{} differences:\n{}", differences.len(),
            differences.iter().take(10).cloned().collect::<Vec<_>>().join("\n"));
}

#[test]
fn the_oracle_file_carries_both_verdicts_so_the_comparison_can_fail() {
    // A replay whose oracle column held one literal only would pass this test with a
    // rule that always answered that literal. The two counts below are the record
    // tools/make_replay.py printed on Wednesday 14 October 2026, and the agreement the
    // stub reaches, 542 of 600, is quoted from them in the chapter.
    let expected = repo_root().join("tests").join("oracle").join("e18.expected.csv");
    let (header, rows) = read_csv(&expected);
    let rule = column(&header, "rule");
    let nominal = rows.iter().filter(|r| fields(r)[rule] == "NOMINAL").count();
    let drift = rows.iter().filter(|r| fields(r)[rule] == "DRIFT").count();
    assert_eq!(nominal, 542);
    assert_eq!(drift, 58);
    assert_eq!(nominal + drift, rows.len());
}
