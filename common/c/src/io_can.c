/* io_can.c: SocketCAN raw frames. Spec is the interface name, "can0" or "vcan0".
 * Every received frame is one event: tag the interface name, v[0] the CAN identifier
 * (11 or 29 bits, flags removed), v[1] the data length, v[2..] the data bytes. A frame
 * from a vcan interface is a software stand-in and the project's figure draws it
 * dashed; this file cannot tell the difference and does not pretend to.
 *
 * On the WS-28164 HAT the names can0 and can1 swap between boots; read
 * "dmesg | grep mcp25" before choosing the spec, as the volume says.
 */
#define _GNU_SOURCE
#include "io_internal.h"

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct can_priv {
    int  fd;
    char tag[IO_TAG_LEN];
} can_priv;

static int can_next(io_source *s, io_event *ev)
{
    can_priv *p = (can_priv *)s->priv;
    struct can_frame frame;
    ssize_t r = read(p->fd, &frame, sizeof(frame));
    int i;

    if (r < (ssize_t)sizeof(frame)) {
        fprintf(stderr, "can: short read on %s\n", p->tag);
        return -1;
    }
    io_event_init(ev, p->tag);
    ev->v[0] = (frame.can_id & CAN_EFF_FLAG) ? (double)(frame.can_id & CAN_EFF_MASK)
                                             : (double)(frame.can_id & CAN_SFF_MASK);
    ev->v[1] = frame.len;
    ev->n = 2;
    for (i = 0; i < frame.len && ev->n < IO_MAX_VALUES; i++)
        ev->v[ev->n++] = frame.data[i];
    return 1;
}

static void can_close(io_source *s)
{
    can_priv *p = (can_priv *)s->priv;
    if (p == NULL)
        return;
    if (p->fd >= 0)
        close(p->fd);
    free(p);
}

io_source *io_can_open(const char *spec, char *err, size_t err_len)
{
    struct ifreq ifr;
    struct sockaddr_can addr;
    can_priv *p = calloc(1, sizeof(*p));
    io_source *s = calloc(1, sizeof(*s));

    if (p == NULL || s == NULL) {
        free(p);
        free(s);
        return io_seterr(err, err_len, "can: out of memory"), NULL;
    }
    if (strlen(spec) >= IFNAMSIZ) {
        free(p);
        free(s);
        return io_seterr(err, err_len, "can: interface name too long"), NULL;
    }
    p->fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (p->fd < 0) {
        free(p);
        free(s);
        return io_seterr(err, err_len, "can: no SocketCAN support in this kernel"), NULL;
    }
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", spec);
    if (ioctl(p->fd, SIOCGIFINDEX, &ifr) < 0) {
        close(p->fd);
        free(p);
        free(s);
        return io_seterr(err, err_len, "can: no interface named %s", spec), NULL;
    }
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(p->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(p->fd);
        free(p);
        free(s);
        return io_seterr(err, err_len, "can: cannot bind to %s (is it up?)", spec), NULL;
    }
    snprintf(p->tag, sizeof(p->tag), "%s", spec);
    s->name = "can";
    s->next = can_next;
    s->actuate = NULL;
    s->close = can_close;
    s->priv = p;
    return s;
}
