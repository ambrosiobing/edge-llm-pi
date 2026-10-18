//! The control rule of project e18: a register whose span over a window is above a
//! stated fraction of its declared range is DRIFT.
//!
//! The rule is what acts. It writes the verdict to the log and to the RS232 line, and
//! it is the only thing that would ever reach an output; the model writes the sentence
//! beside it and is never in that path. The three constants below are the whole rule,
//! and they are also the three numbers the system prompt tells the model, so that the
//! question the chapter asks is whether a 0.5B model can apply a rule it has been
//! given rather than guess one it has not.
//!
//! The same three constants appear in tools/make_replay.py, which writes the oracle
//! file from the same span test in Python. A change on one side without the same
//! change on the other is caught on the next oracle run, which is why they are stated
//! twice on purpose and nowhere else.

use edgellm::io::Event;
use edgellm::rules::{Rule, RuleCtx};

/// Register 0 is declared 0 to 1000 counts by the peer's documentation, so the
/// fraction below is a fraction of that and not of whatever the stream happens to
/// have shown so far. A range inferred from the data is a different rule, and a worse
/// one: it drifts with the drift it is meant to detect.
pub const REG_RANGE: f64 = 1000.0;

/// A tenth of the range. Strict: a span of exactly 100.0 counts stays NOMINAL, as the
/// boundary cases below state.
pub const DRIFT_FRACTION: f64 = 0.10;

/// Frames, the current one and the twenty nine before it. At two seconds per frame
/// that is a minute of history, which is the shortest window in which this
/// installation's level register is expected to be flat.
pub const WINDOW: usize = 30;

/// The threshold in counts, 100.0, named once so the prompt and the chapter quote the
/// same arithmetic.
pub const fn drift_threshold() -> f64 {
    DRIFT_FRACTION * REG_RANGE
}

/// The index of the register the rule watches inside an event's value list.
pub const LEVEL: usize = 0;

/// The literals, letter for letter as grammars/e18_drift.gbnf produces them. NOMINAL
/// is first because the stub answers the first literal on every path.
pub const NAMES: [&str; 2] = ["NOMINAL", "DRIFT"];

pub const NOMINAL: usize = 0;
pub const DRIFT: usize = 1;

/// The span of register 0 over the window: the current event's level and the level of
/// the frames before it, at most WINDOW values in all. Returned separately from the
/// verdict because the service puts the number in the prompt and in the log, and a
/// verdict without the number it came from is not evidence.
pub fn window_span(ctx: &RuleCtx, ev: &Event) -> f64 {
    let mut lo = match ev.v.get(LEVEL) {
        Some(&v) => v,
        None => return 0.0,
    };
    let mut hi = lo;
    let hist = ctx.hist();
    let take = WINDOW.saturating_sub(1).min(hist.len());
    for past in hist[hist.len() - take..].iter() {
        if let Some(&v) = past.v.get(LEVEL) {
            if v < lo {
                lo = v;
            }
            if v > hi {
                hi = v;
            }
        }
    }
    hi - lo
}

/// The rule itself.
pub struct DriftRule;

impl Rule for DriftRule {
    fn project(&self) -> &'static str {
        "e18"
    }

    fn names(&self) -> &'static [&'static str] {
        &NAMES
    }

    fn decide(&self, ctx: &RuleCtx, ev: &Event) -> usize {
        if ev.v.is_empty() {
            return NOMINAL; // a frame with no register block is not a guess
        }
        if window_span(ctx, ev) > drift_threshold() {
            DRIFT
        } else {
            NOMINAL
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn ev(t_s: f64, level: f64) -> Event {
        Event { t_s, tag: "rs485a".to_string(), v: vec![level, 424.0, 58.0, 0.0] }
    }

    /// Feeds the levels in order, exactly as the service does: the rule sees the
    /// current event with the history NOT yet holding it, and the event is pushed
    /// afterwards. Returns the verdict for the last level.
    fn verdict_after(levels: &[f64]) -> usize {
        let rule = DriftRule;
        let mut ctx = RuleCtx::with_capacity(WINDOW);
        let mut idx = NOMINAL;
        for (i, &level) in levels.iter().enumerate() {
            let e = ev(i as f64 * 2.0, level);
            idx = rule.decide(&ctx, &e);
            ctx.push(&e);
        }
        idx
    }

    #[test]
    fn the_threshold_is_a_tenth_of_the_declared_range() {
        assert_eq!(drift_threshold(), 100.0);
        assert_eq!(NAMES[NOMINAL], "NOMINAL");
        assert_eq!(NAMES[DRIFT], "DRIFT");
        assert_eq!(DriftRule.names().len(), 2);
    }

    #[test]
    fn a_flat_level_is_nominal() {
        assert_eq!(verdict_after(&[420.0; WINDOW]), NOMINAL);
    }

    #[test]
    fn the_slow_drift_of_the_replay_never_reaches_the_threshold() {
        // 0.08 counts per frame over the window is 2.4 counts, and the replay's noise
        // adds a few more. This is the case the acceptance test would fail on if the
        // threshold were set from the noise rather than from the range.
        let levels: Vec<f64> = (0..WINDOW).map(|i| 420.0 + 0.08 * i as f64).collect();
        assert_eq!(verdict_after(&levels), NOMINAL);
    }

    #[test]
    fn a_span_of_exactly_the_threshold_stays_nominal() {
        assert_eq!(verdict_after(&[420.0, 520.0]), NOMINAL);
    }

    #[test]
    fn a_span_one_count_past_the_threshold_is_drift() {
        assert_eq!(verdict_after(&[420.0, 520.01]), DRIFT);
    }

    #[test]
    fn a_step_down_is_drift_as_well_as_a_step_up() {
        assert_eq!(verdict_after(&[590.0, 420.0]), DRIFT);
    }

    #[test]
    fn a_window_not_yet_full_can_still_say_drift() {
        assert_eq!(verdict_after(&[420.0, 480.0, 571.0]), DRIFT);
    }

    #[test]
    fn a_step_leaves_the_window_after_the_window_has_passed() {
        // The step is at index 0. After WINDOW further frames at the new level the
        // window no longer holds the old one, so the verdict returns to NOMINAL. This
        // is the shape of the two bands in the replay's oracle column.
        let mut levels = vec![420.0];
        levels.extend(std::iter::repeat(560.0).take(WINDOW - 1));
        assert_eq!(verdict_after(&levels), DRIFT);
        levels.push(560.0);
        assert_eq!(verdict_after(&levels), NOMINAL);
    }

    #[test]
    fn a_frame_with_no_registers_is_nominal_and_not_a_guess() {
        let rule = DriftRule;
        let ctx = RuleCtx::with_capacity(WINDOW);
        let empty = Event { t_s: 0.0, tag: "rs485a".to_string(), v: vec![] };
        assert_eq!(rule.decide(&ctx, &empty), NOMINAL);
    }

    #[test]
    fn the_span_is_reported_as_well_as_the_verdict() {
        let rule = DriftRule;
        let mut ctx = RuleCtx::with_capacity(WINDOW);
        let a = ev(0.0, 420.0);
        rule.decide(&ctx, &a);
        ctx.push(&a);
        let b = ev(2.0, 561.0);
        assert_eq!(rule.decide(&ctx, &b), DRIFT);
        assert!((window_span(&ctx, &b) - 141.0).abs() < 1e-9);
    }
}
