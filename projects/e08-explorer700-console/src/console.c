/* console.c: the console half of project e08, catalogue variant O. One process owns
 * the JOY-iT RB-Explorer700 HAT and never the model: it reads the five joystick
 * contacts through libgpiod 2, the DS3231 and the BMP280 through sysfs, draws on the
 * SSD1306 through the framebuffer the kernel provides, and asks the separate daemon
 * (src/daemon.c) for one sentence over a Unix domain socket. The rule decides the
 * clock state and sounds the buzzer; the model's answer reaches the OLED and the log
 * and nothing else.
 *
 *   edgellm-e08-console --project e08 (--replay <csv> | --live)
 *                       --grammar <gbnf> --prompt <txt> --log <csv>
 *                       [--socket <path> | --daemon <exe>] [--model <gguf>] [--stub]
 *                       [--threads N] [--n-predict N] [--n-ctx N] [--temperature T]
 *                       [--template chatml|zephyr|none] [--interval S] [--max-events N]
 *                       [--retry S] [--retry-max S]
 *                       [--chip <gpiochip>] [--expander <gpiochip>] [--fb <dev>]
 *                       [--iio <dir>] [--rtc <dir>]
 *
 * --replay reads events from a CSV through io.h exactly as every other project does,
 * and touches no hardware; --live reads the HAT. --daemon starts the daemon itself on
 * a socket in a temporary directory, which is how the oracle test runs in CI;
 * --socket connects to one systemd already started. Neither given, the socket is
 * /run/edgellm/e08.sock. The arguments --grammar, --prompt, --model, --threads,
 * --n-predict, --n-ctx, --temperature, --template and --stub are only passed on to a
 * daemon this process starts; a daemon started by systemd carries its own.
 *
 * Event layout, the same in replay and live: tag "explorer700", v[0] the question
 * index 0 to 4, v[1] the DS3231's seconds since the epoch from
 * /sys/class/rtc/rtc0/since_epoch, v[2] the Pi's own seconds since the epoch, v[3] the
 * BMP280 pressure in kPa from in_pressure_input, v[4] its temperature in C from
 * in_temp_input (milli degrees, divided here).
 *
 * A request the daemon does not answer (connect refused, a short read) is retried
 * every --retry seconds (0.5) for at most --retry-max seconds (120) on the same event,
 * and the time from the first lost reply to the first good one, by this process's
 * CLOCK_MONOTONIC, is printed as the recovery figure. That figure is the measurement
 * variant O buys, and it is taken with the daemon stopped by signal 9 under systemd.
 *
 * The live half deviates from the house rule that hardware is reached only through
 * io.h, and says so: the joystick spans two GPIO chips (four contacts on the PCF8574
 * expander, the push contact on the SoC), which io_gpio's one-chip spec cannot name,
 * and no backend of io.h draws a framebuffer. The deviation is confined to the
 * functions under EDGELLM_E08_LIVE below; the replay path is io.h as everywhere.
 */
#define _GNU_SOURCE
#include "edgellm.h"
#include "elog.h"
#include "io.h"
#include "rules.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifdef EDGELLM_E08_LIVE
#include <gpiod.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include "font5x7.h"
#endif

#define N_QUESTIONS 5
#define PROMPT_BUF 1024
#define REPLY_BUF 4096
#define OLED_LINES 8
#define OLED_COLS 21

static const char *const question_text[N_QUESTIONS] = {
    "what is the pressure trend",
    "is the clock drifting",
    "how warm is the box",
    "what time is it by the RTC",
    "summarise the last hour",
};

static const char *const question_label[N_QUESTIONS] = {
    "PRESSURE TREND", "CLOCK DRIFT", "BOX TEMPERATURE", "RTC TIME", "LAST HOUR",
};

typedef struct opts {
    const char *project, *replay, *grammar, *prompt, *log, *socket_path, *daemon_exe;
    const char *model, *template_name, *chip, *expander, *fb, *iio, *rtc;
    int live, want_stub, threads, n_predict, n_ctx, max_events;
    double temperature, interval, retry, retry_max;
} opts;

static volatile sig_atomic_t stop_requested;

static void on_signal(int sig)
{
    (void)sig;
    stop_requested = 1;
}

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void sleep_s(double seconds)
{
    struct timespec ts;
    if (seconds <= 0.0)
        return;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
}

static int usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s --project e08 (--replay <csv> | --live)\n"
            "          --grammar <gbnf> --prompt <txt> --log <csv>\n"
            "          [--socket <path> | --daemon <exe>] [--model <gguf>] [--stub]\n"
            "          [--threads N] [--n-predict N] [--n-ctx N] [--temperature T]\n"
            "          [--template chatml|zephyr|none] [--interval S] [--max-events N]\n"
            "          [--retry S] [--retry-max S] [--chip <gpiochip>]\n"
            "          [--expander <gpiochip>] [--fb <dev>] [--iio <dir>] [--rtc <dir>]\n",
            argv0);
    return 2;
}

