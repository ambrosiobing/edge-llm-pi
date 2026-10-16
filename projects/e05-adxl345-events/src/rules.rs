//! rules.rs: the control rule of project e05, the ADXL345 event reporter. An event carries
//! the three accelerations in metres per second squared, v0 x, v1 y, v2 z, the order the IIO
//! backend delivers from in_accel_x_raw, in_accel_y_raw and in_accel_z_raw times the scale.
//! The rule looks at a window of the last 16 samples (15 from the history plus the current
//! one) and returns an index into NAMES, which is the grammar's literal order letter for
//! letter, see grammars/e05_event.gbnf; the oracle test and the agreement table depend on
//! that equality.
//!
//! Index 0 is NONE, 1 TAP, 2 TILT, 3 FREEFALL. The rule acts (here it only records, nothing
//! is driven); the model proposes the same enum and writes one sentence into the log.

use edgellm::io::Event;
use edgellm::rules::{Rule, RuleCtx};

/// Standard gravity in metres per second squared; the IIO scale delivers that unit.
pub const G: f64 = 9.80665;
/// The window: the last 15 events of the history plus the current one.
pub const WINDOW: usize = 16;
/// TAP when the peak magnitude in the window is above this, in g. Strict.
pub const TAP_G: f64 = 2.0;
/// FREEFALL when the current sample's magnitude is below this, in g. Strict.
pub const FREEFALL_G: f64 = 0.3;
/// cos 30 degrees. The tilt test compares a dot product against it so that the Python
/// oracle and this file take the same floating point path and agree bit for bit.
pub const COS_TILT: f64 = 0.866_025_403_784_438_6;
/// A sample below this, in g, is not an orientation and cannot start or end a tilt.
pub const TILT_MIN_G: f64 = 0.5;

/// The grammar's literals, in the grammar's order.
pub const NAMES: [&str; 4] = ["NONE", "TAP", "TILT", "FREEFALL"];

fn axes(ev: &Event) -> [f64; 3] {
    if ev.v.len() >= 3 { [ev.v[0], ev.v[1], ev.v[2]] } else { [0.0; 3] }
}

fn norm(a: [f64; 3]) -> f64 {
    (a[0] * a[0] + a[1] * a[1] + a[2] * a[2]).sqrt()
}

fn axis_angles_deg(a: [f64; 3]) -> [f64; 3] {
    let m = norm(a);
    if m <= 0.0 {
        return [0.0; 3];
    }
    let angle = |c: f64| (c / m).clamp(-1.0, 1.0).acos().to_degrees();
    [angle(a[0]), angle(a[1]), angle(a[2])]
}

/// The window the rule and the prompt both see: oldest first, the current event last.
pub fn window<'a>(ctx: &'a RuleCtx, ev: &'a Event) -> Vec<&'a Event> {
    let h = ctx.hist();
    let skip = h.len().saturating_sub(WINDOW - 1);
    h[skip..].iter().chain(std::iter::once(ev)).collect()
}

/// The sixteen numbers the prompt carries, and the one boolean the rule needs beyond them.
#[derive(Debug, Clone, PartialEq)]
pub struct Summary {
    pub n: usize,
    pub peak_g: f64,
    pub min_g: f64,
    pub mean_g: f64,
    pub cur_g: f64,
    pub tilted: bool,
    pub tilt_deg: f64,
    pub cur_axis_deg: [f64; 3],
    pub start_axis_deg: [f64; 3],
    pub cur: [f64; 3],
}

pub fn summarise(ctx: &RuleCtx, ev: &Event) -> Summary {
    let w = window(ctx, ev);
    let n = w.len();
    let mags: Vec<f64> = w.iter().map(|e| norm(axes(e)) / G).collect();
    let peak_g = mags.iter().cloned().fold(0.0, f64::max);
    let min_g = mags.iter().cloned().fold(f64::INFINITY, f64::min);
    let mean_g = mags.iter().sum::<f64>() / n as f64;
    let cur = axes(ev);
    let start = axes(w[0]);
    let cur_g = mags[n - 1];
    let start_g = mags[0];
    let ma = norm(start);
    let mb = norm(cur);
    let dot = start[0] * cur[0] + start[1] * cur[1] + start[2] * cur[2];
    let tilted = n >= 2 && start_g > TILT_MIN_G && cur_g > TILT_MIN_G && dot < COS_TILT * ma * mb;
    let tilt_deg = if ma > 0.0 && mb > 0.0 {
        (dot / (ma * mb)).clamp(-1.0, 1.0).acos().to_degrees()
    } else {
        0.0
    };
    Summary {
        n,
        peak_g,
        min_g,
        mean_g,
        cur_g,
        tilted,
        tilt_deg,
        cur_axis_deg: axis_angles_deg(cur),
        start_axis_deg: axis_angles_deg(start),
        cur,
    }
}

