/* Copyright 2022 @ lokher (https://www.keychron.com)
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
#include "wireless.h"
#include "report_buffer.h"
#include "lpm.h"
#include "battery.h"
#include "indicator.h"
#include "transport.h"
#include "rtc_timer.h"
#include "lemokey_wt_common.h"
#include "lemokey_task.h"

extern uint8_t         pairing_indication;
extern host_driver_t   chibios_driver;
extern report_buffer_t kb_rpt;
extern uint32_t        retry_time_buffer;
extern uint8_t         retry;

#ifdef NKRO_ENABLE
extern nkro_t nkro;
#endif

static uint8_t host_index = 0;
static uint8_t led_state  = 0;

extern wt_func_t wireless_transport;
static wt_state_t wt_state                  = WT_RESET;
static bool              pincodeEntry              = false;
uint8_t                  wt_report_protocol = true;

/* declarations */
uint8_t wt_keyboard_leds(void);
void    wt_send_keyboard(report_keyboard_t *report);
void    wt_send_mouse(report_mouse_t *report);
void    wt_send_extra(report_extra_t         *report);
bool    process_record_wireless(uint16_t keycode, keyrecord_t *record);

/* host struct */
host_driver_t wt_driver = {wt_keyboard_leds, wt_send_keyboard, wt_send_mouse, wt_send_extra};

#define WT_EVENT_QUEUE_SIZE 16
wt_event_t wt_event_queue[WT_EVENT_QUEUE_SIZE];
uint8_t           wt_event_queue_head;
uint8_t           wt_event_queue_tail;

void wt_bt_event_queue_init(void) {
    // Initialise the event queue
    memset(&wt_event_queue, 0, sizeof(wt_event_queue));
    wt_event_queue_head = 0;
    wt_event_queue_tail = 0;
}

bool wt_event_queue_enqueue(wt_event_t event) {
    uint8_t next = (wt_event_queue_head + 1) % WT_EVENT_QUEUE_SIZE;
    if (next == wt_event_queue_tail) {
        /* Override the first report */
        wt_event_queue_tail = (wt_event_queue_tail + 1) % WT_EVENT_QUEUE_SIZE;
    }
    wt_event_queue[wt_event_queue_head] = event;
    wt_event_queue_head                 = next;
    return true;
}

static inline bool wt_event_queue_dequeue(wt_event_t *event) {
    if (wt_event_queue_head == wt_event_queue_tail) {
        return false;
    }
    *event              = wt_event_queue[wt_event_queue_tail];
    wt_event_queue_tail = (wt_event_queue_tail + 1) % WT_EVENT_QUEUE_SIZE;
    return true;
}

/*
 * Bluetooth init.
 */
void wt_init(void) {
    wt_state = WT_INITIALIZED;

    wt_bt_event_queue_init();
#ifndef DISABLE_REPORT_BUFFER
    report_buffer_init();
#endif
    indicator_init();
#ifdef BLUETOOTH_INT_INPUT_PIN
    setPinInputHigh(BLUETOOTH_INT_INPUT_PIN);
#endif

    lpm_init();
#if HAL_USE_RTC
    rtc_timer_init();
#endif
#ifdef NKRO_ENABLE
#ifdef BLUETOOTH_NKRO_ENABLE
    keymap_config.raw = eeconfig_read_keymap();
    nkro.bluetooth = keymap_config.nkro;
#endif
#endif
#ifdef LEMOKEY_CALLBACK_ENABLE
    register_wt_tasks();
    register_record_process(process_record_wireless, false);
#endif
}

/*
 * Bluetooth trasponrt init. Bluetooth module driver shall use this function to register a callback
 * to its implementation.
 */
void wt_set_transport(wt_func_t *transport) {
    if (transport) memcpy(&wireless_transport, transport, sizeof(wt_func_t));
}

/*
 * Enter pairing with current host index
 */
void wt_pairing(void) {
    if (battery_is_critical_low()) return;

    wt_pairing_ex(0, NULL);
    wt_state = WT_PARING;
}

/*
 * Enter pairing with specified host index and param
 */
void wt_pairing_ex(uint8_t host_idx, void *param) {
////LOG_TRACE("wt_pairing_ex\n\r");
    if (battery_is_critical_low()) return;

    if (wireless_transport.pairing_ex) wireless_transport.pairing_ex(host_idx, param);
    wt_state = WT_PARING;

    host_index = host_idx;
}