/* ------------------------------------------------------------------ the socket client */

static int write_all(int fd, const char *text, size_t len)
{
    while (len > 0) {
        ssize_t n = write(fd, text, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        text += n;
        len -= (size_t)n;
    }
    return 0;
}

static int read_line(int fd, char *buf, size_t len)
{
    size_t k = 0;
    for (;;) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            return -1;
        if (c == '\n') {
            buf[k] = '\0';
            return 0;
        }
        if (k + 1 >= len)
            return -1;
        buf[k++] = c;
    }
}

static int sock_connect(const char *path)
{
    struct sockaddr_un addr;
    struct timeval tv = { 120, 0 };
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return -1;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (strlen(path) >= sizeof(addr.sun_path)) {
        close(fd);
        return -1;
    }
    strcpy(addr.sun_path, path);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    return fd;
}

/* One exchange: the request line out, one reply line back. Returns 0 on a reply. */
static int sock_exchange(int fd, const char *request, char *reply, size_t len)
{
    char nl = '\n';
    if (write_all(fd, request, strlen(request)) != 0 || write_all(fd, &nl, 1) != 0)
        return -1;
    return read_line(fd, reply, len);
}

static int daemon_version(const char *path, char *out, size_t len)
{
    char reply[256];
    int fd = sock_connect(path);
    if (fd < 0)
        return -1;
    if (sock_exchange(fd, "VERSION", reply, sizeof(reply)) != 0 ||
        strncmp(reply, "VERSION ", 8) != 0) {
        close(fd);
        return -1;
    }
    close(fd);
    snprintf(out, len, "%s", reply + 8);
    return 0;
}

/* DECIDE then STATS on one connection. Returns 0 with res filled, -1 when the daemon is
 * away (no connection, short read), -2 when it answered ERROR. */
static int daemon_decide(const char *path, const char *user, edgellm_result *res, char *reply)
{
    char request[PROMPT_BUF + 8];
    int fd = sock_connect(path);
    if (fd < 0)
        return -1;
    snprintf(request, sizeof(request), "DECIDE %s", user);
    memset(res, 0, sizeof(*res));
    if (sock_exchange(fd, request, reply, REPLY_BUF) != 0) {
        close(fd);
        return -1;
    }
    if (strncmp(reply, "ERROR ", 6) == 0) {
        close(fd);
        return -2;
    }
    snprintf(res->text, sizeof(res->text), "%s", reply);
    if (sock_exchange(fd, "STATS", reply, REPLY_BUF) == 0)
        sscanf(reply, "STATS %lf %lf %d", &res->prompt_ms, &res->generate_ms, &res->n_generated);
    close(fd);
    return 0;
}

/* ------------------------------------------------------------------ the spawned daemon */

static pid_t spawn_daemon(const opts *o, char *dir, size_t dir_len, char *sock, size_t sock_len)
{
    const char *argv[32];
    char threads[16], n_predict[16], n_ctx[16], temperature[16];
    int k = 0;
    pid_t pid;
    double deadline;
    char version[64];

    snprintf(dir, dir_len, "/tmp/edgellm-e08-XXXXXX");
    if (mkdtemp(dir) == NULL)
        return -1;
    snprintf(sock, sock_len, "%s/e08.sock", dir);
    snprintf(threads, sizeof(threads), "%d", o->threads);
    snprintf(n_predict, sizeof(n_predict), "%d", o->n_predict);
    snprintf(n_ctx, sizeof(n_ctx), "%d", o->n_ctx);
    snprintf(temperature, sizeof(temperature), "%.3f", o->temperature);
    argv[k++] = o->daemon_exe;
    argv[k++] = "--socket";      argv[k++] = sock;
    argv[k++] = "--grammar";     argv[k++] = o->grammar;
    argv[k++] = "--prompt";      argv[k++] = o->prompt;
    argv[k++] = "--threads";     argv[k++] = threads;
    argv[k++] = "--n-predict";   argv[k++] = n_predict;
    argv[k++] = "--n-ctx";       argv[k++] = n_ctx;
    argv[k++] = "--temperature"; argv[k++] = temperature;
    argv[k++] = "--template";    argv[k++] = o->template_name;
    if (o->model != NULL) { argv[k++] = "--model"; argv[k++] = o->model; }
    if (o->want_stub) argv[k++] = "--stub";
    argv[k] = NULL;

    pid = fork();
    if (pid < 0)
        return -1;
    if (pid == 0) {
        execv(o->daemon_exe, (char *const *)argv);
        _exit(127);
    }
    deadline = now_s() + 60.0;                /* a real model file takes seconds to load */
    while (now_s() < deadline) {
        int status;
        if (daemon_version(sock, version, sizeof(version)) == 0)
            return pid;
        if (waitpid(pid, &status, WNOHANG) == pid)
            return -1;                        /* it ended before it listened */
        sleep_s(0.05);
    }
    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
    return -1;
}

