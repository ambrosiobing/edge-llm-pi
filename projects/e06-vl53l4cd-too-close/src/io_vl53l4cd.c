/* io_vl53l4cd.c: the live source of project e06, the VL53L4CD through the ST ULD over
 * i2c-dev. io.h has no hook for a backend a project registers, and io_open_live() in
 * common/c/src/io.c knows only the backends compiled into the core, so this file is not an
 * io_source. It is a function the project's own main() runs in a child process: it opens
 * the sensor, ranges at the asked interval, and writes one CSV row per sample into a pipe
 * in the exact shape the replay backend reads, "t_s,tag,v0,v1" then one row per sample
 * with v0 the distance in mm and v1 the range status. The parent hands the pipe's read
 * end to the shared main as --replay, so the loop, the rule, the log and the agreement
 * table are the same code as on the host with a file. The chapter's Variant chosen
 * paragraph says so.
 *
 * Spec, segments separated by ':' after the bus device, the shape io_i2c.c uses:
 *
 *   "/dev/i2c-1:0x29"                                 10 samples per second, budget 50 ms
 *   "/dev/i2c-1:0x29:interval=0.1:budget=50"
 *
 * interval is the inter-measurement period in seconds; budget the ranging budget in ms,
 * which the ULD bounds to 10 to 200 ms. With the interval longer than the budget the
 * sensor runs in its autonomous mode and the sample rate is the interval; with interval 0
 * it ranges continuously at the budget. Only range status 0 is a valid distance; every
 * sample is written, with its status, and the rule decides what a status other than 0
 * means. t_s is CLOCK_MONOTONIC, the clock the parent compares against for freshness.
 *
 * The ULD's names used here, read from the C ULD in STMicroelectronics/x-cube-tof1 on
 * Wednesday 14 October 2026: VL53L4CD_GetSensorId, VL53L4CD_SensorInit,
 * VL53L4CD_SetRangeTiming, VL53L4CD_StartRanging, VL53L4CD_CheckForDataReady,
 * VL53L4CD_GetResult (into a VL53L4CD_ResultsData_t with range_status and distance_mm),
 * VL53L4CD_ClearInterrupt, VL53L4CD_StopRanging. The standalone STSW-IMG026 is to verify
 * against these on the first build.
 */
#define _GNU_SOURCE
#include "vl53l4cd_platform.h"

#include EDGELLM_VL53L4CD_API_HEADER

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define E06_TAG "vl53l4cd"

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static int write_all(int fd, const char *text)
{
    size_t left = strlen(text);
    while (left > 0) {
        ssize_t n = write(fd, text, left);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        text += n;
        left -= (size_t)n;
    }
    return 0;
}

/* Parses the spec into its parts. Returns 0, or -1 with the reason on stderr. */
static int parse_spec(const char *spec, char *dev, size_t dev_len, int *addr7,
                      double *interval_s, unsigned *budget_ms)
{
    char buf[256], *cur = buf, *seg, *end;
    const char *d, *a;
    snprintf(buf, sizeof(buf), "%s", spec);
    d = strsep(&cur, ":");
    a = strsep(&cur, ":");
    if (a == NULL || (*addr7 = (int)strtol(a, &end, 0), *end != '\0') || *addr7 < 0x03 ||
        *addr7 > 0x77) {
        fprintf(stderr, "vl53l4cd: spec needs '<dev>:<7-bit address>[:interval=S][:budget=MS]'\n");
        return -1;
    }
    snprintf(dev, dev_len, "%s", d);
    while ((seg = strsep(&cur, ":")) != NULL) {
        if (strncmp(seg, "interval=", 9) == 0)
            *interval_s = strtod(seg + 9, NULL);
        else if (strncmp(seg, "budget=", 7) == 0)
            *budget_ms = (unsigned)strtoul(seg + 7, NULL, 0);
        else {
            fprintf(stderr, "vl53l4cd: unknown spec item '%s'\n", seg);
            return -1;
        }
    }
    if (*interval_s < 0.0 || *budget_ms < 10 || *budget_ms > 200) {
        fprintf(stderr, "vl53l4cd: budget must be 10..200 ms and interval 0 or positive\n");
        return -1;
    }
    return 0;
}

