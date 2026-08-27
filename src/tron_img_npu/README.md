[English Version (README_en.md)](README_en.md)

# MobileNet画像分類 NPU版 (tron_img_npu)

## プログラム概要
このプログラムは、μT-Kernel 3.0 上で動作し、MIPI-CSI2 カメラ (OV5640) からの入力画像に対して、ルネサス RA8 マイコン内蔵の NPU アクセラレータ「Arm Ethos-U55」を用いて超リアルタイムで画像分類 (Image Classification) を実行するエッジ AI プログラムです。
カメラから取得した 320x240 RGB565 画像を 800x600 に拡大表示するとともに、入力画像を 224x224 RGB888 へ変換し、Ethos-U55 NPU ドライバを介してハードウェアアクセラレーションによる高速な分類推論 (MobileNet V1) を実行します。分類結果の Top-1 カテゴリ名、確信度 (%)、および推論処理時間を液晶画面上にリアルタイムにオーバーレイ表示します。

## ハードウェア／周辺機能
* **マイコン / ボード**: ルネサス RA8 シリーズ (例: EK-RA8D1)
* **アクセラレータ**: Arm Ethos-U55 NPU (ニューラルネットワーク処理用)
* **グラフィックス**: D/AVE 2D エンジン、GLCDC (1024x600 TFT 液晶パネル駆動)
* **カメラ (OV5640)**: MIPI-CSI2 接続によるリアルタイム画像入力 (320x240 分解能)
* **メモリ**: 外部 SDRAM (トリプルフレームバッファ領域、テンソルバッファ領域、モデル領域)
* **制御 I/O**:
  * `MIPI_IF_EN` - MIPI スイッチの有効化ピン
  * `LCD_RST` - 液晶パネルリセットピン
  * `LCD_BLEN` - バックライト有効化ピン

## μT-Kernel 3.0 タスク構成
1. **task_1** (優先度: 10, スタックサイズ: 32KB)
   * メイン描画・カメラ・推論キックタスク。カメラキャプチャの制御、Dave2D による液晶表示、224x224 RGB888 への画像変換 (`image_rgb565_to_rgb888`)、分類結果のオーバーレイ描画、および `tk_wup_tsk(tskid_3)` による AI 推論タスクの起動を行います。
2. **task_2** (優先度: 10, スタックサイズ: 1KB)
   * 動作確認用のダミータスク。7秒ごとに「task 2 running...」を出力します。
3. **tskid_3 (task_3)** (優先度: 11, スタックサイズ: 32KB)
   * AI推論タスク。優先度 11。`task_1` からデータ変換完了の起床シグナルを受けて起動し、Ethos-U55 NPU を用いてハードウェアアクセラレートされた MobileNet V1 推論を実行、分類結果を更新します。

## 処理フロー
1. **初期化処理**: SDRAM 初期化、GLCDC ピン構成、MIPI カメラ起動、Dave2D 初期化、および Ethos-U55 NPU ドライバインスタンスの初期化。
2. **メイン描画・推論起動ループ (task_1)**:
   * Vblank 同期スリープから復帰。
   * Dave2D でカメラ画像 (320x240) を 800x600 に中央拡大コピー。
   * 画面上にモデル名 `Model: MobileNet V1` と NPU 推論時間 (`g_ai_inference_time_ms`) を描画。
   * 推論結果をデコードし、分類結果ラベルと確信度 (%) を画面下部に大きくオーバーレイ表示。
   * キャッシュ操作および液晶バッファ切り替え要求。
   * カメラ画像データを 224x224 RGB888 に変換し `model_buffer_int8` にコピー、キャッシュクリーン後に `tk_wup_tsk(tskid_3)` で推論タスクを起動。
3. **推論処理ループ (task_3)**:
   * 起床待ち。
   * Ethos-U55 NPU を用いて MobileNet V1 の推論を実行。NPU によるアクセラレーションにより、CPU 実行時に比べ推論時間が数ミリ秒に短縮されます。
   * 分類されたカテゴリ確率の上位結果をグローバル配列に格納し、フラグ `g_ai_result_new` を更新。

## 主要なパラメータ・定義
* **モデル**: MobileNet V1 (入力: 224x224x3, int8 量子化, Ethos-U55 用にコンパイル済み Vela モデル)
* `model_buffer_int8`: 推論入力バッファ (224x224 RGB888 用: 150,528 バイト)
* `g_ai_inference_time_ms`: NPU ハードウェアアクセラレータによる推論実行時間 (ms)

## 実行ログ例
```
microT-Kernel Version 3.00

Start User-main program (Camera & LCD D2D Test).
task 2 running...
Testing physical SDRAM at address 0x68000000...
SDRAM verification SUCCESS!

=== Camera MIPI-CSI2 & LCD Display D2D Start ===
=== Setting Bus Slave Arbitration to Fixed Priority (Group 3) ===
Clearing SDRAM framebuffers to black...
Initializing LCD (GLCDC)...
=== Starting Arm Ethos-U55 NPU Task (task 3)... ===
SUCCESS: Arm Ethos-U55 NPU driver initialized successfully!
LCD Backlight enabled.
Initializing D/AVE 2D Graphics Engine...
Initializing MIPI-CSI2 Camera (OV5640)...
[Camera Init] Product ID High: 0x56, Low: 0x40
SUCCESS: Camera initialized and capture started.
Entering Real-time Camera Display Loop...
[VIN CB] First frame captured successfully!
Loop 0: buffer = 0x22074500, vsync_cnt = 15
  Buf Data: 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000
=== NPU Task: Woken up, starting inference... ===
    [MainLoop] Calling mera_invoke()...
    [sub_0000_invoke] Calling ethosu_invoke_v3...
    [sub_0000_invoke] ethosu_invoke_v3 returned: 0
    [MainLoop] mera_invoke() returned.
    [MainLoop] Output pointer: 0x2209C3D0
    [MainLoop] Creating mock TfLiteTensor...
    [MainLoop] Constructing Classifier and ImgClassPostProcess...
    [MainLoop] Calling DoPostProcess()...
    [MainLoop] Calling PresentInferenceResult()...
=== NPU Inference results (Top-5) ===
  Top-1: Category 905 (window shade), Prob: 61%
  Top-2: Category 904 (window screen), Prob: 26%
  Top-3: Category 794 (shower curtain), Prob: 1%
  Top-4: Category 591 (handkerchief), Prob: 1%
  Top-5: Category 753 (radiator), Prob: 0%
====================================
    [MainLoop] main_loop_image_classification COMPLETE!
=== NPU Task: Inference Success! Time: 17 ms ===
```
