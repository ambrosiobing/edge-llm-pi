/* cbor_frames.c: the frame layer of project e11. See cbor_frames.h for the wire
 * contract and for why this file, and not io_serial.c, owns the port.
 *
 * Decoding with tinycbor is a walk: cbor_parser_init over the payload, enter the map,
 * then for every pair read the integer key, advance to the value, take the ones the
 * contract names and cbor_value_advance over the rest, which skips a whole container in
 * one call. Nothing is allocated; the parser works on the frame buffer in place.
 */
#define _GNU_SOURCE
#include "cbor_frames.h"

#include <cbor.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define HAVE_T_US   0x01
#define HAVE_LOWG   0x02
#define HAVE_SEQ    0x04
#define HAVE_PEAK   0x08
#define HAVE_ALL    (HAVE_T_US | HAVE_LOWG | HAVE_SEQ | HAVE_PEAK)

uint16_t cbor_frames_crc16(const uint8_t *p, size_t n)
{
    uint16_t crc = 0xFFFF;
    while (n--) {
        int i;
        crc ^= (uint16_t)(*p++ << 8);
        for (i = 0; i < 8; i++)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

void cbor_frames_init(cbor_frames *f)
{
    memset(f, 0, sizeof(*f));
    f->st.proto_minor = -1;
}

/* A number the contract allows: a single float (what the hub encodes), a double, or an
 * integer. A half float or a string is a contract violation and returns -1. */
static int get_number(const CborValue *v, double *out)
{
    if (cbor_value_is_float(v)) {
        float x;
        if (cbor_value_get_float(v, &x) != CborNoError)
            return -1;
        *out = (double)x;
        return 0;
    }
    if (cbor_value_is_double(v))
        return cbor_value_get_double(v, out) == CborNoError ? 0 : -1;
    if (cbor_value_is_integer(v)) {
        int x;
        if (cbor_value_get_int(v, &x) != CborNoError)
            return -1;
        *out = (double)x;
        return 0;
    }
    return -1;
}

static int get_counter(const CborValue *v, uint64_t *out)
{
    if (!cbor_value_is_unsigned_integer(v))
        return -1;
    return cbor_value_get_uint64(v, out) == CborNoError ? 0 : -1;
}

/* Reads a three-element array of numbers into out[0..2] and advances it past it. */
static int get_triple(CborValue *it, double *out)
{
    CborValue arr;
    int i;
    if (!cbor_value_is_array(it) || cbor_value_enter_container(it, &arr) != CborNoError)
        return -1;
    for (i = 0; i < 3; i++) {
        if (cbor_value_at_end(&arr) || get_number(&arr, &out[i]) != 0 ||
            cbor_value_advance(&arr) != CborNoError)
            return -1;
    }
    while (!cbor_value_at_end(&arr))                    /* a longer array: the rest is dropped */
        if (cbor_value_advance(&arr) != CborNoError)
            return -1;
    return cbor_value_leave_container(it, &arr) == CborNoError ? 0 : -1;
}

int cbor_frames_decode_sample(const uint8_t *payload, size_t n, io_event *ev,
                              cbor_frames_stats *st)
{
    CborParser parser;
    CborValue it, map;
    uint64_t t_us = 0, seq = 0;
    double lowg[3] = { 0.0, 0.0, 0.0 }, peak = 0.0;
    unsigned have = 0;

    if (cbor_parser_init(payload, n, 0, &parser, &it) != CborNoError || !cbor_value_is_map(&it) ||
        cbor_value_enter_container(&it, &map) != CborNoError)
        return -1;
    while (!cbor_value_at_end(&map)) {
        int key;
        if (!cbor_value_is_integer(&map) || cbor_value_get_int(&map, &key) != CborNoError ||
            cbor_value_advance(&map) != CborNoError)
            return -1;                                  /* now at the value */
        switch (key) {
        case CBOR_KEY_T_US:
            if (get_counter(&map, &t_us) != 0 || cbor_value_advance(&map) != CborNoError)
                return -1;
            have |= HAVE_T_US;
            break;
        case CBOR_KEY_SEQ:
            if (get_counter(&map, &seq) != 0 || cbor_value_advance(&map) != CborNoError)
                return -1;
            have |= HAVE_SEQ;
            break;
        case CBOR_KEY_LOWG:
            if (get_triple(&map, lowg) != 0)
                return -1;
            have |= HAVE_LOWG;
            break;
        case CBOR_KEY_HIGHG_PEAK:
            if (get_number(&map, &peak) != 0 || cbor_value_advance(&map) != CborNoError)
                return -1;
            have |= HAVE_PEAK;
            break;
        default:                                        /* keys 2, 3 and anything newer */
            if (cbor_value_advance(&map) != CborNoError)
                return -1;
            break;
        }
    }
    if (cbor_value_leave_container(&it, &map) != CborNoError || have != HAVE_ALL)
        return -1;

    if (st != NULL) {
        uint32_t s = (uint32_t)seq;
        if (st->have_seq && s != st->last_seq + 1u) {
            st->seq_gaps++;
            if (s > st->last_seq + 1u)
                st->seq_missing += (unsigned long)(s - st->last_seq - 1u);
        }
        st->last_seq = s;
        st->have_seq = 1;
    }
    memset(ev, 0, sizeof(*ev));
    ev->t_s = (double)t_us * 1e-6;
    snprintf(ev->tag, sizeof(ev->tag), "%s", CBOR_FRAMES_TAG);
    ev->n = 5;
    ev->v[0] = (double)(uint32_t)seq;
    ev->v[1] = lowg[0];
    ev->v[2] = lowg[1];
    ev->v[3] = lowg[2];
    ev->v[4] = peak;
    return 0;
}

int cbor_frames_decode_hello(const uint8_t *payload, size_t n, cbor_frames_stats *st)
{
    CborParser parser;
    CborValue it, map;

    if (cbor_parser_init(payload, n, 0, &parser, &it) != CborNoError || !cbor_value_is_map(&it) ||
        cbor_value_enter_container(&it, &map) != CborNoError)
        return -1;
    while (!cbor_value_at_end(&map)) {
        int key;
        if (!cbor_value_is_integer(&map) || cbor_value_get_int(&map, &key) != CborNoError ||
            cbor_value_advance(&map) != CborNoError)
            return -1;
        if (key == 0 && cbor_value_is_integer(&map)) {
            if (cbor_value_get_int(&map, &st->proto_minor) != CborNoError)
                return -1;
        } else if (key == 1 && cbor_value_is_text_string(&map)) {
            size_t len = sizeof(st->fw_version);
            if (cbor_value_copy_text_string(&map, st->fw_version, &len, NULL) != CborNoError)
                st->fw_version[0] = '\0';               /* longer than the field: left blank */
            st->fw_version[sizeof(st->fw_version) - 1] = '\0';
        }
        if (cbor_value_advance(&map) != CborNoError)
            return -1;
    }
    return cbor_value_leave_container(&it, &map) == CborNoError ? 0 : -1;
}

/* After a bad header or CRC the bytes after the false SOF are fed through again, so a
 * frame whose SOF byte happened to appear inside a corrupted one is still found. */
static int resync(cbor_frames *f, io_event *ev)
{
    uint8_t tmp[sizeof(f->buf)];
    size_t n = f->fill, i;
    memcpy(tmp, f->buf, n);
    f->fill = 0;
    f->need = 0;
    for (i = 1; i < n; i++)
        if (cbor_frames_push(f, tmp[i], ev))
            return 1;
    return 0;
}

int cbor_frames_push(cbor_frames *f, uint8_t c, io_event *ev)
{
    if (f->fill == 0) {
        if (c != CBOR_FRAMES_SOF) {
            f->st.bytes_skipped++;
            return 0;
        }
        f->buf[f->fill++] = c;
        return 0;
    }
    f->buf[f->fill++] = c;
    if (f->fill == CBOR_FRAMES_HEADER) {
        size_t len = (size_t)f->buf[3] | ((size_t)f->buf[4] << 8);
        if (f->buf[1] != CBOR_FRAMES_MAJOR || len > CBOR_FRAMES_MAX_PAYLOAD) {
            f->st.bad_header++;
            return resync(f, ev);
        }
        f->need = CBOR_FRAMES_HEADER + len + CBOR_FRAMES_TRAILER;
        return 0;
    }
    if (f->fill < CBOR_FRAMES_HEADER || f->fill < f->need)
        return 0;
    {
        size_t len = f->need - CBOR_FRAMES_HEADER - CBOR_FRAMES_TRAILER;
        uint16_t want = (uint16_t)(f->buf[5 + len] | (f->buf[6 + len] << 8));
        uint16_t got = cbor_frames_crc16(f->buf + 1, 4 + len);
        int type = f->buf[2], rc = 0;
        if (want != got) {
            f->st.bad_crc++;
            return resync(f, ev);
        }
        if (type == CBOR_FRAMES_TYPE_SAMPLE) {
            if (cbor_frames_decode_sample(f->buf + 5, len, ev, &f->st) == 0) {
                f->st.frames_ok++;
                rc = 1;
            } else {
                f->st.bad_map++;
            }
        } else if (type == CBOR_FRAMES_TYPE_HELLO) {
            cbor_frames_decode_hello(f->buf + 5, len, &f->st);
            f->st.frames_hello++;
        } else {
            f->st.frames_other++;
        }
        f->fill = 0;
        f->need = 0;
        return rc;
    }
}

static speed_t baud_code(long baud)
{
    switch (baud) {
    case 9600:   return B9600;
    case 19200:  return B19200;
    case 38400:  return B38400;
    case 57600:  return B57600;
    case 115200: return B115200;
    case 230400: return B230400;
    case 460800: return B460800;
    case 921600: return B921600;
    default:     return B0;
    }
}

int cbor_frames_open_port(const char *spec, char *err, size_t err_len)
{
    char dev[256];
    const char *colon = strrchr(spec, ':');
    long baud;
    speed_t code;
    struct termios tio;
    int fd;

    if (colon == NULL || (baud = strtol(colon + 1, NULL, 10)) <= 0 ||
        (code = baud_code(baud)) == B0) {
        snprintf(err, err_len, "cdc: spec needs '<device>:<baud>' with a standard baud");
        return -1;
    }
    snprintf(dev, sizeof(dev), "%.*s", (int)(colon - spec), spec);
    fd = open(dev, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        snprintf(err, err_len, "cdc: cannot open %s", dev);
        return -1;
    }
    if (tcgetattr(fd, &tio) != 0) {
        close(fd);
        snprintf(err, err_len, "cdc: %s is not a terminal", dev);
        return -1;
    }
    cfmakeraw(&tio);
    cfsetispeed(&tio, code);
    cfsetospeed(&tio, code);
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cc[VMIN] = 0;                                 /* a read returns after at most */
    tio.c_cc[VTIME] = 5;                                /* 0.5 s, so the pump can stop */
    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        close(fd);
        snprintf(err, err_len, "cdc: cannot configure %s", dev);
        return -1;
    }
    tcflush(fd, TCIFLUSH);
    return fd;
}

long cbor_frames_pump(int fd, FILE *out, cbor_frames *f, FILE *diag, volatile int *stop)
{
    uint8_t chunk[1024];
    long lines = 0;
    unsigned long hellos = 0;

    fputs("t_s,tag,v0,v1,v2,v3,v4\n", out);
    fflush(out);
    while (!*stop) {
        ssize_t r = read(fd, chunk, sizeof(chunk)), i;
        if (r < 0) {
            if (errno == EINTR)
                continue;
            if (diag != NULL)
                fprintf(diag, "e11: port read failed, the frame layer stops\n");
            break;
        }
        for (i = 0; i < r; i++) {
            io_event ev;
            if (cbor_frames_push(f, chunk[i], &ev)) {
                int w = fprintf(out, "%.6f,%s,%.0f,%.4f,%.4f,%.4f,%.4f\n", ev.t_s, ev.tag,
                                ev.v[0], ev.v[1], ev.v[2], ev.v[3], ev.v[4]);
                if (w < 0 || fflush(out) != 0)
                    return lines;                       /* the loop went away: done */
                lines++;
            }
            if (f->st.frames_hello != hellos && diag != NULL) {
                hellos = f->st.frames_hello;
                fprintf(diag, "e11: hub HELLO: firmware \"%s\", protocol %d.%d\n",
                        f->st.fw_version, CBOR_FRAMES_MAJOR, f->st.proto_minor);
            }
        }
    }
    return lines;
}

void cbor_frames_report(const cbor_frames_stats *st, FILE *out)
{
    fprintf(out, "e11: frame layer: %lu samples, %lu hello, %lu other, %lu bad header, %lu bad "
                 "crc, %lu bad map, %lu bytes skipped, %lu sequence gaps (%lu frames missing), "
                 "firmware \"%s\"\n",
            st->frames_ok, st->frames_hello, st->frames_other, st->bad_header, st->bad_crc,
            st->bad_map, st->bytes_skipped, st->seq_gaps, st->seq_missing, st->fw_version);
}
