// Copyright 2023 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "qmk_settings.h"
#include "gesture.h"
#include "local_settings.h"
#include "secret_storage.h"

// Define local settings variables
uint16_t QS_mouse_speed_mult = 8; // Default 2.0x
uint16_t QS_gesture_threshold = 20;
bool QS_gesture_enabled = true;
uint16_t QS_gesture_timeout = 100;

user_config_t user_config;

// 設定をEEPROMに保存する関数（共通化）
void save_user_config(void) {
    user_config.magic = USER_CONFIG_MAGIC;
    user_config.mouse_speed_mult = (uint8_t)QS_mouse_speed_mult;
    user_config.gesture_threshold = (uint8_t)QS_gesture_threshold;
    user_config.gesture_enabled = QS_gesture_enabled;
    user_config.gesture_timeout = QS_gesture_timeout;
    user_config.os_mode = os_mode;
    user_config.scroll_layer_mask = scroll_layer_mask;
    user_config.precise_layer_mask = precise_layer_mask;
    user_config.fast_layer_mask = fast_layer_mask;
    user_config.gesture_move_layer_mask = gesture_move_layer_mask;
    user_config.gesture_static_layer_mask = gesture_static_layer_mask;
    
    // EECONFIG_USER_DATA 領域に書き込み
    // 戻り値は書き込んだバイト数だが無視
    eeconfig_update_user_datablock(&user_config, 0, sizeof(user_config));
}

// EEPROM初期化（リセット時）
void eeconfig_init_user(void) {
    // デフォルト値設定

    QS_mouse_speed_mult = 8;
    QS_gesture_threshold = 20;
    QS_gesture_enabled = true;
    QS_gesture_timeout = 100;
    os_mode = 0; // Windows
    scroll_layer_mask = (1UL << 3) | (1UL << 8);
    precise_layer_mask = 0;
    fast_layer_mask = 0;
    gesture_move_layer_mask = 0;
    gesture_static_layer_mask = 0;
    
    save_user_config();
    eeconfig_init_kb();
}



#ifdef MIDI_ENABLE
#include "quantum/midi/qmk_midi.h"
#include "quantum/midi/midi.h"
extern MidiDevice midi_device;
#endif
#include "hardware/gpio.h"
#include "hardware/structs/iobank0.h"
#include "hardware/structs/padsbank0.h"
#include "hardware/structs/sio.h"

// Unicode support
#ifdef UNICODE_ENABLE
#include "unicode.h"
// VialのAnyキーで直接Unicodeコードポイントを指定できます
// 例: 0x8000 + 0x2192 で → を入力
// 使い方: Anyキーに 0x8000 + コードポイント を入力
// 例: 0x8000 + 0x2192 (→), 0x8000 + 0x2190 (←), 0x8000 + 0x2665 (♥)

// Linux用Unicode入力のカスタマイズ（確定をEnterに変更）
void unicode_input_finish_linux(void) {
    register_code(KC_ENT);
    unregister_code(KC_ENT);
}

void unicode_input_cancel_linux(void) {
    register_code(KC_ESC);
    unregister_code(KC_ESC);
}
#endif

// OS検出サポート
#ifdef OS_DETECTION_ENABLE
#include "os_detection.h"
#endif

// スクロール変換(押しながらマウス移動をスクロールにする)
void scroll_start(uint16_t keycode);
void scroll_end(void);
bool scroll_is_active(void);

// レイヤーベースのスクロール・精密・高速設定
void scroll_layer_toggle(uint8_t layer);
bool scroll_layer_is_set(uint8_t layer);
void precise_layer_toggle(uint8_t layer);
bool precise_layer_is_set(uint8_t layer);
void fast_layer_toggle(uint8_t layer);
bool fast_layer_is_set(uint8_t layer);

// 押しながら精密・高速モード
void precise_mode_start(void);
void precise_mode_end(void);
void fast_mode_start(void);
void fast_mode_end(void);

// ジェスチャレイヤー設定
void gesture_move_layer_toggle(uint8_t layer);
bool gesture_move_layer_is_set(uint8_t layer);
void gesture_static_layer_toggle(uint8_t layer);
bool gesture_static_layer_is_set(uint8_t layer);

