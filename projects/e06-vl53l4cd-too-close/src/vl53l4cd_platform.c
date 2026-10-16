/* vl53l4cd_platform.c: the ST ULD's platform functions over i2c-dev. The ULD addresses the
 * sensor with 16-bit register indexes sent high byte first, and reads and writes its words
 * big-endian, which is what the ST platform.c for the STM32 HAL does and what the Arduino
 * mirror's platform.cpp does (both read on Wednesday 14 October 2026). A read is one I2C_RDWR
 * transfer with two messages, the index write and the data read joined by a repeated start,
 * so that no other master and no other process gets a stop between them; a write is the
 * index and the data in one message. The ADXL345 of e05 sits on the same bus at 0x53 and is
 * never addressed from here.
 *
 * The bus speed is the kernel's, 100 kHz on this bench (dtparam=i2c_arm_baudrate=100000);
 * nothing here changes it. One sensor per process: the descriptor and the address are file
 * statics because the ULD's Dev_t carries no room for them in its standalone form.
 */
#define _GNU_SOURCE
#include "vl53l4cd_platform.h"
#include "platform.h"                 /* the ULD's declarations: Dev_t and the seven names */

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#ifndef EDGELLM_VL53L4CD_WAIT_NAME
#define EDGELLM_VL53L4CD_WAIT_NAME VL53L4CD_WaitMs
#endif

static int bus_fd = -1;
static int bus_addr = 0x29;

int vl53l4cd_platform_open(const char *dev_path, int addr7)
{
    if (bus_fd >= 0)
        vl53l4cd_platform_close();
    bus_fd = open(dev_path, O_RDWR);
    if (bus_fd < 0) {
        fprintf(stderr, "vl53l4cd: cannot open %s: %s\n", dev_path, strerror(errno));
        return -1;
    }
    if (ioctl(bus_fd, I2C_SLAVE, addr7) < 0) {
        fprintf(stderr, "vl53l4cd: cannot select 0x%02x on %s: %s\n", addr7, dev_path,
                strerror(errno));
        close(bus_fd);
        bus_fd = -1;
        return -1;
    }
    bus_addr = addr7;
    return 0;
}

void vl53l4cd_platform_close(void)
{
    if (bus_fd >= 0)
        close(bus_fd);
    bus_fd = -1;
}

/* Index write and data read in one transfer: a repeated start between the two messages. */
static uint8_t reg_read(uint16_t index, uint8_t *data, uint16_t len)
{
    uint8_t idx[2] = { (uint8_t)(index >> 8), (uint8_t)(index & 0xff) };
    struct i2c_msg msgs[2];
    struct i2c_rdwr_ioctl_data xfer;

    if (bus_fd < 0)
        return 1;
    msgs[0].addr = (uint16_t)bus_addr;
    msgs[0].flags = 0;
    msgs[0].len = 2;
    msgs[0].buf = idx;
    msgs[1].addr = (uint16_t)bus_addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = len;
    msgs[1].buf = data;
    xfer.msgs = msgs;
    xfer.nmsgs = 2;
    return ioctl(bus_fd, I2C_RDWR, &xfer) == 2 ? 0 : 1;
}

/* Index and data in one write: the sensor takes the bytes after the index in order. */
static uint8_t reg_write(uint16_t index, const uint8_t *data, size_t len)
{
    uint8_t buf[2 + 4];
    if (bus_fd < 0 || len > 4)
        return 1;
    buf[0] = (uint8_t)(index >> 8);
    buf[1] = (uint8_t)(index & 0xff);
    memcpy(buf + 2, data, len);
    return write(bus_fd, buf, 2 + len) == (ssize_t)(2 + len) ? 0 : 1;
}

uint8_t VL53L4CD_RdByte(Dev_t dev, uint16_t index, uint8_t *value)
{
    (void)dev;
    return reg_read(index, value, 1);
}

uint8_t VL53L4CD_RdWord(Dev_t dev, uint16_t index, uint16_t *value)
{
    uint8_t b[2];
    uint8_t st = reg_read(index, b, 2);
    (void)dev;
    if (st == 0)
        *value = (uint16_t)(((uint16_t)b[0] << 8) | b[1]);
    return st;
}

uint8_t VL53L4CD_RdDWord(Dev_t dev, uint16_t index, uint32_t *value)
{
    uint8_t b[4];
    uint8_t st = reg_read(index, b, 4);
    (void)dev;
    if (st == 0)
        *value = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
    return st;
}

uint8_t VL53L4CD_WrByte(Dev_t dev, uint16_t index, uint8_t value)
{
    (void)dev;
    return reg_write(index, &value, 1);
}

uint8_t VL53L4CD_WrWord(Dev_t dev, uint16_t index, uint16_t value)
{
    uint8_t b[2] = { (uint8_t)(value >> 8), (uint8_t)(value & 0xff) };
    (void)dev;
    return reg_write(index, b, 2);
}

uint8_t VL53L4CD_WrDWord(Dev_t dev, uint16_t index, uint32_t value)
{
    uint8_t b[4] = { (uint8_t)(value >> 24), (uint8_t)(value >> 16), (uint8_t)(value >> 8),
                     (uint8_t)(value & 0xff) };
    (void)dev;
    return reg_write(index, b, 4);
}

uint8_t EDGELLM_VL53L4CD_WAIT_NAME(Dev_t dev, uint32_t TimeMs)
{
    struct timespec ts;
    (void)dev;
    ts.tv_sec = (time_t)(TimeMs / 1000u);
    ts.tv_nsec = (long)(TimeMs % 1000u) * 1000000L;
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
        ;
    return 0;
}
