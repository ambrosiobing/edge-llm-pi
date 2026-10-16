/* battery.h: the deterministic battery arithmetic of project e16, the part that must
 * not be the model's. Four functions, no state, no clock, no input beyond the profile
 * the PPK2 trace produced and the cell the budget assumes. Every one of them is short
 * enough to read in one sitting, which is the whole reason this is C and not a prompt.
 *
 * The profile comes from one PSM cycle of the SIM7020E, metered by the nRF PPK2 on the
 * module's own supply rail (never on the Raspberry Pi: the instrument cannot source or
 * meter a Pi under load). tools/profile_summarise.py reduces a capture to the four
 * numbers below; tools/make_replay.py writes 100 modelled scenarios in the same shape.
 *
 * The model of a cycle is deliberately the simplest one that can be checked by hand:
 *
 *     charge per cycle = active charge + sleep current x (interval - active window)
 *     charge per day   = charge per cycle x 86400 / interval
 *     life in days     = cell capacity / charge per day
 *
 * and the assumptions it makes are stated here rather than hidden in a constant: no
 * self-discharge, no converter loss, no temperature derating, no capacity lost to the
 * cut-off voltage, and a cell that delivers its full rated charge at the module's
 * supply voltage. A real cell does worse than this function says. The figure is an
 * upper bound and the chapter prints it as one.
 *
 * Monotonicity, which the rule depends on. The rule walks the four intervals from the
 * largest down and stops at the first that meets the target; that is only correct when
 * battery_life_days() does not fall as the interval grows. Differentiating the charge
 * per day with respect to the interval T gives
 *
 *     d(charge per day)/dT = (24 x active_s x sleep_ua - 86400 x active_uah) / T^2
 *
 * so the life rises with the interval exactly when active_uah > active_s x sleep_ua /
 * 3600, that is, when the wake window costs more charge than the same span would cost
 * at the sleep floor. That holds for every profile a radio can produce, and
 * battery_valid_profile() checks it so that a profile which violates it is refused
 * rather than silently searched in the wrong direction.
 */
#ifndef E16_BATTERY_H
#define E16_BATTERY_H

#include "io.h"

#define E16_INTERVALS 4

/* The four allowed sleep intervals in seconds, in the order of the rule's name table
 * and of the grammar's literals: I60, I300, I900, I3600. */
extern const double battery_interval_s[E16_INTERVALS];

typedef struct batt_profile {
    double sleep_ua;       /* PSM floor of the module and its board, microamps */
    double active_uah;     /* charge of one wake window, microamp hours */
    double active_s;       /* length of that wake window, seconds */
    double capacity_mah;   /* the stated cell capacity, milliamp hours */
    double target_days;    /* the battery target the interval must meet */
    double granted_s;      /* the PSM window the network granted, seconds; 0 is none */
} batt_profile;

/* Charge one wake plus the sleep that follows it costs, in microamp hours. An interval
 * shorter than the wake window leaves no sleep and is charged for the wake alone. */
double battery_charge_per_cycle_uah(const batt_profile *p, double interval_s);

/* Cycles in 24 hours at this interval; 0 for a non-positive interval. */
double battery_cycles_per_day(double interval_s);

/* Life in days: the cell's charge divided by the charge one day costs. 0 when the
 * profile would cost nothing per day, which no measured profile does. */
double battery_life_days(const batt_profile *p, double interval_s);

/* 1 when the profile is usable: positive capacity and target, a non-negative floor and
 * window, and the monotonicity condition above. 0 otherwise, and the rule then returns
 * its safe fallback rather than searching. */
int battery_valid_profile(const batt_profile *p);

/* Reads a replay row into a profile: v0 sleep_ua, v1 active_uah, v2 active_s,
 * v3 capacity_mah, v4 target_days, v5 granted_s. Returns 0 on success, -1 when the
 * row carries fewer than six values. */
int battery_profile_from_event(const io_event *ev, batt_profile *p);

#endif /* E16_BATTERY_H */
