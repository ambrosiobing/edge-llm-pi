/* sidecar.c: the e15 ESP32 sidecar. One LED on one GPIO, four patterns, one letter.
 *
 * UART2 at 115200 8N1 on GPIO17 (TX) and GPIO16 (RX) is the only link to the Pi, over
 * the Renkforce USB/TTL cable; the board's own USB connector stays empty on the bench
 * because the PPK2 is the only supply. A line (or a bare letter) selects the pattern:
 *
 *   A  steady, on 100 percent               B  slow blink, 1 Hz, on 50 percent
 *   C  fast blink, 5 Hz, on 20 percent      D  breathing, 0.5 Hz triangle, 50 percent mean
 *   O  off, the baseline the PPK2 harness subtracts
 *
 * and the firmware answers "pattern X\n" with the letter it is now running, or
 * "pattern ?\n" for anything else, so that the host can check every command. The LED
 * is a Joy-it LK-LED10 module, anode on the signal pin, lit when driven high, on
 * GPIO4 through LEDC (channel 0, timer 0, 5 kHz, 10 bits) so that D can dim it and A to
 * C set the duty to full or zero. GPIO4 is not a strapping pin; GPIO0, 2, 5, 12 and 15
 * are (ESP-IDF GPIO reference, read Wednesday 14 October 2026), and an LED there can
 * change the boot mode. Wi-Fi and Bluetooth are never started.
 *
 * ESP-IDF: the API names below are from the v6.1 stable reference pages read on
 * Wednesday 14 October 2026 (ledc_timer_config, ledc_channel_config, ledc_set_duty,
 * ledc_update_duty, uart_driver_install, uart_param_config, uart_set_pin,
 * uart_read_bytes, uart_write_bytes); the version installed on skyhorizon's WSL is
 * recorded in firmware/README.md when it is first built (to verify). The build stamp
 * is printed at boot so a stale flash can never pass for a fresh one.
 */
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_GPIO        4
#define UART_PORT       UART_NUM_2
#define UART_TX_GPIO    17
#define UART_RX_GPIO    16
#define UART_BAUD       115200
#define DUTY_BITS       LEDC_TIMER_10_BIT
#define DUTY_MAX        1023
#define TICK_MS         10                 /* the pattern loop period */

static char pattern = 'O';
static int  t_ms;                           /* milliseconds into the current pattern */

static void led_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = DUTY_BITS,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 5000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_channel_config_t channel = {
        .gpio_num   = LED_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

static void led_duty(int duty)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, (uint32_t)duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

/* The duty for the pattern at t_ms. Every pattern is a function of time, so a command
 * that arrives mid-cycle restarts the cycle and the on-time figures hold per cycle. */
static int duty_for(char p, int ms)
{
    switch (p) {
    case 'A': return DUTY_MAX;                                  /* steady */
    case 'B': return (ms % 1000) < 500 ? DUTY_MAX : 0;          /* 1 Hz, 50 percent */
    case 'C': return (ms % 200) < 40 ? DUTY_MAX : 0;            /* 5 Hz, 20 percent */
    case 'D': {                                                 /* 0.5 Hz triangle */
        int phase = ms % 2000;
        int up = phase < 1000 ? phase : 2000 - phase;           /* 0..1000..0 */
        return up * DUTY_MAX / 1000;
    }
    default:  return 0;                                         /* O and anything else */
    }
}

static void uart_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, 256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_GPIO, UART_RX_GPIO, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));
}

static void reply(char p)
{
    char line[16];
    int n = snprintf(line, sizeof line, "pattern %c\n", p);
    uart_write_bytes(UART_PORT, line, (size_t)n);
}

/* One command letter per line; CR, LF and spaces are separators, the first letter of
 * a line counts, the rest of the line is ignored. */
static void handle_byte(unsigned char c)
{
    static int seen_letter;
    if (c == '\n' || c == '\r') {
        seen_letter = 0;
        return;
    }
    if (c == ' ' || seen_letter)
        return;
    seen_letter = 1;
    if (c == 'A' || c == 'B' || c == 'C' || c == 'D' || c == 'O') {
        pattern = (char)c;
        t_ms = 0;
        led_duty(duty_for(pattern, 0));
        reply(pattern);
    } else {
        reply('?');
    }
}

void app_main(void)
{
    unsigned char buf[32];
    led_init();
    uart_init();
    printf("e15 sidecar, built %s %s, LED on GPIO%d, UART2 TX GPIO%d RX GPIO%d\n",
           __DATE__, __TIME__, LED_GPIO, UART_TX_GPIO, UART_RX_GPIO);
    led_duty(0);
    for (;;) {
        int n = uart_read_bytes(UART_PORT, buf, sizeof buf, pdMS_TO_TICKS(TICK_MS));
        for (int i = 0; i < n; i++)
            handle_byte(buf[i]);
        t_ms += TICK_MS;
        if (t_ms >= 2000)
            t_ms -= 2000;                      /* every pattern's period divides 2000 ms */
        led_duty(duty_for(pattern, t_ms));
    }
}
