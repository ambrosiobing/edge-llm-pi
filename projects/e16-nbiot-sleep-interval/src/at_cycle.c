/* at_cycle.c: the plan, the 3GPP timer octets and the +CEREG reader of at_cycle.h.
 * Everything above the live bring-up is a pure function of its arguments, so
 * src/battery_test.c checks the two worked examples of the application note without a
 * module, a SIM or a port.
 */
#define _GNU_SOURCE
#include "at_cycle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef EDGELLM_IO_SERIAL
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#endif

/* Table 1 of the application note, <Requested_Periodic-TAU>: the base of each unit in
 * seconds. Unit 7 is not in the table (3GPP uses it for a deactivated timer), so it is
 * -1 here and the window is read as not granted. */
static const double tau_base_s[8] = { 600.0, 3600.0, 36000.0, 2.0, 30.0, 60.0, 1152000.0, -1.0 };

/* Table 2, <Requested_Active-Time>. The note lists three units only; every other code
 * is refused rather than guessed. */
static const double active_base_s[8] = { 2.0, 60.0, 360.0, -1.0, -1.0, -1.0, -1.0, -1.0 };

static const at_step plan[] = {
    { "AT",              "OK",      2.0, 3, "the port answers at all" },
    { "ATE0",            "OK",      2.0, 1, "echo off, so a reply is a reply" },
    { "AT+CMEE=2",       "OK",      2.0, 1, "errors as text, not as a number" },
    { "AT+IPR=115200",   "OK",      2.0, 1, "fix the baud rate, as the note's demo does" },
    { "AT+CPIN?",        "+CPIN",   5.0, 1, "the SIM is seated and unlocked" },
    { "AT+CSQ",          "+CSQ",    5.0, 1, "signal quality, written into the lab book" },
    { "AT+CGREG?",       "+CGREG", 20.0, 3, "registered, the HAT wiki's own quick test" },
    { "AT+CEREG=4",      "OK",      2.0, 1, "report the granted timers in +CEREG" },
    { "AT+CPSMSTATUS=1", "OK",      2.0, 1, "report ENTER PSM and EXIT PSM" },
    { NULL,              "OK",      5.0, 1, "request the PSM window, T3412_ext and T3324" },
    { "AT+CEREG?",       "+CEREG:", 20.0, 3, "read back what the network granted" },
};

const at_step *at_cycle_plan(int *n)
{
    if (n != NULL)
        *n = (int)(sizeof plan / sizeof plan[0]);
    return plan;
}

const char *at_cycle_text(const at_step *s, const char *tau_bits, const char *act_bits,
                          char *buf, size_t len)
{
    if (s->cmd != NULL)
        snprintf(buf, len, "%s", s->cmd);
    else if (at_cpsms_request(buf, len, tau_bits, act_bits) != 0)
        snprintf(buf, len, "%s", "AT+CPSMS=0");
    return buf;
}

int at_timer_encode(int unit, int value, char out[9])
{
    int i;
    if (unit < 0 || unit > 7 || value < 0 || value > 31)
        return -1;
    for (i = 0; i < 3; i++)
        out[i] = (char)('0' + ((unit >> (2 - i)) & 1));
    for (i = 0; i < 5; i++)
        out[3 + i] = (char)('0' + ((value >> (4 - i)) & 1));
    out[8] = '\0';
    return 0;
}

static double decode(const char *bits, const double *base)
{
    int unit = 0, value = 0, i;
    if (bits == NULL || strlen(bits) != 8)
        return -1.0;
    for (i = 0; i < 8; i++) {
        if (bits[i] != '0' && bits[i] != '1')
            return -1.0;
        if (i < 3)
            unit = unit * 2 + (bits[i] - '0');
        else
            value = value * 2 + (bits[i] - '0');
    }
    return base[unit] < 0.0 ? -1.0 : base[unit] * value;
}

double at_timer_decode_tau(const char *bits)
{
    return decode(bits, tau_base_s);
}

double at_timer_decode_active(const char *bits)
{
    return decode(bits, active_base_s);
}

int at_cpsms_request(char *buf, size_t len, const char *tau_bits, const char *act_bits)
{
    int n;
    if (tau_bits == NULL || act_bits == NULL || strlen(tau_bits) != 8 || strlen(act_bits) != 8)
        return -1;
    n = snprintf(buf, len, "AT+CPSMS=1,,,\"%s\",\"%s\"", tau_bits, act_bits);
    return (n > 0 && (size_t)n < len) ? 0 : -1;
}

int at_cereg_parse(const char *line, at_granted *out)
{
    const char *p = line;
    char last[9] = "", prev[9] = "";
    int found = 0;

    if (line == NULL || out == NULL || strstr(line, "+CEREG") == NULL)
        return -1;
    /* Every eight-character quoted field of '0' and '1' is a timer octet; the active
     * time is the next to last and the periodic TAU the last, per 3GPP TS 27.007 and
     * the application note's section 6.2.1 example. */
    while ((p = strchr(p, '"')) != NULL) {
        const char *end = strchr(p + 1, '"');
        size_t n;
        if (end == NULL)
            break;
        n = (size_t)(end - p - 1);
        if (n == 8 && strspn(p + 1, "01") >= 8) {
            memcpy(prev, last, sizeof prev);
            memcpy(last, p + 1, 8);
            last[8] = '\0';
            found++;
        }
        p = end + 1;
    }
    if (found < 2)
        return -1;
    snprintf(out->active_bits, sizeof out->active_bits, "%s", prev);
    snprintf(out->tau_bits, sizeof out->tau_bits, "%s", last);
    out->active_s = at_timer_decode_active(out->active_bits);
    out->tau_s = at_timer_decode_tau(out->tau_bits);
    return 0;
}

