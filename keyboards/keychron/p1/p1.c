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

#include "quantum.h"
#include "lemokey_task.h"
#ifdef FACTORY_TEST_ENABLE
#    include "factory_test.h"
#    include "lemokey_common.h"
#endif
#ifdef LK_WIRELESS_ENABLE
#    include "lkbt51.h"
#    include "wireless.h"
#    include "lemokey_wireless_common.h"
#    include "battery.h"
#endif
#if HAL_USE_ADC
#    include "analog.h"
#endif

#ifdef LEMOKEY_CALLBACK_ENABLE
bool lemokey_task_kb(void);
#endif

#define POWER_ON_LED_DURATION 3000
static uint32_t power_on_indicator_timer_buffer;





#ifdef DIP_SWITCH_ENABLE
bool dip_switch_update_kb(uint8_t index, bool active) {
    if (index == 0) {
        //default_layer_set(1UL << (active ? 2 : 0));
		default_layer_set(1UL << (active ? 0 : 2));
    }
    dip_switch_update_user(index, active);

    return true;
}
#endif


void eeconfig_init_kb(void) {
#if (EECONFIG_KB_DATA_SIZE) == 0
    // Reset Keyboard EEPROM value to blank, rather than to a set value
    eeconfig_update_kb(0);
#endif

	keymap_config.raw  = eeconfig_read_keymap();
	keymap_config.nkro = 0;
	eeconfig_update_keymap(keymap_config.raw);

    eeconfig_init_user();

	eeprom_update_byte(EECONFIG_DEFAULT_LAYER, 1U << 2);
	default_layer_set(1U << 2);
}


bool process_record_lemokey_kb(uint16_t keycode, keyrecord_t *record) {
	//LOG_TRACE("p1.c keycode: %08x \n\r", keycode);	
		return true;
}

void keyboard_post_init_kb(void) {
#ifdef LK_WIRELESS_ENABLE
    //palSetLineMode(P2P4_MODE_SELECT_PIN, PAL_MODE_INPUT);
    //palSetLineMode(BT_MODE_SELECT_PIN, PAL_MODE_INPUT);

    palSetLineMode(B2, PAL_MODE_INPUT);
    palSetLineMode(C5, PAL_MODE_INPUT);
	
	setPinOutput(BAT_CHGLED_PIN);
	writePin(BAT_CHGLED_PIN, BAT_LOW_LED_PIN_ON_STATE);


    lkbt51_init(false);
    wireless_init();
#endif

	power_on_indicator_timer_buffer = timer_read32();

#ifdef ENCODER_ENABLE
    encoder_cb_init();
#endif

#ifdef LEMOKEY_CALLBACK_ENABLE
    factory_test_init();
    register_record_process(process_record_lemokey_kb, false);
    register_lemokey_task(lemokey_task_kb, false);
#endif
    keyboard_post_init_user();
}

bool lemokey_task_kb(void) {
    if (power_on_indicator_timer_buffer) {
        if (timer_elapsed32(power_on_indicator_timer_buffer) > POWER_ON_LED_DURATION) {
            power_on_indicator_timer_buffer = 0;

#ifdef LK_WIRELESS_ENABLE
            writePin(BAT_CHGLED_PIN, !BAT_LOW_LED_PIN_ON_STATE);
#endif

        } else {
#ifdef LK_WIRELESS_ENABLE
            writePin(BAT_CHGLED_PIN, BAT_LOW_LED_PIN_ON_STATE);
#endif
        }
    }
    return true;
}

#ifdef LK_WIRELESS_ENABLE
bool lpm_is_kb_idle(void) {
    return power_on_indicator_timer_buffer == 0 && !factory_reset_indicating();
}
#endif
