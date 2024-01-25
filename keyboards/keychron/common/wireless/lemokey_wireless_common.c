/* Copyright 2022 @ Keychron (https://www.keychron.com)
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

#include QMK_KEYBOARD_H
#ifdef LK_WIRELESS_ENABLE
#    include "lkbt51.h"
#    include "wireless.h"
#    include "indicator.h"
#    include "transport.h"
#    include "battery.h"
#    include "bat_level_animation.h"
#    include "lpm.h"
#    include "lemokey_wireless_common.h"
#    include "lemokey_task.h"
#endif
#include "debounce.h"
#include "lemokey_common.h"

bool firstDisconnect = true;

static uint32_t pairing_key_timer;
static uint8_t  host_idx = 0;

#ifdef USB_HOST_LED_MATRIX_LIST
extern bool usb_host_indicator_enable;
extern uint8_t indicator_usb_host_count;
#endif
#ifdef	OsSwitch_LED_MATRIX_LIST
static uint32_t ossw_timer   = 0;
bool ossw_indicator_enable  = false;
#endif
#ifdef	WinLock_LED_MATRIX_LIST
static uint32_t winlock_timer   = 0;
#endif
#ifdef	WIN_LOCK_LED_PIN
static uint32_t winlock_timer   = 0;
#endif

//#define RtcTest
#ifdef  RtcTest
#include "rtc_timer.h"
#endif


bool process_record_lemokey_wireless(uint16_t keycode, keyrecord_t *record) {
    static uint8_t host_idx;
	//LOG_TRACE("keycode-LW: %04x\n\r", keycode);

    switch (keycode) {
        case BT_HST1 ... BT_HST3:			
            //if (get_transport() == TRANSPORT_BLUETOOTH) {
                if (record->event.pressed) {
					set_transport(TRANSPORT_BLUETOOTH);
					eeconfig_update_default_mode(TRANSPORT_BLUETOOTH);
                    host_idx = keycode - BT_HST1 + 1;
                    LOG_TRACE("idx: %d\n\r", host_idx);

                    pairing_key_timer = timer_read32();
                    wireless_connect_ex(host_idx, 0);
                } else {
                    host_idx          = 0;
                    pairing_key_timer = 0;
                }
            //}            
            break;
        case P2P4G:			
            //if (get_transport() == TRANSPORT_P2P4) {
                if (record->event.pressed) {
					set_transport(TRANSPORT_P2P4);
					eeconfig_update_default_mode(TRANSPORT_P2P4);
                    host_idx = P24G_INDEX;
                    //LOG_TRACE("idx: %d\n\r", host_idx);
                    pairing_key_timer = timer_read32();
                    // wireless_connect_ex(host_idx, 0);
                } else {
                    host_idx          = 0;
                    pairing_key_timer = 0;
                }
            //}            
            break;
		case MD_USB:
				if (record->event.pressed) {					
					if (usb_power_connected()) {
							set_transport(TRANSPORT_USB);
							eeconfig_update_default_mode(TRANSPORT_USB);
		                    //host_idx = P24G_INDEX;
		                    //LOG_TRACE("idx: %d\n\r", host_idx);
		                    //pairing_key_timer = timer_read32();
		                    // wireless_connect_ex(host_idx, 0);
						}
                } else {
                    //host_idx          = 0;
                    //pairing_key_timer = 0;
                }			
			break;
#ifdef Custom_Switch
		case KC_OSSW:
            if (record->event.pressed) {                
                ossw_timer = timer_read32();  
				//LOG_TRACE("timer_read32(); : %d\n\r", ossw_timer);
            }else {
                    //host_idx          = 0;
                    //pairing_key_timer = 0;   
                    ossw_timer=0;
                }
            break;
#endif
		case GU_TOGG:
            if (record->event.pressed) {                
                winlock_timer = timer_read32();  
				//LOG_TRACE("timer_read32(); : %d\n\r", ossw_timer);
            }else {
                    //host_idx          = 0;
                    //pairing_key_timer = 0;   
                    winlock_timer=0;
                }
            break;
			
#if defined(LED_MATRIX_ENABLE) || defined(RGB_MATRIX_ENABLE)
        case BAT_LVL:
            if ((get_transport() & TRANSPORT_WIRELESS) && !usb_power_connected()) {
                bat_level_animiation_start(battery_get_percentage());
            }
            break;
#endif

        default:
#ifdef USB_HOST_LED_MATRIX_LIST
        	if(get_transport() == TRANSPORT_USB && USBD1.state!=USB_ACTIVE && keycode<QK_BASIC_MAX){
				usb_host_indicator_enable=true;
				indicator_usb_host_count=0;
        		}
#endif
            break;
    }

    return true;
}

void lkbt51_param_init(void) {
    //LOG_TRACE("lkbt51_param_init 3ms\n\r");
    /* Set bluetooth device name */
    lkbt51_set_local_name(PRODUCT);
    wait_ms(3);
    /* Set bluetooth parameters */
    module_param_t param = {.event_mode             = 0x02,
                            .connected_idle_timeout = 7200,
                            .pairing_timeout        = 180,
                            .pairing_mode           = 0,
                            .reconnect_timeout      = 5,
                            .report_rate            = 90,
                            .vendor_id_source       = 1,
                            .verndor_id             = 0x3434, // Must be 0x362d
                            .product_id             = PRODUCT_ID};
    lkbt51_set_param(&param);
}

void wireless_enter_reset_kb(uint8_t reason) {
    lkbt51_param_init();
}

