# マウスジェスチャ機能

QMK Pointing Device の移動量からストロークを認識し、キーコードや Vial マクロを発火するジェスチャエンジンです。

## 使い方

MGST (Move Gesture) または SGST (Static Gesture) キーを押しながらポインティングデバイスを操作します。
認識されたストロークパターンに応じて、割り当てられたキーコードまたは Vial マクロが発火します。

### モード一覧

| モード | 動作 | 切り替え方法 |
|--------|------|-------------|
| **Move** | 通常のカーソル移動 | デフォルト |
| **MGST** | Move Gesture — カーソル移動しながらジェスチャ認識 | MGST キー Hold / レイヤー設定 |
| **SGST** | Static Gesture — カーソル固定でジェスチャ認識 | SGST キー Hold / レイヤー設定 |
| **Scroll** | マウス移動 → スクロール変換 | SCRL Hold / レイヤー設定 |
| **Precise** | カーソル感度 1/4 | PREC Hold / レイヤー設定 |
| **Fast** | カーソル感度 2倍 | FAST Hold / レイヤー設定 |

### カスタムキーコード

Vial から設定可能なキーコード一覧は [keymap.c](../keymaps/vial/keymap.c) の
`enum custom_keycodes` を参照してください。

---

## ジェスチャと Vial マクロ番号の対応

ジェスチャは `keymap.c` の `gesture_mappings_mgst` / `gesture_mappings_sgst` で
Vial マクロ番号に割り当てています。

### MGST — Move Gesture

MGST はカーソルを動かしながらジェスチャを記録します。
1〜3ストロークのパターンを Vial Macro 0〜52 に割り当てています。

| Vial Macro | ストローク |
|------------|------------|
| 0 | ↑ |
| 1 | ↓ |
| 2 | ← |
| 3 | → |
| 4 | ↑ → |
| 5 | ↑ ← |
| 6 | ↑ ↓ |
| 7 | ↓ → |
| 8 | ↓ ← |
| 9 | ↓ ↑ |
| 10 | ← ↑ |
| 11 | ← ↓ |
| 12 | ← → |
| 13 | → ↑ |
| 14 | → ↓ |
| 15 | → ← |
| 16 | ↑ → ↑ |
| 17 | ↑ → ↓ |
| 18 | ↑ → ← |
| 19 | ↑ ← ↑ |
| 20 | ↑ ← ↓ |
| 21 | ↑ ← → |
| 22 | ↑ ↓ ↑ |
| 23 | ↑ ↓ ← |
| 24 | ↑ ↓ → |
| 25 | ↓ → ↓ |
| 26 | ↓ → ↑ |
| 27 | ↓ → ← |
| 28 | ↓ ← ↓ |
| 29 | ↓ ← ↑ |
| 30 | ↓ ← → |
| 31 | ↓ ↑ ↓ |
| 32 | ↓ ↑ ← |
| 33 | ↓ ↑ → |
| 34 | ← ↑ ← |
| 35 | ← ↑ ↓ |
| 36 | ← ↑ → |
| 37 | ← ↓ ← |
| 38 | ← ↓ ↑ |
| 39 | ← ↓ → |
| 40 | ← → ← |
| 41 | ← → ↑ |
| 42 | ← → ↓ |
| 43 | → ↑ → |
| 44 | → ↑ ↓ |
| 45 | → ↑ ← |
| 46 | → ↓ → |
| 47 | → ↓ ↑ |
| 48 | → ↓ ← |
| 49 | → ← → |
| 50 | → ← ↑ |
| 51 | → ← ↓ |
| 52 | → ← ← |

### SGST — Static Gesture

SGST はカーソルを固定してジェスチャを記録します。
現在は 1 ストロークのみを Vial Macro 53〜56 に割り当てています。

| Vial Macro | ストローク |
|------------|------------|
| 53 | ↑ |
| 54 | ↓ |
| 55 | ← |
| 56 | → |

---

## ファイル構成

```
keymaps/vial/
├── gesture.c          # ジェスチャ認識エンジン本体
├── gesture.h          # ジェスチャ API
├── modes.c            # モード制御 (scroll/precise/fast + gesture統合)
├── local_settings.h   # EEPROM保存用設定構造体
├── keymap.c           # カスタムキーコード処理
└── rules.mk
```

## 実装アーキテクチャ

```
Pointing Device 移動量 (x, y)
      │
      ├─→ pointing_device_task_user()  [modes.c]
      │       │
      │       ├→ gesture_record_movement()  ── ストローク検出
      │       ├→ 速度倍率 (precise/fast/QS)
      │       └→ スクロール変換 (hold/layer)
      │
      ├─→ matrix_scan_user()  [modes.c]
      │       │
      │       ├→ レイヤー変更検出 → gesture_start/end
      │       ├→ gesture_check_timeout() ── SGST自動発火
      │       └→ Rapid Fire
      │
      └─→ process_record_user()  [keymap.c]
              │
              └→ gesture_start() / gesture_end()
                    → gesture_fire()
                        → パターンマッチ → キーコード発火
```

## 移植ガイド

ジェスチャエンジンは PS/2 ではなく QMK Pointing Device API に依存します。
入力元が PS/2 TrackPoint、トラックボール、トラックパッド、光学センサー、custom pointing driver のいずれでも、`pointing_device_task_user()` に移動量が渡ってくれば利用できます。

### 必要なファイル

| コピー元 | コピー先 |
|----------|----------|
| `gesture.c` / `gesture.h` | `keymaps/<your>/` |
| `modes.c` | `keymaps/<your>/` |
| `local_settings.h` | `keymaps/<your>/` |
| （必要に応じて）`keymap.c` の `enum custom_keycodes` と `process_record_user` | → 既存の keymap.c にマージ |

### rules.mk

```makefile
SRC += gesture.c modes.c
TAP_DANCE_ENABLE = yes    # キーマップで TD(...) を使う場合のみ
COMBO_ENABLE = yes
EXTRAKEY_ENABLE = yes
MOUSEKEY_ENABLE = yes
CONSOLE_ENABLE = yes

# Vial マクロ発火には Vial/Dynamic Keymap マクロが必要です。
VIA_ENABLE = yes
VIAL_ENABLE = yes
```

### 必要なフック

**modes.c** に `pointing_device_task_user()` と `matrix_scan_user()` を実装します。
これらはポインティングデータの加工とジェスチャ制御を行います。

ジェスチャだけを既存キーマップへ組み込む場合、必要なフックは次の通りです。

- `pointing_device_task_user()` から `gesture_record_movement(mouse_report.x, mouse_report.y)` を呼ぶ
- `matrix_scan_user()` から `gesture_check_timeout()` を呼ぶ
- `process_record_user()` で `gesture_start()` / `gesture_end()` / `gesture_cancel()` を呼ぶ

### EEPROM 設定

```c
#define EECONFIG_USER_DATA_SIZE 64  // keymaps/<your>/config.h
```

設定の保存・読み込みは `local_settings.h` の `user_config_t` 構造体と
`eeconfig_read_user_datablock()` / `eeconfig_update_user_datablock()` で行います。
詳細は `keymap.c` の `keyboard_post_init_user()` と `save_user_config()` を参照。
