/* Copyright 2021 @ Keychron (https://www.keychron.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ckled2001-simple_spi.h"
#include "spi_master.h"
#include "wait.h"
#include "util.h"

#ifndef CKLED2001_TIMEOUT
#    define CKLED2001_TIMEOUT 100
#endif

#ifndef CKLED2001_PERSISTENCE
#    define CKLED2001_PERSISTENCE 0
#endif

#ifndef PHASE_CHANNEL
#    define PHASE_CHANNEL MSKPHASE_12CHANNEL
#endif

#ifndef CKLED2001_CURRENT_TUNE
#    define CKLED2001_CURRENT_TUNE \
        { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }
#endif

#define CLKED_LED_COUNT 192

#define CKLED_WRITE (0 << 7)
#define CKLED_READ (1 << 7)
#define CKLED_PATTERN (2 << 4)

// uint8_t g_spi_transfer_buffer[67];

// These buffers match the CKLED2001 PWM registers.
// The control buffers match the PG0 LED On/Off registers.
// Storing them like this is optimal for I2C transfers to the registers.
// We could optimize this and take out the unused registers from these
// buffers and the transfers in CKLED2001_write_pwm_buffer() but it's
// probably not worth the extra complexity.
uint8_t g_pwm_buffer[DRIVER_COUNT][192];
bool    g_pwm_buffer_update_required[DRIVER_COUNT] = {false};

uint8_t g_led_control_registers[DRIVER_COUNT][24]             = {0};
bool    g_led_control_registers_update_required[DRIVER_COUNT] = {false};

#ifdef DRIVER_CS_PINS
pin_t cs_pins[] = DRIVER_CS_PINS;
#else
error "no DRIVER_CS_PINS defined"
#endif

bool CKLED2001_write(uint8_t index, uint8_t page, uint8_t reg, uint8_t *data, uint8_t len) {
    // //LOG_TRACE("%x, w: p:%d, r:%x, d:%x, len:%d\n\r", index & 0xFF, page, reg, data[0], len);
    static uint8_t spi_transfer_buffer[19] = {0};

    if (index > ARRAY_SIZE(((pin_t[])DRIVER_CS_PINS)) - 1) return false;

    if (!spi_start(cs_pins[index], false, 0, CKLED_SPI_DIVISOR)) {
        spi_stop();
        return false;
    }

    spi_transfer_buffer[0] = CKLED_WRITE | CKLED_PATTERN | (page & 0x0F);
    spi_transfer_buffer[1] = reg;

    if (spi_transmit(spi_transfer_buffer, 2) != SPI_STATUS_SUCCESS) {
        spi_stop();
        return false;
    }

    if (spi_transmit(data, len) != SPI_STATUS_SUCCESS) {
        spi_stop();
        return false;
    }

    spi_stop();
    return true;
}

bool CKLED2001_write_register(uint8_t index, uint8_t page, uint8_t reg, uint8_t data) {
    return CKLED2001_write(index, page, reg, &data, 1);
}

bool CKLED2001_write_pwm_buffer(uint8_t index, uint8_t *pwm_buffer) {
#if 1
    if (g_pwm_buffer_update_required[index]) {
        CKLED2001_write(index, LED_PWM_PAGE, 0, g_pwm_buffer[index], CLKED_LED_COUNT);
    }
    g_pwm_buffer_update_required[index] = false;
#else
    // Assumes PG1 is already selected.
    // If any of the transactions fails function returns false.
    // Transmit PWM registers in 3 transfers of 64 bytes.

    // Iterate over the pwm_buffer contents at 64 byte intervals.
    for (uint8_t i = 0; i < 192; i += 64) {
        g_spi_transfer_buffer[0] = i;
        // Copy the data from i to i+63.
        // Device will auto-increment register for data after the first byte
        // Thus this sets registers 0x00-0x0F, 0x10-0x1F, etc. in one transfer.
        for (uint8_t j = 0; j < 64; j++) {
            g_spi_transfer_buffer[1 + j] = pwm_buffer[i + j];
        }

        if (spi_transmit(index, g_spi_transfer_buffer, 65, CKLED2001_TIMEOUT) != 0) {
            spi_stop();
            return false;
        }
    }
#endif
    return true;
}

void CKLED2001_init(uint8_t index) {
    setPinOutput(cs_pins[index]);
    writePinHigh(cs_pins[index]);
    // Setting LED driver to shutdown mode
    CKLED2001_write_register(index, FUNCTION_PAGE, CONFIGURATION_REG, MSKSW_SHUT_DOWN_MODE);
    // Setting internal channel pulldown/pullup
    CKLED2001_write_register(index, FUNCTION_PAGE, PDU_REG, MSKSET_CA_CB_CHANNEL);
    // Select number of scan phase
    CKLED2001_write_register(index, FUNCTION_PAGE, SCAN_PHASE_REG, PHASE_CHANNEL);
    // Setting PWM Delay Phase
    CKLED2001_write_register(index, FUNCTION_PAGE, SLEW_RATE_CONTROL_MODE1_REG, MSKPWM_DELAY_PHASE_ENABLE);
    // Setting Driving/Sinking Channel Slew Rate
    CKLED2001_write_register(index, FUNCTION_PAGE, SLEW_RATE_CONTROL_MODE2_REG, MSKDRIVING_SINKING_CHHANNEL_SLEWRATE_ENABLE);
    // Setting Iref
    CKLED2001_write_register(index, FUNCTION_PAGE, SOFTWARE_SLEEP_REG, MSKSLEEP_DISABLE);

    // Set LED CONTROL PAGE (Page 0)
    uint8_t on_off_reg[LED_CONTROL_ON_OFF_LENGTH] = {0};
    CKLED2001_write(index, LED_CONTROL_PAGE, 0, on_off_reg, LED_CONTROL_ON_OFF_LENGTH);

    // Set PWM PAGE (Page 1)
    uint8_t pwm_reg[LED_PWM_LENGTH];
    memset(pwm_reg, 0, LED_PWM_LENGTH);
    CKLED2001_write(index, LED_PWM_PAGE, 0, pwm_reg, LED_PWM_LENGTH);

    // Set CURRENT PAGE (Page 4)
    uint8_t current_tune_reg[LED_CURRENT_TUNE_LENGTH] = CKLED2001_CURRENT_TUNE;
    CKLED2001_write(index, CURRENT_TUNE_PAGE, 0, current_tune_reg, LED_CURRENT_TUNE_LENGTH);

    //    // Enable LEDs ON/OFF
    //    memset(on_off_reg, 0xFF, LED_CONTROL_ON_OFF_LENGTH);
    //    CKLED2001_write(index, LED_CONTROL_PAGE, 0, on_off_reg, LED_CONTROL_ON_OFF_LENGTH);

    // Setting LED driver to normal mode
    CKLED2001_write_register(index, FUNCTION_PAGE, CONFIGURATION_REG, MSKSW_NORMAL_MODE);
}

void CKLED2001_set_value(int index, uint8_t value) {
    ckled2001_led led;
    if (index >= 0 && index < LED_MATRIX_LED_COUNT) {
        memcpy_P(&led, (&g_ckled2001_leds[index]), sizeof(led));

        g_pwm_buffer[led.driver][led.v]          = value;
        g_pwm_buffer_update_required[led.driver] = true;
    }
}

void CKLED2001_set_value_all(uint8_t value) {
    for (int i = 0; i < LED_MATRIX_LED_COUNT; i++) {
        CKLED2001_set_value(i, value);
    }
}

void CKLED2001_set_led_control_register(uint8_t index, bool value) {
    ckled2001_led led;
    memcpy_P(&led, (&g_ckled2001_leds[index]), sizeof(led));

    uint8_t control_register = led.v / 8;
    uint8_t bit_value        = led.v % 8;

    if (value) {
        g_led_control_registers[led.driver][control_register] |= (1 << bit_value);
    } else {
        g_led_control_registers[led.driver][control_register] &= ~(1 << bit_value);
    }

    g_led_control_registers_update_required[led.driver] = true;
}

void CKLED2001_update_pwm_buffers(uint8_t index) {
    if (g_pwm_buffer_update_required[index]) {
        if (!CKLED2001_write_pwm_buffer(index, g_pwm_buffer[index])) {
            g_led_control_registers_update_required[index] = true;
        }
    }
    g_pwm_buffer_update_required[index] = false;
}

void CKLED2001_update_led_control_registers(uint8_t index) {
    if (g_led_control_registers_update_required[index]) {
        CKLED2001_write(index, LED_CONTROL_PAGE, 0, g_led_control_registers[index], 24);
    }
    g_led_control_registers_update_required[index] = false;
}

void CKLED2001_sw_return_normal(uint8_t index) {
    // Select to function page
    // Setting LED driver to normal mode
    CKLED2001_write_register(index, FUNCTION_PAGE, CONFIGURATION_REG, MSKSW_NORMAL_MODE);
}

void CKLED2001_sw_shutdown(uint8_t index) {
    // Select to function page
    // Setting LED driver to shutdown mode
    CKLED2001_write_register(index, FUNCTION_PAGE, CONFIGURATION_REG, MSKSW_SHUT_DOWN_MODE);
    // Write SW Sleep Register
    CKLED2001_write_register(index, FUNCTION_PAGE, SOFTWARE_SLEEP_REG, MSKSLEEP_ENABLE);
}
