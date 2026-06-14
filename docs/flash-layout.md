# フラッシュレイアウトとストレージ設計

RP2040 フラッシュの構成と、Vial Dynamic Keymap・Raw HID ストレージ API・
ユーザー設定の保存設計について解説します。

## フラッシュレイアウト

`PICO_FLASH_SIZE_BYTES` は `keymaps/vial/config.h` で設定可能です。
実機のフラッシュ容量に合わせて変更してください。

### 4MB フラッシュ搭載時 (`PICO_FLASH_SIZE_BYTES = 4MB`)

```
Flash Offset
0x000000  ┌──────────────────────────────────────┐
          │  ファームウェア                        │  ~128KB
          ├──────────────────────────────────────┤
          │  未使用 (空き領域)                    │  ~2.75MB
          ├──────────────────────────────────────┤
          │  Raw HID ストレージ                    │   1MB
          │  SECRET_STORAGE_BASE = 0x2E0000      │
          ├──────────────────────────────────────┤
          │  Wear-leveling バッファ               │ 128KB
          │  (フラッシュ末尾に配置)               │
0x400000  └──────────────────────────────────────┘
```

### 2MB フラッシュ搭載時 (`PICO_FLASH_SIZE_BYTES = 2MB`)

```
0x000000  ┌──────────────────────────────────────┐
          │  ファームウェア                        │  ~128KB
          ├──────────────────────────────────────┤
          │  未使用                                │  ~768KB
          ├──────────────────────────────────────┤
          │  Raw HID ストレージ                    │   1MB
          │  SECRET_STORAGE_BASE = 0x0E0000       │
          ├──────────────────────────────────────┤
          │  Wear-leveling バッファ               │ 128KB
0x200000  └──────────────────────────────────────┘
```

`SECRET_STORAGE_BASE` の計算式（`secret_storage.c` より）:

```c
#define SECRET_STORAGE_BASE \
    (PICO_FLASH_SIZE_BYTES - WEAR_LEVELING_BACKING_SIZE - SECRET_STORAGE_SIZE)
```

### Raw HID ストレージ — API 仕様

USB Raw HID 経由でホストと通信する汎用ストレージ領域です。
32 バイトの Raw HID ペイロードでコマンドを送受信します。

```c
#define SECRET_STORAGE_SIZE (1 * 1024 * 1024)  // 1MB (設定変更可)
```

#### パケット形式

全コマンド共通で、Raw HID ペイロードは 32 バイト固定長です。
先頭バイトにコマンド識別子、それ以降がコマンド固有のパラメータ。
後続バイトは 0 埋めします。

#### コマンド一覧

##### INFO (0xA0) — ストレージ情報取得

```
Request:  [0xA0, 0x00...]
Response: [0xA0, status(1), total_size(4), flash_size(4), backing_size(4), base_addr(4), max_read(1), max_write(1), padding...]
```

| オフセット | サイズ | フィールド | 説明 |
|-----------|--------|-----------|------|
| 0 | 1 | コマンド | 0xA0 (応答も同じ) |
| 1 | 1 | status | 0x00=OK |
| 2-5 | 4 | total_size | SECRET_STORAGE_SIZE (ビッグエンディアン) |
| 6-9 | 4 | flash_size | PICO_FLASH_SIZE_BYTES |
| 10-13 | 4 | backing_size | WEAR_LEVELING_BACKING_SIZE |
| 14-17 | 4 | base_addr | SECRET_STORAGE_BASE |
| 18 | 1 | max_read | SECRET_MAX_READ (28) |
| 19 | 1 | max_write | SECRET_MAX_WRITE (26) |

##### READ (0xA1) — オフセット指定読み取り

```
Request:  [0xA1, offset(4), size(1), padding...]
Response: [0xA1, status(1), size(1), data(size)...]
```

| オフセット | サイズ | 説明 |
|-----------|--------|------|
| 0 | 1 | コマンド 0xA1 |
| 1-4 | 4 | 読み取り開始オフセット (ビッグエンディアン, 0 起点) |
| 5 | 1 | 読み取りサイズ (1〜28) |

制約: `offset + size <= SECRET_STORAGE_SIZE`

##### WRITE (0xA2) — オフセット指定書き込み

```
Request:  [0xA2, offset(4), size(1), data(size), padding...]
Response: [0xA2, status(1), padding...]
```

| オフセット | サイズ | 説明 |
|-----------|--------|------|
| 0 | 1 | コマンド 0xA2 |
| 1-4 | 4 | 書き込み開始オフセット (ビッグエンディアン) |
| 5 | 1 | 書き込みサイズ (1〜26) |
| 6〜 | size | 書き込むデータ |

制約: `offset + size <= SECRET_STORAGE_SIZE`