static void stop_daemon(pid_t pid, const char *dir, const char *sock)
{
    if (pid > 0) {
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
    }
    if (sock[0] != '\0')
        unlink(sock);
    if (dir[0] != '\0')
        rmdir(dir);
}

/* ------------------------------------------------------------------ the user text */

static void hms_utc(double epoch_s, char *out, size_t len)
{
    time_t t = (time_t)epoch_s;
    struct tm tm;
    gmtime_r(&t, &tm);
    snprintf(out, len, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
}

/* The user text the daemon gets, after the chat template is added there. It names the
 * question, both clocks, the difference, the rule's state, the pressure with the
 * previous pressure from the history, and the temperature; nothing else. */
static const char *format_prompt(const io_event *ev, const rule_ctx *ctx, const char *state,
                                 char *buf, size_t len)
{
    char rtc[16], pi[16];
    int q = (int)ev->v[0];
    double prev = ctx->n > 0 && ctx->hist[ctx->n - 1].n > 3 ? ctx->hist[ctx->n - 1].v[3]
                                                            : ev->v[3];
    if (q < 0 || q >= N_QUESTIONS)
        q = 0;
    hms_utc(ev->v[1], rtc, sizeof(rtc));
    hms_utc(ev->v[2], pi, sizeof(pi));
    snprintf(buf, len,
             "Question: %s. RTC %s UTC, Pi %s UTC, difference %.1f s, clock %s. "
             "Pressure %.2f kPa, previous %.2f kPa. Temperature %.1f C. Answer.",
             question_text[q], rtc, pi, ev->v[1] - ev->v[2], state, ev->v[3], prev,
             ev->v[4]);
    return buf;
}

/* ------------------------------------------------------------------ the live HAT */

#ifdef EDGELLM_E08_LIVE

/* Joystick, expander lines P0 to P3 and SoC GPIO20, from the JOY-iT manual's page 3
 * block diagram (read Wednesday 14 October 2026): net A is Left on P0, B is Up on P1,
 * C is Down on P2, D is Right on P3, Push is GPIO20. The buzzer is net Buzz on P7.
 * Every contact closes to ground, so every line is active low; the expander's
 * quasi-bidirectional lines idle high by themselves and GPIO20 gets the SoC pull-up.
 * The direction mapping is read off a rendered page and is the least certain number
 * in this file: a permuted joystick works and is wrong. evtest or a thumb on the
 * bench settles it. */
#define JOY_LEFT 0
#define JOY_UP 1
#define JOY_DOWN 2
#define JOY_RIGHT 3
#define JOY_PRESS 4
#define EXPANDER_LABEL "pcf8574"
#define PRESS_OFFSET 20u
#define BUZZER_OFFSET 7u
#define BUZZER_MS 150
#define POLL_MS 25

typedef struct hat {
    struct gpiod_chip *soc, *exp;
    struct gpiod_line_request *dirs, *press, *buzz;
    int fb;
    unsigned fb_w, fb_h, fb_bpp, fb_stride;
    unsigned char *frame;
    size_t frame_len;
    char iio_dir[256], rtc_dir[256];
    int was[5];
} hat;

static int read_text_file(const char *path, char *out, size_t len)
{
    FILE *fp = fopen(path, "r");
    size_t n;
    if (fp == NULL)
        return -1;
    n = fread(out, 1, len - 1, fp);
    fclose(fp);
    out[n] = '\0';
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' '))
        out[--n] = '\0';
    return 0;
}

static int read_number(const char *dir, const char *leaf, double *value)
{
    char path[512], text[64], *end;
    snprintf(path, sizeof(path), "%s/%s", dir, leaf);
    if (read_text_file(path, text, sizeof(text)) != 0)
        return -1;
    *value = strtod(text, &end);
    return end == text ? -1 : 0;
}

/* The framebuffer whose driver name mentions an SSD13xx, found by name because the
 * index follows probe order: with HDMI absent the panel can be fb0. */
static int find_fb(char *out, size_t len)
{
    char path[128], name[64];
    int i;
    for (i = 0; i < 8; i++) {
        snprintf(path, sizeof(path), "/sys/class/graphics/fb%d/name", i);
        if (read_text_file(path, name, sizeof(name)) != 0)
            continue;
        if (strstr(name, "ssd13") != NULL) {
            snprintf(out, len, "/dev/fb%d", i);
            return 0;
        }
    }
    return -1;
}

static int find_iio(char *out, size_t len)
{
    char path[128], name[64];
    int i;
    for (i = 0; i < 16; i++) {
        snprintf(path, sizeof(path), "/sys/bus/iio/devices/iio:device%d/name", i);
        if (read_text_file(path, name, sizeof(name)) != 0)
            continue;
        if (strcmp(name, "bmp280") == 0 || strcmp(name, "bmp180") == 0 ||
            strcmp(name, "bme280") == 0) {
            snprintf(out, len, "/sys/bus/iio/devices/iio:device%d", i);
            return 0;
        }
    }
    return -1;
}

