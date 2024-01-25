/* Copyright 2023 @ Keychron (https://www.keychron.com)
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

#pragma once

/* I2C Driver Configuration */
#define I2C1_SCL_PIN B8
#define I2C1_SDA_PIN B9
#define I2C1_CLOCK_SPEED 400000
#define I2C1_DUTY_CYCLE FAST_DUTY_CYCLE_2

/* EEPROM Driver Configuration */
#define EXTERNAL_EEPROM_BYTE_COUNT 2048
#define EXTERNAL_EEPROM_PAGE_SIZE  32//64
#define EXTERNAL_EEPROM_WRITE_TIME 3//5

/* User used eeprom */
#define EECONFIG_USER_DATA_SIZE 1

#define I2C1_OPMODE OPMODE_I2C
#define EXTERNAL_EEPROM_I2C_BASE_ADDRESS 0b10100010

/* key matrix size */
#define MATRIX_ROWS 6
#define MATRIX_COLS 15

/* Key matrix pins */
#define MATRIX_ROW_PINS \
    { C12, D2, B3, B4, B5, B6 }
#define MATRIX_COL_PINS \
    { C0, C1, C2, C3, A0, A1, A2, A3, B10, B12, B13, C7, C8, C9, A10 }

/* COL2ROW or ROW2COL */
#define DIODE_DIRECTION ROW2COL

/* Debounce Time */
#define DEBOUNCE 20

/* Encoder Configuration */
#define ENCODER_DEFAULT_POS 0x3
/* Workaround encoder doesn't work correctly after waking up from OS sleep,
   which caused by USB driver */
#define ENCODER_MAP_KEY_DELAY 2

/* Turn off effects when suspended */
#define RGB_DISABLE_WHEN_USB_SUSPENDED

/* DIP switch for Mac/win OS switch */
/*
#define DIP_SWITCH_PINS \
    { C13 }
*/

#define DYNAMIC_KEYMAP_LAYER_COUNT 4
// #define DYNAMIC_KEYMAP_EEPROM_MAX_ADDR 2047

#  define LED_DRIVER_SHUTDOWN_PIN B14//B7
#ifdef LK_WIRELESS_ENABLE
/* Hardware configuration */

//#    define P2P4_MODE_SELECT_PIN A8//A10
//#    define BT_MODE_SELECT_PIN A9

#    define LKBT51_RESET_PIN C4
#    define LKBT51_INT_INPUT_PIN B1
#    define BLUETOOTH_INT_OUTPUT_PIN A4

#    define USB_POWER_SENSE_PIN B0
#    define USB_POWER_CONNECTED_LEVEL 0

#    define BAT_CHARGING_PIN C10//B13
#    define BAT_CHARGING_LEVEL 0
#    define BAT_CHGLED_PIN A15

#    define LOW_BAT_IND_INDEX 74
//#    define  BAT_LOW_LED_PIN   A15 
#    define BAT_LOW_LED_PIN_ON_STATE 1

#    define BT_HOST_DEVICES_COUNT 3

#    if defined(RGB_MATRIX_ENABLE) || defined(LED_MATRIX_ENABLE)

//#        define LED_DRIVER_SHUTDOWN_PIN B14//B7

#        define BT_HOST_LED_MATRIX_LIST \
            { 15, 16, 17 }

#        define P2P4G_HOST_LED_MATRIX_LIST \
	        { 18 }

#        define BAT_LEVEL_LED_LIST \
            { 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 }

#        define USB_HOST_LED_MATRIX_LIST \
			{ 19 }
/*
#        define OsSwitch_LED_MATRIX_LIST \
			{20}


#        define WinLock_LED_MATRIX_LIST \
			{72}
*/

/* Backlit disable timeout when keyboard is disconnected(unit: second) */
#        define DISCONNECTED_BACKLIGHT_DISABLE_TIMEOUT 40

/* Backlit disable timeout when keyboard is connected(unit: second) */
#        define CONNECTED_BACKLIGHT_DISABLE_TIMEOUT 600

/* Reinit LED driver on tranport changed */
#        define REINIT_LED_DRIVER 1

#    endif

/* Keep USB connection in blueooth mode */
#    define KEEP_USB_CONNECTION_IN_WIRELESS_MODE

/* Enable bluetooth NKRO */
#    define WIRELESS_NKRO_ENABLE

/* Raw hid command for factory test and bluetooth DFU */
#    define RAW_HID_CMD 0xAA ... 0xAB
#else
/* Raw hid command for factory test */
#    define RAW_HID_CMD 0xAB
#endif

/* Factory test keys */
#define FN_KEY_1 MO(1)
#define FN_KEY_2 MO(3)
#define FN_BL_TRIG_KEY KC_END

#define MATRIX_IO_DELAY 10

//#define Custom_Switch
