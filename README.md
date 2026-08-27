# μT-Kernel 3.0とNPU/GPUによるリアルタイム画像AI認識

本リポジトリは、ルネサスエレクトロニクス製マイコン **EK-RA8P1**（Cortex-M85 / Ethos-U55 NPU / Dave2D GPU 搭載）とリアルタイムOS **μT-Kernel 3.0** を用いた、TRONプログラミングコンテスト2026応募用の開発プロジェクトです。
カメラ入力・画像描画・AI推論を完全に並列化することで、超高速かつチラつきのない表示とリアルタイム制御を実現しています。

---

## 1. システム概要

本システムは、MIPI-CSI2カメラ（OV5640）からリアルタイムに入力される動画像に対して、物体検出（YOLO-Fastest、FOMO）および画像分類（MobileNet V1）などの深層学習モデルによる推論を実行し、ディスプレイ（1024x600 TFT）へリアルタイムに結果を重ね描きして出力するスマートエッジデバイス・アプリケーションです。

### 「TRON×AI」の親和性
組み込みAIにおける最大の課題は、AI推論（重い行列演算）がCPUパワーを長時間占有してしまい、画面表示のガクつき（ジッタ）や、センサー監視の取りこぼしを引き起こす点にあります。
本システムでは、μT-Kernel 3.0 の優先度ベースのマルチタスクスケジューリングと、独立した専用アクセラレータ（NPU/GPU）を組み合わせることで、**「UI/カメラ表示の応答性（60Hz）の完全維持」** と **「推論のバックグラウンド実行（数ms〜数十ms）」** の両立を可能とし、極めて実用的で低遅延なリアルタイムAIエッジアプリケーションを実証しました。

---

## 2. ハードウェア構成

* **MCU / ボード**: ルネサス RA8 シリーズ（EK-RA8P1 / Cortex-M85 480MHz）
* **AIアクセラレータ**: Arm Ethos-U55 NPU（ニューラルネットワーク用）
* **2D GPU**: D/AVE 2D グラフィックスエンジン（画像縮小・ベクトル枠描画用）
* **カメラ**: MIPI-CSI2 接続 OV5640（320x240 RGB565入力）
* **液晶表示**: GLCDC制御 1024x600 TFTカラー液晶パネル
* **外部メモリ**: SDRAM 32MB（トリプルバッファおよびNPUテンソル領域に使用）

| ハードウェア・ブロック構成 | EK-RA8P1 評価ボード外観 |
| :---: | :---: |
| ![Hardware Block Diagram](img/Hardware_Block.png) | ![EK-RA8P1 評価ボード](img/EK-RA8P1.png) |

---

## 3. システムアーキテクチャ

### μT-Kernel 3.0 マルチタスク設計（並列駆動）
本システムでは、描画タスクとAI推論タスクをRTOS上で分離し、以下のようにパイプラインで並列動作させます。

![μT-Kernel 3.0 マルチタスク・データフロー設計](img/task_architecture.png)

* **UI/カメラタスク (`task_ui` / 優先度10)**: 
  カメラ入力画像の取り込み、GLCDC画面切り替え制御、Dave2D (GPU) への描画コマンド発行、顔枠などのバウンディングボックス重ね描きを担当。
* **AI推論タスク (`task_ai` / 優先度11)**: 
  TFLite Micro および Ethos-U55 NPU ドライバを介したAI推論の実行を担当。

### Dave2D GPU と Ethos-U55 NPU の協調動作シーケンス
液晶の表示更新（60Hz）を一切阻害せずにバックグラウンドでAI推論を並行駆動させるため、マイコン内蔵の2D GPU（Dave2D）とNPUを並列にパイプライン制御する以下のシーケンスを実装しています。

![並列パイプライン・シーケンス図](img/parallel_pipeline_architecture.png)