/* The expander's gpiochip, found by its label, which gpio-pcf857x sets from the I2C
 * client name "pcf8574"; the number after gpiochip follows probe order. */
static int find_expander(char *out, size_t len)
{
    int i;
    for (i = 0; i < 16; i++) {
        char path[32];
        struct gpiod_chip *chip;
        struct gpiod_chip_info *info;
        int hit = 0;
        snprintf(path, sizeof(path), "/dev/gpiochip%d", i);
        chip = gpiod_chip_open(path);
        if (chip == NULL)
            continue;
        info = gpiod_chip_get_info(chip);
        if (info != NULL) {
            hit = strcmp(gpiod_chip_info_get_label(info), EXPANDER_LABEL) == 0;
            gpiod_chip_info_free(info);
        }
        gpiod_chip_close(chip);
        if (hit) {
            snprintf(out, len, "%s", path);
            return 0;
        }
    }
    return -1;
}

static struct gpiod_line_request *request_lines(struct gpiod_chip *chip, const unsigned *offs,
                                                size_t n, int input, int pull_up)
{
    struct gpiod_line_settings *ls = gpiod_line_settings_new();
    struct gpiod_line_config *lc = gpiod_line_config_new();
    struct gpiod_request_config *rc = gpiod_request_config_new();
    struct gpiod_line_request *req = NULL;
    if (ls == NULL || lc == NULL || rc == NULL)
        goto out;
    if (input) {
        gpiod_line_settings_set_direction(ls, GPIOD_LINE_DIRECTION_INPUT);
        gpiod_line_settings_set_active_low(ls, true);
        if (pull_up)
            gpiod_line_settings_set_bias(ls, GPIOD_LINE_BIAS_PULL_UP);
    } else {
        gpiod_line_settings_set_direction(ls, GPIOD_LINE_DIRECTION_OUTPUT);
        gpiod_line_settings_set_output_value(ls, GPIOD_LINE_VALUE_INACTIVE);
    }
    if (gpiod_line_config_add_line_settings(lc, offs, n, ls) != 0)
        goto out;
    gpiod_request_config_set_consumer(rc, "edgellm-e08");
    req = gpiod_chip_request_lines(chip, rc, lc);
out:
    if (rc != NULL) gpiod_request_config_free(rc);
    if (lc != NULL) gpiod_line_config_free(lc);
    if (ls != NULL) gpiod_line_settings_free(ls);
    return req;
}

static void hat_close(hat *h)
{
    if (h->buzz != NULL) {
        gpiod_line_request_set_value(h->buzz, BUZZER_OFFSET, GPIOD_LINE_VALUE_INACTIVE);
        gpiod_line_request_release(h->buzz);
    }
    if (h->press != NULL) gpiod_line_request_release(h->press);
    if (h->dirs != NULL) gpiod_line_request_release(h->dirs);
    if (h->exp != NULL) gpiod_chip_close(h->exp);
    if (h->soc != NULL) gpiod_chip_close(h->soc);
    if (h->fb >= 0) close(h->fb);
    free(h->frame);
    memset(h, 0, sizeof(*h));
    h->fb = -1;
}