実装上、書き込み時は該当セクタ(4096B)を読み出し→変更→消去→プログラムするため、
1回の WRITE コマンドでもセクタ単位の書き換えが発生します。

##### ERASE (0xA3) — セクタ単位消去

```
Request:  [0xA3, offset(4), size(4), padding...]
Response: [0xA3, status(1), padding...]
```

| オフセット | サイズ | 説明 |
|-----------|--------|------|
| 0 | 1 | コマンド 0xA3 |
| 1-4 | 4 | 消去開始オフセット (ビッグエンディアン) |
| 5-8 | 4 | 消去サイズ (ビッグエンディアン) |

制約:
- `offset` は `FLASH_SECTOR_SIZE` (4096) の倍数
- `size` は `FLASH_SECTOR_SIZE` (4096) の倍数
- `offset + size <= SECRET_STORAGE_SIZE`

#### ステータスコード

| コード | 名前 | 意味 |
|-------|------|------|
| 0x00 | SECRET_STATUS_OK | 成功 |
| 0x01 | SECRET_STATUS_ERR | 一般的なエラー |
| 0x02 | SECRET_STATUS_BUSY | 前回の操作が未完了（ESC で中断可能） |
| 0x03 | SECRET_STATUS_RANGE | オフセット + サイズがストレージ範囲外 |
| 0x04 | SECRET_STATUS_ALIGN | 消去時のオフセット/サイズがセクタ境界に合わない |
| 0x05 | SECRET_STATUS_ABORT | ESC キーにより操作が中断された |

#### 組み込み方法

```c
#include "secret_storage.h"

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // 秘密ストレージ操作中はキー入力をブロック
    if (!secret_storage_process_record_user(keycode, record)) {
        return false;
    }
    // ... 通常のキー処理
}
```

操作中 (`secret_busy == true`) はすべてのキー入力がブロックされます。
`ESC` キーを処理できた場合は `secret_abort = true` になり、消去/書き込みループの
次の確認タイミングで中断します。ただし Raw HID の各コマンド処理は同期実行のため、
フラッシュ操作中に必ず ESC を処理できるとは限りません。

### ユーザー設定 (EEPROM)

`EECONFIG_USER_DATA_SIZE 64` の中に `user_config_t` を保存。
フィールド合計は 33 バイトですが、C 構造体のアライメントにより実際の `sizeof(user_config_t)` は通常 36 バイトです。

| フィールド | 型 | サイズ |
|-----------|-----|--------|
| `mouse_speed_mult` | uint8_t | 1 |
| `gesture_threshold` | uint8_t | 1 |
| `gesture_enabled` | bool | 1 |
| `gesture_timeout` | uint16_t | 2 |
| `os_mode` | uint8_t | 1 |
| (padding) | uint8_t[3] | 3 |
| `scroll_layer_mask` | uint32_t | 4 |
| `precise_layer_mask` | uint32_t | 4 |
| `fast_layer_mask` | uint32_t | 4 |
| `gesture_move_layer_mask` | uint32_t | 4 |
| `gesture_static_layer_mask` | uint32_t | 4 |
| `magic` (=0x504F5449) | uint32_t | 4 |
| **フィールド合計** | | **33** |

実際の保存サイズは `sizeof(user_config_t)` で決まり、現在の構造体ではアライメント込みで 36 バイトです。

### Vial エントリ数

| 機能 | 設定 | 値 |
|------|------|-----|
| タップダンス | `VIAL_TAP_DANCE_ENTRIES` | 64 |
| コンボ | `VIAL_COMBO_ENTRIES` | 128 |
| キーオーバーライド | `VIAL_KEY_OVERRIDE_ENTRIES` | 128 |
| Alt Repeat Key | `VIAL_ALT_REPEAT_KEY_ENTRIES` | 64 |
| マクロ | `DYNAMIC_KEYMAP_MACRO_COUNT` | 255 |

## カスタマイズ例

```c
// 16MB フラッシュ基板の場合
#define PICO_FLASH_SIZE_BYTES (16 * 1024 * 1024)
#define SECRET_STORAGE_SIZE   (10 * 1024 * 1024)  // 秘密ストレージ 10MB

// EEPROM の wear-leveling 物理領域を増やす場合
// Dynamic Keymap が使える論理領域は現在 64KiB まで
#define WEAR_LEVELING_BACKING_SIZE  262144    // 256KiB
#define WEAR_LEVELING_LOGICAL_SIZE   65536    // 64KiB
```

## 注意事項

- 論理 EEPROM サイズを増やすと RAM 使用量も増加（論理サイズ × 1バイト程度）
- ストレージサイズ変更後は既存データが無効に（フォーマット相当）
- ストレージ操作中は ESC で中断要求可能。ただし同期的なフラッシュ操作中に必ず処理できるとは限らない