1. **Vblank同期**: UIタスクがVblankの割り込みハンドラから起床 (`tk_wup_tsk`)。
2. **描画コマンド発行**: カメラ画像を取得し、GPUへ「拡大コピー命令」「前フレームの検出枠描画命令」をキューイング。
3. **AIタスク起動**: GPUの処理待ちに入る「前」に、AIタスクへ起床通知 (`tk_wup_tsk(tskid_ai)`)。
4. **ハードウェア並列動作**: AIタスクがNPUに推論を指示し、**「GPUが描画を実行している」のと同時に「NPUが推論を実行している」状態**が生まれます。
5. **Vsyncフリップ**: GPUの完了を待ち (`d2_flushframe`)、液晶バッファを反転（フリップ）してタスクは次のフレームまでスリープします。

これにより、CPU負荷を最小に抑えたまま、60Hzでのチラつきのない滑らかなカメラ映像と、ミリ秒オーダーの超リアルタイムなAI検出表示を完全に並列化しています。

### CPUからNPUへのAI推論高速化（リアルタイム性能と決定論的制御）
本プロジェクトでは、NPUによる処理能力を客観的に評価するため、**NPUアクセラレータを使用せずにCortex-M85 CPU単体（TFLite Micro CPU実行）で推論を行う「CPU版プログラム」も並行して開発・ビルドし、実機上での詳細なベンチマーク測定を実施しました。**

これにより、重いAI推論処理をマイコン内蔵の専用アクセラレータ（NPU）へオフロードすることで、CPU単体での演算実行時に比べて圧倒的なリアルタイム性能向上を達成していることを証明しました。NPUへの処理委託によってCPU負荷がほぼゼロになり、RTOSのスレッドスケジュール機能が活き、決定論的なリアルタイム制御を極めて容易に実現できます。

* **実機測定によるNPU高速化ベンチマーク効果 (CPU実行 vs NPU実行)**:
  * **MobileNet V1 画像分類**: CPU上で約1.5秒（1,512ms）かかっていた推論を **約 17 ms（約88.9倍の高速化）** に短縮。
  * **YOLO顔検出**: CPU上で約2秒（2,090ms）要していた推論時間を、NPUを用いることで **約 16 ms（約130.6倍の高速化）** に短縮。
  * **FOMO部品検出**: CPU上の 278 ms から **約 5 ms（約55.6倍の高速化）** へと短縮。

![CPU vs NPU 性能比較グラフ](img/cpu_vs_npu_comparison.png)

---

## 4. 収録プログラム一覧 (src 配下)

### 4-1. ファームウェア (基礎ペリフェラル・RTOS検証)
RTOSマルチタスクの基本スケジューリング、シリアル出力、I2C接続、Dave2Dによる基本描画、カメラと液晶パネル의 ダイレクト接続テストを検証した、システムの土台となるプログラム群です。

| フォルダ名 | アプリケーションの役割 | 使用エンジン (AI / 描画) |
| :--- | :--- | :--- |
| **[tron_serial_test](src/tron_serial_test)** | T-Monitorシリアル並行出力検証 | なし (シリアル通信のみ) |
| **[tron_i2c_test](src/tron_i2c_test)** | カメラ接続検証テストプログラム | なし (シリアル診断のみ) |
| **[tron_d2_test](src/tron_d2_test)** | 液晶描画およびSDRAM物理テスト | なし / Dave2D |
| **[tron_mipi_test_ori](src/tron_mipi_test_ori)** | カメラ表示検証プログラム（初期版） | なし / Dave2D |