static int hat_open(hat *h, const opts *o)
{
    static const unsigned dir_offs[4] = { 0u, 1u, 2u, 3u };
    static const unsigned press_offs[1] = { PRESS_OFFSET };
    static const unsigned buzz_offs[1] = { BUZZER_OFFSET };
    char expander[64], fbdev[64];
    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;

    memset(h, 0, sizeof(*h));
    h->fb = -1;
    if (o->expander != NULL)
        snprintf(expander, sizeof(expander), "%s", o->expander);
    else if (find_expander(expander, sizeof(expander)) != 0) {
        fprintf(stderr, "e08: no gpiochip labelled %s; is dtoverlay=pcf857x,pcf8574,addr=0x20 "
                        "in config.txt? Name it with --expander\n", EXPANDER_LABEL);
        return -1;
    }
    if (o->fb != NULL)
        snprintf(fbdev, sizeof(fbdev), "%s", o->fb);
    else if (find_fb(fbdev, sizeof(fbdev)) != 0) {
        fprintf(stderr, "e08: no framebuffer named ssd13xx under /sys/class/graphics; is "
                        "dtoverlay=ssd1306-spi,dc_pin=16,reset_pin=19 in config.txt?\n");
        return -1;
    }
    if (o->iio != NULL)
        snprintf(h->iio_dir, sizeof(h->iio_dir), "%s", o->iio);
    else if (find_iio(h->iio_dir, sizeof(h->iio_dir)) != 0) {
        fprintf(stderr, "e08: no IIO device named bmp280; is dtoverlay=i2c-sensor,bmp280,"
                        "addr=0x76 in config.txt?\n");
        return -1;
    }
    snprintf(h->rtc_dir, sizeof(h->rtc_dir), "%s",
             o->rtc != NULL ? o->rtc : "/sys/class/rtc/rtc0");

    h->soc = gpiod_chip_open(o->chip != NULL ? o->chip : "/dev/gpiochip0");
    h->exp = gpiod_chip_open(expander);
    if (h->soc == NULL || h->exp == NULL) {
        fprintf(stderr, "e08: cannot open the GPIO chips\n");
        hat_close(h);
        return -1;
    }
    h->dirs = request_lines(h->exp, dir_offs, 4, 1, 0);
    h->press = request_lines(h->soc, press_offs, 1, 1, 1);
    h->buzz = request_lines(h->exp, buzz_offs, 1, 0, 0);
    if (h->dirs == NULL || h->press == NULL || h->buzz == NULL) {
        fprintf(stderr, "e08: a line request was refused (joystick P0 to P3, GPIO20, buzzer "
                        "P7); is another consumer holding them?\n");
        hat_close(h);
        return -1;
    }
    h->fb = open(fbdev, O_RDWR | O_CLOEXEC);
    if (h->fb < 0 || ioctl(h->fb, FBIOGET_VSCREENINFO, &var) != 0 ||
        ioctl(h->fb, FBIOGET_FSCREENINFO, &fix) != 0) {
        fprintf(stderr, "e08: cannot open %s\n", fbdev);
        hat_close(h);
        return -1;
    }
    h->fb_w = var.xres;
    h->fb_h = var.yres;
    h->fb_bpp = var.bits_per_pixel;
    h->fb_stride = fix.line_length;
    if (h->fb_bpp != 32 && h->fb_bpp != 16 && h->fb_bpp != 1) {
        fprintf(stderr, "e08: %s is %u bits per pixel; this console draws 32, 16 or 1\n",
                fbdev, h->fb_bpp);
        hat_close(h);
        return -1;
    }
    h->frame_len = (size_t)h->fb_stride * h->fb_h;
    h->frame = calloc(1, h->frame_len);
    if (h->frame == NULL) {
        hat_close(h);
        return -1;
    }
    fprintf(stderr, "e08: HAT open: expander %s, press on GPIO%u, %s %ux%u at %u bpp, %s, %s\n",
            expander, PRESS_OFFSET, fbdev, h->fb_w, h->fb_h, h->fb_bpp, h->iio_dir, h->rtc_dir);
    return 0;
}

static void put_pixel(hat *h, unsigned x, unsigned y)
{
    unsigned char *row = h->frame + (size_t)y * h->fb_stride;
    if (x >= h->fb_w || y >= h->fb_h)
        return;
    if (h->fb_bpp == 32) {
        row[4 * x] = 0xFF; row[4 * x + 1] = 0xFF; row[4 * x + 2] = 0xFF; row[4 * x + 3] = 0x00;
    } else if (h->fb_bpp == 16) {
        row[2 * x] = 0xFF; row[2 * x + 1] = 0xFF;
    } else {
        row[x / 8] |= (unsigned char)(0x80 >> (x % 8));
    }
}

/* Up to OLED_LINES lines of OLED_COLS characters; the whole frame is rewritten so
 * nothing from a previous program survives on the panel. */
static int oled_draw(hat *h, const char *const *lines, int n_lines)
{
    int r, c, dx, dy;
    memset(h->frame, 0, h->frame_len);
    for (r = 0; r < n_lines && r < OLED_LINES; r++) {
        const char *text = lines[r] != NULL ? lines[r] : "";
        for (c = 0; c < OLED_COLS && text[c] != '\0'; c++) {
            const unsigned char *g = font5x7_glyph((unsigned char)text[c]);
            for (dx = 0; dx < FONT5X7_W; dx++)
                for (dy = 0; dy < FONT5X7_H; dy++)
                    if (g[dx] & (1u << dy))
                        put_pixel(h, (unsigned)(c * FONT5X7_CELL_W + dx),
                                  (unsigned)(r * FONT5X7_CELL_H + dy));
        }
    }
    return pwrite(h->fb, h->frame, h->frame_len, 0) == (ssize_t)h->frame_len ? 0 : -1;
}

/* Polls the five contacts every POLL_MS and returns the index of a contact that just
 * closed, or -1 when stop_requested. Edge detection is not used: the expander's lines
 * have no interrupt without the INT wire, and polling at 25 ms is what gpio-keys-polled
 * would do with the same lines. */
static int joystick_wait(hat *h)
{
    while (!stop_requested) {
        int now[5], i;
        for (i = 0; i < 4; i++)
            now[i] = gpiod_line_request_get_value(h->dirs, (unsigned)i) == GPIOD_LINE_VALUE_ACTIVE;
        now[4] = gpiod_line_request_get_value(h->press, PRESS_OFFSET) == GPIOD_LINE_VALUE_ACTIVE;
        for (i = 0; i < 5; i++) {
            int pressed = now[i] && !h->was[i];
            h->was[i] = now[i];
            if (pressed)
                return i;
        }
        sleep_s(POLL_MS / 1000.0);
    }
    return -1;
}

