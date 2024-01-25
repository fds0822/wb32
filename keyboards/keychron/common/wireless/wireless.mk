OPT_DEFS += -DLK_WIRELESS_ENABLE
OPT_DEFS += -DNO_USB_STARTUP_CHECK
OPT_DEFS += -DCORTEX_ENABLE_WFI_IDLE=TRUE

WT_DIR = common/wireless
SRC += \
     $(WT_DIR)/wireless.c \
     $(WT_DIR)/report_buffer.c \
     $(WT_DIR)/lkbt51.c \
     $(WT_DIR)/indicator.c \
     $(WT_DIR)/wireless_main.c \
     $(WT_DIR)/transport.c \
     $(WT_DIR)/lpm.c \
     $(WT_DIR)/lpm_stm32f401.c \
     $(WT_DIR)/battery.c \
     $(WT_DIR)/bat_level_animation.c \
     $(WT_DIR)/rtc_timer.c \
     $(WT_DIR)/lp_sleep.c \
     $(WT_DIR)/lemokey_wireless_common.c

VPATH += $(TOP_DIR)/keyboards/keychron/$(WT_DIR)

