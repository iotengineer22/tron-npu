[English Version (README_en.md)](README_en.md)

# FOMO IC検出 NPU版 (tron_edge_fomo_ic)

## プログラム概要
このプログラムは、μT-Kernel 3.0 上で動作し、MIPI-CSI2 カメラ (OV5640) からの画像入力を用いて、IC (集積回路) チップなどの特定オブジェクトを検出・カウントするエッジ AI プログラムです。
カメラから取得した 320x240 RGB565 画像を 800x600 にバイリニア拡大表示し、192x192 の推論画像スケールから算出した IC 検出位置に緑色の枠 (Bounding Box) と確信度を重ねて表示します。推論処理にはルネサス RA8 マイコン内蔵の NPU アクセラレータ「Arm Ethos-U55」を使用し、超高速でバックグラウンド実行されます。

## ハードウェア／周辺機能
* **マイコン / ボード**: ルネサス RA8 シリーズ (例: EK-RA8D1)
* **グラフィックス**: D/AVE 2D エンジン、GLCDC (1024x600 TFT 液晶パネル駆動)
* **カメラ (OV5640)**: MIPI-CSI2 接続によるリアルタイム画像入力 (320x240 分解能)
* **メモリ**: 外部 SDRAM (トリプルバッファフレームバッファ領域)
* **制御 I/O**:
  * `MIPI_IF_EN` - MIPI スイッチの有効化ピン
  * `LCD_RST` - 液晶パネルリセットピン
  * `LCD_BLEN` - バックライト有効化ピン

## μT-Kernel 3.0 タスク構成
1. **task_1** (優先度: 10, スタックサイズ: 32KB)
   * メイン描画・カメラ制御タスク。カメラのキャプチャ制御、液晶への画像転送、検出された IC 用バウンディングボックスの描画、および Vblank 同期によるフリップを行います。AIタスクがビジーでない場合に推論をキックします。
2. **task_2** (優先度: 10, スタックサイズ: 1KB)
   * 動作確認用のダミータスク。7秒ごとに「task 2 running...」を出力します。
3. **tskid_ai (task_ai)** (優先度: 11, スタックサイズ: 32KB)
   * AI推論タスク。優先度を 11 に設定し、`ai_inference_task` を実行して 192x192 解像度にダウンサンプリングされた画像で IC 検出推論を実行します。

## 処理フロー
1. **初期化シーケンス**: SDRAM 初期化、GLCDC ピン構成の適用、MIPI カメラリセットと起動、Dave2D の初期化。
2. **描画＆表示同期ループ (task_1)**:
   * Vblank 割り込みを待ちスリープから復帰。
   * Dave2D を使ってカメラ画像 (320x240) を 800x600 に拡大コピー。
   * グローバル配列 `g_ai_detection` に格納された IC 検出結果 (192x192 スケールから 800x600 画面中央へ変換: スケール倍率 3.125倍、X方向オフセット 212.0f) に基づき、Bounding Box と確信度 (%) を描画。
   * 画面左上に AI 推論時間 (`g_ai_inference_time_ms`) と検出した IC の総数 (`IC : %d`) を描画。
   * GLCDC のフリップと、AIタスクへの起床通知 (`tk_wup_tsk`)。
3. **AI推論ループ (task_ai)**:
   * `tk_slp_tsk` による起床待ち。
   * 入力画像から前処理を行い、Arm Ethos-U55 NPUによる推論を実行。結果を `g_ai_detection` に更新。

## 主要なパラメータ・定義
* `AI_MAX_DETECTION_NUM`: 最大検出数。
* **画面変換パラメータ**:
  * 検出スケール倍率: `3.125f`
  * 描画 X オフセット: `212.0f` (液晶 800px 幅中央、X=112 基準＋カメラ画像内オフセット)

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
Loop 0: buffer = 0x22073580, vsync_cnt = 15
  Buf Data: 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000
[Diag] Cam buffer: min=0x0000, max=0x0021, avg=0x0009 | NPU Input: min=-128, max=-120, avg=-127
[NPU Input Dump] first 10 bytes: -128 -128 -128 -128 -124 -128 -128 -128 -128 -128
[NPU Invoke] Status: 0
[NPU Time Debug] cycles: 5213572
[NPU Output Stats] C0: [127, 127] avg 127 | C1: [-128, -128] avg -128 | C2: [-128, -128] avg -128
[PostProcess] Detection results count: 0
AI Inference (NPU): IC detection complete in 10 ms
[Diag] Cam buffer: min=0x0000, max=0x18C3, avg=0x000C | NPU Input: min=-128, max=-96, avg=-127
[NPU Input Dump] first 10 bytes: -128 -128 -128 -128 -128 -120 -128 -124 -128 -128
[NPU Invoke] Status: 0
[NPU Time Debug] cycles: 5211780
[NPU Output Stats] C0: [127, 127] avg 127 | C1: [-128, -128] avg -128 | C2: [-128, -128] avg -128
[PostProcess] Detection results count: 0
AI Inference (NPU): IC detection complete in 10 ms
[Diag] Cam buffer: min=0x0000, max=0xFFFF, avg=0x391B | NPU Input: min=-128, max=127, avg=-78
[NPU Input Dump] first 10 bytes: -128 -108 -54 -120 -96 -46 -112 -104 -30 -120
[NPU Invoke] Status: 0
[NPU Time Debug] cycles: 5211708
[NPU Output Stats] C0: [-120, 127] avg 123 | C1: [-128, -128] avg -128 | C2: [-128, 120] avg -124
[PostProcess] Detection results count: 8
[NPU Result Present] Presenting 8 ICs:
  - IC 0: x0=28, y0=100, w=16, h=16, score=90%
[update_detection_result] idx=0, x=28, y=100, w=16, h=16, prob=90%
  - IC 1: x0=28, y0=108, w=16, h=16, score=93%
[update_detection_result] idx=1, x=28, y=108, w=16, h=16, prob=93%
  - IC 2: x0=28, y0=116, w=16, h=16, score=67%
[update_detection_result] idx=2, x=28, y=116, w=16, h=16, prob=67%
  - IC 3: x0=28, y0=132, w=16, h=16, score=95%
[update_detection_result] idx=3, x=28, y=132, w=16, h=16, prob=95%
  - IC 4: x0=28, y0=140, w=16, h=16, score=96%
[update_detection_result] idx=4, x=28, y=140, w=16, h=16, prob=96%
  - IC 5: x0=28, y0=148, w=16, h=16, score=95%
[update_detection_result] idx=5, x=28, y=148, w=16, h=16, prob=95%
  - IC 6: x0=28, y0=164, w=16, h=16, score=75%
[update_detection_result] idx=6, x=28, y=164, w=16, h=16, prob=75%
  - IC 7: x0=156, y0=164, w=16, h=16, score=96%
[update_detection_result] idx=7, x=156, y=164, w=16, h=16, prob=96%
AI Inference (NPU): IC detection complete in 10 ms
[Diag] Cam buffer: min=0x0043, max=0xFFFF, avg=0x4800 | NPU Input: min=-128, max=127, avg=-53
[NPU Input Dump] first 10 bytes: -112 -92 -46 -104 -80 -38 -96 -80 -22 -112
[NPU Invoke] Status: 0
[NPU Time Debug] cycles: 5211768
[NPU Output Stats] C0: [-120, 127] avg 125 | C1: [-128, -128] avg -128 | C2: [-128, 120] avg -126
[PostProcess] Detection results count: 3
[NPU Result Present] Presenting 3 ICs:
...
```
