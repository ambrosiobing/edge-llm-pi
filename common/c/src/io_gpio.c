/* io_gpio.c: libgpiod 2 lines. Spec, segments separated by ':' after the chip path:
 *
 *   "/dev/gpiochip0:17,27,22"                    three output lines, index 0, 1, 2
 *   "/dev/gpiochip0:17,27,22:in=23:interval=0.5"  plus one input with edge detection
 *
 * io_actuate("led:<index>", value) drives output line <index> high for a nonzero value,
 * because the bench LEDs (Joy-it LK-LED10) carry the anode on the signal pin and light
 * when driven high. "line:<offset>" names the BCM offset directly.
 *
 * io_next: with inputs, it waits up to the interval for an edge and reports it as tag
 * "gpio<offset>", v[0] offset, v[1] 1 rising or 0 falling. Without an edge, and without
 * inputs, it returns one event per interval, tag "gpio", holding the current value of
 * every requested line in order (outputs first, then inputs). Interval default 1 s.
 *
 * The libgpiod v2 API verified at https://github.com/brgl/libgpiod include/gpiod.h on
 * Wednesday 14 October 2026.
 */
#define _GNU_SOURCE
#include "io_internal.h"

#include <gpiod.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GPIO_MAX_LINES 8

typedef struct gpio_priv {
    struct gpiod_chip *chip;
    struct gpiod_line_request *out_req;
    struct gpiod_line_request *in_req;
    struct gpiod_edge_event_buffer *events;
    unsigned int out[GPIO_MAX_LINES];
    unsigned int in[GPIO_MAX_LINES];
    size_t n_out, n_in;
    double interval_s;
    double next_due_s;
} gpio_priv;

static int parse_list(const char *text, unsigned int *out, size_t *n)
{
    char buf[128];
    char *cur = buf, *tok;
    snprintf(buf, sizeof(buf), "%s", text);
    while ((tok = strsep(&cur, ",")) != NULL) {
        char *end;
        unsigned long v;
        if (tok[0] == '\0')
            continue;
        v = strtoul(tok, &end, 10);
        if (*end != '\0' || *n >= GPIO_MAX_LINES)
            return -1;
        out[(*n)++] = (unsigned int)v;
    }
    return 0;
}

static struct gpiod_line_request *request(struct gpiod_chip *chip, const unsigned int *offs,
                                          size_t n, int input)
{
    struct gpiod_line_settings *ls = gpiod_line_settings_new();
    struct gpiod_line_config *lc = gpiod_line_config_new();
    struct gpiod_request_config *rc = gpiod_request_config_new();
    struct gpiod_line_request *req = NULL;

    if (ls == NULL || lc == NULL || rc == NULL)
        goto out;
    if (input) {
        gpiod_line_settings_set_direction(ls, GPIOD_LINE_DIRECTION_INPUT);
        gpiod_line_settings_set_edge_detection(ls, GPIOD_LINE_EDGE_BOTH);
    } else {
        gpiod_line_settings_set_direction(ls, GPIOD_LINE_DIRECTION_OUTPUT);
        gpiod_line_settings_set_output_value(ls, GPIOD_LINE_VALUE_INACTIVE);
    }
    if (gpiod_line_config_add_line_settings(lc, offs, n, ls) != 0)
        goto out;
    gpiod_request_config_set_consumer(rc, "edgellm");
    req = gpiod_chip_request_lines(chip, rc, lc);
out:
    if (rc != NULL)
        gpiod_request_config_free(rc);
    if (lc != NULL)
        gpiod_line_config_free(lc);
    if (ls != NULL)
        gpiod_line_settings_free(ls);
    return req;
}

static void fill_values(gpio_priv *p, io_event *ev)
{
    size_t i;
    for (i = 0; i < p->n_out; i++)
        ev->v[ev->n++] = gpiod_line_request_get_value(p->out_req, p->out[i]) ==
                         GPIOD_LINE_VALUE_ACTIVE;
    for (i = 0; i < p->n_in; i++)
        ev->v[ev->n++] = gpiod_line_request_get_value(p->in_req, p->in[i]) ==
                         GPIOD_LINE_VALUE_ACTIVE;
}

