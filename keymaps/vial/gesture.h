// ジェスチャ処理のヘッダ
#pragma once

#include <stdint.h>
#include <stdbool.h>

// ジェスチャ設定
#define MAX_GESTURE_STROKES 8

// ジェスチャ方向
typedef enum {
    GESTURE_NONE = 0,
    GESTURE_UP,
    GESTURE_DOWN,
    GESTURE_LEFT,
    GESTURE_RIGHT
} gesture_direction_t;

// ジェスチャ状態
typedef struct {
    bool active;                                    // ジェスチャ記録中か
    bool cursor_move;                               // ジェスチャ中にカーソルを動かすか
    bool firing;                                    // ジェスチャ発火中（この間はカーソル抑制）
    uint16_t trigger_keycode;                       // トリガーキーコード
    gesture_direction_t strokes[MAX_GESTURE_STROKES]; // ストローク記録
    uint8_t stroke_count;                           // 現在のストローク数
    int16_t accumulated_x;                          // 現在のストロークの累積X
    int16_t accumulated_y;                          // 現在のストロークの累積Y
    gesture_direction_t last_direction;             // 最後に記録した方向
    uint32_t last_stroke_time;                      // 最後にストロークが更新された時刻
} gesture_state_t;

// ジェスチャマッピング
typedef struct {
    uint8_t pattern[MAX_GESTURE_STROKES];
    uint8_t length;
    uint16_t keycode;
    const char* description;
} gesture_mapping_t;

// 公開API
void gesture_start(uint16_t keycode, bool cursor_move);
void gesture_end(void);
void gesture_fire(void);
void gesture_cancel(void);
bool gesture_is_active(void);
void gesture_record_movement(int8_t x, int8_t y);
void gesture_check_timeout(void);
bool gesture_should_suppress_cursor(void);
