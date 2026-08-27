[English Version (README_en.md)](README_en.md)

# MIPI カメラ表示テスト 初期版 (tron_mipi_test_ori)

## プログラム概要
このプログラムは、μT-Kernel 3.0 上で動作し、MIPI-CSI2 接続された CMOS イメージセンサーカメラ (OV5640) からの画像入力を行い、グラフィックディスプレイにリアルタイムで表示するカメラ表示テストプログラムのオリジナル/初期バージョンです。
カメラからキャプチャした 320x240 RGB565 画像データを D/AVE 2D エンジンによって 800x600 の解像度に拡大（バイリニアフィルタ）し、液晶画面の中央に描画します。Vblank (垂直同期) 割り込みに同期したトリプルバッファフリップを実行することで、チラつきのない滑らかな表示を行います。

## ハードウェア／周辺機能
* **マイコン / ボード**: ルネサス RA8 シリーズ (例: EK-RA8D1)
* **グラフィックス**: D/AVE 2D エンジン、GLCDC (1024x600 TFT 液晶パネル駆動)
* **カメラ (OV5640)**: MIPI-CSI2 接続によるリアルタイム画像入力 (320x240 分解能)
* **メモリ**: 外部 SDRAM (トリプルフレームバッファ領域)
* **制御 I/O**:
  * `MIPI_IF_EN` - MIPI スイッチの有効化ピン (マルチプレクサをカメラ側に接続)
  * `LCD_RST` - 液晶パネルリセットピン
  * `LCD_BLEN` - バックライト有効化ピン

## μT-Kernel 3.0 タスク構成
1. **task_1** (優先度: 10, スタックサイズ: 32KB)
   * カメラ画像表示のメインループ。SDRAM・GLCDC・MIPIカメラ・Dave2Dの初期化後に、カメラ画像をキャプチャし、液晶画面への拡大転送描画と Vblank 割り込み待ち (`tk_slp_tsk`) による同期フリップを実行します。
2. **task_2** (優先度: 10, スタックサイズ: 1KB)
   * 動作確認用のダミータスク。7秒周期で動作メッセージをコンソールに出力します。

## 処理フロー
1. **初期設定**: 外部 SDRAM 初期化、GLCDC ピン構成、MIPI カメラリセットと初期化、Dave2D ドライバのオープン・初期化。
2. **リアルタイム表示ループ**:
   * Vblank 割り込みを検知するまでスリープ待機 (CPU負荷0%)。
   * カメラの入力バッファポインタ `p_camera_capture_buffer_stored` が有効であることを確認。
   * `d2_startframe` を呼び出し描画を開始。
   * Dave2D の `d2_blitcopy` を用い、バイリニアフィルタ (`d2_tm_filter`) を有効にして 320x240 画像を 800x600 に拡大コピー、X=112 のオフセットを加えて液晶の中央に配置。
   * コマンドリストのフラッシュと GPU の完了待ち。
   * バッファを切り替え要求 (`R_GLCDC_BufferChange`)。
   * バッファインデックスをローテーション。

## 主要なパラメータ・定義
* `DISPLAY_HSIZE_INPUT0` / `DISPLAY_VSIZE_INPUT0`: LCD の表示解像度。
* `fb_background` / `fb_background_2`: フレームバッファ領域。

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
LCD Backlight enabled.
Initializing D/AVE 2D Graphics Engine...
Initializing MIPI-CSI2 Camera (OV5640)...
[Camera Init] Product ID High: 0x56, Low: 0x40
SUCCESS: Camera initialized and capture started.
Entering Real-time Camera Display Loop...
[VIN CB] First frame captured successfully!
Loop 0: buffer = 0x22053580, vsync_cnt = 15
  Buf Data: 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000 0x0000
task 2 running...
Loop 100: buffer = 0x22008580, vsync_cnt = 274
  Buf Data: 0x18C3 0x18C4 0x08A6 0x08C5 0x08A5 0x10C5 0x10C5 0x08C4
task 2 running...
task 2 running...
Loop 200: buffer = 0x2202DD80, vsync_cnt = 533
  Buf Data: 0x10A4 0x18E4 0x10C6 0x08A6 0x18C5 0x10A5 0x18C5 0x20A5
```