#ifdef EDGELLM_IO_SERIAL

static speed_t baud_code(long baud)
{
    switch (baud) {
    case 9600:   return B9600;
    case 19200:  return B19200;
    case 38400:  return B38400;
    case 57600:  return B57600;
    case 115200: return B115200;
    case 230400: return B230400;
    default:     return B0;
    }
}

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* Opens the port with the settings the HAT wiki gives, 115200 8N1 by default. The
 * project owns the port here rather than going through io.h because it must write as
 * well as read, which common/c/src/io_serial.c says plainly it does not do. */
static int open_port(const char *spec)
{
    char dev[256];
    const char *colon = strrchr(spec, ':');
    struct termios tio;
    speed_t code;
    long baud;
    int fd;

    if (colon == NULL || (baud = strtol(colon + 1, NULL, 10)) <= 0 ||
        (code = baud_code(baud)) == B0)
        return -1;
    snprintf(dev, sizeof dev, "%.*s", (int)(colon - spec), spec);
    fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0)
        return -1;
    if (tcgetattr(fd, &tio) != 0) {
        close(fd);
        return -1;
    }
    cfmakeraw(&tio);
    cfsetispeed(&tio, code);
    cfsetospeed(&tio, code);
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        close(fd);
        return -1;
    }
    tcflush(fd, TCIFLUSH);
    return fd;
}

/* Reads lines until one starts with expect or the deadline passes. Every line read is
 * written to trace, and a +CEREG: line fills granted on the way past. Returns 1 on a
 * match, 0 on the timeout. The deadline is enforced by poll(), so a module that answers
 * nothing stops the step rather than the program. */
static int wait_for(int fd, const char *expect, double deadline, at_granted *granted,
                    FILE *trace)
{
    char line[512];
    size_t fill = 0;

    while (now_s() < deadline) {
        struct pollfd pfd;
        char c;
        int ready;
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        ready = poll(&pfd, 1, (int)((deadline - now_s()) * 1000.0) + 1);
        if (ready <= 0)
            continue;
        if (read(fd, &c, 1) != 1)
            continue;
        if (c != '\n' && c != '\r') {
            if (fill < sizeof line - 1)
                line[fill++] = c;
            continue;
        }
        if (fill == 0)
            continue;
        line[fill] = '\0';
        fill = 0;
        if (trace != NULL)
            fprintf(trace, "    < %s\n", line);
        if (granted != NULL && strstr(line, "+CEREG:") != NULL)
            at_cereg_parse(line, granted);
        if (strncmp(line, expect, strlen(expect)) == 0)
            return 1;
        if (strncmp(line, "ERROR", 5) == 0 || strncmp(line, "+CME ERROR", 10) == 0)
            return 0;
    }
    return 0;
}

int at_cycle_run(const char *spec, const char *tau_bits, const char *act_bits,
                 at_granted *granted, FILE *trace)
{
    const at_step *steps;
    char text[128];
    int n = 0, i, fd;

    steps = at_cycle_plan(&n);
    fd = open_port(spec);
    if (fd < 0)
        return -1;
    if (granted != NULL)
        memset(granted, 0, sizeof *granted);
    for (i = 0; i < n; i++) {
        int attempt;
        at_cycle_text(&steps[i], tau_bits, act_bits, text, sizeof text);
        for (attempt = 0; attempt <= steps[i].retries; attempt++) {
            if (trace != NULL)
                fprintf(trace, "  > %s    (%s)\n", text, steps[i].why);
            if (write(fd, text, strlen(text)) < 0 || write(fd, "\r\n", 2) < 0) {
                close(fd);
                return i + 1;
            }
            if (wait_for(fd, steps[i].expect, now_s() + steps[i].timeout_s, granted, trace))
                break;
            if (trace != NULL)
                fprintf(trace, "    ! no %s within %.0f s, attempt %d of %d\n",
                        steps[i].expect, steps[i].timeout_s, attempt + 1, steps[i].retries + 1);
        }
        if (attempt > steps[i].retries) {
            close(fd);
            return i + 1;
        }
    }
    close(fd);
    return 0;
}

#else /* EDGELLM_IO_SERIAL */

int at_cycle_run(const char *spec, const char *tau_bits, const char *act_bits,
                 at_granted *granted, FILE *trace)
{
    (void)spec;
    (void)tau_bits;
    (void)act_bits;
    (void)granted;
    fprintf(trace != NULL ? trace : stderr,
            "at_cycle: this binary has no serial backend; configure with "
            "-DEDGELLM_IO_SERIAL=ON to use --at\n");
    return -2;
}

#endif /* EDGELLM_IO_SERIAL */
