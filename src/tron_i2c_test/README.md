[English Version (README_en.md)](README_en.md)

# I2C カメラ接続検証テスト (tron_i2c_test)

## プログラム概要
このプログラムは、μT-Kernel 3.0 上で動作し、RA マイコンに接続されたカメラモジュール (OV5640) の I2C 通信検証を行うテストプログラムです。
カメラのハードウェアリセットをトグルし、GPT (General Purpose Timer) クロックを用いてカメラに動作クロックを提供した後、I2C マスタドライバを介して OV5640 の Product ID レジスタの内容を読み出し、正しくカメラが接続・認識されているかを確認します。

## ハードウェア／周辺機能
* **マイコン / ボード**: ルネサス RA8 シリーズ (例: EK-RA8D1)
* **カメラ (OV5640)**: I2C (IICチャネル1) 通信 (スレーブアドレス `0x3C`)
* **GPT (タイマー)**: P501 ピンからカメラ用 XCLK (24MHz クロック) を出力
* **制御 I/O**:
  * `CAMERA_RESET` (P709) - カメラのハードウェアリセット制御ピン

## μT-Kernel 3.0 タスク構成
1. **task_1** (優先度: 10, スタックサイズ: 2KB)
   * I2C テストメインタスク。リセット制御、カメラ用クロック起動、I2C オープンと OV5640 Product ID レジスタ (`0x300a`, `0x300b`) の読み出しテストを実行します。成否にかかわらず、その後は 10 秒周期でループ処理を行います。
2. **task_2** (優先度: 10, スタックサイズ: 1KB)
   * 動作確認用のダミータスク。7 秒周期で動作メッセージをコンソールに出力します。

## 処理フロー
1. **カメラリセットの実行**: `CAMERA_RESET` ピンを Low にして 100ms 待機し、High に戻して 10ms 待機 (ハードウェア初期化)。
2. **XCLK (カメラクロック) 出力開始**: GPT (g_cam_clk) をオープンし、P501 ピンに 24MHz のクロック出力を開始。
3. **I2C マスタドライバ起動**: I2C (g_cam_i2c_master) をオープンし、スレーブアドレスを 7ビットアドレス `0x3C` (OV5640のアドレス) に設定。
4. **Product ID の検証**:
   * レジスタ `0x300A` から高位バイト、`0x300B` から低位バイトを読み出します。
   * 読み出しに成功し、Product ID が `0x5640` (あるいは `0x5641`, `0x564C`) であるかを比較します。
   * 接続確認結果をコンソールに出力します。
5. **周期待機ループ**: 以降、10 秒ごとにタスクが起床して待機を繰り返します。

## 主要なパラメータ・定義
* `CAMERA_RESET` (P709): カメラリセットピン。
* スレーブアドレス: `0x3C` (OV5640)
* 検証レジスタ:
  * Product ID High: `0x300a` (期待値: `0x56`)
  * Product ID Low: `0x300b` (期待値: `0x40`, `0x41` または `0x4C`)

## 実行ログ例
```text
=== Camera I2C Connection Test Start ===
Resetting Camera (CAMERA_RESET -> P709)...
task 2 running...
Starting GPT Clock for Camera XCLK (g_cam_clk)...
Opening I2C Master (g_cam_i2c_master)...
Reading OV5640 Product ID registers via I2C...
Product ID Read: H = 0x56, L = 0x40
SUCCESS: Camera connection verified! (OV5640 detected)
task 1 heart beat...
task 1 heart beat...
task 2 running...
```
