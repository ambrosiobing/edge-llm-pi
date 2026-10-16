//! rules: every project's control rule, one type each, a few lines of logic, deterministic,
//! unit tested beside it. The rule is what acts; the model only proposes or explains. A rule
//! returns an index into its own name table so the log and the grammar's literals agree
//! letter for letter, exactly as rules.h fixes it for C.
//!
//! A rule sees the current event and a short history (oldest first) the loop keeps for it;
//! it never sees the model's answer.

use crate::io::Event;

/// How many past events the loop keeps, as RULE_HISTORY in rules.h.
pub const RULE_HISTORY: usize = 16;

/// The history a rule sees: oldest first, at most RULE_HISTORY events. The current event
/// is not in it when the rule runs; the loop pushes it afterwards.
#[derive(Debug, Clone, Default, PartialEq)]
pub struct RuleCtx {
    hist: Vec<Event>,
}

impl RuleCtx {
    pub fn new() -> RuleCtx {
        RuleCtx { hist: Vec::with_capacity(RULE_HISTORY) }
    }

    /// Push ev, dropping the oldest when full.
    pub fn push(&mut self, ev: &Event) {
        if self.hist.len() == RULE_HISTORY {
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
