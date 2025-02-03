#include <stdint.h>
#include "quantum.h"
#define LAYOUT_split( \
    L00, L01, L02, L03, L04, L05, \
    L10, L11, L12, L13, L14, L15, \
         L20, L21, L22, L23, L24, \
              L30, L31, L32, L33, L34,\
    R00, R01, R02, R03, R04, R05, \
    R10, R11, R12, R13, R14, R15, \
    R20, R21, R22, R23, R24, \
    R30, R31, R32, R33, \
    R40, R41, R42, R43, R44 \
) { \
    { L00, L01, L02, L03, L04, L05, R00, R01, R02, R03, R04 }, \
    { L10, L11, L12, L13, L14, L15, R10, R11, R12, R13, R14 }, \
    { L20, L21, L22, L23, L24, R20, R21, R22, R23, R24, KC_NO }, \
    { L30, L31, L32, L33, L34, R30, R31, R32, R33, KC_NO, KC_NO }, \
    { KC_NO, KC_NO, KC_NO, KC_NO, R40, R41, R42, R43, R44, KC_NO, KC_NO } \
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT_split(
        KC_ESCAPE, KC_Q,      KC_L,      KC_U,       KC_COMMA,  KC_DOT,     \
        KC_TAB,    KC_E,      KC_I,      KC_A,       KC_O,      KC_MINUS,   \
        KC_LSFT,   KC_Z,      KC_X,      KC_C,       KC_V,                  \
        KC_LALT,   KC_LGUI,   KC_LCTL,   KC_LSFT,    KC_SPACE,            \
        KC_F,      KC_W,      KC_R,      KC_Y,       KC_SLASH,  KC_SEMICOLON, \
        KC_K,      KC_T,      KC_N,      KC_S,       KC_H,      KC_P,         \
        KC_G,      KC_D,      KC_M,      KC_J,       KC_B,               \
        KC_BSPC,   KC_RCTL,   KC_RGUI,    KC_RALT,  \
        KC_UP,     KC_DOWN,   KC_LEFT,   KC_RIGHT,   KC_ENTER \
    )
};