static int sensors_read(hat *h, int q, io_event *ev)
{
    double rtc_s, p_kpa, t_milli;
    if (read_number(h->rtc_dir, "since_epoch", &rtc_s) != 0 ||
        read_number(h->iio_dir, "in_pressure_input", &p_kpa) != 0 ||
        read_number(h->iio_dir, "in_temp_input", &t_milli) != 0)
        return -1;
    memset(ev, 0, sizeof(*ev));
    ev->t_s = now_s();
    snprintf(ev->tag, sizeof(ev->tag), "explorer700");
    ev->v[0] = q;
    ev->v[1] = rtc_s;
    ev->v[2] = (double)time(NULL);
    ev->v[3] = p_kpa;
    ev->v[4] = t_milli / 1000.0;
    ev->n = 5;
    return 0;
}

#endif /* EDGELLM_E08_LIVE */

/* ------------------------------------------------------------------ the rule's output */

/* The ONLY place that writes the buzzer. It is called with the rule's index, before the
 * daemon is asked, and it sounds the buzzer once, for BUZZER_MS, on the transition into
 * LOST; a run of LOST events sounds it once, and nothing about the model's answer can
 * reach this function. In a replay run there is no HAT and the call is counted. */
static int console_actuate(void *hat_or_null, int rule_index, int *last_index, long *n_buzz)
{
    int entering_lost = rule_index == 2 && *last_index != 2;
    *last_index = rule_index;
    if (!entering_lost)
        return 0;
    (*n_buzz)++;
#ifdef EDGELLM_E08_LIVE
    if (hat_or_null != NULL) {
        hat *h = (hat *)hat_or_null;
        if (gpiod_line_request_set_value(h->buzz, BUZZER_OFFSET, GPIOD_LINE_VALUE_ACTIVE) != 0)
            return -1;
        sleep_s(BUZZER_MS / 1000.0);
        return gpiod_line_request_set_value(h->buzz, BUZZER_OFFSET, GPIOD_LINE_VALUE_INACTIVE);
    }
#else
    (void)hat_or_null;
#endif
    return 0;
}

/* ------------------------------------------------------------------ one event */

typedef struct run {
    const opts *o;
    const rule_desc *rule;
    rule_ctx ctx;
    elog *log;
    void *hat;
    char *prompt_buf, *reply_buf;
    int last_index;
    long n_events, n_model, n_lost_events, n_buzz;
    double lost_since_s, sum_prompt_ms, sum_generate_ms, last_recovery_s;
    char answer[512];
    const char *state;
} run;

/* Pulls the sentence out of {"answer":"..."} for the OLED; the log keeps the object. */
static void answer_text(const char *json, char *out, size_t len)
{
    const char *p = strstr(json, "\"answer\"");
    size_t k = 0;
    if (p != NULL)
        p = strchr(p + 8, '"');
    if (p == NULL) {
        snprintf(out, len, "%s", json);
        return;
    }
    for (p++; *p != '\0' && *p != '"' && k + 1 < len; p++)
        out[k++] = *p;
    out[k] = '\0';
}

static int process_event(run *r, const io_event *ev)
{
    int idx = r->rule->fn(&r->ctx, ev);
    int n_names = rule_name_count(r->rule);
    edgellm_result res;
    const edgellm_result *logged = NULL;
    const char *user;
    double t_first_try;

    if (idx < 0 || idx >= n_names) {
        fprintf(stderr, "e08: rule returned %d outside its %d names\n", idx, n_names);
        return -1;
    }
    r->state = r->rule->names[idx];
    if (console_actuate(r->hat, idx, &r->last_index, &r->n_buzz) != 0)
        fprintf(stderr, "e08: buzzer write failed\n");

    user = format_prompt(ev, &r->ctx, r->state, r->prompt_buf, PROMPT_BUF);
    t_first_try = now_s();
    r->answer[0] = '\0';
    for (;;) {
        int rc = daemon_decide(r->o->socket_path, user, &res, r->reply_buf);
        if (rc == 0) {
            if (r->lost_since_s > 0.0) {
                r->last_recovery_s = now_s() - r->lost_since_s;
                fprintf(stderr, "e08: daemon recovered after %.3f s (lost reply to first good "
                                "one, CLOCK_MONOTONIC of the console)\n", r->last_recovery_s);
                r->lost_since_s = 0.0;
            }
            logged = &res;
            r->n_model++;
            r->sum_prompt_ms += res.prompt_ms;
            r->sum_generate_ms += res.generate_ms;
            answer_text(res.text, r->answer, sizeof(r->answer));
            break;
        }
        if (rc == -2) {
            fprintf(stderr, "e08: daemon answered ERROR: %s\n", r->reply_buf);
            memset(&res, 0, sizeof(res));
            logged = &res;
            break;
        }
        if (r->lost_since_s <= 0.0) {
            r->lost_since_s = now_s();
            r->n_lost_events++;
            fprintf(stderr, "e08: daemon away at t=%.3f, retrying every %.1f s\n",
                    r->lost_since_s, r->o->retry);
        }
        if (stop_requested || now_s() - t_first_try > r->o->retry_max) {
            snprintf(r->answer, sizeof(r->answer), "MODEL AWAY");
            break;
        }
        sleep_s(r->o->retry);
    }
    if (elog_row(r->log, ev, r->state, logged) != 0) {
        fprintf(stderr, "e08: log write failed\n");
        return -1;
    }
    rule_ctx_push(&r->ctx, ev);
    r->n_events++;
    return 0;
}

