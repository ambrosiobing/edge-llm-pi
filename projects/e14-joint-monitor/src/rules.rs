// rules.rs: the control rule of project e14, the joint monitor, and the node table it
// inherits from project e13, the CAN honesty agent.
//
// Why the node table is written here instead of being imported from e13: on
// Wednesday 14 October 2026 projects/e13-can-honesty does not exist in this tree. There
// is no Cargo.toml to depend on, so there is no library crate to reuse and no file a
// chapter could print. What is written here is e13's design, not a copy of e13's code:
// a heartbeat period per node, a node ABSENT after three missed periods, a node whose
// source interface is a vcan MODELLED, everything else PRESENT. When e13 lands with a
// Cargo.toml and a lib target, the node half of this file is deleted and the service
// depends on it instead; the migration is one path dependency and one use statement,
// and node_state() below is the function whose behaviour must stay identical for the
// two chapters to be comparable.
//
// The decision rule is e14's own either way. The error counters of a CAN controller are
// cumulative, so the rule works on the difference over one minute, which is what
// err_delta() computes; a counter that went backwards means the interface was restarted
// and the difference is reported as zero rather than as a negative rise.
//
// The rule acts. The model proposes the same verdict and one sentence, and INSPECT is a
// recommendation to a person: nothing in this project is actuated at all.

/// The vibration peak above which a minute counts as a mechanical event, in g. Strict:
/// a peak of exactly 2.0 g stays in the quiet band. The ADXL345 of the DFRobot SEN0032
/// is read at its default range, and the peak of a minute is the largest magnitude of
/// the acceleration vector over that minute, minus the 1 g the module reads at rest.
pub const PEAK_G: f64 = 2.0;

/// Missed heartbeat periods after which e13's node table calls a node ABSENT.
pub const HEARTBEAT_MISSES: u32 = 3;

/// The decision enum, in the same order as grammars/e14_inspect.gbnf, which is also the
/// order the CI stub walks: the quiet state is first.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Decision {
    Ok,
    Inspect,
}

/// The literals the grammar allows, letter for letter. The log's rule column, the
/// grammar's alternatives and this table are the same bytes or the agreement table is
/// meaningless.
pub const NAMES: [&str; 2] = ["OK", "INSPECT"];

impl Decision {
    pub fn name(self) -> &'static str {
        match self {
            Decision::Ok => NAMES[0],
            Decision::Inspect => NAMES[1],
        }
    }
}

/// e13's three node states. PRESENT is hardware answering on the bus, MODELLED is a
/// software stand-in (a vcan peer), ABSENT is a node that has stopped answering.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum NodeState {
    Present,
    Modelled,
    Absent,
}

impl NodeState {
    pub fn name(self) -> &'static str {
        match self {
            NodeState::Present => "PRESENT",
            NodeState::Modelled => "MODELLED",
            NodeState::Absent => "ABSENT",
        }
    }
}

/// One row of e13's node table. The bus of this project is the WS-28164's two
/// controllers wired to each other with two jumper wires, so two rows are hardware and
/// the third is the Nucleo step that has not been taken.
pub struct NodeRow {
    pub id: u32,
    pub label: &'static str,
    pub period_ms: u64,
}

pub const NODE_TABLE: [NodeRow; 3] = [
    NodeRow { id: 0x101, label: "classic CAN, MCP2515 with the SN65HVD230", period_ms: 1000 },
    NodeRow { id: 0x102, label: "MCP2518FD, classic frames only", period_ms: 1000 },
    NodeRow { id: 0x103, label: "Nucleo with the loose SN65HVD230, not wired", period_ms: 1000 },
];

/// e13's rule, unchanged: a node whose frames arrive on a vcan interface is MODELLED, a
/// node that has missed three heartbeat periods is ABSENT, anything else is PRESENT.
pub fn node_state(missed_periods: u32, on_vcan: bool) -> NodeState {
    if missed_periods >= HEARTBEAT_MISSES {
        NodeState::Absent
    } else if on_vcan {
        NodeState::Modelled
    } else {
        NodeState::Present
    }
}

/// The difference of a cumulative counter over one minute. A counter that went backwards
/// means the interface was brought down and up again, which is not a rise.
pub fn err_delta(previous: u64, current: u64) -> u64 {
    current.saturating_sub(previous)
}

/// What one minute of the bus and the module amounts to. err_total is the cumulative
/// interface error count read at the end of the minute, err_delta its rise over the
/// minute, peak_g the vibration peak in g, nodes_present how many rows of the node
/// table were PRESENT during the minute.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Minute {
    pub t_s: f64,
    pub err_total: u64,
    pub err_delta: u64,
    pub peak_g: f64,
    pub nodes_present: u32,
}

/// The control rule. Three lines: a minute with no node on the bus cannot be a
/// recommendation to inspect anything, and otherwise a rising error counter together
/// with a vibration peak above the threshold in the same minute is INSPECT.
pub fn decide(m: &Minute) -> Decision {
    if m.nodes_present == 0 {
        return Decision::Ok; /* nothing was on the bus; there is nothing to inspect */
    }
    if m.err_delta > 0 && m.peak_g > PEAK_G {
        Decision::Inspect /* the bus complained and the joint moved, in one minute */
    } else {
        Decision::Ok
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn minute(err_delta: u64, peak_g: f64, nodes_present: u32) -> Minute {
        Minute { t_s: 0.0, err_total: 100 + err_delta, err_delta, peak_g, nodes_present }
    }

    #[test]
    fn quiet_minute_is_ok() {
        assert_eq!(decide(&minute(0, 0.04, 2)), Decision::Ok);
    }

    #[test]
    fn errors_without_vibration_are_ok() {
        assert_eq!(decide(&minute(37, 0.11, 2)), Decision::Ok);
    }

    #[test]
    fn vibration_without_errors_is_ok() {
        assert_eq!(decide(&minute(0, 6.2, 2)), Decision::Ok);
    }

    #[test]
    fn both_in_one_minute_is_inspect() {
        assert_eq!(decide(&minute(4, 2.01, 2)), Decision::Inspect);
    }

    #[test]
    fn the_peak_threshold_is_strict() {
        // 2.0 g exactly stays in the quiet band, which is the boundary the replay file
        // and the oracle both hold a row for.
        assert_eq!(decide(&minute(9, 2.0, 2)), Decision::Ok);
    }

    #[test]
    fn no_node_present_cannot_be_inspect() {
        assert_eq!(decide(&minute(120, 9.5, 0)), Decision::Ok);
    }

    #[test]
    fn a_restarted_counter_is_not_a_rise() {
        assert_eq!(err_delta(4096, 12), 0);
        assert_eq!(err_delta(12, 4096), 4084);
        assert_eq!(err_delta(7, 7), 0);
    }

    #[test]
    fn node_states_follow_e13() {
        assert_eq!(node_state(0, false), NodeState::Present);
        assert_eq!(node_state(2, false), NodeState::Present);
        assert_eq!(node_state(3, false), NodeState::Absent);
        assert_eq!(node_state(0, true), NodeState::Modelled);
        // Three missed periods on a vcan peer is still ABSENT: a stand-in that stopped
        // answering has stopped answering.
        assert_eq!(node_state(4, true), NodeState::Absent);
    }

    #[test]
    fn the_names_are_the_grammars_literals() {
        assert_eq!(Decision::Ok.name(), "OK");
        assert_eq!(Decision::Inspect.name(), "INSPECT");
        assert_eq!(NAMES[0], "OK");
        assert_eq!(NODE_TABLE.len(), 3);
    }
}
