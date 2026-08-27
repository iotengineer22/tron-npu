[English Version (README_en.md)](README_en.md)

# FOMO 物体検出 NPU版 (tron_edge_fomo_npu_type)

## プログラム概要
このプログラムは、μT-Kernel 3.0 上で動作し、MIPI-CSI2 カメラ (OV5640) からの画像入力に対して、ルネサス RA8 マイコン内蔵の NPU アクセラレータ「Arm Ethos-U55」を用いて超リアルタイムで軽量物体検出 (FOMO) を実行するエッジ AI プログラムです。
320x240 RGB565 画像を 800x600 に拡大表示し、PCB基板上の特定部品 (FPC, Pico, Xiao, nRF54L15) の検出箇所に緑枠 (Bounding Box) を重ね描きます。NPU を活用した極めて高速な推論処理 (数ミリ秒オーダー) により、CPU 負荷を最小限に抑えつつ高フレームレートな物体検出を実現しています。

## ハードウェア／周辺機能
* **マイコン / ボード**: ルネサス RA8 シリーズ (例: EK-RA8D1)
* **アクセラレータ**: Arm Ethos-U55 NPU (ニューラルネットワーク処理用)
* **グラフィックス**: D/AVE 2D エンジン、GLCDC (1024x600 TFT 液晶パネル駆動)
* **カメラ (OV5640)**: MIPI-CSI2 接続によるリアルタイム画像入力 (320x240 分解能)
* **メモリ**: 外部 SDRAM (トリプルフレームバッファ領域、モデル領域)
* **制御 I/O**:
  * `MIPI_IF_EN` - MIPI スイッチの有効化ピン
  * `LCD_RST` - 液晶パネルリセットピン
  * `LCD_BLEN` - バックライト有効化ピン

## μT-Kernel 3.0 タスク構成
1. **task_1** (優先度: 10, スタックサイズ: 32KB)
   * メイン描画・カメラ制御タスク。画像キャプチャ、Dave2D による液晶拡大表示、検出された Bounding Box の重ね描き、Vblank 同期フリップを実行。AI タスクのビジー状態を監視し、`tk_wup_tsk(tskid_ai)` を呼び出して NPU 推論をキックします。
2. **task_2** (優先度: 10, スタックサイズ: 1KB)
   * 動作確認用のダミータスク。7秒ごとに「task 2 running...」を出力します。
3. **tskid_ai (task_ai)** (優先度: 11, スタックサイズ: 32KB)
   * AI推論タスク。優先度を 11 に設定。`ai_inference_task` を実行し、モデルデータを入力して Arm Ethos-U55 NPU ドライバ経由でハードウェアアクセラレータによる超高速な推論を実行します。

## 処理フロー
1. **初期化シーケンス**: SDRAM 初期化、GLCDC ピン構成、MIPI カメラリセットと初期化、Dave2D 初期化、Ethos-U55 NPU ドライバのオープン・初期化。
2. **描画＆表示同期ループ (task_1)**:
   * Vblank 割り込みを待ちスリープから復帰。
   * 新しいカメラ画像 (320x240) を 800x600 に中央拡大コピー。
   * NPU 推論で得られた 96x96 検出座標系から液晶画面上の 800x600 エリア（倍率 6.25倍、オフセット 212.0f）に位置変換して、Bounding Box とクラス名（FPC, Pico, Xiao, nRF54L15 などの PCB 部品）を描画。
   * 画面左上に NPU による推論時間 (`g_ai_inference_time_ms`) および個数を描画。
   * フリップと、NPU 推論の起床通知 (`tk_wup_tsk`)。
3. **AI推論ループ (task_ai)**:
   * 起床待ち (`tk_slp_tsk`)。
   * 前処理されたカメラデータを NPU 用入力テンソルへコピー。
   * Ethos-U55 NPU による推論を実行。完了後、結果を `g_ai_detection` に格納。

## 主要なパラメータ・定義
* **検出対象クラス**: Background, FPC, nRF54L15, Pico, Xiao
* `AI_MAX_DETECTION_NUM`: 最大検出数。
* **画面変換パラメータ**:
  * 検出スケール倍率: `6.25f`
  * 描画 X オフセット: `212.0f`

## 実行ログ例
```text
microT-Kernel Version 3.00

Start User-main program (Camera & LCD D2D Test).
task 2 running...
=== Starting AI Inference Task (NPU-based FOMO) ===
Testing physical SDRAM at address 0x68000000...
SDRAM verification SUCCESS!

=== Camera MIPI-CSI2 & LCD Display D2D Start ===
=== Setting Bus Slave Arbitration to Fixed Priority (Group 3) ===
Clearing SDRAM framebuffers to black...
Initializing LCD (GLCDC)...
SUCCESS: Arm Ethos-U55 NPU driver initialized successfully!
LCD Backlight enabled.
Initializing D/AVE 2D Graphics Engine...
Initializing MIPI-CSI2 Camera (OV5640)...
[Camera Init] Product ID High: 0x56, Low: 0x40
SUCCESS: Camera initialized and capture started.
Entering Real-time Camera Display Loop...
[VIN CB] First frame captured successfully!

[Diag] Cam buffer: min=0x0000, max=0xF77F, avg=0x001E | NPU Input: min=-128, max=110, avg=-127
[NPU Input Dump] first 10 bytes: -128 -128 -128 -128 -128 -128 -128 -128 -128 -128
[NPU Invoke] Status: 0
[NPU Time Debug] cycles: 2791736
[NPU Output Stats] C0: [99, 127] avg 121 | C1: [-128, -128] avg -128 | C2: [-128, -128] avg -128 | C3: [-128, -99] avg -121 | C4: [-128, -128] avg -128
[PostProcess] Detection results count: 0
AI Inference (NPU): IC detection complete in 5 ms

... (物体をかざすと C3:Picoクラスのスコアが急上昇) ...

[Diag] Cam buffer: min=0x0005, max=0xFFFF, avg=0x476A | NPU Input: min=-128, max=127, avg=-54
[NPU Input Dump] first 10 bytes: -112 -84 3 -112 -96 -13 -112 -84 11 -104
[NPU Invoke] Status: 0
[NPU Time Debug] cycles: 2791692
[NPU Output Stats] C0: [-97, 127] avg 117 | C1: [-128, -128] avg -128 | C2: [-128, -117] avg -127 | C3: [-128, 94] avg -119 | C4: [-128, -96] avg -127
[PostProcess] Detection results count: 4
[NPU Result Present] Presenting 4 PCBs:
  - Pico 0: x0=28, y0=68, w=16, h=16, score=72%
[update_detection_result] idx=0, x=28, y=68, w=16, h=16, prob=72%, class=3
  - Pico 1: x0=36, y0=68, w=16, h=16, score=69%
[update_detection_result] idx=1, x=36, y=68, w=16, h=16, prob=69%, class=3
  - Pico 2: x0=28, y0=76, w=16, h=16, score=86%
[update_detection_result] idx=2, x=28, y=76, w=16, h=16, prob=86%, class=3
  - Pico 3: x0=52, y0=76, w=16, h=16, score=83%
[update_detection_result] idx=3, x=52, y=76, w=16, h=16, prob=83%, class=3
AI Inference (NPU): IC detection complete in 5 ms
```