/* ------------------------------------------------------------------ main */

static int parse(opts *o, int argc, char **argv)
{
    int i;
    memset(o, 0, sizeof(*o));
    o->template_name = "chatml";
    o->threads = 4;
    o->n_predict = 64;
    o->n_ctx = 512;
    o->temperature = 0.3;
    o->retry = 0.5;
    o->retry_max = 120.0;
    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = (i + 1 < argc) ? argv[i + 1] : NULL;
        if (strcmp(a, "--stub") == 0) { o->want_stub = 1; continue; }
        if (strcmp(a, "--live") == 0) { o->live = 1; continue; }
        if (v == NULL)
            return -1;
        if (strcmp(a, "--project") == 0) o->project = v;
        else if (strcmp(a, "--replay") == 0) o->replay = v;
        else if (strcmp(a, "--grammar") == 0) o->grammar = v;
        else if (strcmp(a, "--prompt") == 0) o->prompt = v;
        else if (strcmp(a, "--log") == 0) o->log = v;
        else if (strcmp(a, "--socket") == 0) o->socket_path = v;
        else if (strcmp(a, "--daemon") == 0) o->daemon_exe = v;
        else if (strcmp(a, "--model") == 0) o->model = v;
        else if (strcmp(a, "--template") == 0) o->template_name = v;
        else if (strcmp(a, "--threads") == 0) o->threads = atoi(v);
        else if (strcmp(a, "--n-predict") == 0) o->n_predict = atoi(v);
        else if (strcmp(a, "--n-ctx") == 0) o->n_ctx = atoi(v);
        else if (strcmp(a, "--temperature") == 0) o->temperature = atof(v);
        else if (strcmp(a, "--interval") == 0) o->interval = atof(v);
        else if (strcmp(a, "--max-events") == 0) o->max_events = atoi(v);
        else if (strcmp(a, "--retry") == 0) o->retry = atof(v);
        else if (strcmp(a, "--retry-max") == 0) o->retry_max = atof(v);
        else if (strcmp(a, "--chip") == 0) o->chip = v;
        else if (strcmp(a, "--expander") == 0) o->expander = v;
        else if (strcmp(a, "--fb") == 0) o->fb = v;
        else if (strcmp(a, "--iio") == 0) o->iio = v;
        else if (strcmp(a, "--rtc") == 0) o->rtc = v;
        else return -1;
        i++;
    }
    if (o->project == NULL || strcmp(o->project, "e08") != 0 || o->grammar == NULL ||
        o->prompt == NULL || o->log == NULL || (o->replay == NULL) == (o->live == 0) ||
        (o->socket_path != NULL && o->daemon_exe != NULL))
        return -1;
    if (o->retry <= 0.0 || o->retry_max <= 0.0)
        return -1;
    return 0;
}

