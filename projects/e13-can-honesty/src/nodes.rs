// nodes.rs: the node table of project e13, and what a node's presence is allowed to mean.
//
// The table is this project's subject. Every node carries a source, and the source decides
// which outline the node is drawn with in every figure of the chapter and which verdict the
// rule may reach for it:
//
//   Source::Hardware  frames arrive from a CAN controller on the WS-28164 HAT     solid
//   Source::Vcan      frames arrive from a virtual CAN interface, no transceiver   dashed
//   Source::Absent    the node is not connected and is listed so it is counted     dotted
//
// The interface a frame arrives on is not a constant. On the WS-28164 the kernel names the
// two controllers in the order their drivers probe, and that order changes between boots
// (seen on the bench on Tuesday 7 October 2026). So Source::Hardware carries the DRIVER
// name, which is stable, and bind_by_driver() reads /sys/class/net/<iface>/device/driver
// for every interface and binds each node to the name the kernel gave it this boot.
// Nothing in this file assumes can0. A replay file cannot swap between boots, so a replay
// run binds the fixed names the generator wrote (bind_for_replay).
//
// The heartbeat identifiers 0x701 to 0x704 are this volume's own, one per node, carrying
// one byte of node state in the shape the joint node volume's chapter 12 uses for a
// periodic state frame. They are not a standard's object numbers.

use std::fs;

use crate::rules::{SOURCE_ABSENT, SOURCE_HARDWARE, SOURCE_VCAN};

/// The heartbeat period of this bus, in seconds. The joint node volume produces a
/// heartbeat every 100 ms with a consumer timeout of 350 ms; this service is ten times
/// slower on purpose, because one model decision on a Pi 4 Model B takes of the order of
/// a second and a service that cannot answer inside its own period is not a service.
pub const HEARTBEAT_PERIOD_S: f64 = 1.0;

/// Three heartbeat periods with nothing heard is ABSENT. One missed frame is noise, two is
/// suspicious, three is a pattern: the joint node volume's choice, kept here unchanged.
pub const ABSENT_AFTER_PERIODS: f64 = 3.0;

#[derive(Clone, Copy, Debug)]
pub enum Source {
    Hardware {
        controller: &'static str,
        driver: &'static str,
        chip_select: &'static str,
        interrupt: &'static str,
    },
    Vcan {
        interface: &'static str,
    },
    Absent {
        reason: &'static str,
    },
}

impl Source {
    pub fn is_hardware(self) -> bool {
        matches!(self, Source::Hardware { .. })
    }

    // Called only by the live reader. allow rather than cfg, because the stub build is the
    // only build any job compiles and gating this out would remove its only compile.
    #[cfg_attr(not(feature = "live"), allow(dead_code))]
    pub fn is_absent(self) -> bool {
        matches!(self, Source::Absent { .. })
    }

    /// The word the chapter, the figures and the log use for this class of source.
    pub fn class(self) -> &'static str {
        match self {
            Source::Hardware { .. } => "hardware controller",
            Source::Vcan { .. } => "vcan replay",
            Source::Absent { .. } => "absent",
        }
    }

    /// The source column of a node-period event, which is the only thing about a source
    /// the rule is allowed to see.
    pub fn code(self) -> f64 {
        match self {
            Source::Hardware { .. } => SOURCE_HARDWARE,
            Source::Vcan { .. } => SOURCE_VCAN,
            Source::Absent { .. } => SOURCE_ABSENT,
        }
    }
}

#[derive(Clone, Debug)]
pub struct Node {
    pub name: &'static str,
    pub heartbeat_id: u32,
    pub source: Source,
    /// The name the generator wrote in the replay file. A replay is one fixed ordering.
    pub replay_iface: &'static str,
    /// The interface this node is bound to, set once by bind_by_driver or bind_for_replay.
    pub iface: Option<String>,
    pub last_heard_s: Option<f64>,
    pub frames: u64,
    pub frames_this_period: u32,
    pub missed_periods: u32,
    pub state_byte: u8,
    /// The rule's index from the previous period, which is how the service notices a
    /// change and decides which node is worth this period's one model decision.
    pub last_verdict: Option<usize>,
}

impl Node {
    fn new(name: &'static str, heartbeat_id: u32, source: Source, replay_iface: &'static str)
           -> Node {
        Node {
            name,
            heartbeat_id,
            source,
            replay_iface,
            iface: None,
            last_heard_s: None,
            frames: 0,
            frames_this_period: 0,
            missed_periods: 0,
            state_byte: 0,
            last_verdict: None,
        }
    }

    /// Seconds since the last frame from this node, or -1.0 when none ever arrived. The
    /// negative value is the event column's and the log's own encoding of "never heard",
    /// so that the rule, the prompt, the log and the oracle all read one number.
    pub fn silent_for(&self, now_s: f64) -> f64 {
        match self.last_heard_s {
            Some(t) => now_s - t,
            None => -1.0,
        }
    }

