//! rules: every project's control rule, one type each, a few lines of logic, deterministic,
//! unit tested beside it. The rule is what acts; the model only proposes or explains. A rule
//! returns an index into its own name table so the log and the grammar's literals agree
//! letter for letter, exactly as rules.h fixes it for C.
//!
//! A rule sees the current event and a short history (oldest first) the loop keeps for it;
//! it never sees the model's answer.

use crate::io::Event;

/// How many past events a context made by new() keeps, as RULE_HISTORY in rules.h. It is
/// the default and no longer the only possibility: with_capacity(n) keeps n.
pub const RULE_HISTORY: usize = 16;

/// The history a rule sees: oldest first, at most this context's own capacity. The current
/// event is not in it when the rule runs; the loop pushes it afterwards.
///
/// The capacity became per-context on Sunday 18 October 2026, for e18, whose rule is
/// specified over a thirty-value window where this crate kept sixteen. Taking the
/// compiler's advice on that call and dropping the argument would have halved the window in
/// silence and left e18's own unit test asserting over a history the loop cannot produce.
/// The change is additive: new() keeps RULE_HISTORY as before, so e05, e13 and e14 are
/// untouched, and the C core stays fixed-size until someone wants a capacity field in
/// rules.h to mirror this.
#[derive(Debug, Clone, PartialEq)]
pub struct RuleCtx {
    hist: Vec<Event>,
    cap: usize,
}

/// Hand-written rather than derived: a derived Default would give a capacity of 0, and a
/// context that can hold nothing is not a usable default.
impl Default for RuleCtx {
    fn default() -> RuleCtx {
        RuleCtx::new()
    }
}

impl RuleCtx {
    /// A context keeping RULE_HISTORY events, which is what common/c keeps.
    pub fn new() -> RuleCtx {
        RuleCtx::with_capacity(RULE_HISTORY)
    }

    /// A context keeping n events. A capacity of 0 is raised to 1, so that a context always
    /// holds at least the event just pushed and push can never run off an empty vector.
    pub fn with_capacity(n: usize) -> RuleCtx {
        let cap = if n == 0 { 1 } else { n };
        RuleCtx { hist: Vec::with_capacity(cap), cap }
    }

    /// The capacity this context was made with.
    pub fn capacity(&self) -> usize {
        self.cap
    }

    /// Push ev, dropping the oldest while full. A loop rather than one remove, so that a
    /// context whose capacity is smaller than its current length still converges.
    pub fn push(&mut self, ev: &Event) {
        while self.hist.len() >= self.cap {
            self.hist.remove(0);
        }
        self.hist.push(ev.clone());
    }

    pub fn hist(&self) -> &[Event] {
        &self.hist
    }

    pub fn len(&self) -> usize {
        self.hist.len()
    }

    pub fn is_empty(&self) -> bool {
        self.hist.is_empty()
    }
}

/// A control rule: the project it belongs to, the enum literals its grammar allows, in the
/// same order, and the decision itself, never negative and never past the name table.
pub trait Rule {
    fn project(&self) -> &str;
    fn names(&self) -> &[&'static str];
    fn decide(&self, ctx: &RuleCtx, ev: &Event) -> usize;
}

/// The rules of one executable, looked up by project name as rule_find() does. A Rust
/// binary registers its rule in main() rather than through a constructor; the shape is the
/// same so that a test binary carrying several projects' rules reads like tests/rules_test.
#[derive(Default)]
pub struct Registry {
    rules: Vec<Box<dyn Rule>>,
}

impl Registry {
    pub fn new() -> Registry {
        Registry { rules: Vec::new() }
    }

    pub fn register(&mut self, rule: Box<dyn Rule>) {
        self.rules.push(rule);
    }

    pub fn find(&self, project: &str) -> Option<&dyn Rule> {
        self.rules.iter().find(|r| r.project() == project).map(|r| r.as_ref())
    }

    pub fn count(&self) -> usize {
        self.rules.len()
    }

    pub fn at(&self, index: usize) -> Option<&dyn Rule> {
        self.rules.get(index).map(|r| r.as_ref())
    }
}

/// One test case in the shape of rules_test_expect(): the history, the event, the index
/// wanted, and a sentence naming the case. Panics with the names on both sides.
pub fn expect(rule: &dyn Rule, hist: &[Event], ev: &Event, want: usize, what: &str) {
    let mut ctx = RuleCtx::new();
    for h in hist {
        ctx.push(h);
    }
    let got = rule.decide(&ctx, ev);
    let names = rule.names();
    assert_eq!(
        got,
        want,
        "{}: {}: wanted {} ({}), got {} ({})",
        rule.project(),
        what,
        want,
        names.get(want).copied().unwrap_or("?"),
        got,
        names.get(got).copied().unwrap_or("?")
    );
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Always(usize);

    impl Rule for Always {
        fn project(&self) -> &str {
            "e00"
        }
        fn names(&self) -> &[&'static str] {
            &["A", "B"]
        }
        fn decide(&self, _ctx: &RuleCtx, _ev: &Event) -> usize {
            self.0
        }
    }

    #[test]
    fn the_history_keeps_the_newest_sixteen() {
        let mut ctx = RuleCtx::new();
        for i in 0..20 {
            ctx.push(&Event::new(i as f64, "t", &[i as f64]));
        }
        assert_eq!(ctx.len(), RULE_HISTORY);
        assert_eq!(ctx.hist()[0].t_s, 4.0);
        assert_eq!(ctx.hist()[RULE_HISTORY - 1].t_s, 19.0);
    }

    #[test]
    fn a_thirty_slot_window_keeps_the_newest_thirty() {
        // e18's rule is specified over thirty. Forty pushed, thirty kept, and the oldest
        // kept is the eleventh: the figure that would have silently become sixteen.
        let mut ctx = RuleCtx::with_capacity(30);
        for i in 0..40 {
            ctx.push(&Event::new(i as f64, "t", &[i as f64]));
        }
        assert_eq!(ctx.capacity(), 30);
        assert_eq!(ctx.len(), 30);
        assert_eq!(ctx.hist()[0].t_s, 10.0);
        assert_eq!(ctx.hist()[29].t_s, 39.0);
    }

    #[test]
    fn the_default_and_new_both_keep_the_c_cores_history() {
        // The other direction of the same change: adding a capacity field must not move
        // what every other project gets. Default is hand-written for this reason, because
        // a derived one would hand back a context of capacity 0.
        assert_eq!(RuleCtx::new().capacity(), RULE_HISTORY);
        assert_eq!(RuleCtx::default().capacity(), RULE_HISTORY);
    }

    #[test]
    fn a_capacity_of_zero_still_holds_the_event_just_pushed() {
        let mut ctx = RuleCtx::with_capacity(0);
        ctx.push(&Event::new(1.0, "t", &[1.0]));
        assert_eq!(ctx.len(), 1);
        assert_eq!(ctx.hist()[0].t_s, 1.0);
    }

    #[test]
    fn a_registry_finds_a_rule_by_project() {
        let mut reg = Registry::new();
        reg.register(Box::new(Always(1)));
        assert_eq!(reg.count(), 1);
        assert_eq!(reg.find("e00").unwrap().names(), &["A", "B"]);
        assert!(reg.find("e99").is_none());
        assert_eq!(reg.at(0).unwrap().project(), "e00");
        expect(reg.at(0).unwrap(), &[], &Event::default(), 1, "always B");
    }

    #[test]
    #[should_panic]
    fn expect_panics_on_a_wrong_index() {
        expect(&Always(0), &[], &Event::default(), 1, "wanted B, the rule says A");
    }
}