// カスタムキーコード
enum custom_keycodes {
    GESTURE_MOVE = QK_KB_0,  // カーソル移動しながらジェスチャ (MGST)
    GESTURE_STATIC,          // カーソル固定でジェスチャ (SGST)
    SCROLL_HOLD,             // 押しながらマウス移動をスクロールに変換
    SCROLL_LAYER_SET,        // 現在のレイヤーをスクロールレイヤーに設定/解除
    PRECISE_HOLD,            // 押しながら精密動作（0.25倍）
    PRECISE_LAYER_SET,       // 現在のレイヤーを精密レイヤーに設定/解除
    FAST_HOLD,               // 押しながら高速動作（2倍）
    FAST_LAYER_SET,          // 現在のレイヤーを高速レイヤーに設定/解除
    GESTURE_MOVE_LAYER_SET,  // 現在のレイヤーをMGSTレイヤーに設定/解除
    GESTURE_STATIC_LAYER_SET,// 現在のレイヤーをSGSTレイヤーに設定/解除
    SET_OS_WIN,              // OSモードをWinに手動設定
    SET_OS_MAC,              // OSモードをMacに手動設定
    SET_OS_LINUX,            // OSモードをLinuxに手動設定
    TP_SENS_UP,              // トラックポイント/ポインタ速度+ (QS_mouse_speed_mult++)
    TP_SENS_DOWN,            // トラックポイント/ポインタ速度- (QS_mouse_speed_mult--)
    GST_SENS_UP,             // ジェスチャしきい値+ (QS_gesture_threshold += step)
    GST_SENS_DOWN,           // ジェスチャしきい値- (QS_gesture_threshold -= step)
    GST_TOGGLE,              // ジェスチャON/OFF切替 (QS_gesture_enabled)
    GST_TIME_UP,             // SGST自動発火タイムアウト+50ms
    GST_TIME_DOWN,           // SGST自動発火タイムアウト-50ms
    SHOW_STATUS,             // 現在の設定をコンソールに出力
    RAPID_FIRE,              // 連打マクロ (左クリック連打)
};

// 設定値の範囲定義
#define MOUSE_SPEED_MIN 1      // 0.25x
#define MOUSE_SPEED_MAX 20     // 5.0x
#define MOUSE_SPEED_DEFAULT 8  // 2.0x
#define GESTURE_THRESHOLD_MIN 5
#define GESTURE_THRESHOLD_MAX 200
#define GESTURE_THRESHOLD_DEFAULT 20
#define GESTURE_THRESHOLD_STEP 5
#define GESTURE_TIMEOUT_MIN 100     // 最小100ms
#define GESTURE_TIMEOUT_MAX 2000    // 最大2秒
#define GESTURE_TIMEOUT_DEFAULT 100 // デフォルト100ms（SGSTの発火速度）
#define GESTURE_TIMEOUT_STEP 50     // 50msずつ調整

// Vialマクロ呼び出し用ヘルパ
// カスタムキーコード（QK_KB_0 = 0x5F00〜）と重複しないよう QK_USER_0（0x5F80〜）を使用
#define GVM(n) (QK_USER_0 + (n))

