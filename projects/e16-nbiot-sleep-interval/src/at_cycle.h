/* at_cycle.h: the AT sequence that brings the SIM7020E up, asks the network for a
 * power-saving-mode window, and reads back the window the network actually granted.
 *
 * Nothing here decides anything. An attach takes minutes and a PSM cycle takes minutes,
 * so project e16 decides offline on a logged profile; this file exists to produce that
 * profile's one remaining number, the granted window, and to turn the 3GPP timer octets
 * into seconds without anybody doing it by hand on a bench afternoon.
 *
 * Sources, both read on Wednesday 14 October 2026 and cited in the chapter:
 *
 *   SIM7020 Series_Low Power Mode_Application Note V1.03 (SIMCom, released Friday
 *   10 May 2019), section 3.2 and section 6.2. It gives the parameter order
 *
 *       +CPSMS: <mode>,<Requested_Periodic-RAU>,<Requested_GPRS-Ready-timer>,
 *               <Requested_periodic-TAU>,<Requested_Active-Time>
 *
 *   with the two middle parameters not needed, <Requested_periodic-TAU> being T3412_ext
 *   and <Requested_Active-Time> being T3324; each is one octet written as eight
 *   characters, the high three bits a unit and the low five bits a multiplier. Its
 *   Table 1 lists the TAU units and its Table 2 the active-time units, both reproduced
 *   in the two tables below. Its own worked examples are the two cases at_cycle checks:
 *   a TAU octet "01000111" is unit 2 (10 hours) times 7, that is 70 hours, and an
 *   active-time octet "00001111" is unit 0 (2 seconds) times 15, that is 30 seconds.
 *   Its section 3.3 note 3 is the reason this file exists at all: "The actual value of
 *   T3324 and T3412 is issued by the operator", read back through AT+CEREG.
 *
 *   Waveshare SIM7020E NB-IoT HAT wiki, for the board: VCCIO jumper 3.3 V or 5 V with
 *   3.3 V the default, the PWR jumper putting the power key on BCM 4, a 5 V supply, and
 *   115200 baud by default. The wiki's own quick test uses AT+CGREG?, which is why the
 *   plan below reads both AT+CGREG? and AT+CEREG?; the SIM7020 Series AT Command Manual
 *   V1.03 is listed in the wiki's downloads and has NOT been read on this bench, so
 *   every form here comes from the application note or from 3GPP TS 27.007.
 *
 * The timer units are tables, not arithmetic in the caller: a unit code outside the
 * application note's tables (the deactivated code 7 among them) decodes to -1 and the
 * rule then treats the window as not granted, which is the safe reading.
 */
#ifndef E16_AT_CYCLE_H
#define E16_AT_CYCLE_H

#include <stddef.h>
#include <stdio.h>

/* One step of the plan: what to send, what reply token proves it worked, how long to
 * wait for that token, and how many times to send it again before stopping. */
typedef struct at_step {
    const char *cmd;        /* the literal command, or NULL for the built AT+CPSMS line */
    const char *expect;     /* the leading text of the reply that proves the step */
    double      timeout_s;
    int         retries;
    const char *why;        /* one line for the operator reading the printed plan */
} at_step;

/* The granted timers, as read back from AT+CEREG?. The two octets are kept as text so
 * that an unlisted unit can be printed exactly as the network sent it. */
typedef struct at_granted {
    char   active_bits[9];  /* T3324, eight characters */
    char   tau_bits[9];     /* T3412 extended, eight characters */
    double active_s;        /* decoded, or -1 when the unit is not in Table 2 */
    double tau_s;           /* decoded, or -1 when the unit is not in Table 1 */
} at_granted;

/* The plan, in order. Writes the step count into n and returns the table. */
const at_step *at_cycle_plan(int *n);

/* The text of one step: the literal command, or the AT+CPSMS line built from the two
 * requested octets. Returns buf. */
const char *at_cycle_text(const at_step *s, const char *tau_bits, const char *act_bits,
                          char *buf, size_t len);

/* Eight characters of '0' and '1' from a unit (0 to 7) and a multiplier (0 to 31).
 * Returns 0, or -1 when either is out of range. */
int at_timer_encode(int unit, int value, char out[9]);

/* Seconds, or -1.0 when the octet is malformed or its unit is not in the table. */
double at_timer_decode_tau(const char *bits);
double at_timer_decode_active(const char *bits);

/* AT+CPSMS=1,,,"<tau>","<act>" into buf. Returns 0, or -1 when buf is too short. */
int at_cpsms_request(char *buf, size_t len, const char *tau_bits, const char *act_bits);

/* Reads the granted octets out of one +CEREG: line, which carries them as the last two
 * eight-character quoted fields, the active time first and the periodic TAU second.
 * Returns 0 on success, -1 when the line holds no such pair. */
int at_cereg_parse(const char *line, at_granted *out);

/* The live bring-up, compiled only where the termios backend is asked for
 * (-DEDGELLM_IO_SERIAL=ON). Opens the port itself, walks the plan with the per-step
 * timeout and retry budget, writes every line it sent and received to trace, and fills
 * granted from the last +CEREG: reply. Returns 0 when every step answered, the
 * one-based index of the step that did not, or -1 when the port cannot be opened.
 * A build without the backend returns -2 and prints the option it needs. */
int at_cycle_run(const char *spec, const char *tau_bits, const char *act_bits,
                 at_granted *granted, FILE *trace);

#endif /* E16_AT_CYCLE_H */