int main(int argc, char **argv)
{
    opts o;
    run r;
    io_source *src = NULL;
    pid_t daemon_pid = -1;
    char tmp_dir[64] = "", tmp_sock[96] = "", version[64];
    struct sigaction sa;
    int rc = 0;
#ifdef EDGELLM_E08_LIVE
    hat h;
    int cursor = 0;
#endif

    if (parse(&o, argc, argv) != 0)
        return usage(argv[0]);
    memset(&r, 0, sizeof(r));
    r.o = &o;
    r.last_index = -1;
    r.rule = rule_find("e08");
    if (r.rule == NULL) {
        fprintf(stderr, "%s: no rule registered for project e08\n", argv[0]);
        return 1;
    }
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    if (o.daemon_exe != NULL) {
        daemon_pid = spawn_daemon(&o, tmp_dir, sizeof(tmp_dir), tmp_sock, sizeof(tmp_sock));
        if (daemon_pid < 0) {
            fprintf(stderr, "%s: cannot start %s\n", argv[0], o.daemon_exe);
            stop_daemon(-1, tmp_dir, tmp_sock);
            return 1;
        }
        o.socket_path = tmp_sock;
    } else if (o.socket_path == NULL) {
        o.socket_path = "/run/edgellm/e08.sock";
    }
    if (daemon_version(o.socket_path, version, sizeof(version)) != 0) {
        /* A daemon that is not there at start is logged with its engine unknown; the
         * first good reply fills the recovery figure, not the engine column. */
        snprintf(version, sizeof(version), "unknown, daemon away at start");
    }
    r.log = elog_open(o.log, version);
    r.prompt_buf = malloc(PROMPT_BUF);
    r.reply_buf = malloc(REPLY_BUF);
    if (r.log == NULL || r.prompt_buf == NULL || r.reply_buf == NULL) {
        fprintf(stderr, "%s: cannot write %s\n", argv[0], o.log);
        rc = 1;
        goto out;
    }

    if (o.replay != NULL) {
        char err[256];
        io_event ev;
        int n;
        src = io_open_replay(o.replay, err, sizeof(err));
        if (src == NULL) {
            fprintf(stderr, "%s: %s\n", argv[0], err);
            rc = 1;
            goto out;
        }
        while (!stop_requested && (n = io_next(src, &ev)) == 1) {
            if (process_event(&r, &ev) != 0) {
                rc = 1;
                break;
            }
            if (o.max_events > 0 && r.n_events >= o.max_events)
                break;
            if (o.interval > 0.0)
                sleep_s(o.interval);
        }
        if (rc == 0 && n < 0)
            rc = 1;
    } else {
#ifdef EDGELLM_E08_LIVE
        const char *lines[OLED_LINES];
        char menu[N_QUESTIONS][OLED_COLS + 1], status[OLED_COLS + 1];
        char wrapped[6][OLED_COLS + 1];
        int i;
        if (hat_open(&h, &o) != 0) {
            rc = 1;
            goto out;
        }
        r.hat = &h;
        for (;;) {
            int key;
            /* The menu: the five questions, the cursor on one of them. */
            for (i = 0; i < N_QUESTIONS; i++)
                snprintf(menu[i], sizeof(menu[i]), "%c %s", i == cursor ? '>' : ' ',
                         question_label[i]);
            lines[0] = "E08 CONSOLE  UP/DOWN";
            for (i = 0; i < N_QUESTIONS; i++)
                lines[1 + i] = menu[i];
            lines[6] = "PRESS = ASK";
            lines[7] = r.state != NULL ? r.state : "";
            oled_draw(&h, lines, OLED_LINES);
            key = joystick_wait(&h);
            if (key < 0)
                break;
            if (key == JOY_UP) { cursor = (cursor + N_QUESTIONS - 1) % N_QUESTIONS; continue; }
            if (key == JOY_DOWN) { cursor = (cursor + 1) % N_QUESTIONS; continue; }
            if (key != JOY_PRESS && key != JOY_RIGHT)
                continue;
            {
                io_event ev;
                size_t k, len;
                if (sensors_read(&h, cursor, &ev) != 0) {
                    fprintf(stderr, "e08: a sysfs read failed\n");
                    continue;
                }
                lines[0] = question_label[cursor];
                lines[1] = "ASKING THE MODEL";
                for (i = 2; i < OLED_LINES; i++)
                    lines[i] = "";
                oled_draw(&h, lines, OLED_LINES);
                if (process_event(&r, &ev) != 0) {
                    rc = 1;
                    break;
                }
                snprintf(status, sizeof(status), "%s %+.1fs", r.state, ev.v[1] - ev.v[2]);
                lines[1] = status;
                len = strlen(r.answer);
                for (i = 0, k = 0; i < 6; i++) {
                    size_t take = len - k > OLED_COLS ? OLED_COLS : len - k;
                    memcpy(wrapped[i], r.answer + k, take);
                    wrapped[i][take] = '\0';
                    lines[2 + i] = wrapped[i];
                    k += take;
                }
                oled_draw(&h, lines, OLED_LINES);
                if (o.max_events > 0 && r.n_events >= o.max_events)
                    break;
                /* The answer stays until the next contact closes. */
                key = joystick_wait(&h);
                if (key < 0)
                    break;
            }
        }
        hat_close(&h);
        r.hat = NULL;
#else
        fprintf(stderr, "%s: --live needs a build with -DEDGELLM_IO_GPIO=ON (libgpiod 2)\n",
                argv[0]);
        rc = 1;
        goto out;
#endif
    }

    fprintf(stderr, "e08: %ld events, %ld model answers, %ld events with the daemon away, "
                    "%ld buzzer sounds, mean prompt %.1f ms, mean generate %.1f ms, last "
                    "recovery %.3f s, rss %ld kB, engine %s, source %s\n",
            r.n_events, r.n_model, r.n_lost_events, r.n_buzz,
            r.n_model > 0 ? r.sum_prompt_ms / (double)r.n_model : 0.0,
            r.n_model > 0 ? r.sum_generate_ms / (double)r.n_model : 0.0, r.last_recovery_s,
            edgellm_rss_kb(), version, o.replay != NULL ? "replay" : "live");
out:
    free(r.reply_buf);
    free(r.prompt_buf);
    elog_close(r.log);
    io_close(src);
    stop_daemon(daemon_pid, tmp_dir, tmp_sock);
    return rc;
}
