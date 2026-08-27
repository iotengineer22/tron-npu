[English Version (README_en.md)](README_en.md)

# YOLO顔検出 CPU版 (tron_yolo_face_cpu)

## プログラム概要
このプログラムは、μT-Kernel 3.0 上で動作し、MIPI-CSI2 カメラ (OV5640) からの画像に対して、顔検出モデル (YOLO形式) を用いてリアルタイムで人物の顔を検出・表示するエッジ AI プログラムです。
カメラから取得した 320x240 RGB565 画像を 800x600 に拡大表示し、192x192 解像度で推論された顔の座標を液晶画面座標（倍率 3.125倍、X方向オフセット 212.0f）に変換し、検出位置に緑の枠 (Bounding Box) と確信度 (%) を重ね描きします。推論処理には TensorFlow Lite Micro を用い、CPU 上でバックグラウンド実行されます。

## ハードウェア／周辺機能
* **マイコン / ボード**: ルネサス RA8 シリーズ (例: EK-RA8D1)
* **グラフィックス**: D/AVE 2D エンジン、GLCDC (1024x600 TFT 液晶パネル駆動)
* **カメラ (OV5640)**: MIPI-CSI2 接続によるリアルタイム画像入力 (320x240 分解能)
* **メモリ**: 外部 SDRAM (トリプルフレームバッファ領域、モデル入力領域)
* **制御 I/O**:
  * `MIPI_IF_EN` - MIPI スイッチの有効化ピン
  * `LCD_RST` - 液晶パネルリセットピン
  * `LCD_BLEN` - バックライト有効化ピン

## μT-Kernel 3.0 タスク構成
1. **task_1** (優先度: 10, スタックサイズ: 32KB)
   * メイン描画・カメラ制御タスク。画像キャプチャ、Dave2D による液晶拡大表示、検出された顔への枠線の重ね描き、Vblank 同期フリップを実行。AI タスクの完了を監視し、`tk_wup_tsk(tskid_ai)` を呼び出して推論を起動します。
2. **task_2** (優先度: 10, スタックサイズ: 1KB)
   * 動作確認用のダミータスク。7秒ごとに「task 2 running...」を出力します。
3. **tskid_ai (task_ai)** (優先度: 11, スタックサイズ: 32KB)
   * AI推論タスク。優先度を 11 に設定。`ai_inference_task` を実行し、TFLite Micro モデルを用いて CPU 上で顔検出推論を実行、顔の位置と確信度を算出します。

## 処理フロー
1. **初期化設定**: SDRAM 初期化、GLCDC ピン構成、MIPI カメラリセットと初期設定、Dave2D ドライバのオープン・初期化。
2. **描画＆表示同期ループ (task_1)**:
   * Vblank 割り込みを待ちスリープから復帰。
   * 新しいカメラ画像 (320x240) を 800x600 に中央拡大コピー。
   * AI 検出結果 (座標、確信度) を取得し、液晶画面に合わせた Bounding Box と確信度テキストを描画。
   * 画面左上に AI 推論時間 (`g_ai_inference_time_ms`) および検出された顔の数 (`Faces : %d`) を描画。
   * GLCDC バッファフリップと、AIタスクへの起床通知 (`tk_wup_tsk`)。
3. **AI推論ループ (task_ai)**:
   * `tk_slp_tsk` による起床待ち。
   * 入力データへの前処理、および TFLite Micro を用いた CPU 推論の実行。結果を `g_ai_detection` に格納。

## 主要なパラメータ・定義
* `AI_MAX_DETECTION_NUM`: 最大検出数。
* **画面変換パラメータ**:
  * 検出スケール倍率: `3.125f`
  * 描画 X オフセット: `212.0f` (カメラ画像の配置および液晶座標オフセット調整)
* `g_ai_inference_time_ms`: 顔検出推論の実行時間。

## 実行ログ例
```
microT-Kernel Version 3.00

Start User-main program (Camera & LCD D2D Test).
task 2 running...
=== Starting AI Inference Task (CPU-based YOLO-Fastest) ===
Testing physical SDRAM at address 0x68000000...
SDRAM verification SUCCESS!

=== Camera MIPI-CSI2 & LCD Display D2D Start ===
=== Setting Bus Slave Arbitration to Fixed Priority (Group 3) ===
Clearing SDRAM framebuffers to black...
Initializing LCD (GLCDC)...
SUCCESS: TensorFlow Lite Micro initialized successfully!
Input tensor size: 36864 bytes, shape: 192 x 192 x 1
LCD Backlight enabled.
Initializing D/AVE 2D Graphics Engine...
Initializing MIPI-CSI2 Camera (OV5640)...
[Camera Init] Product ID High: 0x56, Low: 0x40
SUCCESS: Camera initialized and capture started.
Entering Real-time Camera Display Loop...
[VIN CB] First frame captured successfully!
Loop 0: buffer = 0x22073580, vsync_cnt = 15
  Buf Data: 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000
AI Inference: Found 0 face(s) in 2081 ms (Pre: 9 ms, Inv: 2072 ms, Post: 0 ms)
task 2 running...
AI Inference: Found 0 face(s) in 2091 ms (Pre: 9 ms, Inv: 2082 ms, Post: 0 ms)
AI Inference: Found 0 face(s) in 2090 ms (Pre: 9 ms, Inv: 2080 ms, Post: 0 ms)
AI Inference: Found 0 face(s) in 2090 ms (Pre: 9 ms, Inv: 2081 ms, Post: 0 ms)
AI Inference: Found 0 face(s) in 2090 ms (Pre: 9 ms, Inv: 2072 ms, Post: 9 ms)
Loop 100: buffer = 0x22028580, vsync_cnt = 274
  Buf Data: 0x0020 0x0020 0x0020 0x0020 0x0020 0x0040 0x0020 0x0040
task 2 running...
AI Inference: Found 1 face(s) in 2102 ms (Pre: 9 ms, Inv: 2092 ms, Post: 0 ms)
  Face 0: Box[18, 57, 61, 64], Conf: 97%
AI Inference: Found 1 face(s) in 2092 ms (Pre: 9 ms, Inv: 2083 ms, Post: 0 ms)
  Face 0: Box[0, 70, 56, 66], Conf: 97%
```
