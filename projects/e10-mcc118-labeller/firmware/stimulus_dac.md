# The stimulus: the NUCLEO-H7A3ZI-Q DAC on PA4

Without a source the DAQ reads nothing, and a Pi GPIO is not an analog source. The
stimulus for e10 is the Nucleo's DAC1, channel 1, on PA4, driven to produce steps and
ramps of a commanded amplitude between 0 and 3.3 V (VREF+ is the board's 3.3 V), wired
to the MCC 118's CH0 with a ground wire between the boards. The board bring-up, the
clock configuration, the console and the build and flash sequence are the firmware
volume's project 6 (the MCC 118 as the witness of a 1 kHz sampler, with the marker on
PB4 at CN7 pin 19); they are not repeated here. This note adds only the DAC.

## Facts and what is still to verify

- DAC1_OUT1 is PA4 on this part; the Zio pin that carries PA4 on the MB1363 board is
  CN7 pin 17 (to verify against UM2408, the Nucleo-144 user manual, before a wire goes in).
- The DAC's accuracy is read from the STM32H7A3xI datasheet's DAC characteristics table
  (offset, gain error, DNL and INL, with the output buffer on): to verify. The chapter's
  2 percent acceptance band is wider than a 12-bit DAC's typical total error with the
  buffer enabled, which is the reason the DAC can stand in for a calibrator here; if the
  datasheet says otherwise, the band moves, not the claim.
- With the output buffer on, the DAC does not reach either rail; the settled levels are
  kept between 0.20 V and 3.20 V, which the replay generator also respects.
- The marker line of project 6 (PB4) is not used by e10; a trigger on it is a future
  exercise of the chapter.

## The sketch

Thirty lines, register level, in the style of the firmware volume. The clock and GPIO
enables assume project 6's configuration (the RCC enables for GPIOA and DAC1 are the
two lines that differ). Amplitudes are commanded in millivolts over the console; a step
is one write, a ramp is a run of writes paced by a busy wait whose length is the ramp
duration in microseconds divided by the number of codes.

```c
/* stimulus_dac.c: DAC1 channel 1 on PA4, steps and ramps of a commanded amplitude. */
#include "stm32h7xx.h"

#define VREF_MV 3300u

static void dac_init(void)
{
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN;
    RCC->APB1LENR |= RCC_APB1LENR_DAC12EN;
    GPIOA->MODER |= (3u << (4u * 2u));                 /* PA4 analog */
    DAC1->CR = DAC_CR_EN1;                             /* buffer on, no trigger */
}

static void dac_write_mv(uint32_t mv)
{
    DAC1->DHR12R1 = (mv * 4095u) / VREF_MV;            /* 12-bit right aligned */
}

void stimulus_step(uint32_t low_mv, uint32_t amp_mv)
{
    dac_write_mv(low_mv);
    dac_write_mv(low_mv + amp_mv);                     /* one write: the edge is one sample */
}

void stimulus_ramp(uint32_t low_mv, uint32_t amp_mv, uint32_t duration_us)
{
    uint32_t codes = (amp_mv * 4095u) / VREF_MV, i;
    for (i = 0; i <= codes; i++) {
        DAC1->DHR12R1 = (low_mv * 4095u) / VREF_MV + i;
        delay_us(duration_us / (codes ? codes : 1u)); /* project 6's cycle counter */
    }
}
```

## The sequence the bench runs

For each commanded amplitude in 0.5, 1.0, 1.5, 2.0, 2.5, 3.0 V: fifty steps spaced
50 ms apart, half rising and half falling, then fifty ramps of 1 ms, the same way. The
spacing is far longer than a 2 ms window, so at most one event lands in a window; an
edge that lands on a window boundary is seen by neither window and is counted as a
missed stimulus, not as a disagreement. Reflash immediately before every capture; the
console banner prints `__DATE__` and `__TIME__` and is copied beside the log.
