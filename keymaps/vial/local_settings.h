#pragma once
#include <stdint.h>
#include <stdbool.h>

// 既存の設定
extern uint16_t QS_mouse_speed_mult;
extern uint16_t QS_gesture_threshold;
extern bool QS_gesture_enabled;
extern uint16_t QS_gesture_timeout;

// 追加の設定（各ファイルからstaticを外して共有）
extern uint8_t os_mode;
extern uint32_t scroll_layer_mask;
extern uint32_t precise_layer_mask;
extern uint32_t fast_layer_mask;
extern uint32_t gesture_move_layer_mask;
extern uint32_t gesture_static_layer_mask;

// EEPROM保存用の構成
// ブロックデータとして扱うため、共用体ではなく構造体にする（パディングは許容、またはpacked属性をつけるが、今回は余裕あるのでそのままでOK）
typedef struct {
    uint8_t mouse_speed_mult;
    uint8_t gesture_threshold;
    bool    gesture_enabled;
    uint16_t gesture_timeout;
    uint8_t os_mode;
    uint8_t padding[3]; // アライメント調整用（任意）
    uint32_t scroll_layer_mask;
    uint32_t precise_layer_mask;
    uint32_t fast_layer_mask;
    uint32_t gesture_move_layer_mask;
    uint32_t gesture_static_layer_mask;
    // チェックサムやバージョン管理用にマジックナンバーを入れると良いが、今回は簡易的にゼロチェックで判定
    uint32_t magic; 
} user_config_t;

#define USER_CONFIG_MAGIC 0x504F5449 // "POTI"

extern user_config_t user_config;

// 設定保存用のヘルパー関数
void save_user_config(void);
