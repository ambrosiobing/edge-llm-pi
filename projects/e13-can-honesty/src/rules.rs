// rules.rs: the control rule of project e13 and the cases that hold its boundaries.
//
// The rule is what acts and what the report shows. The model is asked the same question
// from the same counters and its answer goes into the log beside this one, never anywhere
// else. Three lines, in this order, and the order is the rule:
//
//   1. a vcan source is MODELLED, whatever its counters say, because a virtual interface
//      cannot become hardware by behaving well;
//   2. a node silent for three heartbeat periods, or never heard at all, is ABSENT;
//   3. otherwise frames arrived from a hardware controller, which is PRESENT.
//
// The literals PRESENT, MODELLED and ABSENT are the same bytes as the alternatives of
// grammars/e13_presence.gbnf, in the same order, with the quiet state first as the
// repository's grammar convention requires. The oracle test and the agreement table both
// depend on that equality.
//
// Why "never heard" is ABSENT from the first period rather than after three: the timeout
// is a rule about losing a node, and a node that was never there was never lost. Reporting
// a node PRESENT because the run is younger than the timeout would be the one thing this
// project exists to refuse.
//
// This is a Rust project, so the rule implements the shared crate's Rule trait rather than
// registering through the C constructor registry of common/c/include/rules.h, and its cases
// run under cargo test, which the CI manifests job runs for every projects/*/Cargo.toml.

use edgellm::io::Event;
use edgellm::rules::{Rule, RuleCtx};

use crate::nodes::{ABSENT_AFTER_PERIODS, HEARTBEAT_PERIOD_S};

/// The columns of a node-period event, which is what the tracker in main.rs hands the rule
/// once per node per heartbeat period: how many frames arrived in the period, how many
/// periods have been missed in a row, how long the node has been silent in seconds (a
/// negative value means it has never been heard), and the source class below.
pub const V_FRAMES: usize = 0;
pub const V_MISSED: usize = 1;
pub const V_SILENT: usize = 2;
pub const V_SOURCE: usize = 3;

pub const SOURCE_HARDWARE: f64 = 0.0;
pub const SOURCE_VCAN: f64 = 1.0;
pub const SOURCE_ABSENT: f64 = 2.0;

pub const IDX_PRESENT: usize = 0;
pub const IDX_MODELLED: usize = 1;
pub const IDX_ABSENT: usize = 2;

/// The names, in the grammar's order.
///
/// This table and the literal inside names() below are two copies of one thing, held equal
/// by the test the_name_table_is_the_grammar_order. names() cannot simply return &NAMES yet,
/// because chapter 13 prints that function with its literal and the chapter is generated
/// from the catalogue's sections/e13.tex; changing the file without the source would make
/// the printed code wrong. Until that source is edited and the chapter regenerated, the
/// table is read only by the test, and the bin build is told so rather than warned.
#[cfg_attr(not(test), allow(dead_code))]
pub const NAMES: [&str; 3] = ["PRESENT", "MODELLED", "ABSENT"];

/// The source class an event carries; a short event is read as a hardware node, which the
/// silence accessor below then reports as never heard, so a malformed event can only ever
/// become ABSENT.
fn source_of(ev: &Event) -> f64 {
    ev.v.get(V_SOURCE).copied().unwrap_or(SOURCE_HARDWARE)
}

/// Seconds of silence, or a negative value when the node has never been heard.
fn silent_of(ev: &Event) -> f64 {
    ev.v.get(V_SILENT).copied().unwrap_or(-1.0)
}

pub struct PresenceRule;

impl Rule for PresenceRule {
    fn project(&self) -> &str {
        "e13"
    }

    fn names(&self) -> &[&'static str] {
        &["PRESENT", "MODELLED", "ABSENT"]
    }

    /// The whole control rule.
    fn decide(&self, _ctx: &RuleCtx, ev: &Event) -> usize {
        if source_of(ev) == SOURCE_VCAN {
            return IDX_MODELLED;
        }
        if silent_of(ev) < 0.0 || silent_of(ev) >= ABSENT_AFTER_PERIODS * HEARTBEAT_PERIOD_S {
            return IDX_ABSENT;
        }
        IDX_PRESENT
    }
}

/// A node-period event, the one shape the rule, the log and the oracle all read.
pub fn node_event(t_s: f64, name: &str, frames: u32, missed: u32, silent_s: f64, source: f64)
                  -> Event {
    // Built through the V_ indices rather than positionally, so the layout the four
    // constants document is the layout this function writes and neither can drift from the
    // other. Positional, the two the rule does not read were unused and the compiler said so.
    let mut v = [0.0_f64; 4];
    v[V_FRAMES] = frames as f64;
    v[V_MISSED] = missed as f64;
    v[V_SILENT] = silent_s;
    v[V_SOURCE] = source;
    Event::new(t_s, name, &v)
}

#[cfg(test)]
mod tests {
    use super::*;
    use edgellm::rules::expect;

    #[test]
    fn hardware_heard_this_period_is_present() {
        expect(&PresenceRule, &[], &node_event(11.0, "joint-a", 1, 0, 0.98, SOURCE_HARDWARE),
               IDX_PRESENT, "a frame arrived from a controller this period");
    }

    #[test]
    fn hardware_silent_under_three_periods_is_present() {
        expect(&PresenceRule, &[], &node_event(11.0, "joint-a", 0, 2, 2.98, SOURCE_HARDWARE),
               IDX_PRESENT, "two missed periods is suspicious, not absent");
    }

    #[test]
    fn hardware_silent_exactly_three_periods_is_absent() {
        expect(&PresenceRule, &[], &node_event(11.0, "joint-a", 0, 3, 3.0, SOURCE_HARDWARE),
               IDX_ABSENT, "the boundary: three periods exactly is already absent");
    }

    #[test]
    fn hardware_never_heard_is_absent() {
        expect(&PresenceRule, &[], &node_event(1.0, "joint-b", 0, 1, -1.0, SOURCE_HARDWARE),
               IDX_ABSENT, "a node never heard is not present, whatever the run's age");
    }

    #[test]
    fn vcan_heard_is_modelled_not_present() {
        expect(&PresenceRule, &[], &node_event(11.0, "joint-c", 1, 0, 0.94, SOURCE_VCAN),
               IDX_MODELLED, "a virtual interface is modelled by declaration");
    }

    #[test]
    fn vcan_silent_for_ten_periods_is_still_modelled() {
        expect(&PresenceRule, &[], &node_event(11.0, "joint-c", 0, 10, 10.0, SOURCE_VCAN),
               IDX_MODELLED, "a vcan node cannot become absent by going quiet");
    }

    #[test]
    fn an_absent_node_is_absent_by_the_same_timeout() {
        expect(&PresenceRule, &[], &node_event(1.0, "joint-d", 0, 1, -1.0, SOURCE_ABSENT),
               IDX_ABSENT, "the dotted node needs no branch of its own");
    }

    #[test]
    fn a_short_event_can_only_become_absent() {
        expect(&PresenceRule, &[], &Event::new(1.0, "joint-a", &[]), IDX_ABSENT,
               "an event without counters says nothing, and nothing is not present");
    }

    #[test]
    fn the_name_table_is_the_grammar_order() {
        assert_eq!(PresenceRule.names(), &NAMES[..]);
        assert_eq!(NAMES[IDX_PRESENT], "PRESENT");
        assert_eq!(NAMES[IDX_MODELLED], "MODELLED");
        assert_eq!(NAMES[IDX_ABSENT], "ABSENT");
    }
}