/*
 * Initiate connection request to paired host
 */
void wt_connect(void) {
    /*  Work around empty report after wakeup, which leads to reconneect/disconnected loop */
    if (battery_is_critical_low() || timer_read32() == 0) return;

    wireless_transport.connect_ex(0, 0);
    wt_state = WT_RECONNECTING;
}

/*
 * Initiate connection request to paired host with argument
 */
void wt_connect_ex(uint8_t host_idx, uint16_t timeout) {
////LOG_TRACE("wt_connect_ex %d\n\r", host_idx);
    if (battery_is_critical_low()) return;

    if (host_idx != 0) {
        if (host_index == host_idx && wt_state == WT_CONNECTED) return;
        host_index = host_idx;
        led_state  = 0;
    }
    wireless_transport.connect_ex(host_idx, timeout);
    wt_state = WT_RECONNECTING;
}

/* Initiate a disconnection */
void wt_disconnect(void) {
//LOG_TRACE("wt_disconnect\n\r");
    if (wireless_transport.disconnect) wireless_transport.disconnect();
}


/* Called when the BT device is reset. */
static void wt_enter_reset(uint8_t reason) {
    wt_state = WT_RESET;
    wt_enter_reset_kb(reason);
}

/* Enters discoverable state. Upon entering this state we perform the following actions:
 *   - change state to WT_PARING
 *   - set pairing indication
 */
static void wt_enter_discoverable(uint8_t host_idx) {
    ////LOG_TRACE("wt_enter_discoverable: %d\n\r", host_idx);

    wt_state = WT_PARING;
    indicator_set(wt_state, host_idx);
    wt_enter_discoverable_kb(host_idx);
}

/*
 * Enters reconnecting state. Upon entering this state we perform the following actions:
 *   - change state to RECONNECTING
 *   - set reconnect indication
 */
static void wt_enter_reconnecting(uint8_t host_idx) {
uprintf("wt_enter_reconnecting %d", host_idx);
    wt_state = WT_RECONNECTING;
    indicator_set(wt_state, host_idx);
    wt_enter_reconnecting_kb(host_idx);
}

/* Enters connected state. Upon entering this state we perform the following actions:
 *   - change state to CONNECTED
 *   - set connected indication
 *   - enable bluetooth NKRO is support
 */
static void wt_enter_connected(uint8_t host_idx) {
    ////LOG_TRACE("wt_enter_connected %d\n\r", host_idx);

    wt_state = WT_CONNECTED;
    indicator_set(wt_state, host_idx);
    host_index = host_idx;

    clear_keyboard();

    /* Enable NKRO since it may be disabled in pin code entry */
#if defined(NKRO_ENABLE) && defined(BLUETOOTH_NKRO_ENABLE)
    keymap_config.nkro = nkro.bluetooth;
#else
    keymap_config.nkro = false;
#endif

    wt_enter_connected_kb(host_idx);
#ifdef BAT_LOW_LED_PIN
    if (battery_is_empty()) {
        indicator_battery_low_enable(true);
    }
#endif
}

/* Enters disconnected state. Upon entering this state we perform the following actions:
 *   - change state to DISCONNECTED
 *   - set disconnected indication
 */
static void wt_enter_disconnected(uint8_t host_idx) {
    LOG_TRACE("\wt_enter_disconnected %d\r", host_idx);

    uint8_t previous_state = wt_state;
    wt_state               = WT_DISCONNECTED;

    if (previous_state == WT_CONNECTED) {
        lpm_timer_reset();
        indicator_set(WT_SUSPEND, host_idx);
    } else
        indicator_set(wt_state, host_idx);

#ifndef DISABLE_REPORT_BUFFER
    report_buffer_init();
#endif
    retry = 0;
    wt_enter_disconnected_kb(host_idx);
#ifdef BAT_LOW_LED_PIN
    indicator_battery_low_enable(false);
#endif
#if defined(LOW_BAT_IND_INDEX)
    indicator_battery_low_backlit_enable(false);
#endif
}

/* Enter pin code entry state. */
static void bluetooth_enter_pin_code_entry(void) {
#if defined(NKRO_ENABLE)
    keymap_config.nkro = FALSE;
#endif
    pincodeEntry = true;
    wt_enter_pin_code_entry_kb();
}