// ジェスチャマッピングテーブル
// ここに好きなパターンを追加できます
// Separate mappings for MGST (cursor-moving gestures) and SGST (static gestures)
const gesture_mapping_t gesture_mappings_mgst[] = {
    // MGST: カーソル動くジェスチャ（1ストローク/2ストローク/3ストローク）
    {{1, 0}, 1, GVM(0), "MGST Up: Vial Macro 0"},      // 上
    {{2, 0}, 1, GVM(1), "MGST Down: Vial Macro 1"},    // 下
    {{3, 0}, 1, GVM(2), "MGST Left: Vial Macro 2"},    // 左
    {{4, 0}, 1, GVM(3), "MGST Right: Vial Macro 3"},   // 右

    // 2ストローク（VialマクロID 4..15）
    {{1, 4, 0}, 2, GVM(4),  "MGST Up-Right: Vial Macro 4"},   // 上→右
    {{1, 3, 0}, 2, GVM(5),  "MGST Up-Left: Vial Macro 5"},    // 上→左
    {{1, 2, 0}, 2, GVM(6),  "MGST Up-Down: Vial Macro 6"},    // 上→下
    {{2, 4, 0}, 2, GVM(7),  "MGST Down-Right: Vial Macro 7"}, // 下→右
    {{2, 3, 0}, 2, GVM(8),  "MGST Down-Left: Vial Macro 8"},  // 下→左
    {{2, 1, 0}, 2, GVM(9),  "MGST Down-Up: Vial Macro 9"},    // 下→上
    {{3, 1, 0}, 2, GVM(10), "MGST Left-Up: Vial Macro 10"},    // 左→上
    {{3, 2, 0}, 2, GVM(11), "MGST Left-Down: Vial Macro 11"},  // 左→下
    {{3, 4, 0}, 2, GVM(12), "MGST Left-Right: Vial Macro 12"}, // 左→右
    {{4, 1, 0}, 2, GVM(13), "MGST Right-Up: Vial Macro 13"},    // 右→上
    {{4, 2, 0}, 2, GVM(14), "MGST Right-Down: Vial Macro 14"},  // 右→下
    {{4, 3, 0}, 2, GVM(15), "MGST Right-Left: Vial Macro 15"},  // 右→左

    // 3ストローク
    {{1, 4, 1, 0}, 3, GVM(16), "MGST Up-Right-Up: Vial Macro 16"},
    {{1, 4, 2, 0}, 3, GVM(17), "MGST Up-Right-Down: Vial Macro 17"},
    {{1, 4, 3, 0}, 3, GVM(18), "MGST Up-Right-Left: Vial Macro 18"},
    {{1, 3, 1, 0}, 3, GVM(19), "MGST Up-Left-Up: Vial Macro 19"},
    {{1, 3, 2, 0}, 3, GVM(20), "MGST Up-Left-Down: Vial Macro 20"},
    {{1, 3, 4, 0}, 3, GVM(21), "MGST Up-Left-Right: Vial Macro 21"},
    {{1, 2, 1, 0}, 3, GVM(22), "MGST Up-Down-Up: Vial Macro 22"},
    {{1, 2, 3, 0}, 3, GVM(23), "MGST Up-Down-Left: Vial Macro 23"},
    {{1, 2, 4, 0}, 3, GVM(24), "MGST Up-Down-Right: Vial Macro 24"},
    {{2, 4, 2, 0}, 3, GVM(25), "MGST Down-Right-Down: Vial Macro 25"},
    {{2, 4, 1, 0}, 3, GVM(26), "MGST Down-Right-Up: Vial Macro 26"},
    {{2, 4, 3, 0}, 3, GVM(27), "MGST Down-Right-Left: Vial Macro 27"},
    {{2, 3, 2, 0}, 3, GVM(28), "MGST Down-Left-Down: Vial Macro 28"},
    {{2, 3, 1, 0}, 3, GVM(29), "MGST Down-Left-Up: Vial Macro 29"},
    {{2, 3, 4, 0}, 3, GVM(30), "MGST Down-Left-Right: Vial Macro 30"},
    {{2, 1, 2, 0}, 3, GVM(31), "MGST Down-Up-Down: Vial Macro 31"},
    {{2, 1, 3, 0}, 3, GVM(32), "MGST Down-Up-Left: Vial Macro 32"},
    {{2, 1, 4, 0}, 3, GVM(33), "MGST Down-Up-Right: Vial Macro 33"},
    {{3, 1, 3, 0}, 3, GVM(34), "MGST Left-Up-Left: Vial Macro 34"},
    {{3, 1, 2, 0}, 3, GVM(35), "MGST Left-Up-Down: Vial Macro 35"},
    {{3, 1, 4, 0}, 3, GVM(36), "MGST Left-Up-Right: Vial Macro 36"},
    {{3, 2, 3, 0}, 3, GVM(37), "MGST Left-Down-Left: Vial Macro 37"},
    {{3, 2, 1, 0}, 3, GVM(38), "MGST Left-Down-Up: Vial Macro 38"},
    {{3, 2, 4, 0}, 3, GVM(39), "MGST Left-Down-Right: Vial Macro 39"},
    {{3, 4, 3, 0}, 3, GVM(40), "MGST Left-Right-Left: Vial Macro 40"},
    {{3, 4, 1, 0}, 3, GVM(41), "MGST Left-Right-Up: Vial Macro 41"},
    {{3, 4, 2, 0}, 3, GVM(42), "MGST Left-Right-Down: Vial Macro 42"},
    {{4, 1, 4, 0}, 3, GVM(43), "MGST Right-Up-Right: Vial Macro 43"},
    {{4, 1, 2, 0}, 3, GVM(44), "MGST Right-Up-Down: Vial Macro 44"},
    {{4, 1, 3, 0}, 3, GVM(45), "MGST Right-Up-Left: Vial Macro 45"},
    {{4, 2, 4, 0}, 3, GVM(46), "MGST Right-Down-Right: Vial Macro 46"},
    {{4, 2, 1, 0}, 3, GVM(47), "MGST Right-Down-Up: Vial Macro 47"},
    {{4, 2, 3, 0}, 3, GVM(48), "MGST Right-Down-Left: Vial Macro 48"},
    {{4, 3, 4, 0}, 3, GVM(49), "MGST Right-Left-Right: Vial Macro 49"},
    {{4, 3, 1, 0}, 3, GVM(50), "MGST Right-Left-Up: Vial Macro 50"},
    {{4, 3, 2, 0}, 3, GVM(51), "MGST Right-Left-Down: Vial Macro 51"},
    {{4, 3, 3, 0}, 3, GVM(52), "MGST Right-Left-Left: Vial Macro 52"},
};

const gesture_mapping_t gesture_mappings_sgst[] = {
    // SGST: カーソル動かないジェスチャ（1ストロークのみ）
    {{1, 0}, 1, GVM(53), "SGST Up: Vial Macro 53"},      // 上
    {{2, 0}, 1, GVM(54), "SGST Down: Vial Macro 54"},    // 下
    {{3, 0}, 1, GVM(55), "SGST Left: Vial Macro 55"},    // 左
    {{4, 0}, 1, GVM(56), "SGST Right: Vial Macro 56"},   // 右
};

const uint8_t GESTURE_MGST_COUNT = sizeof(gesture_mappings_mgst) / sizeof(gesture_mappings_mgst[0]);
const uint8_t GESTURE_SGST_COUNT = sizeof(gesture_mappings_sgst) / sizeof(gesture_mappings_sgst[0]);

const uint8_t GESTURE_MAPPING_COUNT = GESTURE_MGST_COUNT + GESTURE_SGST_COUNT;

