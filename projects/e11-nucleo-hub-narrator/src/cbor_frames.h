/* cbor_frames.h: the frame layer of project e11, between the Nucleo sensor hub's USB CDC
 * stream and the shared loop. The hub is the one the Linux volume's project 12 built: a
 * NUCLEO-H7A3ZI-Q carrying the X-NUCLEO-IKS5A1, streaming framed CBOR over the ST-LINK
 * virtual COM port. This file decodes those frames; nothing here knows about the model.
 *
 * Why a frame layer and not io_serial: common/c/src/io_serial.c is line-oriented, one
 * event per text line with the numbers parsed from it, and a binary frame carries bytes
 * 0x0A and 0x0D inside its payload. So the frame layer owns the port itself, with the
 * same termios settings, turns every SAMPLE frame into one replay-shaped CSV line, and
 * hands that stream to the untouched shared main through a pipe (see e11_main.c). The
 * loop therefore runs the identical code on the bench and on a replayed file; only the
 * tag column of the log says which: "iks5a1" from the frame layer, "replay" from a file.
 *
 * The wire contract, protocol 1.1 (major 1 from project 12, minor 1 for the two keys
 * this hub firmware adds; a layout change bumps the major and the decoder refuses it):
 *
 *   offset  size  field
 *   0       1     SOF, always 0xA5
 *   1       1     protocol major version, 1
 *   2       1     message type: 0x01 HELLO, 0x02 SAMPLE (others are counted and skipped)
 *   3       2     payload length, little endian, at most 512
 *   5       n     CBOR payload, one map with integer keys
 *   5+n     2     CRC-16/CCITT-FALSE over bytes 1 .. 4+n, little endian
 *
 *   HELLO   {0: proto_minor, 1: fw_version text, 2: [sensor names]}
 *   SAMPLE  {0: t_us, 1: [ax, ay, az] low-g in g, 2: [gx, gy, gz], 3: temp_c,
 *            4: seq, 5: highg_peak_g}
 *
 * Keys 0, 1, 4 and 5 are required by this decoder; 2 and 3 are the 1.0 keys and are
 * skipped; any other key is skipped too, which is the minor-version rule of project 12.
 * The io_event a SAMPLE becomes: t_s from t_us, v0 the sequence counter, v1 to v3 the
 * low-g axes in g, v4 the high-g peak-hold in g, the largest magnitude the high-g
 * channel saw since the previous frame (an event quantity, not a sample). The sequence
 * counter is 32 bit and increments by one per frame; a frame whose counter is not the
 * previous plus one is a gap, counted here and again by the loop.
 *
 * Decoder API: tinycbor (MIT), the names cbor_parser_init, cbor_value_is_map,
 * cbor_value_enter_container, cbor_value_at_end, cbor_value_is_integer,
 * cbor_value_get_int, cbor_value_advance, cbor_value_is_unsigned_integer,
 * cbor_value_get_uint64, cbor_value_is_array, cbor_value_is_float, cbor_value_get_float,
 * cbor_value_is_double, cbor_value_get_double, cbor_value_is_text_string,
 * cbor_value_copy_text_string, cbor_value_leave_container and cbor_error_string, read
 * from src/cbor.h at tag v0.6.1 on Wednesday 14 October 2026.
 */
#ifndef E11_CBOR_FRAMES_H
#define E11_CBOR_FRAMES_H

#include "io.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define CBOR_FRAMES_SOF          0xA5
#define CBOR_FRAMES_MAJOR        1
#define CBOR_FRAMES_TYPE_HELLO   0x01
#define CBOR_FRAMES_TYPE_SAMPLE  0x02
#define CBOR_FRAMES_HEADER       5
#define CBOR_FRAMES_TRAILER      2
#define CBOR_FRAMES_MAX_PAYLOAD  512
#define CBOR_FRAMES_TAG          "iks5a1"

enum cbor_frames_key {
    CBOR_KEY_T_US = 0,
    CBOR_KEY_LOWG = 1,
    CBOR_KEY_GYRO = 2,        /* protocol 1.0, skipped */
    CBOR_KEY_TEMP = 3,        /* protocol 1.0, skipped */
    CBOR_KEY_SEQ = 4,
    CBOR_KEY_HIGHG_PEAK = 5
};

typedef struct cbor_frames_stats {
    unsigned long frames_ok;         /* SAMPLE frames decoded into an event */
    unsigned long frames_hello;
    unsigned long frames_other;      /* a type this decoder does not know */
    unsigned long bad_header;        /* wrong major version or a length over the bound */
    unsigned long bad_crc;
    unsigned long bad_map;           /* a required key missing or of the wrong type */
    unsigned long bytes_skipped;     /* bytes outside any frame, e.g. before the first SOF */
    unsigned long seq_gaps;          /* frames whose counter was not the previous plus one */
    unsigned long seq_missing;       /* counter values that never arrived */
    uint32_t      last_seq;
    int           have_seq;
    int           proto_minor;       /* from the last HELLO, -1 before any */
    char          fw_version[48];    /* from the last HELLO; carries the firmware's build stamp */
} cbor_frames_stats;

typedef struct cbor_frames {
    uint8_t  buf[CBOR_FRAMES_HEADER + CBOR_FRAMES_MAX_PAYLOAD + CBOR_FRAMES_TRAILER];
    size_t   fill;
    size_t   need;                   /* the frame's full length once the header is in */
    cbor_frames_stats st;
} cbor_frames;

/* CRC-16/CCITT-FALSE, the same function the hub firmware runs (project 12's proto.c). */
uint16_t cbor_frames_crc16(const uint8_t *p, size_t n);

void cbor_frames_init(cbor_frames *f);

/* Feeds one byte from the port. Returns 1 when a SAMPLE frame completed and ev holds it,
 * 0 otherwise. A HELLO updates st.fw_version and st.proto_minor. Resynchronises on a bad
 * header or CRC by rescanning the bytes after the false SOF. */
int cbor_frames_push(cbor_frames *f, uint8_t c, io_event *ev);

/* Decodes one SAMPLE payload (without header and CRC) into ev and updates the sequence
 * counters in st. Returns 0, or -1 when a required key is missing or of the wrong type. */
int cbor_frames_decode_sample(const uint8_t *payload, size_t n, io_event *ev,
                              cbor_frames_stats *st);

/* Decodes one HELLO payload into st. Returns 0 or -1. */
int cbor_frames_decode_hello(const uint8_t *payload, size_t n, cbor_frames_stats *st);

/* Opens "<device>:<baud>" raw, 8N1, with a 0.5 s read timeout so a reader can stop.
 * The split is at the last ':' so a /dev/serial/by-id name may carry its own colons.
 * Returns the descriptor, or -1 with a one-line reason in err. */
int cbor_frames_open_port(const char *spec, char *err, size_t err_len);

/* The translator: reads bytes from fd until *stop is set or the port fails, and writes
 * one replay-shaped CSV line per SAMPLE to out (header first), flushed per line. HELLO
 * frames are reported on diag with the firmware version. Returns the lines written. */
long cbor_frames_pump(int fd, FILE *out, cbor_frames *f, FILE *diag, volatile int *stop);

/* One line of counters for the end-of-run report. */
void cbor_frames_report(const cbor_frames_stats *st, FILE *out);

#endif /* E11_CBOR_FRAMES_H */
