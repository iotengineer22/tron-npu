[English Version (README_en.md)](README_en.md)

# シリアル通信テスト (tron_serial_test)

## プログラム概要
このプログラムは、μT-Kernel 3.0 上で動作する、基本的なシリアル出力検証用プログラムです。
μT-Kernel 内蔵の T-Monitor 関数 (`tm_printf`, `tm_putstring`) を用いて、独立した2つのスレッド (タスク) から異なる周期で文字列をシリアルデバッグポートに出力し、マルチタスク動作とシリアル通信経路が正常に機能しているかをテストします。

## ハードウェア／周辺機能
* **マイコン / ボード**: ルネサス RA シリーズ (例: EK-RA8D1 等)
* **通信ポート**: デバッグ用シリアルポート (UART / T-Monitor インターフェース)

## μT-Kernel 3.0 タスク構成
1. **task_1** (優先度: 10, スタックサイズ: 1KB)
   * シリアル出力タスク 1。500ms 周期で「task 1」を出力し続けます。
2. **task_2** (優先度: 10, スタックサイズ: 1KB)
   * シリアル出力タスク 2。700ms 周期で「task 2」を出力し続けます。

## 処理フロー
1. **起動メッセージ送信**: T-Monitor の `tm_putstring` を用いて、usermain から "Start User-main program." を送信。
2. **タスクの作成・起動**: `tk_cre_tsk` および `tk_sta_tsk` により、2つの同一優先度タスク (task_1, task_2) を作成して起動。
3. **並行シリアル出力処理**:
   * `task_1` が 500ms ごとに起床して `tm_printf` により文字列を出力。
   * `task_2` が 700ms ごとに起床して `tm_printf` により文字列を出力。
   * 双方のタスクは `tk_dly_tsk` により周期制御され、CPU リソースを共有して動作します。

## 主要なパラメータ・定義
* `ctsk_1` / `ctsk_2`: タスク生成用の属性構造体 (`T_CTSK`)。未使用のスタックバッファポインタ `bufptr` に `NULL` を明示設定することでコンパイラの警告を防いでいます。

## 実行ログ例
```text
microT-Kernel Version 3.00

Start User-main program.
task 1
task 2
task 1
task 2
task 1
task 2
task 1
```
