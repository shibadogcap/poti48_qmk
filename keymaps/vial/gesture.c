// ジェスチャ処理モジュール
// マウス移動をストロークとして記録し、パターン登録したマクロを発火する

#include QMK_KEYBOARD_H
#include "qmk_settings.h"
#include "dynamic_keymap.h"
#include "gesture.h"
#include "local_settings.h"


// ジェスチャ状態
static gesture_state_t gesture_state = {0};

// ジェスチャをリセット
static void gesture_reset(void) {
    gesture_state.active = false;
    gesture_state.firing = false;
    gesture_state.stroke_count = 0;
    gesture_state.accumulated_x = 0;
    gesture_state.accumulated_y = 0;
    gesture_state.last_direction = GESTURE_NONE;
}

// ジェスチャをキャンセル（発火せずにクリア）
void gesture_cancel(void) {
    if (gesture_state.active) {
        uprintf("Gesture cancelled (had %u strokes)\n", gesture_state.stroke_count);
    }
    gesture_reset();
}

// ジェスチャを開始
void gesture_start(uint16_t keycode, bool cursor_move) {
    if (!QS_gesture_enabled) {
        return;
    }
    if (!gesture_state.active) {
        gesture_state.active = true;
        gesture_state.cursor_move = cursor_move;
        gesture_state.trigger_keycode = keycode;
        gesture_state.stroke_count = 0;
        gesture_state.accumulated_x = 0;
        gesture_state.accumulated_y = 0;
        gesture_state.last_direction = GESTURE_NONE;
        gesture_state.last_stroke_time = timer_read32();
    }
}

// 外部のジェスチャマッピングテーブルを参照
extern const gesture_mapping_t gesture_mappings_mgst[];
extern const uint8_t GESTURE_MGST_COUNT;
extern const gesture_mapping_t gesture_mappings_sgst[];
extern const uint8_t GESTURE_SGST_COUNT;