**デモ動画:**
| Fast 2D Graphics Rendering on EK-RA8P1 with RTOS | Real-time MIPI Camera Stream to LCD on EK-RA8P1 |
| :---: | :---: |
| [![Fast 2D Graphics Rendering](https://img.youtube.com/vi/kwVPgD5SHRA/hqdefault.jpg)](https://youtu.be/kwVPgD5SHRA)<br>[YouTubeリンク (https://youtu.be/kwVPgD5SHRA)](https://youtu.be/kwVPgD5SHRA) | [![Real-time MIPI Camera Stream](https://img.youtube.com/vi/Kv0S4wUMbmw/hqdefault.jpg)](https://youtu.be/Kv0S4wUMbmw)<br>[YouTubeリンク (https://youtu.be/Kv0S4wUMbmw)](https://youtu.be/Kv0S4wUMbmw) |

### 4-2. 画像分類 MobileNet V1
入力画像のサイズ変換を行い、ニューラルネットワーク（MobileNet V1）を用いて写っている物体のカテゴリを分類するAIプログラム群です。

| フォルダ名 | アプリケーションの役割 | 使用エンジン (AI / 描画) |
| :--- | :--- | :--- |
| **[tron_img_cpu](src/tron_img_cpu)** | MobileNet V1 画像分類 CPU版 | TensorFlow Lite Micro (CPU) / Dave2D |
| **[tron_img_npu](src/tron_img_npu)** | MobileNet V1 画像分類 NPU版 | **Arm Ethos-U55 NPU** / Dave2D |

**デモ動画:**
[![Ethos-U55 NPU Image Processing Demo with RTOS](https://img.youtube.com/vi/FbrsUrJ6Ovw/hqdefault.jpg)](https://youtu.be/FbrsUrJ6Ovw)
* [YouTubeリンク: Ethos-U55 NPU Image Processing Demo with RTOS](https://youtu.be/FbrsUrJ6Ovw)

### 4-3. YOLO顔検出
カメラのリアルタイム画像から人物の顔を認識し、その座標に緑色の検出枠を重ねて表示する物体検出プログラム群です。

| フォルダ名 | アプリケーションの役割 | 使用エンジン (AI / 描画) |
| :--- | :--- | :--- |
| **[tron_yolo_face_cpu](src/tron_yolo_face_cpu)** | YOLO顔検出 CPU版 | TensorFlow Lite Micro (CPU) / Dave2D |
| **[tron_yolo_face_npu](src/tron_yolo_face_npu)** | YOLO顔検出 NPU高速版 | **Arm Ethos-U55 NPU** / Dave2D |

**デモ動画:**
[![High-speed YOLO Face Detection with Ethos-U55 NPU](https://img.youtube.com/vi/cH7dd1agzxg/hqdefault.jpg)](https://youtu.be/cH7dd1agzxg)
* [YouTubeリンク: High-speed YOLO Face Detection with Ethos-U55 NPU](https://youtu.be/cH7dd1agzxg)

### 4-4. PCB部品検出 (FOMO)
基板上の極小の電子部品（Pico、Xiao、nRF54L15など）やICチップなどの特定オブジェクトをリアルタイムに検出し、カウントする高精度・軽量物体検出AIプログラム群です。

| フォルダ名 | アプリケーションの役割 | 使用エンジン (AI / 描画) |
| :--- | :--- | :--- |
| **[tron_edge_fomo_cpu_type](src/tron_edge_fomo_cpu_type)** | PCB部品検出（CPU版） | TensorFlow Lite Micro (CPU) / Dave2D |
| **[tron_edge_fomo_npu_type](src/tron_edge_fomo_npu_type)** | PCB部品検出 NPU高速版 | **Arm Ethos-U55 NPU** / Dave2D |
| **[tron_edge_fomo_ic](src/tron_edge_fomo_ic)** (参考) | ICチップ検出 NPU高速版（参考） | **Arm Ethos-U55 NPU** / Dave2D |

**デモ動画:**
[![PCB Object Detection using Ethos-U55 NPU](https://img.youtube.com/vi/_uKRamoLaNA/hqdefault.jpg)](https://youtu.be/_uKRamoLaNA)
* [YouTubeリンク: PCB Object Detection using Ethos-U55 NPU](https://youtu.be/_uKRamoLaNA)

---

## 5. 各プログラムのキーポイントと技術的アピール

### 1. ファームウェア層（基礎ペリフェラル・RTOS検証）
* **対象フォルダ**: [tron_d2_test](src/tron_d2_test) / [tron_mipi_test_ori](src/tron_mipi_test_ori)
- **技術概要**:
  マイコン内蔵の2D GPU（Dave2D）やGLCDC（液晶表示コントローラ）、MIPI-CSI2カメラモジュールといった高度な周辺機能を μT-Kernel 3.0 上で動作検証し、基盤となる協調表示機構を構築。
- **コードにおける重要ポイント**:
  - `draw_buf`, `pending_buf`, `display_buf` の3つを用意する**トリプルバッファローテーション制御**を `usermain.cpp` に実装。GLCDCのスキャンアウト割り込みをフックし、`tk_slp_tsk` / `tk_wup_tsk` を介した同期起床スリープにより、16.6msごとのVblank期間内での完全同期切り替えを実現。
  - D/AVE 2Dによるバイリニア（双線形）補間拡大コピー命令をドライバ経由でGPUへオフロードし、320x240のカメラ入力をCPU負荷ほぼゼロで 800x600 へ拡大描画。

### 2. 画像分類（MobileNet V1）
* **対象フォルダ**: [tron_img_cpu](src/tron_img_cpu) / [tron_img_npu](src/tron_img_npu)
- **技術概要**:
  TensorFlow Lite Micro (TFLite Micro) を μT-Kernel 3.0 タスクとして実行させ、MobileNet V1 モデルを用いた実世界物体のリアルタイム分類を実現。
- **コードにおける重要ポイント**:
  - `image_rgb565_to_rgb888` によるカメラ画像から推論用（224x224 RGB888）データへのCPUによる高速フォーマット変換コード。
  - TFLite Micro の推論エンジンをバックグラウンドNPU（Ethos-U55）に接続するための、`RM_ETHOSU_Open` によるNPUドライバ初期化処理と、キャッシュ同期のための DCache Invalidate/Clean 命令の厳密な呼び出しタイミング制御。

### 3. YOLO顔検出
* **対象フォルダ**: [tron_yolo_face_cpu](src/tron_yolo_face_cpu) / [tron_yolo_face_npu](src/tron_yolo_face_npu)
- **技術概要**:
  Cortex-M85 CPU のみでは推論に約2.09秒を要していた YOLO モデルを、Ethos-U55 NPUアクセラレータ上での実行へと移行。推論時間をミリ秒オーダー（約16ms）へ圧縮し、1秒間に60回描画を崩さず実機上で非同期に顔枠の座標追従を行うことに成功。
- **コードにおける重要ポイント**:
  - `task_ui` (描画・カメラ) と `task_ai` (推論・優先度11) を非同期かつ安全にオーバーラップさせるための、排他制御変数 `g_ai_task_busy` による**AI推論自動フレームスキップ機構**。
  - 推論完了時に得られる量子化されたバウンディングボックス座標（`int8` 型）を実画面上のピクセル座標に逆量子化するポストプロセス関数（`yolo_face_postprocess`）の最適化。

### 4. PCB部品検出 (FOMO)
* **対象フォルダ**: [tron_edge_fomo_cpu_type](src/tron_edge_fomo_cpu_type) / [tron_edge_fomo_npu_type](src/tron_edge_fomo_npu_type) / [tron_edge_fomo_ic](src/tron_edge_fomo_ic)
- **技術概要**:
  基板上の極小のチップ部品やICなどの複数オブジェクトをリアルタイムに同時識別し、その数と位置を検出する Edge Impulse FOMO モデルを Ethos-U55 NPU 上で動作検証。
- **コードにおける重要ポイント**:
  - グリッドセルベースの検出モデル（FOMO）の出力テンソルから、ピーク確信度を持つセルを高速に抽出して座標にマッピングするポストプロセッサ（`fomo_postprocess`）の実装。
  - キャッシュライン幅（32バイト）に合わせた `BSP_ALIGN_VARIABLE(32)` マクロによるテンソルメモリ領域の静的アライメント定義により、キャッシュ無効化による隣接メモリ汚染を回避。

---

## 6. 動作確認方法と開発環境
* **統合開発環境**: e2 studio (Renesas) / FSP v6.5.0
* **リアルタイムOS**: μT-Kernel 3.0
* **実行環境**: EK-RA8P1 評価ボード
* **ビルド方法**: 各プログラムフォルダ内の `build.bat` を実行、または e2 studio 上でインポートしてビルドします。シリアルコンソール（115200 bps）を接続し、起動時の接続確認・推論結果ログを確認できます。
