VIA_ENABLE = yes
VIAL_ENABLE = yes
QMK_SETTINGS = yes
RAW_ENABLE = yes
SRC += secret_storage.c

# PS/2 mouse (右側のみで使用、左側では初期化スキップ)
# ps2_vendor.c はローカルにカスタム版を配置（左側スキップ対応）
PS2_MOUSE_ENABLE = yes
PS2_DRIVER = vendor

TAP_DANCE_ENABLE = yes
COMBO_ENABLE = yes
KEY_OVERRIDE_ENABLE = yes
EXTRAKEY_ENABLE = yes
MOUSEKEY_ENABLE = yes
UNICODE_ENABLE = yes
SRC += ps2_split.c gesture.c modes.c
SPLIT_TRANSPORT = serial

# MIDI 機能を有効化
MIDI_ENABLE = yes

# OS自動検出を有効化
OS_DETECTION_ENABLE = yes

# Enable pointing-device split pathway using a custom driver so the
# slave can populate the shared pointing report and the master will send HID.
POINTING_DEVICE_ENABLE = yes
SPLIT_POINTING_ENABLE = yes
# Use a 'custom' pointing driver to avoid sensor-specific driver inclusion
SRC += ps2_pointing.c

POINTING_DEVICE_DRIVER = custom
VIAL_INSECURE = no
CONSOLE_ENABLE = yes
DEBUG_ENABLE = yes

BOOTMAGIC_ENABLE = full


## Use RP2040 wear-leveling driver to expose more EEPROM for Vial dynamic data
WEAR_LEVELING_DRIVER = rp2040_flash

# Raw HID for secret storage over WebHID
RAW_ENABLE = yes

# Secret storage implementation

MIDI_ENABLE = yes