pub struct E05Rule;

impl Rule for E05Rule {
    fn project(&self) -> &str {
        "e05"
    }

    fn names(&self) -> &[&'static str] {
        &NAMES
    }

    fn decide(&self, ctx: &RuleCtx, ev: &Event) -> usize {
        if ev.v.len() < 3 {
            return 0; // a short event is NONE, not a guess
        }
        let s = summarise(ctx, ev);
        if s.peak_g > TAP_G {
            return 1; // TAP: a peak above 2 g anywhere in the window of 16
        }
        if s.cur_g < FREEFALL_G {
            return 3; // FREEFALL: this sample below 0.3 g
        }
        if s.tilted {
            return 2; // TILT: more than 30 degrees from the start of the window
        }
        0 // NONE
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use edgellm::rules::expect;

    fn ev(t: f64, x: f64, y: f64, z: f64) -> Event {
        Event::new(t, "adxl345", &[x, y, z])
    }

    fn rest(n: usize) -> Vec<Event> {
        (0..n).map(|i| ev(i as f64 * 0.02, 0.0, 0.0, G)).collect()
    }

    fn tilted_by(deg: f64) -> Event {
        let r = deg.to_radians();
        ev(1.0, G * r.sin(), 0.0, G * r.cos())
    }

    #[test]
    fn the_six_cases_and_the_window() {
        let rule = E05Rule;
        expect(&rule, &rest(15), &ev(1.0, 0.0, 0.0, G), 0, "at rest is NONE");
        expect(&rule, &rest(15), &ev(1.0, 1.99 * G, 0.0, 0.0), 0, "1.99 g is NONE");
        expect(&rule, &rest(15), &ev(1.0, 2.01 * G, 0.0, 0.0), 1, "2.01 g is TAP");
        expect(&rule, &rest(15), &ev(1.0, 0.0, 0.0, 0.31 * G), 0, "0.31 g is NONE");
        expect(&rule, &rest(15), &ev(1.0, 0.0, 0.0, 0.29 * G), 3, "0.29 g is FREEFALL");
        expect(&rule, &rest(15), &tilted_by(29.0), 0, "29 degrees is NONE");
        expect(&rule, &rest(15), &tilted_by(31.0), 2, "31 degrees is TILT");

        let mut spike_then_rest = vec![ev(0.0, 3.0 * G, 0.0, 0.0)];
        spike_then_rest.extend(rest(10));
        expect(&rule, &spike_then_rest, &ev(1.0, 0.0, 0.0, G), 1,
               "a spike 11 samples ago is still a TAP: the window peak decides");
        let mut old_spike = vec![ev(0.0, 3.0 * G, 0.0, 0.0)];
        old_spike.extend(rest(15));
        expect(&rule, &old_spike, &ev(1.0, 0.0, 0.0, G), 0,
               "a spike 16 samples ago has left the window of 16");
        expect(&rule, &spike_then_rest, &ev(1.0, 0.0, 0.0, 0.1 * G), 1,
               "a freefall sample after a spike in the window is TAP: the peak has priority");
    }

    #[test]
    fn a_tilt_needs_an_orientation_at_both_ends() {
        let rule = E05Rule;
        let falling_start = vec![ev(0.0, 0.0, 0.0, 0.1 * G)];
        expect(&rule, &falling_start, &ev(1.0, 0.0, 0.0, 0.99 * G), 0,
               "a window that starts below 0.5 g cannot be a tilt");
        expect(&rule, &[], &tilted_by(45.0), 0, "one sample alone has nothing to tilt from");
    }

    #[test]
    fn a_short_event_is_none() {
        expect(&E05Rule, &rest(3), &Event::new(1.0, "adxl345", &[1.0]), 0, "two values are NONE");
    }

    #[test]
    fn the_summary_carries_sixteen_numbers() {
        let mut ctx = RuleCtx::new();
        for e in rest(15) {
            ctx.push(&e);
        }
        let s = summarise(&ctx, &tilted_by(40.0));
        assert_eq!(s.n, 16);
        assert!((s.tilt_deg - 40.0).abs() < 1e-9);
        assert!((s.cur_axis_deg[2] - 40.0).abs() < 1e-9);
        assert!((s.start_axis_deg[2]).abs() < 1e-9);
        assert!((s.peak_g - 1.0).abs() < 1e-9);
        assert!((s.min_g - 1.0).abs() < 1e-9);
        assert!((s.mean_g - 1.0).abs() < 1e-9);
        assert!(s.tilted);
    }
}