/* Exit pin code entry state. */
static void bluetooth_exit_pin_code_entry(void) {
#if defined(NKRO_ENABLE)
        keymap_config.nkro = true;
#endif
    pincodeEntry              = false;
    wt_exit_pin_code_entry_kb();
}

/* Enters disconnected state. Upon entering this state we perform the following actions:
 *   - change state to DISCONNECTED
 *   - set disconnected indication
 */
static void wt_enter_sleep(void) {
    ////LOG_TRACE("wt_enter_sleep\n\r");

    //uint8_t previous_state = wt_state;
    wt_state               = WT_SUSPEND;

    wt_enter_sleep_kb();
    indicator_set(wt_state, 0);
#ifdef BAT_LOW_LED_PIN
    indicator_battery_low_enable(false);
#endif
#if defined(LOW_BAT_IND_INDEX)
    indicator_battery_low_backlit_enable(false);
#endif
}

__attribute__((weak)) void wirless_enter_reset_kb(uint8_t reason){}
__attribute__((weak)) void wirless_enter_discoverable_kb(uint8_t host_idx){}
__attribute__((weak)) void wirless_enter_reconnecting_kb(uint8_t host_idx){}
__attribute__((weak)) void wirless_enter_connected_kb(uint8_t host_idx){}
__attribute__((weak)) void wirless_enter_disconnected_kb(uint8_t host_idx){}
__attribute__((weak)) void wirless_enter_bluetooth_pin_code_entry_kb(void) {}
__attribute__((weak)) void wirless_exit_bluetooth_pin_code_entry_kb(void){}
__attribute__((weak)) void wirless_enter_sleep_kb(void){}

/*  */
static void wt_hid_set_protocol(bool report_protocol) {
    wt_report_protocol = false;
}

uint8_t wt_keyboard_leds(void) {
    if (wt_state == WT_CONNECTED) {
        return led_state;
    }

    return 0;
}

extern keymap_config_t keymap_config;

void wt_send_keyboard(report_keyboard_t *report) {
    if (wt_state == WT_PARING && !pincodeEntry) return;

    if (wt_state == WT_CONNECTED || (wt_state == WT_PARING && pincodeEntry)) {
#    if defined(NKRO_ENABLE)
        if (wt_report_protocol && keymap_config.nkro) {
            if (wireless_transport.send_nkro) {
#        ifndef DISABLE_REPORT_BUFFER
//                bool firstBuffer = false;
//                if (report_buffer_is_empty() && report_buffer_next_inverval() && report_buffer_get_retry() == 0) {
//                    firstBuffer = true;
//                }

                report_buffer_t report_buffer;
                report_buffer.type = REPORT_TYPE_NKRO;
                memcpy(&report_buffer.keyboard, report, sizeof(report_keyboard_t));
                report_buffer_enqueue(&report_buffer);
//
//                if (firstBuffer) {
//                    report_buffer_set_retry(30);
//                    report_buffer_task();
//                }
#        else
                wireless_transport.send_nkro(&report->nkro.mods);
#        endif
            }
        } else
#    endif
        {
            //#ifdef KEYBOARD_SHARED_EP
            if (wireless_transport.send_keyboard) {
#    ifndef DISABLE_REPORT_BUFFER
//                bool firstBuffer = false;
//                if (report_buffer_is_empty() && report_buffer_next_inverval() && report_buffer_get_retry() == 0) {
//                    firstBuffer = true;
//                }

//                if (report_buffer_is_empty() && report_buffer_next_inverval()) {
//                    wireless_transport.send_keyboard(&report->mods);
//                    report_buffer_update_timer();
//                } else
                {
                    report_buffer_t report_buffer;
                    report_buffer.type = REPORT_TYPE_KB;
                    memcpy(&report_buffer.keyboard, report, sizeof(report_keyboard_t));
                    report_buffer_enqueue(&report_buffer);
                }

//                if (firstBuffer) {
//                    report_buffer_set_retry(30);
//                    report_buffer_task();
//                }
#    else
                wireless_transport.send_keyboard(&report->mods);
#    endif
            }
            //#endif
        }

    } else if (wt_state != WT_RESET) {
        wt_connect();
    }
}

