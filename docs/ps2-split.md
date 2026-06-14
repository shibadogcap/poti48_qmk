# PS/2 トラックポイント分割対応

片側（右）のみに接続されたトラックポイント (PS/2) の入力を、
分割キーボードの左右で共有するための実装です。

## 概要

通常の QMK の PS/2 ドライバはシングル MCU 前提です。
左右分割構成で PS/2 を片側だけに接続するには:

1. **PS/2 非接続側（左）では初期化をスキップ**
2. **PS/2 データを左右シリアル通信で転送**
3. **マスター側（USB接続側）から HID 送信**

Poti48 では右側の RP2040 に PS/2 を接続し、`SPLIT_POINTING_ENABLE`
メカニズムで左右のデータを同期します。

### 左右どちらを USB 接続しても動作

左右判別は GP29 ピンで行います（HIGH = 左 / LOW = 右）。
左右で同じファームウェア (.uf2) を書き込むことができ、USB は左右どちらにつないでも動作します。

---

## ファイル構成

```
keymaps/vial/
├── ps2_vendor.c       # カスタムPS/2 PIOドライバ (左側スキップ対応)
├── ps2_split.c        # PS/2 → 分割ポインティング ブリッジ
├── ps2_pointing.c     # カスタムポインティングドライバ (薄いラッパ)
├── modes.c            # ポインティングデータ加工 (scroll/speed/gesture)
└── rules.mk
```

## アーキテクチャ

### ケースA: 左側を USB 接続（通常）

USB マスター = 左、PS/2 スレーブ = 右:

```
       左側 (マスター:USB=Master)          右側 (スレーブ:USB=not connected)
┌──────────────────────────────┐    ┌──────────────────────────────┐
│ keyboard_pre_init_user()     │    │ keyboard_pre_init_user()     │
│  GP29=HIGH → PS/2 無効化     │    │  GP29=LOW  → PS/2 初期化実行 │
│                              │    │                              │
│  modes.c:                    │    │  ps2_vendor.c (PIO0)         │
│   pointing_device_task_user()│    │   PS/2 データ受信            │
│   ├ scroll変換               │    │        ↓                     │
│   ├ gesture統合              │    │  ps2_split.c                 │
│   ├ 速度倍率調整             │    │   ps2_mouse_moved_user()     │
│   └→ USB HID 送信            │    │   レポートを保存・ゼロクリア │
│                              │    │        ↓                     │
│                              │◄───│── serial (PIO1) ──────────── │
│                              │    │   ps2_pointing_get_report()  │
└──────────────────────────────┘    └──────────────────────────────┘
```

- 左側: `keyboard_pre_init_user()` で GP29=HIGH → PS/2 スキップ
- 右側: PS/2 ドライバ稼働、レポートをシリアル経由で左側に送信
- 左側: `is_keyboard_master()=true` で scroll/gesture/speed 処理後、USB HID 送信

### ケースB: 右側を USB 接続

USB マスター = 右、PS/2 も同一 MCU:

```
       左側 (スレーブ:USB=not connected)    右側 (マスター:USB=Master)
┌──────────────────────────────┐    ┌──────────────────────────────┐
│ keyboard_pre_init_user()     │    │ keyboard_pre_init_user()     │
│  GP29=HIGH → PS/2 無効化     │    │  GP29=LOW  → PS/2 初期化実行 │
│                              │    │                              │
│  modes.c:                    │    │  ps2_vendor.c (PIO0)         │
│  pointing_device_task_user() │    │   PS/2 データ受信            │
│  → is_keyboard_master()=false│    │        ↓                     │
│    何もせずそのまま返す      │    │  ps2_split.c                 │
│                              │    │   ps2_mouse_moved_user()     │
│                              │    │   レポートを保存・ゼロクリア │
│                              │    │        ↓                     │
│                              │    │   ps2_pointing_get_report()  │
│                              │    │   (同一 MCU 内で取得)        │
│                              │    │        ↓                     │
│                              │    │  modes.c:                    │
│                              │    │   is_keyboard_master()=true  │
│                              │    │   → scroll/gesture/speed     │
│                              │    │   → USB HID 送信             │
└──────────────────────────────┘    └──────────────────────────────┘
```

- 右側: PS/2 ドライバ稼働 + USB マスター、同一 MCU 内でデータ完結
- 左側: `is_keyboard_master()=false` なので `pointing_device_task_user` は素通し
- このケースでは TrackPoint のレポートは右側 MCU 内で `ps2_mouse_moved_user()` → `ps2_pointing_get_report()` → `pointing_device_task_user()` に渡り、左側から serial 経由で入力されるわけではありません。

## 移植ガイド

### 必要なファイル

| コピー元 | コピー先 |
|----------|----------|
| `ps2_vendor.c` | `keymaps/<your>/ps2_vendor.c` |
| `ps2_split.c` | `keymaps/<your>/ps2_split.c` |
| `ps2_pointing.c` | `keymaps/<your>/ps2_pointing.c` |

### rules.mk

```makefile
PS2_MOUSE_ENABLE = yes
PS2_DRIVER = vendor
SERIAL_DRIVER = vendor
SPLIT_TRANSPORT = serial
POINTING_DEVICE_ENABLE = yes
SPLIT_POINTING_ENABLE = yes
POINTING_DEVICE_DRIVER = custom
SRC += ps2_pointing.c ps2_split.c ps2_vendor.c
```

### keyboard.json

```json
{
    "split": { "handedness": { "pin": "GP29" } },
    "matrix_pins": {
        "cols": ["GP28","GP27","GP26","GP22","GP20","GP23","GP21"],
        "rows": ["GP2","GP3","GP4","GP5","GP6"]
    },
    "split": {
        "matrix_pins": {
            "right": {
                "cols": ["GP23","GP20","GP22","GP26","GP27","GP28","GP21"],
                "rows": ["GP2","GP3","GP4","GP5","GP6"]
            }
        }
    }
}
```

### config.h

```c
#define PS2_CLOCK_PIN GP9
#define PS2_DATA_PIN  GP8
#define PS2_MOUSE_USE_REMOTE_MODE
#define SERIAL_PIO_USE_PIO1
#define PS2_PIO_USE_PIO0
#define POINTING_DEVICE_RIGHT
#define SPLIT_POINTING_ENABLE
#define SPLIT_TRANSPORT_MIRROR
#define SPLIT_WATCHDOG_ENABLE
#define SPLIT_MAX_CONNECTION_ERRORS 10
```

### 注意事項

- **RP2040 (PIO) 専用** — `ps2_vendor.c` は PIO ドライバを使用
- **PS/2 DATA/CLOCK ピンは連番必須** (`PS2_DATA_PIN + 1 == PS2_CLOCK_PIN`)
- 左右で同じ .uf2 ファイルを書き込めます