    /// What the service knows about where this node's frames come from, in the words the
    /// prompt uses. Never the rule's verdict: the model is given the counters, not the
    /// answer.
    pub fn describe_source(&self) -> String {
        match self.source {
            Source::Hardware { controller, .. } => {
                let iface = self.iface.as_deref().unwrap_or("unbound");
                format!("{} {} on {}", self.source.class(), controller, iface)
            }
            Source::Vcan { interface } => format!("{} on {}", self.source.class(), interface),
            Source::Absent { .. } => "not connected".to_string(),
        }
    }
}

/// The four nodes of this bus. Two are the HAT's own controllers, wired to each other with
/// two jumper wires between the CAN FD and the CAN screw terminals; one is a heartbeat
/// generator on a virtual interface with no transceiver; one is not connected at all.
pub fn table() -> Vec<Node> {
    vec![
        Node::new(
            "joint-a",
            0x701,
            Source::Hardware {
                controller: "MCP2518FD",
                driver: "mcp251xfd",
                chip_select: "SPI0 CE1",
                interrupt: "GPIO24",
            },
            "can0",
        ),
        Node::new(
            "joint-b",
            0x702,
            Source::Hardware {
                controller: "MCP2515",
                driver: "mcp251x",
                chip_select: "SPI0 CE0",
                interrupt: "GPIO23",
            },
            "can1",
        ),
        Node::new(
            "joint-c",
            0x703,
            Source::Vcan { interface: "vcan0" },
            "vcan0",
        ),
        Node::new(
            "joint-d",
            0x704,
            Source::Absent {
                reason: "the joint node of the robot joint volume: it is on the shelf, and \
                         the Nucleo end has no CAN transceiver fitted",
            },
            "",
        ),
    ]
}

/// Looks up the node a heartbeat identifier belongs to.
pub fn index_of_id(nodes: &[Node], can_id: u32) -> Option<usize> {
    nodes.iter().position(|n| n.heartbeat_id == can_id)
}

/// Binds the fixed interface names a replay file carries. A file is one ordering and
/// cannot swap, so this is the one place a name is taken on trust, and only for a replay.
pub fn bind_for_replay(nodes: &mut [Node]) {
    for n in nodes.iter_mut() {
        n.iface = if n.replay_iface.is_empty() {
            None
        } else {
            Some(n.replay_iface.to_string())
        };
    }
}

/// Binds every hardware node to the interface whose driver link names its controller's
/// driver, by reading /sys/class/net/<iface>/device/driver. This is the whole answer to
/// "can0 and can1 swap between boots": the driver name does not swap, the interface name
/// does, and the service never prints an interface without the controller beside it.
///
/// Returns one line per hardware node saying what it bound to, for the log and the
/// evidence file. A node whose driver is not present stays unbound and is reported.
#[cfg_attr(not(feature = "live"), allow(dead_code))] // live reader only; see is_absent
pub fn bind_by_driver(nodes: &mut [Node]) -> Result<Vec<String>, String> {
    let mut found: Vec<(String, String)> = Vec::new();
    let entries = fs::read_dir("/sys/class/net")
        .map_err(|e| format!("cannot read /sys/class/net: {}", e))?;
    for entry in entries {
        let entry = entry.map_err(|e| format!("cannot read /sys/class/net: {}", e))?;
        let iface = entry.file_name().to_string_lossy().to_string();
        let link = entry.path().join("device").join("driver");
        if let Ok(target) = fs::read_link(&link) {
            if let Some(driver) = target.file_name() {
                found.push((iface, driver.to_string_lossy().to_string()));
            }
        }
    }
    let mut report = Vec::new();
    for n in nodes.iter_mut() {
        match n.source {
            Source::Hardware {
                controller, driver, chip_select, interrupt,
            } => {
                let hits: Vec<&(String, String)> =
                    found.iter().filter(|(_, d)| d == driver).collect();
                if hits.len() == 1 {
                    n.iface = Some(hits[0].0.clone());
                    report.push(format!(
                        "{}: {} ({}, interrupt {}, driver {}) is {} this boot",
                        n.name, controller, chip_select, interrupt, driver, hits[0].0
                    ));
                } else {
                    report.push(format!(
                        "{}: driver {} matched {} interfaces; node left unbound",
                        n.name, driver, hits.len()
                    ));
                }
            }
            Source::Vcan { interface } => {
                n.iface = Some(interface.to_string());
                report.push(format!("{}: {} is a virtual interface, MODELLED by declaration",
                                    n.name, interface));
            }
            Source::Absent { reason } => {
                report.push(format!("{}: absent, {}", n.name, reason));
            }
        }
    }
    Ok(report)
}
