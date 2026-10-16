//! main.rs: project e05, the ADXL345 event reporter, on the shared Rust crate. The crate
//! owns the loop (edgellm::run_loop, the order of common/c/src/edgellm_main.c): it opens the
//! source, a replayed CSV on the host or the IIO device on the Pi 3 Model B, runs the rule of
//! src/rules.rs on every sample, asks this file for the user text, asks the engine for one
//! grammar-constrained answer, and writes the log row with the rule column beside the model
//! column. This file owns the ask policy and the prompt, nothing else.
//!
//!   edgellm-e05 --project e05 --replay replay/adxl345_events.csv --grammar grammar.gbnf \
//!       --prompt prompt.txt --log run.csv --stub
//!   edgellm-e05 --project e05 --live iio /sys/bus/iio/devices/iio:device0:0.02 \
//!       --grammar grammar.gbnf --prompt prompt.txt --n-predict 64 \
//!       --model ~/models/SmolLM2-360M-Instruct-Q4_K_M.gguf --log evidence/e05_run.csv

mod rules;

use std::process;

use edgellm::io::{Event, IoSource};
use edgellm::rules::{Registry, Rule, RuleCtx};
use edgellm::{parse_args, run_loop, Error, Project, USAGE};

use rules::{summarise, E05Rule};

/// The ask policy. The loop runs at the sample rate, 50 per second on the bench, and a
/// decision on the Pi 3 costs seconds, so the model is asked only at the onset of a
/// non-NONE decision: the first sample at which the rule's index differs from the previous
/// sample's and is not NONE. Every other sample logs the rule alone, with an empty model
/// column. `actuate` drives nothing here (e05 has no output); it is where the loop hands
/// over the rule's index, so the policy is kept there. The model never sees that index: the
/// user text carries the sixteen numbers and nothing else.
struct E05Project {
    last: usize,
    onset: bool,
}

impl Project for E05Project {
    fn actuate(
        &mut self,
        _src: &mut dyn IoSource,
        _rule: &dyn Rule,
        index: usize,
        _ev: &Event,
    ) -> Result<(), Error> {
        self.onset = index != 0 && index != self.last;
        self.last = index;
        Ok(())
    }

    fn format_prompt(&mut self, ev: &Event, ctx: &RuleCtx) -> Option<String> {
        if !self.onset {
            return None;
        }
        let s = summarise(ctx, ev);
        Some(format!(
            "peak {:.2} g, min {:.2} g, mean {:.2} g, now {:.2} g, tilt {:.1} deg over {} \
             samples; now to x {:.1}, y {:.1}, z {:.1} deg; start to x {:.1}, y {:.1}, z {:.1} \
             deg; now {:.2}, {:.2}, {:.2} m/s2. Decide.",
            s.peak_g, s.min_g, s.mean_g, s.cur_g, s.tilt_deg, s.n,
            s.cur_axis_deg[0], s.cur_axis_deg[1], s.cur_axis_deg[2],
            s.start_axis_deg[0], s.start_axis_deg[1], s.start_axis_deg[2],
            s.cur[0], s.cur[1], s.cur[2]
        ))
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let cfg = match parse_args(&args) {
        Ok(c) => c,
        Err(reason) => {
            eprintln!("edgellm-e05: {}", reason);
            eprintln!("{}", USAGE);
            process::exit(2);
        }
    };
    let mut registry = Registry::new();
    registry.register(Box::new(E05Rule));
    let rule = match registry.find(&cfg.project) {
        Some(r) => r,
        None => {
            eprintln!("edgellm-e05: no rule registered for project {}", cfg.project);
            process::exit(1);
        }
    };
    let mut project = E05Project { last: 0, onset: false };
    if let Err(e) = run_loop(&cfg, rule, &mut project) {
        eprintln!("edgellm-e05: {}", e);
        process::exit(1);
    }
}
