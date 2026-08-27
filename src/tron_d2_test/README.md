[English Version (README_en.md)](README_en.md)

# Dave2D グラフィックス描画テスト (tron_d2_test)

## プログラム概要
このプログラムは、μT-Kernel 3.0 上で動作し、ルネサス RA マイコン搭載の 2D グラフィックスアクセラレータ「D/AVE 2D (Dave2D)」および GLCDC (Global LCD Controller) を用いた描画テストプログラムです。
外部 SDRAM 上にトリプルバッファ用のフレームバッファを確保し、Vblank (垂直同期) 割り込みに完全同期した、チラつき (ティアリング) のない「バウンシングボール (跳ね返る球)」のアニメーションを描画・表示します。また、I2C 通信によるカメラ (OV5640) の接続確認テストおよび物理 SDRAM のセルフテストも同時に実行します。

## ハードウェア／周辺機能
* **マイコン / ボード**: ルネサス RA8 シリーズ (例: EK-RA8D1)
* **グラフィックス**: D/AVE 2D エンジン、GLCDC (1024x600 TFT 液晶パネル駆動)
* **カメラ (OV5640)**: I2C 通信 (スレーブアドレス `0x3C`) による接続・ID 読み出し検証用
* **メモリ**: 外部 SDRAM (フレームバッファ領域、Triple Buffer 構成: 3つのフレームバッファを使用)
* **I/O ピン**:
  * `CAMERA_RESET` (P709) - カメラリセット用ピン
  * `DISP_RESET` (P511など) - 液晶パネルリセット用ピン
  * `DISP_BLEN` (P514) - 液晶バックライト制御ピン

## μT-Kernel 3.0 タスク構成
1. **task_1** (優先度: 10, スタックサイズ: 32KB)
   * メイン処理タスク。周辺ペリフェラル (SDRAM, GLCDC, Dave2D) の初期化、カメラ接続確認、SDRAM セルフテストを実行した後、D/AVE 2D 描画および Vblank 同期制御ループを実行します。
2. **task_2** (優先度: 10, スタックサイズ: 1KB)
   * 動作確認用のダミータスク。7秒ごとに「task 2 running...」を出力します。

## 処理フロー
1. **コールドスタート対策ウェイト**: 電源電圧の安定化のため、タスク起動直後に 500ms 待機。
2. **外部 SDRAM 初期化＆セルフテスト**: SDRAM にテストデータを書き込んで読み戻し、物理メモリの健全性を検証。
3. **AXIバス調停設定**: GLCDC のバス帯域を最優先にするため、AXI バススレーブ調停を「固定優先 (Group 3)」に設定。
4. **カメラ接続検証**: `CAMERA_RESET` トグル後に GPT から 24MHz のカメラクロック (XCLK) を出力し、I2C 経由で OV5640 の Product ID レジスタ (`0x300a`, `0x300b`) を読み出し接続を確認。
5. **液晶パネルリセット＆GLCDC起動**: 液晶パネルの堅牢なハードウェアリセット (500ms High -> 200ms Low -> 500ms High) を実行後、GLCDC および液晶バックライトを有効化。
6. **D/AVE 2D グラフィックスエンジン初期化**: Dave2D HW 初期化およびパラメータ (コピーモード、アンチエイリアス無効など) を設定。
7. **トリプルバッファリング描画ループ**:
   * `d2_startframe` / `d2_endframe` でバウンシングボール、静的円、矩形を描画。
   * キャッシュコヒーレンシの維持 (`SCB_CleanInvalidateDCache`) を行い、`d2_flushframe` で Dave2D の完了をブロック待機。
   * `R_GLCDC_BufferChange` で画面バッファを切り替え要求。
   * GLCDC の Vblank 割り込みを待つため、`tk_slp_tsk` にてタスクをスリープさせ、割り込みコールバック内の `tk_wup_tsk` で即座に起床 (完全同期)。
   * バッファインデックスをローテーション。

## 主要なパラメータ・定義
* `DISPLAY_HSIZE_INPUT0` / `DISPLAY_VSIZE_INPUT0`: 液晶表示サイズ。
* `CAMERA_RESET` (P709), `DISP_RESET` (P511), `DISP_BLEN` (P514): 制御用 GPIO ピン。
* `fb_background` / `fb_background_2`: SDRAM 領域に配置されたフレームバッファ。

## 実行ログ例
```
microT-Kernel Version 3.00

Start User-main program (Camera & LCD D2D Test).
task 2 running...

=== Camera I2C & LCD D2D Connection Test Start ===
Resetting Camera (CAMERA_RESET -> P709)...
Starting GPT Clock for Camera XCLK (g_cam_clk)...
Opening I2C Master (g_cam_i2c_master)...
Reading OV5640 Product ID registers via I2C...
Product ID Read: H = 0x56, L = 0x40
SUCCESS: Camera connection verified! (OV5640 detected)
Testing physical SDRAM at address 0x68000000...
SDRAM verification SUCCESS!
=== Setting Bus Slave Arbitration to Fixed Priority (Group 3) ===
Clearing SDRAM framebuffers to black...
Initializing LCD (GLCDC)...
LCD Backlight enabled.
Initializing D/AVE 2D Graphics Engine...
Starting D/AVE 2D Rendering Loop (SDRAM Triple Buffer Bouncing Ball)...
Loop 0: rendering bouncing ball... (Vblank IRQs: 1)
GLCDC BufferChange Error: 1006
Loop 100: rendering bouncing ball... (Vblank IRQs: 101)
task 2 running...
Loop 200: rendering bouncing ball... (Vblank IRQs: 201)
task 2 running...
Loop 300: rendering bouncing ball... (Vblank IRQs: 301)
Loop 400: rendering bouncing ball... (Vblank IRQs: 401)
task 2 running...
Loop 500: rendering bouncing ball... (Vblank IRQs: 501)
Loop 600: rendering bouncing ball... (Vblank IRQs: 601)
task 2 running...
Loop 700: rendering bouncing ball... (Vblank IRQs: 701)
task 2 running...
```
