[English Version (README_en.md)](README_en.md)

# FOMO 物体検出 CPU版 (tron_edge_fomo_cpu_type)

## プログラム概要
このプログラムは、μT-Kernel 3.0 上で動作し、MIPI-CSI2 カメラ (OV5640) からの入力画像を CPU で処理し、軽量物体検出モデル「FOMO (Faster Objects, More Objects)」を用いて特定の電子部品 (PCB基板等) を検出・表示するエッジ AI プログラムです。
カメラから取得した 320x240 RGB565 画像を D/AVE 2D を用いて液晶画面の中央に 800x600 にバイリニア拡大表示し、検出されたオブジェクトの枠 (Bounding Box)、クラス名、および確信度をリアルタイムに重ね描きします。推論処理は TensorFlow Lite Micro (CPU 実行) を用いてバックグラウンドで並列実行され、描画タスクと協調して動作します。

## ハードウェア／周辺機能
* **マイコン / ボード**: ルネサス RA8 シリーズ (例: EK-RA8D1)
* **グラフィックス**: D/AVE 2D エンジン、GLCDC (1024x600 TFT 液晶パネル駆動)
* **カメラ (OV5640)**: MIPI-CSI2 接続によるリアルタイム画像入力 (320x240 分解能)
* **メモリ**: 外部 SDRAM (トリプルフレームバッファ領域)
* **制御 I/O**:
  * `MIPI_IF_EN` - MIPI スイッチの有効化ピン
  * `LCD_RST` - 液晶パネルリセットピン
  * `LCD_BLEN` - バックライト有効化ピン

## μT-Kernel 3.0 タスク構成
1. **task_1** (優先度: 10, スタックサイズ: 32KB)
   * メイン描画・カメラ制御タスク。カメラの起動、キャプチャ制御、液晶への画像転送、検出されたバウンディングボックスの描画、および Vblank 同期によるフリップを行います。AIタスクがビジーでない場合に `tk_wup_tsk(tskid_ai)` を呼び出して推論を起動します。
2. **task_2** (優先度: 10, スタックサイズ: 1KB)
   * 動作確認用のダミータスク。7秒ごとに「task 2 running...」を出力します。
3. **tskid_ai (task_ai)** (優先度: 11, スタックサイズ: 32KB)
   * AI推論タスク。描画タスク (10) より低い優先度 (11) に設定することで、画面描画を保護。`ai_inference_task` を実行し、TFLite Micro モデルに画像を入力して CPU で推論を実行します。

## 処理フロー
1. **初期化シーケンス**: SDRAM 初期化、GLCDC ピン割り当て、MIPI カメラリセットと初期化、Dave2D の初期化。
2. **描画＆表示同期ループ (task_1)**:
   * Vblank 割り込みを待ちスリープから復帰。
   * 新しいカメラ画像を Dave2D を使って 320x240 から 800x600 へ拡大コピー (`d2_blitcopy`)。
   * 現在取得している AI 検出結果 (座標、クラスID、確信度) を用いて Bounding Box とテキスト情報を重ねて描画。
   * 描画完了後、GPU コマンドのフラッシュと完了待ちを実行。
   * GLCDC のバッファを切り替え。
   * AIタスクが処理中でない (`!g_ai_task_busy`) 場合、`tk_wup_tsk(tskid_ai)` で推論タスクを起動。
3. **AI推論ループ (task_ai)**:
   * 起床待ち (`tk_slp_tsk` / `tk_wup_tsk`)。
   * カメラの入力バッファから画像を取得し、前処理を実行。
   * TFLite Micro による FOMO モデルの CPU 推論を実行。
   * 推論結果 (PCB, Pico, Xiao, nRF54L15 などのクラスと位置情報) をグローバル配列 `g_ai_detection` に格納。

## 主要なパラメータ・定義
* **検出対象クラス (pcb_class_names)**:
  1. FPC
  2. nRF54L15
  3. Pico
  4. Xiao
* `AI_MAX_DETECTION_NUM`: 最大検出数。
* `g_ai_inference_time_ms`: 推論にかかった時間 (ms)。

