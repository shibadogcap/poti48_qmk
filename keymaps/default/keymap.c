// Copyright 2023 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

// ↑　←　↓　→　center

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(
                KC_ESCAPE,  KC_Q, KC_L, KC_U, KC_COMMA, KC_DOT,                                                 KC_F,KC_W, KC_R, KC_Y, KC_SLASH, KC_SEMICOLON,
                KC_TAB,     KC_E, KC_I, KC_A, KC_O, KC_MINUS,                                                 KC_K, KC_T, KC_N, KC_S, KC_H, KC_P,
                            KC_LSFT, KC_Z, KC_X, KC_C, KC_V,                                                 KC_G, KC_D, KC_M, KC_J, KC_B,
                                     KC_LALT, KC_LGUI, KC_LCTL, KC_ENT, KC_SPC,                            KC_BSPC, KC_RCTL, KC_RGUI, KC_RALT,
                                     KC_NO,                                               KC_UP, KC_LEFT, KC_DOWN, KC_RIGHT, KC_ENT)
};