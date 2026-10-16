/* vl53l4cd_platform.h: the project side of the ST ULD's platform layer. The ULD (STSW-IMG026,
 * out of tree, pointed at by EDGELLM_VL53L4CD_ULD_DIR) declares in its own platform.h the
 * six register functions and the millisecond wait it needs from the host; this project
 * defines them in vl53l4cd_platform.c over i2c-dev. The two functions below open and close
 * the bus the six functions use, and the handle define is what the project passes to every
 * ULD call as its Dev_t.
 *
 * Names and signatures of the ULD side, read on Wednesday 14 October 2026 from the C ULD in
 * STMicroelectronics/x-cube-tof1 (Drivers/BSP/Components/vl53l4cd/porting/platform.h): the
 * reads and writes are uint8_t VL53L4CD_RdByte(Dev_t, uint16_t, uint8_t *) and the five like
 * it, the wait is uint8_t VL53L4CD_WaitMs(Dev_t, uint32_t). The Arduino mirror of 2021
 * spells the wait WaitMs; CMake passes the spelling as EDGELLM_VL53L4CD_WAIT_NAME. Dev_t is
 * the ULD's own type (a 16-bit address in the standalone ULD, an object pointer in the
 * X-CUBE port); this platform treats it as opaque and keeps the bus state in its own file.
 * The ST download itself was not reachable on that day and its platform.h is to verify
 * on the first build.
 */
#ifndef E06_VL53L4CD_PLATFORM_H
#define E06_VL53L4CD_PLATFORM_H

#include <stdint.h>

/* The handle handed to the ULD as Dev_t. The ULD's examples use the eight-bit address
 * 0x52 (0x29 shifted left by one); the bus functions here ignore it and use the seven-bit
 * address given to vl53l4cd_platform_open, so the value only has to be stable. */
#define VL53L4CD_PLATFORM_DEV 0x52

/* Opens dev_path (for example /dev/i2c-1) and selects the seven-bit address with
 * I2C_SLAVE. Returns 0, or -1 with the reason on stderr. One sensor per process. */
int vl53l4cd_platform_open(const char *dev_path, int addr7);

void vl53l4cd_platform_close(void);

#endif /* E06_VL53L4CD_PLATFORM_H */