// OSモード（グローバル）: 0=Win, 1=Mac, 2=Linux
// local_settings.h で extern 宣言されているため static を外す
uint8_t os_mode = 0;
// OS自動検出が有効かどうか（初回検出まではfalse）

static bool os_auto_detected = false;

// 連打マクロ（Rapid Fire）用変数
bool rapid_fire_enabled = false;
uint16_t rapid_fire_timer = 0;
#define RAPID_FIRE_INTERVAL 10 // 連打間隔 (ms)

// OS検出時のコールバック（QMKから自動呼び出し）
#ifdef OS_DETECTION_ENABLE
bool process_detected_host_os_user(os_variant_t detected_os) {
    os_auto_detected = true;
    
    // 検出結果に応じてos_modeとUnicode入力モードを自動設定
    switch (detected_os) {
        case OS_MACOS:
        case OS_IOS:
            os_mode = 1; // Mac
#ifdef UNICODE_ENABLE
            set_unicode_input_mode(UNICODE_MODE_MACOS);
            uprintf("OS detected: macOS/iOS -> Unicode mode MACOS\n");
#endif
            break;
        case OS_WINDOWS:
            os_mode = 0; // Windows
#ifdef UNICODE_ENABLE
            set_unicode_input_mode(UNICODE_MODE_WINDOWS);
            uprintf("OS detected: Windows -> Unicode mode WINDOWS\n");
#endif
            break;
        case OS_LINUX:
            os_mode = 2; // Linux
#ifdef UNICODE_ENABLE
            set_unicode_input_mode(UNICODE_MODE_LINUX);
            uprintf("OS detected: Linux -> Unicode mode LINUX\n");
#endif
            break;
        case OS_UNSURE:
            // 不明な場合はデフォルトのまま（下記の初期化で設定）
            uprintf("OS detected: UNSURE -> keeping default\n");
            break;
    }
    return true;
}
#endif

// キーボード初期化時にデフォルトのUnicodeモードを設定
void keyboard_post_init_user(void) {

#ifdef UNICODE_ENABLE
    // デフォルトはWindows（OS検出後に上書きされる）
    set_unicode_input_mode(UNICODE_MODE_WINDOWS);
    uprintf("Unicode mode initialized to WINDOWS (will auto-switch on OS detection)\n");
#endif

    // EEPROMから設定読み込み
    if (!eeconfig_is_enabled()) {
        eeconfig_init();
    }
    
    // ブロック読み込み (オフセット0から)
    eeconfig_read_user_datablock(&user_config, 0, sizeof(user_config));
    
    // マジックナンバーチェック（未初期化判定）

    if (user_config.magic != USER_CONFIG_MAGIC) {
        // デフォルト値で初期化
        eeconfig_init_user();
    } else {
        // 読み込んだ値をグローバル変数に反映
        QS_mouse_speed_mult = user_config.mouse_speed_mult;
        QS_gesture_threshold = user_config.gesture_threshold;
        QS_gesture_enabled = user_config.gesture_enabled;
        QS_gesture_timeout = user_config.gesture_timeout;
        
        os_mode = user_config.os_mode;
        scroll_layer_mask = user_config.scroll_layer_mask;
        precise_layer_mask = user_config.precise_layer_mask;
        fast_layer_mask = user_config.fast_layer_mask;
        gesture_move_layer_mask = user_config.gesture_move_layer_mask;
        gesture_static_layer_mask = user_config.gesture_static_layer_mask;
        
        // 安全装置（ゼロ対策）
        if (QS_mouse_speed_mult == 0) QS_mouse_speed_mult = 8;
        if (QS_gesture_threshold == 0) QS_gesture_threshold = 20;
    }

    uprintf("Loaded settings from EEPROM: Speed=%u, Thr=%u, En=%u, Time=%u, OS=%u\n",
        QS_mouse_speed_mult, QS_gesture_threshold, QS_gesture_enabled, QS_gesture_timeout, os_mode);
}



// Z-prefix入力システムはここから削除

// 現在アクティブなレイヤーのOSモードを取得
uint8_t get_active_layer_os_mode(void) {
    return os_mode;
}

