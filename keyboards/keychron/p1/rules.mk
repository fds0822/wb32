# MCU name
MCU = WB32F3G71

# Bootloader selection
BOOTLOADER = wb32-dfu

# Build Options
#   change yes to no to disable
#
BOOTMAGIC_ENABLE = yes      # Enable boot magic
MOUSEKEY_ENABLE = yes       # Mouse keys
EXTRAKEY_ENABLE = yes       # Audio control and System control
CONSOLE_ENABLE = no         # Console for debug
COMMAND_ENABLE = no         # Commands for debug and configuration
NKRO_ENABLE = yes           # USB Nkey Rollover
DIP_SWITCH_ENABLE = no
RAW_ENABLE = yes
ENCODER_ENABLE = yes        # Enable Encoder
ENCODER_MAP_ENABLE = yes
EEPROM_DRIVER = i2c         # wear_leveling
# WEAR_LEVELING_DRIVER = embedded_flash
SEND_STRING_ENABLE = yes

DEBOUNCE_TYPE = sym_eager_pk
#    "build": {
#        "debounce_type": "sym_eager_pk"
#    },
# Enter lower-power sleep mode when on the ChibiOS idle thread
#OPT_DEFS += -O1
#OPT_DEFS += -DWERELESS_PRESSURE_TEST

#SRC += analog.c

include keyboards/keychron/common/wireless/wireless.mk

include keyboards/keychron/common/lemokey_common.mk


VPATH += $(TOP_DIR)/keyboards/keychron
SRC += ./gpio_uart.c