void wireless_enter_disconnected_kb(uint8_t host_idx) {
    /* CKBT51 bluetooth module boot time is slower, it enters disconnected after boot,
       so we place initialization here. */
    if (firstDisconnect && timer_read32() < 1000) {
        lkbt51_param_init();
        if (get_transport() == TRANSPORT_BLUETOOTH) wireless_connect();
        firstDisconnect = false;
    }
}

void lemokey_wireless_common_task(void) {
    if (pairing_key_timer) {
        if (timer_elapsed32(pairing_key_timer) > 2000) {
            pairing_key_timer = 0;
            wireless_pairing_ex(host_idx, NULL);
        }
    }
}

void wireless_pre_task(void) {	
	static uint8_t mode = 0;
	static uint32_t time = 0;
	
#ifdef RtcTest
	static uint32_t rtc_time_test = 0;
	if (rtc_time_test == 0) {
		rtc_time_test=rtc_timer_read_ms();
		}

	if (rtc_time_test && rtc_timer_elapsed_ms(rtc_time_test) > 2000) {
			LOG_TRACE("rtc_time_test: %d\n\r",rtc_timer_elapsed_ms(rtc_time_test));
			rtc_time_test=0;	
		}
#endif

		if (time == 0) {			
					mode = eeconfig_read_default_mode();
					time = timer_read32();
					LOG_TRACE("mode1: %d\n\r", mode);
					switch (mode) {
					case TRANSPORT_USB:
						usb_host_indicator_enable=true;
						//if (usb_power_connected()) {
								LOG_TRACE("TP_USB\n\r");
								set_transport(TRANSPORT_USB);	
						//	}
						break;
					case TRANSPORT_BLUETOOTH:
						LOG_TRACE("TP_BT\n\r");
						set_transport(TRANSPORT_BLUETOOTH);						
						break;
					case TRANSPORT_P2P4:
						LOG_TRACE("TP_24\n\r");
						set_transport(TRANSPORT_P2P4);
						break;
					default:
						break;
						}
		}
		
#ifdef	OsSwitch_LED_MATRIX_LIST
	if (ossw_timer && timer_elapsed32(ossw_timer) > 3000) {
			//LOG_TRACE("ossw_timer: %d\n\r",timer_elapsed32(ossw_timer));
			default_layer_xor(1U << 0);
	        default_layer_xor(1U << 2);
	        eeconfig_update_default_layer(default_layer_state);
			ossw_timer=0;	
			ossw_indicator_enable=true;		
			//LOG_TRACE("default_layer_state: %d\n\r",default_layer_state);  default_layer_state：1-》mac     4->win
			#if defined (WinLock_LED_MATRIX_LIST) ||  defined(WIN_LOCK_LED_PIN)
			led_update_kb(host_keyboard_led_state());
			#endif
		}
#endif

#        if defined (WinLock_LED_MATRIX_LIST) ||  defined(WIN_LOCK_LED_PIN)
		if(keymap_config.no_gui)
		{
			//LOG_TRACE("keymap_config.no_gui00: %x\n\r",keymap_config.no_gui);
			if (winlock_timer && timer_elapsed32(winlock_timer) > 1) {				
				winlock_timer=0;
				keymap_config.no_gui = !keymap_config.no_gui;
				eeconfig_update_keymap(keymap_config.raw);
				led_update_kb(host_keyboard_led_state());
			}
		}
		else if (winlock_timer && timer_elapsed32(winlock_timer) > 3000) {				
				winlock_timer=0;
				keymap_config.no_gui = !keymap_config.no_gui;
				eeconfig_update_keymap(keymap_config.raw);
				led_update_kb(host_keyboard_led_state());
		}
#endif

#if 0
    static uint8_t mode = 0;

    // mode = readPin(BT_MODE_SELECT_PIN) << 1 | readPin(2P4_MODE_SELECT_PIN);
    static uint32_t time = 0;

    if (time == 0) {
        if ((readPin(BT_MODE_SELECT_PIN) << 1 | readPin(P2P4_MODE_SELECT_PIN)) != mode) {
            mode = readPin(BT_MODE_SELECT_PIN) << 1 | readPin(P2P4_MODE_SELECT_PIN);
            time = timer_read32();
			LOG_TRACE("mode1: %d\n\r", mode);
        }
    }

    if ((time && timer_elapsed32(time) > 100) || get_transport() == TRANSPORT_NONE) {
        if ((readPin(BT_MODE_SELECT_PIN) << 1 | readPin(P2P4_MODE_SELECT_PIN)) == mode) {
            time = 0;
            LOG_TRACE("mode: %d\n\r", mode);
            switch (mode) {
                case 0x01:
                    // //LOG_TRACE("TP_BT\n\r");
                    set_transport(TRANSPORT_BLUETOOTH);
                    break;
                case 0x02:
                    // //LOG_TRACE("TP_24\n\r");
                    set_transport(TRANSPORT_P2P4);
                    break;
                case 0x03:
                    //LOG_TRACE("TP_USB\n\r");
                    //set_transport(TRANSPORT_USB);

					//LOG_TRACE("TP_BT\n\r");
                    set_transport(TRANSPORT_BLUETOOTH);
                    break;
                default:
                    break;
            }
        } else {
            mode = readPin(BT_MODE_SELECT_PIN) << 1 | readPin(P2P4_MODE_SELECT_PIN);
            time = timer_read32();
        }
    }
#endif	
}