// ジェスチャパターンをマッピングテーブルから検索
static uint16_t find_gesture_keycode(gesture_direction_t *strokes, uint8_t count) {
    // パターンを配列に変換
    uint8_t pattern[MAX_GESTURE_STROKES] = {0};
    for (uint8_t i = 0; i < count && i < MAX_GESTURE_STROKES; i++) {
        pattern[i] = (uint8_t)strokes[i];
    }

    // どちらのテーブルを検索するかは cursor_move フラグで決める
    const gesture_mapping_t *mappings = gesture_state.cursor_move ? gesture_mappings_mgst : gesture_mappings_sgst;
    uint8_t mapping_count = gesture_state.cursor_move ? GESTURE_MGST_COUNT : GESTURE_SGST_COUNT;

    uprintf("  Searching %s table (%u entries) for pattern: ", 
        gesture_state.cursor_move ? "MGST" : "SGST", mapping_count);
    for (uint8_t i = 0; i < count; i++) {
        uprintf("%u ", pattern[i]);
    }
    uprintf("\n");

    for (uint8_t i = 0; i < mapping_count; i++) {
        if (mappings[i].length == count) {
            bool match = true;
            for (uint8_t j = 0; j < count; j++) {
                if (mappings[i].pattern[j] != pattern[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                uprintf("  Match found at index %u: keycode=0x%04X\n", i, mappings[i].keycode);
                return mappings[i].keycode;
            }
        }
    }

    return KC_NO;
}

// ジェスチャを発火（ストロークがあれば実行し、状態をリセットして次のジェスチャ入力待ちにする）
void gesture_fire(void) {
    if (gesture_state.stroke_count > 0) {
        // デバッグ: 発火時のストローク数と経過時間
        uprintf("Gesture FIRE: %u strokes, elapsed=%lums, cursor_move=%s\n", 
            gesture_state.stroke_count,
            (unsigned long)timer_elapsed32(gesture_state.last_stroke_time),
            gesture_state.cursor_move ? "YES" : "NO");
        
        // 発火中フラグをセット（pointing_device_task_userでカーソル抑制）
        gesture_state.firing = true;
        
        // マッピングテーブルから対応するキーコードを検索
        uint16_t keycode = find_gesture_keycode(gesture_state.strokes, gesture_state.stroke_count);
        
        uprintf("  Found keycode: 0x%04X\n", keycode);
        
        // キーコードが設定されていれば実行
        if (keycode != KC_NO) {
            // 0x5F00-0x5F7F の範囲をVialマクロIDとして扱い、直接送出
            if (keycode >= 0x5F00 && keycode <= 0x5F7F) {
                uint8_t macro_id = (uint8_t)(keycode - 0x5F00);
                uprintf("  Executing Vial macro ID: %u\n", macro_id);
                dynamic_keymap_macro_send(macro_id);
            } else {
                uprintf("  Executing keycode: 0x%04X\n", keycode);
                register_code16(keycode);
                wait_ms(10);
                unregister_code16(keycode);
            }
        } else {
            uprintf("  No mapping found for gesture pattern\n");
        }
        // ストロークをリセットして次のジェスチャ入力待ちにする（activeはそのまま）
        gesture_state.stroke_count = 0;
        gesture_state.accumulated_x = 0;
        gesture_state.accumulated_y = 0;
        gesture_state.last_direction = GESTURE_NONE;
        gesture_state.last_stroke_time = timer_read32();
        
        // 発火完了
        gesture_state.firing = false;
    }
}

// ジェスチャを終了して対応するキーを発火
void gesture_end(void) {
    if (!QS_gesture_enabled) {
        gesture_reset();
        return;
    }
    if (gesture_state.active) {
        gesture_fire();  // 残っているジェスチャがあれば発火
    }
    gesture_reset();
}

// 外部から参照できるようにジェスチャ状態を公開
bool gesture_is_active(void) {
    return gesture_state.active;
}

// ジェスチャ記録処理（pointing_device_task_userから呼ばれる）
void gesture_record_movement(int8_t x, int8_t y) {
    if (!gesture_state.active || !QS_gesture_enabled) {
        return;
    }
    
    // SGSTの場合、モーション検出時刻を更新（カーソル停止検出用）
    if (!gesture_state.cursor_move && (x != 0 || y != 0)) {
        gesture_state.last_stroke_time = timer_read32();
    }
    
    if (x != 0 || y != 0) {
        gesture_state.accumulated_x += x;
        gesture_state.accumulated_y += y;
        
        // ドミナント方向の判定
        int16_t abs_x = gesture_state.accumulated_x > 0 ? gesture_state.accumulated_x : -gesture_state.accumulated_x;
        int16_t abs_y = gesture_state.accumulated_y > 0 ? gesture_state.accumulated_y : -gesture_state.accumulated_y;
        
        gesture_direction_t new_direction = GESTURE_NONE;
        
        // どちらかの軸がしきい値を超えたら方向を決定
        uint16_t threshold = QS_gesture_threshold;
        if (abs_x >= threshold || abs_y >= threshold) {
            // ドミナント判定にマージンを設けて斜め判定を抑制
            uint16_t margin = threshold / 2; // 調整可能
            if (abs_x > abs_y + margin && abs_x >= threshold) {
                new_direction = gesture_state.accumulated_x > 0 ? GESTURE_RIGHT : GESTURE_LEFT;
            } else if (abs_y > abs_x + margin && abs_y >= threshold) {
                new_direction = gesture_state.accumulated_y > 0 ? GESTURE_DOWN : GESTURE_UP;
            } else {
                new_direction = GESTURE_NONE; // 十分にドミナントでない -> 無視
            }
            
            // 新しい方向が前回と異なり、まだ記録可能な場合
            if (new_direction != GESTURE_NONE && 
                new_direction != gesture_state.last_direction && 
                gesture_state.stroke_count < MAX_GESTURE_STROKES) {
                gesture_state.strokes[gesture_state.stroke_count++] = new_direction;
                gesture_state.last_direction = new_direction;
                gesture_state.accumulated_x = 0;
                gesture_state.accumulated_y = 0;
                // ストローク更新時刻を記録（タイムアウト自動発火用）
                gesture_state.last_stroke_time = timer_read32();
                // デバッグ: ストローク記録
                const char* dir_names[] = {"NONE", "UP", "DOWN", "LEFT", "RIGHT"};
                uprintf("Gesture stroke %u: %s\n", gesture_state.stroke_count, dir_names[new_direction]);
            }
        }
    }
}

// ジェスチャのタイムアウトチェック（matrix_scan_userから呼ばれる）
// SGST（カーソル動かないジェスチャ）はカーソル停止時に自動発火
void gesture_check_timeout(void) {
    if (gesture_state.active && gesture_state.stroke_count > 0) {
        // カーソル動かないジェスチャ（SGST）：カーソルが止まったら自動発火
        if (!gesture_state.cursor_move) {
            uint16_t timeout = QS_gesture_timeout;
            uint32_t elapsed = timer_elapsed32(gesture_state.last_stroke_time);
            if (timeout > 0 && elapsed > timeout) {
                uprintf("SGST auto-fire triggered: elapsed=%lums, timeout=%ums\n", 
                    (unsigned long)elapsed, timeout);
                gesture_fire();
            }
        }
    }
}

// カーソル抑制が必要かを返す
bool gesture_should_suppress_cursor(void) {
    return gesture_state.firing || (gesture_state.active && !gesture_state.cursor_move);
}