// VILレイアウトを反映したkeymap
// Layer 0-3: US配列用, Layer 4-8: JIS配列用

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    // ============== US配列 ==============
    // Layer 0 (US Default)
    [0] = LAYOUT(
                KC_ESC,     LT(3, KC_Q), KC_L, KC_U, KC_COMMA, KC_DOT,                                         KC_F, KC_W, KC_R, KC_Y, KC_P, KC_SCLN,
                KC_TAB,     LT(2, KC_E), KC_I, KC_A, LT(1, KC_O), KC_MINUS,                                    KC_K, LT(1, KC_T), LT(3, KC_N), KC_S, LT(2, KC_H), KC_SLASH,
                           KC_LSFT, KC_Z, KC_X, KC_C, KC_V,                                                   KC_G, KC_D, KC_M, KC_J, KC_B,
                                     KC_LALT, KC_LCTL, LGUI_T(KC_LNG2), LSFT_T(KC_SPC),            RALT_T(KC_SPC), RCTL_T(KC_LNG1), KC_RGUI, KC_RSFT,
                                     TD(1),                                             KC_BTN2, TD(0), KC_BTN1, KC_BTN3, KC_WHOM),

    // Layer 1 — US symbols
    [1] = LAYOUT(
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                                          LSFT(KC_BSLS), KC_LBRC, LSFT(KC_QUOT), KC_RBRC, LSFT(KC_6), LSFT(KC_SCLN),
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                                          LSFT(KC_3), LSFT(KC_9), KC_QUOT, LSFT(KC_0), LSFT(KC_2), KC_BSLS,
                           KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                                        LSFT(KC_4), LSFT(KC_LBRC), KC_GRV, LSFT(KC_RBRC), LSFT(KC_GRV),
                                     KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                           KC_TRNS, LSFT(KC_7), KC_TRNS, KC_TRNS,
                                     KC_TRNS,                                           KC_WH_U, KC_WH_L, KC_WH_D, KC_WH_R, KC_TRNS),

    // Layer 2 — US numbers/navigation
    [2] = LAYOUT(
                KC_TRNS, KC_VOLU, KC_1, KC_2, KC_3, LSFT(KC_EQL),                                              KC_F2, KC_HOME, KC_UP, KC_END, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_MUTE, KC_4, KC_5, KC_6, KC_EQL,                                                    KC_BSPC, KC_LEFT, KC_DOWN, KC_RGHT, KC_DEL, KC_TRNS,
                           KC_VOLD, KC_7, KC_8, KC_9, LSFT(KC_8),                                              KC_F5, KC_F7, KC_F10, KC_TRNS, KC_TRNS,
                                     KC_TRNS, KC_TRNS, KC_0, KC_SLASH,                             KC_ENT, LCTL(KC_ENT), KC_TRNS, KC_TRNS,
                                     LSFT(KC_5),                                        KC_NO, LSFT(KC_1), KC_ENT, LSFT(KC_SLASH), KC_BSPC),

    // Layer 3 — US functions + layer switch
    [3] = LAYOUT(
                KC_TRNS, KC_TRNS, KC_F1, KC_F2, KC_F3, KC_TRNS,                                                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_TRNS, KC_F4, KC_F5, KC_F6, KC_TRNS,                                                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                           KC_TRNS, KC_F7, KC_F8, KC_F9, KC_TRNS,                                              KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                                     KC_F10, KC_F11, KC_F12, KC_TRNS,                              TO(4), TO(5), QK_MAGIC_TOGGLE_CTL_GUI, QK_BOOT,
                                     QK_BOOT,                                           RCTL(KC_WH_U), RCTL(KC_WH_L), RCTL(KC_WH_D), RCTL(KC_WH_R), KC_TRNS),

    // ============== JIS配列 (US) ==============
    // Layer 4 (JIS Default - US keyboard)
    [4] = LAYOUT(
                KC_ESC,     LT(8, KC_Q), KC_L, KC_U, KC_COMMA, KC_DOT,                                         KC_F, KC_W, KC_R, KC_Y, KC_P, KC_SCLN,
                KC_TAB,     LT(7, KC_E), KC_I, KC_A, LT(6, KC_O), KC_MINUS,                                    KC_K, LT(6, KC_T), LT(8, KC_N), KC_S, LT(7, KC_H), KC_SLASH,
                           KC_LSFT, KC_Z, KC_X, KC_C, KC_V,                                                   KC_G, KC_D, KC_M, KC_J, KC_B,
                                     KC_LALT, KC_LCTL, LGUI_T(KC_LNG2), LSFT_T(KC_SPC),            RALT_T(KC_SPC), RCTL_T(KC_LNG1), KC_RGUI, KC_RSFT,
                                     TD(1),                                             KC_BTN2, TD(0), KC_BTN1, KC_BTN3, KC_WHOM),

    // Layer 5 (JIS Default - Japanese keyboard with MHEN/HENK)
    [5] = LAYOUT(
                KC_ESC,     LT(8, KC_Q), KC_L, KC_U, KC_COMMA, KC_DOT,                                         KC_F, KC_W, KC_R, KC_Y, KC_P, KC_SCLN,
                KC_TAB,     LT(7, KC_E), KC_I, KC_A, LT(6, KC_O), KC_MINUS,                                    KC_K, LT(6, KC_T), LT(8, KC_N), KC_S, LT(7, KC_H), KC_SLASH,
                           KC_LSFT, KC_Z, KC_X, KC_C, KC_V,                                                   KC_G, KC_D, KC_M, KC_J, KC_B,
                                     KC_LALT, KC_LCTL, LGUI_T(KC_INT5), LSFT_T(KC_SPC),            RALT_T(KC_SPC), RCTL_T(KC_INT4), KC_RGUI, KC_RSFT,
                                     TD(1),                                             KC_BTN2, TD(0), KC_BTN1, KC_BTN3, KC_WHOM),

    // Layer 6 — JIS symbols
    [6] = LAYOUT(
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                                          LSFT(KC_INT3), KC_RBRC, LSFT(KC_2), KC_NUHS, KC_EQL, KC_QUOT,
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                                          LSFT(KC_3), LSFT(KC_8), LSFT(KC_7), LSFT(KC_9), KC_LBRC, KC_INT1,
                           KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                                        LSFT(KC_4), LSFT(KC_RBRC), LSFT(KC_LBRC), LSFT(KC_NUHS), LSFT(KC_EQL),
                                     KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                           KC_TRNS, LSFT(KC_6), KC_TRNS, KC_TRNS,
                                     KC_TRNS,                                           KC_WH_U, KC_WH_L, KC_WH_D, KC_WH_R, KC_TRNS),

    // Layer 7 — JIS numbers/navigation
    [7] = LAYOUT(
                KC_TRNS, KC_VOLU, KC_1, KC_2, KC_3, LSFT(KC_SCLN),                                             KC_F2, KC_HOME, KC_UP, KC_END, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_MUTE, KC_4, KC_5, KC_6, LSFT(KC_MINS),                                             KC_BSPC, KC_LEFT, KC_DOWN, KC_RGHT, KC_DEL, KC_TRNS,
                           KC_VOLD, KC_7, KC_8, KC_9, LSFT(KC_QUOT),                                           KC_F5, KC_F7, KC_F10, KC_TRNS, KC_TRNS,
                                     KC_TRNS, KC_TRNS, KC_0, KC_SLASH,                             KC_ENT, LCTL(KC_ENT), KC_TRNS, KC_TRNS,
                                     LSFT(KC_5),                                        KC_TRNS, LSFT(KC_1), KC_ENT, KC_BSPC, KC_BSPC),

    // Layer 8 — JIS functions + layer switch
    [8] = LAYOUT(
                KC_TRNS, KC_TRNS, KC_F1, KC_F2, KC_F3, KC_TRNS,                                                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_TRNS, KC_F4, KC_F5, KC_F6, KC_TRNS,                                                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                           KC_TRNS, KC_F7, KC_F8, KC_F9, KC_TRNS,                                              KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                                     KC_F10, KC_F11, KC_F12, KC_TRNS,                              TO(0), TO(0), QK_MAGIC_TOGGLE_CTL_GUI, QK_BOOT,
                                     QK_BOOT,                                           RCTL(KC_WH_U), RCTL(KC_WH_L), RCTL(KC_WH_D), RCTL(KC_WH_R), KC_TRNS)
};

