// tests/oracle.rs: the stub run over the 1000 period replay, compared row by row against
// tests/oracle/e13.expected.csv.
//
// This is the Rust form of what edgellm_add_oracle_test does for a C project: run the
// binary with --stub over the replay file and check that every rule cell and every model
// cell matches the file the generator wrote. It passes only when all four thousand rows
// agree, so a change to the rule, to the node table, to the choice of the node asked
// about, or to the grammar's first alternatives is caught here and not on the bench.
//
//     cargo test --manifest-path projects/e13-can-honesty/Cargo.toml
//
// The expected file is written by tools/make_replay.py from the same three line rule and
// from grammar.gbnf, so no literal in it is hand written; the two files are one pair and
// are regenerated together.
//
// The CSV is split by the shared crate's own splitter, the one tests/oracle_check.c uses,
// so a quoting difference between this log and a C project's log cannot hide here.

use edgellm::elog::split_csv_line;
use std::path::{Path, PathBuf};
use std::process::Command;

fn project_dir() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
}

fn repo_root() -> PathBuf {
    project_dir().parent().unwrap().parent().unwrap().to_path_buf()
}

/// The four columns that matter, pulled out of either file by index.
fn read_rows(path: &Path, columns: [usize; 4]) -> Vec<[String; 4]> {
    let text = std::fs::read_to_string(path)
        .unwrap_or_else(|e| panic!("cannot read {}: {}", path.display(), e));
    let want = columns.iter().copied().max().unwrap();
    let mut rows = Vec::new();
    for (n, line) in text.lines().enumerate() {
        if n == 0 || line.trim().is_empty() {
            continue;
        }
        let f = split_csv_line(line);
        assert!(f.len() > want, "{}: line {} has {} fields", path.display(), n + 1, f.len());
        rows.push([
            f[columns[0]].clone(),
            f[columns[1]].clone(),
            f[columns[2]].clone(),
            f[columns[3]].clone(),
        ]);
    }
    rows
}

fn oracle_path() -> PathBuf {
    repo_root().join("tests").join("oracle").join("e13.expected.csv")
}

#[test]
fn the_stub_run_matches_the_oracle() {
    let project = project_dir();
    let log = std::env::temp_dir().join("e13_oracle_run.csv");
    let output = Command::new(env!("CARGO_BIN_EXE_edgellm-e13"))
        .args(["--project", "e13"])
        .arg("--replay")
        .arg(project.join("replay").join("heartbeats_1000.csv"))
        .arg("--grammar")
        .arg(project.join("grammar.gbnf"))
        .arg("--prompt")
        .arg(project.join("prompt.txt"))
        .arg("--log")
        .arg(&log)
        .args(["--periods", "1000", "--stub"])
        .output()
        .expect("cannot run edgellm-e13");
    assert!(output.status.success(), "edgellm-e13 failed: {}",
            String::from_utf8_lossy(&output.stderr));

    // The log is t_s,tag,raw,rule,model,...; the oracle is t_s,tag,rule,model.
    let got = read_rows(&log, [0, 1, 3, 4]);
    let want = read_rows(&oracle_path(), [0, 1, 2, 3]);
    assert_eq!(got.len(), 4000, "four rows per period over 1000 periods");
    assert_eq!(got.len(), want.len(), "row count differs from the oracle");
    for (i, (g, w)) in got.iter().zip(want.iter()).enumerate() {
        assert_eq!(g, w, "row {} differs: got {:?}, oracle {:?}", i + 2, g, w);
    }
    let _ = std::fs::remove_file(&log);
}

#[test]
fn every_model_cell_holds_one_literal_or_nothing() {
    let rows = read_rows(&oracle_path(), [0, 1, 2, 3]);
    let mut scored = 0;
    for row in rows.iter() {
        assert!(["PRESENT", "MODELLED", "ABSENT"].contains(&row[2].as_str()),
                "rule cell {:?} is not one of the three literals", row[2]);
        if row[3].is_empty() {
            continue;
        }
        scored += 1;
        let literals = ["\"PRESENT\"", "\"MODELLED\"", "\"ABSENT\""];
        assert!(literals.iter().any(|l| row[3].contains(l)),
                "model cell {:?} holds no literal", row[3]);
    }
    assert_eq!(scored, 1000, "one model answer per period, no more and no fewer");
}

#[test]
fn the_absent_node_is_never_reported_present() {
    let rows = read_rows(&oracle_path(), [0, 1, 2, 3]);
    let mut seen = 0;
    for row in rows.iter() {
        if row[1] != "joint-d" {
            continue;
        }
        seen += 1;
        assert_eq!(row[2], "ABSENT", "the dotted node read {} at t {}", row[2], row[0]);
    }
    assert_eq!(seen, 1000, "one row per period for the node that is not connected");
}

#[test]
fn the_vcan_node_is_always_modelled() {
    let rows = read_rows(&oracle_path(), [0, 1, 2, 3]);
    let mut seen = 0;
    for row in rows.iter() {
        if row[1] != "joint-c" {
            continue;
        }
        seen += 1;
        assert_eq!(row[2], "MODELLED", "the dashed node read {} at t {}", row[2], row[0]);
    }
    assert_eq!(seen, 1000, "one row per period for the node on the virtual interface");
}