void wt_send_mouse(report_mouse_t *report) {
    if (wt_state == WT_CONNECTED) {
        if (wireless_transport.send_mouse) wireless_transport.send_mouse((uint8_t *)report);
    } else if (wt_state != WT_RESET) {
        wt_connect();
    }
}

void wt_send_system(uint16_t data) {
    if (wt_state == WT_CONNECTED) {
        if (wireless_transport.send_system) wireless_transport.send_system(data);
    } else if (wt_state != WT_RESET) {
        wt_connect();
    }
}

void wt_send_consumer(uint16_t data) {
    if (wt_state == WT_CONNECTED) {
#ifndef DISABLE_REPORT_BUFFER
        if (report_buffer_is_empty() && report_buffer_next_inverval()) {
            if (wireless_transport.send_consumer) wireless_transport.send_consumer(data);
            report_buffer_update_timer();
        } else {
            report_buffer_t report_buffer;
            report_buffer.type     = REPORT_TYPE_CONSUMER;
            report_buffer.consumer = data;
            report_buffer_enqueue(&report_buffer);
        }
#else
        if (wireless_transport.send_consumer) wireless_transport.send_consumer(data);
#endif
    } else if (wt_state != WT_RESET) {
        wt_connect();
    }
}

void wt_send_extra(report_extra_t         *report) {
    if (report->report_id == REPORT_ID_SYSTEM) {
        wt_send_system(report->usage);
    }
    else if (report->report_id == REPORT_ID_CONSUMER) {
        wt_send_consumer(report->usage);
    }
}

void wt_low_battery_shutdown(void) {
#ifdef BAT_LOW_LED_PIN
    indicator_battery_low_enable(false);
#endif
#if defined(LOW_BAT_IND_INDEX)
    indicator_battery_low_backlit_enable(false);
#endif
    wt_disconnect();
}

void wt_event_queue_task(void) {
    wt_event_t event;
    while (wt_event_queue_dequeue(&event)) {
        switch (event.evt_type) {
            case EVT_RESET:
                wt_enter_reset(event.params.reason);
                break;
            case EVT_CONNECTED:
                wt_enter_connected(event.params.hostIndex);
                break;
            case EVT_DISCOVERABLE:
                wt_enter_discoverable(event.params.hostIndex);
                break;
            case EVT_RECONNECTING:
                wt_enter_reconnecting(event.params.hostIndex);
                break;
            case EVT_DISCONNECTED:
                led_state = 0;
                wt_enter_disconnected(event.params.hostIndex);
                break;
            case EVT_PINCODE_ENTRY:
                bluetooth_enter_pin_code_entry();
                break;
            case EVT_EXIT_PINCODE_ENTRY:
                bluetooth_exit_pin_code_entry();
                break;
            case EVT_SLEEP:
                wt_enter_sleep();
                break;
            case EVT_HID_INDICATOR:
                led_state = event.params.led;
                break;
            case EVT_HID_SET_PROTOCOL:
                wt_hid_set_protocol(event.params.protocol);
                break;
            case EVT_CONECTION_INTERVAL:
                report_buffer_set_inverval(event.params.interval);
                break;
            default:
                break;
        }
    }
}

void wt_task(void) {
	//LOG_TRACE("wt_task"); 
    wireless_transport.task();
    wt_event_queue_task();
#ifndef DISABLE_REPORT_BUFFER
    report_buffer_task();
#endif
    indicator_task();
    lemokey_wt_common_task();
    battery_task();
    lpm_task();
}

wt_state_t wt_get_state(void) {
    return wt_state;
};

bool process_record_wt(uint16_t keycode, keyrecord_t *record) {

    if (get_transport() == TRANSPORT_BLUETOOTH) {
        lpm_timer_reset();

#if defined(BAT_LOW_LED_PIN) || defined(LOW_BAT_IND_INDEX)
        if (battery_is_empty() && wt_get_state() == WT_CONNECTED && record->event.pressed) {
#    if defined(BAT_LOW_LED_PIN)
            indicator_battery_low_enable(true);
#    endif
#    if defined(LOW_BAT_IND_INDEX)
            indicator_battery_low_backlit_enable(true);
#    endif
        }
#endif
    }

    if (!process_record_lemokey_wireless(keycode, record))
        return false;

    return true;
    // return process_record_user(keycode, record);
}