## 実行ログ例
```text
microT-Kernel Version 3.00

Start User-main program (Camera & LCD D2D Test).
task 2 running...
=== Starting AI Inference Task (CPU-based FOMO) ===
Testing physical SDRAM at address 0x68000000...
SDRAM verification SUCCESS!

=== Camera MIPI-CSI2 & LCD Display D2D Start ===
=== Setting Bus Slave Arbitration to Fixed Priority (Group 3) ===
Clearing SDRAM framebuffers to black...
Initializing LCD (GLCDC)...
LCD Backlight enabled.
Initializing D/AVE 2D Graphics Engine...
Initializing MIPI-CSI2 Camera (OV5640)...
[Camera Init] Product ID High: 0x56, Low: 0x40
SUCCESS: Camera initialized and capture started.
Entering Real-time Camera Display Loop...
[VIN CB] First frame captured successfully!
Loop 0: buffer = 0x22073580, vsync_cnt = 15
  Buf Data: 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000
[Diag] Cam buffer: min=0x0000, max=0xDE9C, avg=0x0022 | NPU Input: min=-128, max=85, avg=-127
[CPU Input Dump] first 10 bytes: -128 -128 -128 -128 -128 -128 -128 -128 -128 -128
[CPU Invoke] Status: 0
[CPU Time Debug] cycles: 133601684
[CPU Output Stats] C0: [101, 127] avg 122 | C1: [-128, -128] avg -128 | C2: [-128, -128] avg -128 | C3: [-128, -101] avg -122 | C4: [-128, -128] avg -128
[PostProcess] Detection results count: 0
AI Inference (NPU): IC detection complete in 278 ms
[Diag] Cam buffer: min=0x0000, max=0xFFFF, avg=0x2BCC | NPU Input: min=-128, max=127, avg=-83
[CPU Input Dump] first 10 bytes: -128 -112 -22 -128 -112 -5 -128 -100 -22 -128
[CPU Invoke] Status: 0
[CPU Time Debug] cycles: 133654476
[CPU Output Stats] C0: [120, 127] avg 126 | C1: [-128, -128] avg -128 | C2: [-128, -127] avg -127 | C3: [-128, -120] avg -127 | C4: [-128, -127] avg -127
[PostProcess] Detection results count: 0
AI Inference (NPU): IC detection complete in 278 ms
[Diag] Cam buffer: min=0x0003, max=0xFFFF, avg=0x3D2F | NPU Input: min=-128, max=127, avg=-61
[CPU Input Dump] first 10 bytes: -120 -100 3 -128 -104 3 -120 -96 28 -120
[CPU Invoke] Status: 0
[CPU Time Debug] cycles: 133663272
[CPU Output Stats] C0: [126, 127] avg 126 | C1: [-128, -128] avg -128 | C2: [-128, -126] avg -127 | C3: [-128, -126] avg -127 | C4: [-128, -128] avg -128
[PostProcess] Detection results count: 0
AI Inference (NPU): IC detection complete in 278 ms
[Diag] Cam buffer: min=0x0022, max=0xFFFF, avg=0x4555 | NPU Input: min=-128, max=127, avg=-56
[CPU Input Dump] first 10 bytes: -128 -92 3 -120 -100 -22 -112 -92 3 -120
[CPU Invoke] Status: 0
[CPU Time Debug] cycles: 133601628
[CPU Output Stats] C0: [125, 127] avg 126 | C1: [-128, -128] avg -128 | C2: [-128, -126] avg -127 | C3: [-128, -125] avg -127 | C4: [-128, -128] avg -128
[PostProcess] Detection results count: 0
AI Inference (NPU): IC detection complete in 278 ms
[Diag] Cam buffer: min=0x0023, max=0xFFFF, avg=0x491A | NPU Input: min=-128, max=127, avg=-50
[CPU Input Dump] first 10 bytes: -120 -96 -5 -120 -96 -5 -120 -92 20 -112
[CPU Invoke] Status: 0
[CPU Time Debug] cycles: 133654464
[CPU Output Stats] C0: [126, 127] avg 126 | C1: [-128, -128] avg -128 | C2: [-128, -126] avg -127 | C3: [-128, -127] avg -127 | C4: [-128, -128] avg -128
[PostProcess] Detection results count: 0
AI Inference (NPU): IC detection complete in 278 ms
[Diag] Cam buffer: min=0x0022, max=0xFFFF, avg=0x4CA1 | NPU Input: min=-128, max=127, avg=-46
[CPU Input Dump] first 10 bytes: -120 -88 -5 -128 -100 -13 -112 -84 3 -112
[CPU Invoke] Status: 0
[CPU Time Debug] cycles: 133628044
[CPU Output Stats] C0: [125, 127] avg 126 | C1: [-128, -128] avg -128 | C2: [-128, -125] avg -127 | C3: [-128, -128] avg -128 | C4: [-128, -128] avg -128
[PostProcess] Detection results count: 0
AI Inference (NPU): IC detection complete in 278 ms
[Diag] Cam buffer: min=0x0023, max=0xFFFF, avg=0x4640 | NPU Input: min=-128, max=127, avg=-53
[CPU Input Dump] first 10 bytes: -128 -100 3 -128 -96 3 -120 -92 11 -112
[CPU Invoke] Status: 0
[CPU Time Debug] cycles: 133522468
[CPU Output Stats] C0: [127, 127] avg 127 | C1: [-128, -128] avg -128 | C2: [-128, -127] avg -127 | C3: [-128, -127] avg -127 | C4: [-128, -128] avg -128
[PostProcess] Detection results count: 0
AI Inference (NPU): IC detection complete in 278 ms
[Diag] Cam buffer: min=0x0022, max=0xFFFF, avg=0x4157 | NPU Input: min=-128, max=127, avg=-60
[CPU Input Dump] first 10 bytes: -120 -100 -13 -120 -96 -13 -112 -92 11 -120
[CPU Invoke] Status: 0
[CPU Time Debug] cycles: 133619288
[CPU Output Stats] C0: [-89, 127] avg 121 | C1: [-128, -128] avg -128 | C2: [-128, -126] avg -127 | C3: [-128, 88] avg -122 | C4: [-128, -126] avg -127
[PostProcess] Detection results count: 2
[NPU Result Present] Presenting 2 PCBs:
  - Pico 0: x0=4, y0=68, w=16, h=16, score=56%
[update_detection_result] idx=0, x=4, y=68, w=16, h=16, prob=56%, class=3
  - Pico 1: x0=20, y0=76, w=16, h=16, score=84%
[update_detection_result] idx=1, x=20, y=76, w=16, h=16, prob=84%, class=3
AI Inference (NPU): IC detection complete in 278 ms
[Diag] Cam buffer: min=0x0001, max=0xFFFF, avg=0x3C68 | NPU Input: min=-128, max=127, avg=-64
[CPU Input Dump] first 10 bytes: -128 -100 -5 -120 -104 -5 -120 -100 -5 -128
[CPU Invoke] Status: 0
[CPU Time Debug] cycles: 133865668
[CPU Output Stats] C0: [-110, 127] avg 101 | C1: [-128, -128] avg -128 | C2: [-128, -114] avg -127 | C3: [-128, 110] avg -103 | C4: [-128, -118] avg -127
[PostProcess] Detection results count: 9
[NPU Result Present] Presenting 9 PCBs:
  - Pico 0: x0=20, y0=44, w=16, h=16, score=52%
[update_detection_result] idx=0, x=20, y=44, w=16, h=16, prob=52%, class=3
  - Pico 1: x0=28, y0=52, w=16, h=16, score=74%
[update_detection_result] idx=1, x=28, y=52, w=16, h=16, prob=74%, class=3
  - Pico 2: x0=28, y0=60, w=16, h=16, score=92%
[update_detection_result] idx=2, x=28, y=60, w=16, h=16, prob=92%, class=3
  - Pico 3: x0=36, y0=60, w=16, h=16, score=56%
[update_detection_result] idx=3, x=36, y=60, w=16, h=16, prob=56%, class=3
  - Pico 4: x0=12, y0=68, w=16, h=16, score=79%
[update_detection_result] idx=4, x=12, y=68, w=16, h=16, prob=79%, class=3
  - Pico 5: x0=20, y0=68, w=16, h=16, score=86%
[update_detection_result] idx=5, x=20, y=68, w=16, h=16, prob=86%, class=3
  - Pico 6: x0=28, y0=68, w=16, h=16, score=77%
[update_detection_result] idx=6, x=28, y=68, w=16, h=16, prob=77%, class=3
  - Pico 7: x0=12, y0=76, w=16, h=16, score=91%
[update_detection_result] idx=7, x=12, y=76, w=16, h=16, prob=91%, class=3
  - Pico 8: x0=20, y0=76, w=16, h=16, score=72%
[update_detection_result] idx=8, x=20, y=76, w=16, h=16, prob=72%, class=3
AI Inference (NPU): IC detection complete in 278 ms
[Diag] Cam buffer: min=0x0000, max=0xFFFF, avg=0x36A0 | NPU Input: min=-128, max=127, avg=-72
[CPU Input Dump] first 10 bytes: -128 -104 -22 -128 -108 -5 -128 -104 3 -128
[CPU Invoke] Status: 0
[CPU Time Debug] cycles: 134877664
[CPU Output Stats] C0: [-115, 127] avg 99 | C1: [-128, -128] avg -128 | C2: [-128, -117] avg -127 | C3: [-128, 115] avg -100 | C4: [-128, -88] avg -127
```