// 注意: keycode_config() と mod_config() をオーバーライドすると
// Magic キーコード（MAGIC_TOGGLE_CTL_GUI等）によるキースワップが無効化されます。
// Vial互換性が必要な場合は別の方法を検討してください。
// ここではデフォルトの quantum/keycode_config.c を使用します。

// キー処理
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // ジェスチャ中にESCが押されたらキャンセル
    if (keycode == KC_ESC && record->event.pressed && gesture_is_active()) {
        gesture_cancel();
        return false;  // ESCは処理済み（通常のESCを送らない）
    }

    // 秘密ストレージ操作中は入力をブロック (ESCで緊急解除)
    if (!secret_storage_process_record_user(keycode, record)) {
        return false;
    }
    
    switch (keycode) {
        case GESTURE_MOVE:
            if (record->event.pressed) {
                gesture_start(keycode, true);  // カーソル移動あり
            } else {
                gesture_end();
            }
            return false;
        case GESTURE_STATIC:
            if (record->event.pressed) {
                gesture_start(keycode, false); // カーソル固定
            } else {
                gesture_end();
            }
            return false;
        case GESTURE_MOVE_LAYER_SET:
            if (record->event.pressed) {
                uint8_t layer = biton32(layer_state);
                gesture_move_layer_toggle(layer);
            }
            return false;
        case GESTURE_STATIC_LAYER_SET:
            if (record->event.pressed) {
                uint8_t layer = biton32(layer_state);
                gesture_static_layer_toggle(layer);
            }
            return false;
        case SCROLL_HOLD:
            if (record->event.pressed) {
                scroll_start(keycode);
            } else {
                scroll_end();
            }
            return false;
        case SCROLL_LAYER_SET:
            if (record->event.pressed) {
                uint8_t layer = biton32(layer_state);
                scroll_layer_toggle(layer);
            }
            return false;
        case PRECISE_HOLD:
            if (record->event.pressed) {
                precise_mode_start();
            } else {
                precise_mode_end();
            }
            return false;
        case PRECISE_LAYER_SET:
            if (record->event.pressed) {
                uint8_t layer = biton32(layer_state);
                precise_layer_toggle(layer);
            }
            return false;
        case FAST_HOLD:
            if (record->event.pressed) {
                fast_mode_start();
            } else {
                fast_mode_end();
            }
            return false;
        case FAST_LAYER_SET:
            if (record->event.pressed) {
                uint8_t layer = biton32(layer_state);
                fast_layer_toggle(layer);
            }
            return false;
        case TP_SENS_UP: {
            if (record->event.pressed) {
                uint16_t mult = QS_mouse_speed_mult;
                if (mult < MOUSE_SPEED_MAX) {
                    mult++;
                    QS_mouse_speed_mult = mult;
                    save_user_config();
                    uprintf("Pointer speed: x%u.%u (range: x0.25-x5.00)\n", mult/4, (mult%4)*25);


                } else {
                    uprintf("Pointer speed: x5.00 (MAX)\n");
                }
            }
            return false;
        }
        case TP_SENS_DOWN: {
            if (record->event.pressed) {
                uint16_t mult = QS_mouse_speed_mult;
                if (mult > MOUSE_SPEED_MIN) {
                    mult--;
                    QS_mouse_speed_mult = mult;
                    save_user_config();
                    uprintf("Pointer speed: x%u.%u (range: x0.25-x5.00)\n", mult/4, (mult%4)*25);


                } else {
                    uprintf("Pointer speed: x0.25 (MIN)\n");
                }
            }
            return false;
        }
        case GST_SENS_UP: {
            if (record->event.pressed) {
                uint16_t thr = QS_gesture_threshold;
                if (thr < GESTURE_THRESHOLD_MAX) {
                    thr += GESTURE_THRESHOLD_STEP;
                    if (thr > GESTURE_THRESHOLD_MAX) thr = GESTURE_THRESHOLD_MAX;
                    QS_gesture_threshold = thr;
                    save_user_config();
                    uprintf("Gesture threshold: %u (range: %u-%u)\n", thr, GESTURE_THRESHOLD_MIN, GESTURE_THRESHOLD_MAX);


                } else {
                    uprintf("Gesture threshold: %u (MAX)\n", thr);
                }
            }
            return false;
        }
        case GST_SENS_DOWN: {
            if (record->event.pressed) {
                uint16_t thr = QS_gesture_threshold;
                if (thr > GESTURE_THRESHOLD_MIN) {
                    thr -= GESTURE_THRESHOLD_STEP;
                    if (thr < GESTURE_THRESHOLD_MIN) thr = GESTURE_THRESHOLD_MIN;
                    QS_gesture_threshold = thr;
                    save_user_config();
                    uprintf("Gesture threshold: %u (range: %u-%u)\n", thr, GESTURE_THRESHOLD_MIN, GESTURE_THRESHOLD_MAX);


                } else {
                    uprintf("Gesture threshold: %u (MIN)\n", thr);
                }
            }
            return false;
        }
        case GST_TOGGLE: {
            if (record->event.pressed) {
                QS_gesture_enabled = !QS_gesture_enabled;
                uint8_t en = QS_gesture_enabled;
                save_user_config();
                uprintf("Gesture %s\n", en ? "ON" : "OFF");


            }
            return false;
        }
        case GST_TIME_UP: {
            if (record->event.pressed) {
                uint16_t timeout = QS_gesture_timeout;
                if (timeout < GESTURE_TIMEOUT_MAX) {
                    timeout += GESTURE_TIMEOUT_STEP;
                    if (timeout > GESTURE_TIMEOUT_MAX) timeout = GESTURE_TIMEOUT_MAX;
                    QS_gesture_timeout = timeout;
                    save_user_config();
                    uprintf("SGST auto-fire timeout: %ums (range: %u-%u)\n", timeout, GESTURE_TIMEOUT_MIN, GESTURE_TIMEOUT_MAX);


                } else {
                    uprintf("SGST auto-fire timeout: %ums (MAX)\n", timeout);
                }
            }
            return false;
        }
        case GST_TIME_DOWN: {
            if (record->event.pressed) {
                uint16_t timeout = QS_gesture_timeout;
                if (timeout > GESTURE_TIMEOUT_MIN) {
                    timeout -= GESTURE_TIMEOUT_STEP;
                    if (timeout < GESTURE_TIMEOUT_MIN) timeout = GESTURE_TIMEOUT_MIN;
                    QS_gesture_timeout = timeout;

                    save_user_config();


                    uprintf("SGST auto-fire timeout: %ums (range: %u-%u)\n", timeout, GESTURE_TIMEOUT_MIN, GESTURE_TIMEOUT_MAX);

                } else {
                    uprintf("SGST auto-fire timeout: %ums (MIN)\n", timeout);
                }
            }
            return false;
        }
        case SHOW_STATUS: {
            if (record->event.pressed) {
                uprintf("\n=== Keyboard Settings Status ===\n");
                // ポインタ速度
                uprintf("Pointer Speed: x%u.%u (range: x0.25-x5.00, default: x%u.%u)\n", 
                    QS_mouse_speed_mult/4, (QS_mouse_speed_mult%4)*25,
                    MOUSE_SPEED_DEFAULT/4, (MOUSE_SPEED_DEFAULT%4)*25);
                // ジェスチャ設定
                uprintf("Gesture Enabled: %s\n", QS_gesture_enabled ? "ON" : "OFF");
                uprintf("Gesture Threshold: %u (range: %u-%u, default: %u)\n", 
                    QS_gesture_threshold, GESTURE_THRESHOLD_MIN, GESTURE_THRESHOLD_MAX, GESTURE_THRESHOLD_DEFAULT);
                uprintf("SGST Auto-fire Timeout: %ums (range: %u-%u, default: %u)\n",
                    QS_gesture_timeout, GESTURE_TIMEOUT_MIN, GESTURE_TIMEOUT_MAX, GESTURE_TIMEOUT_DEFAULT);
                // OS検出
#ifdef OS_DETECTION_ENABLE
                os_variant_t detected = detected_host_os();
                const char* os_names[] = {"UNSURE", "Linux", "Windows", "macOS", "iOS"};
                uprintf("Detected OS: %s\n", os_names[detected]);
                uprintf("Auto-detected: %s\n", os_auto_detected ? "YES" : "NO (manual)");
#endif
                const char* os_mode_names[] = {"Windows", "macOS", "Linux"};
                uprintf("Current OS Mode: %s\n", os_mode_names[os_mode]);
                // Unicode入力モード
#ifdef UNICODE_ENABLE
                uint8_t umode = get_unicode_input_mode();
                const char* unicode_names[] = {"MACOS", "LINUX", "WINCOMPOSE", "EMACS", "BSD"};
                if (umode < 5) {
                    uprintf("Unicode Input Mode: %s\n", unicode_names[umode]);
                } else {
                    uprintf("Unicode Input Mode: %u\n", umode);
                }
#endif
                // レイヤー設定
                uprintf("Layer settings:\n");
                uprintf("  Scroll layers: ");
                bool first = true;
                for (uint8_t i = 0; i < 16; i++) {  // 0-15を表示
                    if (scroll_layer_is_set(i)) {
                        if (!first) uprintf(", ");
                        uprintf("%u", i);
                        first = false;
                    }
                }
                if (first) uprintf("(none)");
                uprintf("\n");
                
                uprintf("  Precise layers: ");
                first = true;
                for (uint8_t i = 0; i < 16; i++) {
                    if (precise_layer_is_set(i)) {
                        if (!first) uprintf(", ");
                        uprintf("%u", i);
                        first = false;
                    }
                }
                if (first) uprintf("(none)");
                uprintf("\n");
                
                uprintf("  Fast layers: ");
                first = true;
                for (uint8_t i = 0; i < 16; i++) {
                    if (fast_layer_is_set(i)) {
                        if (!first) uprintf(", ");
                        uprintf("%u", i);
                        first = false;
                    }
                }
                if (first) uprintf("(none)");
                uprintf("\n");
                
                uprintf("  Gesture-Move (MGST) layers: ");
                first = true;
                for (uint8_t i = 0; i < 16; i++) {
                    if (gesture_move_layer_is_set(i)) {
                        if (!first) uprintf(", ");
                        uprintf("%u", i);
                        first = false;
                    }
                }
                if (first) uprintf("(none)");
                uprintf("\n");
                
                uprintf("  Gesture-Static (SGST) layers: ");
                first = true;
                for (uint8_t i = 0; i < 16; i++) {
                    if (gesture_static_layer_is_set(i)) {
                        if (!first) uprintf(", ");
                        uprintf("%u", i);
                        first = false;
                    }
                }
                if (first) uprintf("(none)");
                uprintf("\n");
                
                uprintf("================================\n\n");
            }
            return false;
        }
        case SET_OS_WIN:
        case SET_OS_MAC:
        case SET_OS_LINUX: {
            if (record->event.pressed) {
                uint8_t mode = (keycode == SET_OS_WIN) ? 0 : (keycode == SET_OS_MAC) ? 1 : 2;
                os_mode = mode;
                // 手動設定した場合は自動検出フラグをクリア
                os_auto_detected = false;
                
                save_user_config();
                
                uprintf("OS mode (manual) -> %s\n", (mode == 0) ? "Win" : (mode == 1) ? "Mac" : "Linux");

#ifdef UNICODE_ENABLE
                // Unicode入力モードも手動で切り替え
                if (mode == 0) set_unicode_input_mode(UNICODE_MODE_WINDOWS);
                else if (mode == 1) set_unicode_input_mode(UNICODE_MODE_MACOS);
                else set_unicode_input_mode(UNICODE_MODE_LINUX);
                uprintf("Unicode mode switched manually\n");
#endif
            }
            return false;
        }
        case RAPID_FIRE:
            if (record->event.pressed) {
                rapid_fire_enabled = !rapid_fire_enabled;
                if (!rapid_fire_enabled) {
                    // 解除したときにマウスボタンも離す
                    unregister_code(KC_BTN1);
                }
                uprintf("Rapid Fire %s\n", rapid_fire_enabled ? "ON" : "OFF");
            }
            return false;
    }
    return true;
}

// matrix_scan_user は ps2_split.c に定義（連打ロジックも含む）
