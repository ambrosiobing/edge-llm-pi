//! oracle.rs: the stub loop over the replay file, compared row by row with
//! tests/oracle/e05.expected.csv on the rule and model columns, the same check as
//! tests/oracle_check.c runs for a C project, with no Python in the path. Runs in CI with the
//! default feature, stub; a bench log from the real engine is compared on the bench with
//! tools/agreement.py, never here.

#![cfg(feature = "stub")]

use std::path::PathBuf;
use std::process::Command;

use edgellm::elog::split_csv_line;

fn column(header: &[String], name: &str) -> usize {
    header
        .iter()
        .position(|h| h == name)
        .unwrap_or_else(|| panic!("no column {} in {:?}", name, header))
}

#[test]
fn the_stub_run_matches_the_oracle_row_for_row() {
    let project = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let repo = project.join("..").join("..");
    let log = std::env::temp_dir().join(format!("edgellm_e05_oracle_{}.csv", std::process::id()));
    let status = Command::new(env!("CARGO_BIN_EXE_edgellm-e05"))
        .args(["--project", "e05", "--replay"])
        .arg(project.join("replay").join("adxl345_events.csv"))
        .arg("--grammar")
        .arg(project.join("grammar.gbnf"))
        .arg("--prompt")
        .arg(project.join("prompt.txt"))
        .arg("--log")
        .arg(&log)
        .arg("--stub")
        .status()
        .expect("edgellm-e05 runs");
    assert!(status.success(), "the stub run must exit 0");

    let got = std::fs::read_to_string(&log).expect("the log was written");
    let want = std::fs::read_to_string(repo.join("tests").join("oracle").join("e05.expected.csv"))
        .expect("tests/oracle/e05.expected.csv exists");
    let got_rows: Vec<Vec<String>> = got.lines().map(split_csv_line).collect();
    let want_rows: Vec<Vec<String>> = want.lines().map(split_csv_line).collect();
    assert!(got_rows.len() > 1 && want_rows.len() > 1, "both files carry rows");
    assert_eq!(got_rows.len(), want_rows.len(), "row counts differ");

    let g_rule = column(&got_rows[0], "rule");
    let g_model = column(&got_rows[0], "model");
    let g_engine = column(&got_rows[0], "engine");
    let w_rule = column(&want_rows[0], "rule");
    let w_model = column(&want_rows[0], "model");
    for (n, (g, w)) in got_rows.iter().zip(want_rows.iter()).enumerate().skip(1) {
        assert_eq!(g[g_rule], w[w_rule], "row {}: rule column", n + 1);
        assert_eq!(g[g_model], w[w_model], "row {}: model column", n + 1);
        assert_eq!(g[g_engine], "stub", "row {}: a CI log names the stub", n + 1);
    }
    let _ = std::fs::remove_file(&log);
}

#[test]
fn a_binary_without_the_engine_refuses_a_model_run() {
    let project = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let status = Command::new(env!("CARGO_BIN_EXE_edgellm-e05"))
        .args(["--project", "e05", "--replay"])
        .arg(project.join("replay").join("adxl345_events.csv"))
        .arg("--grammar")
        .arg(project.join("grammar.gbnf"))
        .arg("--prompt")
        .arg(project.join("prompt.txt"))
        .args(["--log", "unused.csv", "--model", "none.gguf"])
        .status()
        .expect("edgellm-e05 runs");
    assert_eq!(status.code(), Some(2), "exit 2, the usage code, before anything is opened");
}