static int gpio_next(io_source *s, io_event *ev)
{
    gpio_priv *p = (gpio_priv *)s->priv;
    double now = io_now_s();

    if (p->in_req != NULL) {
        double wait = p->next_due_s - now;
        int r = gpiod_line_request_wait_edge_events(p->in_req,
                                                    wait > 0.0 ? (int64_t)(wait * 1e9) : 0);
        if (r < 0) {
            fprintf(stderr, "gpio: waiting for an edge failed\n");
            return -1;
        }
        if (r > 0) {
            struct gpiod_edge_event *e;
            if (gpiod_line_request_read_edge_events(p->in_req, p->events, 1) < 1)
                return -1;
            e = gpiod_edge_event_buffer_get_event(p->events, 0);
            io_event_init(ev, "gpio");
            snprintf(ev->tag, sizeof(ev->tag), "gpio%u", gpiod_edge_event_get_line_offset(e));
            ev->v[0] = gpiod_edge_event_get_line_offset(e);
            ev->v[1] = gpiod_edge_event_get_event_type(e) == GPIOD_EDGE_EVENT_RISING_EDGE;
            ev->n = 2;
            return 1;
        }
    } else if (now < p->next_due_s) {
        io_sleep_s(p->next_due_s - now);
    }
    p->next_due_s = io_now_s() + p->interval_s;
    io_event_init(ev, "gpio");
    fill_values(p, ev);
    return 1;
}

static int gpio_actuate(io_source *s, const char *what, int value)
{
    gpio_priv *p = (gpio_priv *)s->priv;
    unsigned int offset;
    char *end;
    enum gpiod_line_value v = value ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;

    if (p->out_req == NULL)
        return -1;
    if (strncmp(what, "led:", 4) == 0) {
        unsigned long idx = strtoul(what + 4, &end, 10);
        if (*end != '\0' || idx >= p->n_out)
            return -1;
        offset = p->out[idx];
    } else if (strncmp(what, "line:", 5) == 0) {
        offset = (unsigned int)strtoul(what + 5, &end, 10);
        if (*end != '\0')
            return -1;
    } else {
        return -1;
    }
    return gpiod_line_request_set_value(p->out_req, offset, v) == 0 ? 0 : -1;
}

static void gpio_close(io_source *s)
{
    gpio_priv *p = (gpio_priv *)s->priv;
    if (p == NULL)
        return;
    if (p->events != NULL)
        gpiod_edge_event_buffer_free(p->events);
    if (p->in_req != NULL)
        gpiod_line_request_release(p->in_req);
    if (p->out_req != NULL)
        gpiod_line_request_release(p->out_req);
    if (p->chip != NULL)
        gpiod_chip_close(p->chip);
    free(p);
}

io_source *io_gpio_open(const char *spec, char *err, size_t err_len)
{
    char buf[256];
    char *cur = buf, *seg;
    const char *path;
    gpio_priv *p = calloc(1, sizeof(*p));
    io_source *s = calloc(1, sizeof(*s));

    if (p == NULL || s == NULL) {
        free(p);
        free(s);
        return io_seterr(err, err_len, "gpio: out of memory"), NULL;
    }
    p->interval_s = 1.0;
    snprintf(buf, sizeof(buf), "%s", spec);
    path = strsep(&cur, ":");
    while ((seg = strsep(&cur, ":")) != NULL) {
        int bad;
        if (strncmp(seg, "in=", 3) == 0)
            bad = parse_list(seg + 3, p->in, &p->n_in);
        else if (strncmp(seg, "interval=", 9) == 0)
            bad = (p->interval_s = strtod(seg + 9, NULL)) <= 0.0;
        else
            bad = parse_list(seg, p->out, &p->n_out);
        if (bad) {
            free(p);
            free(s);
            return io_seterr(err, err_len, "gpio: bad spec segment '%s'", seg), NULL;
        }
    }
    s->name = "gpio";
    s->next = gpio_next;
    s->actuate = gpio_actuate;
    s->close = gpio_close;
    s->priv = p;

    p->chip = gpiod_chip_open(path);
    if (p->chip == NULL) {
        io_close(s);
        return io_seterr(err, err_len, "gpio: cannot open %s", path), NULL;
    }
    if (p->n_out > 0 && (p->out_req = request(p->chip, p->out, p->n_out, 0)) == NULL) {
        io_close(s);
        return io_seterr(err, err_len, "gpio: output request refused on %s", path), NULL;
    }
    if (p->n_in > 0) {
        p->in_req = request(p->chip, p->in, p->n_in, 1);
        p->events = gpiod_edge_event_buffer_new(4);
        if (p->in_req == NULL || p->events == NULL) {
            io_close(s);
            return io_seterr(err, err_len, "gpio: input request refused on %s", path), NULL;
        }
    }
    return s;
}
