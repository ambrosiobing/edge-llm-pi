/* battery.c: the four functions of battery.h. Nothing here reads a file, a clock or a
 * device; every number comes from the profile the caller hands in, so the same input
 * always gives the same answer and src/battery_test.c can check it against arithmetic
 * done by hand. This is the part of project e16 that is deliberately not the model's.
 */
#include "battery.h"

const double battery_interval_s[E16_INTERVALS] = { 60.0, 300.0, 900.0, 3600.0 };

double battery_charge_per_cycle_uah(const batt_profile *p, double interval_s)
{
    double sleep_s = interval_s - p->active_s;   /* the floor covers the rest of the cycle */
    if (sleep_s < 0.0)
        sleep_s = 0.0;                           /* a wake longer than the interval never sleeps */
    return p->active_uah + p->sleep_ua * sleep_s / 3600.0;
}

double battery_cycles_per_day(double interval_s)
{
    return interval_s > 0.0 ? 86400.0 / interval_s : 0.0;
}

double battery_life_days(const batt_profile *p, double interval_s)
{
    double per_day = battery_charge_per_cycle_uah(p, interval_s)
                     * battery_cycles_per_day(interval_s);
    return per_day > 0.0 ? p->capacity_mah * 1000.0 / per_day : 0.0;
}

int battery_valid_profile(const batt_profile *p)
{
    if (p->capacity_mah <= 0.0 || p->target_days <= 0.0)
        return 0;
    if (p->sleep_ua < 0.0 || p->active_uah <= 0.0 || p->active_s < 0.0 || p->granted_s < 0.0)
        return 0;
    /* The monotonicity condition of battery.h: the wake must cost more charge than the
     * same span at the sleep floor, or a longer interval would shorten the life and the
     * rule's largest-first search would stop in the wrong place. */
    return p->active_uah > p->active_s * p->sleep_ua / 3600.0;
}

int battery_profile_from_event(const io_event *ev, batt_profile *p)
{
    if (ev->n < 6)
        return -1;
    p->sleep_ua = ev->v[0];
    p->active_uah = ev->v[1];
    p->active_s = ev->v[2];
    p->capacity_mah = ev->v[3];
    p->target_days = ev->v[4];
    p->granted_s = ev->v[5];
    return 0;
}