/* Ranges until *stop is set or the pipe closes. Returns 0, or 1 when the sensor refused
 * a step, with the step named on stderr. */
int io_vl53l4cd_stream(const char *spec, int fd, volatile sig_atomic_t *stop)
{
    char dev[128], row[96];
    int addr7 = 0x29;
    double interval_s = 0.1;
    unsigned budget_ms = 50, inter_ms;
    uint16_t id = 0;
    Dev_t sensor = (Dev_t)(uintptr_t)VL53L4CD_PLATFORM_DEV;
    VL53L4CD_ResultsData_t res;
    unsigned long n_rows = 0, n_invalid = 0;

    if (parse_spec(spec, dev, sizeof(dev), &addr7, &interval_s, &budget_ms) != 0)
        return 1;
    if (vl53l4cd_platform_open(dev, addr7) != 0)
        return 1;
    if (VL53L4CD_GetSensorId(sensor, &id) != 0) {
        fprintf(stderr, "vl53l4cd: no answer at 0x%02x on %s (sensor id read failed)\n", addr7,
                dev);
        vl53l4cd_platform_close();
        return 1;
    }
    fprintf(stderr, "vl53l4cd: sensor id 0x%04x at 0x%02x on %s\n", id, addr7, dev);
    if (VL53L4CD_SensorInit(sensor) != 0) {
        fprintf(stderr, "vl53l4cd: VL53L4CD_SensorInit refused (boot not seen inside 1 s)\n");
        vl53l4cd_platform_close();
        return 1;
    }
    inter_ms = (unsigned)(interval_s * 1000.0 + 0.5);
    if (inter_ms <= budget_ms)
        inter_ms = 0;                                  /* continuous: the budget sets the rate */
    if (VL53L4CD_SetRangeTiming(sensor, budget_ms, inter_ms) != 0) {
        fprintf(stderr, "vl53l4cd: VL53L4CD_SetRangeTiming(%u, %u) refused\n", budget_ms,
                inter_ms);
        vl53l4cd_platform_close();
        return 1;
    }
    if (VL53L4CD_StartRanging(sensor) != 0) {
        fprintf(stderr, "vl53l4cd: VL53L4CD_StartRanging refused\n");
        vl53l4cd_platform_close();
        return 1;
    }
    if (write_all(fd, "t_s,tag,v0,v1\n") != 0) {
        VL53L4CD_StopRanging(sensor);
        vl53l4cd_platform_close();
        return 1;
    }
    while (!*stop) {
        uint8_t ready = 0;
        double t0 = now_s();
        /* Poll for the sample; give up on this one after four intervals or one second. */
        while (!*stop && !ready) {
            if (VL53L4CD_CheckForDataReady(sensor, &ready) != 0) {
                fprintf(stderr, "vl53l4cd: data ready read failed\n");
                goto done;
            }
            if (!ready) {
                if (now_s() - t0 > (interval_s > 0.25 ? 4.0 * interval_s : 1.0)) {
                    fprintf(stderr, "vl53l4cd: no sample inside the wait, still ranging\n");
                    t0 = now_s();
                }
                usleep(1000);
            }
        }
        if (*stop)
            break;
        if (VL53L4CD_GetResult(sensor, &res) != 0) {
            fprintf(stderr, "vl53l4cd: VL53L4CD_GetResult failed\n");
            goto done;
        }
        VL53L4CD_ClearInterrupt(sensor);
        if (res.range_status != 0)
            n_invalid++;
        snprintf(row, sizeof(row), "%.6f,%s,%u,%u\n", now_s(), E06_TAG,
                 (unsigned)res.distance_mm, (unsigned)res.range_status);
        if (write_all(fd, row) != 0)
            break;                                     /* the loop went away: stop quietly */
        n_rows++;
    }
done:
    VL53L4CD_StopRanging(sensor);
    vl53l4cd_platform_close();
    fprintf(stderr, "vl53l4cd: %lu samples, %lu with a range status other than 0\n", n_rows,
            n_invalid);
    return 0;
}
