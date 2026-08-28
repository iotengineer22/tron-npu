# EK-RA8P1 書き込み用バイナリフォルダ (debug)

本フォルダ is、μT-Kernel 3.0 および各種エッジAIプログラムを Renesas EK-RA8P1 評価ボードへ書き込むためのバイナリファイル（`.srec` および `.elf`）を集約・整理したフォルダです。

---

## 1. フォルダ構成

本フォルダは、作業効率化のために以下のように役割別に整理されています。

* **`debug/` 直下 (メイン)**:
  実機での動作デモや審査で使用する、**Arm Ethos-U55 NPUによる高速AI処理**が有効になった代表的な3つのプログラムです。
  * `tron_yolo_face_npu.srec` (YOLO顔検出 NPU高速版)
  * `tron_img_npu.srec` (MobileNet画像分類 NPU高速版)
  * `tron_edge_fomo_npu_type.srec` (FOMO基板部品検出 NPU高速版)
* **`base_firmware/`**:
  液晶描画やカメラ接続テストなど、基本ペリフェラルとRTOSの動作検証用プログラムのSRECです。
* **`cpu_ai_versions/`**:
  NPU高速化の効果を比較するために開発された、**CPU通常処理版**のAIプログラムのSRECです（推論速度の違いを測定するために使用します）。
* **`elf_files/ (参考)`**:
  e2 studioのデバッガ等からロードしてステップ実行などを行う場合に使用する、シンボル情報付きのデバッグ用ELFファイル群です。

---

## 2. 書き込みツールの入手先

書き込みには、ルネサスエレクトロニクス公式の無料書き込みソフトウェア **Renesas Flash Programmer (Programming GUI)** を使用します。

* **ダウンロードページ**:
  [Renesas Flash Programmer (Programming GUI) 公式サイト](https://www.renesas.com/ja/software-tool/renesas-flash-programmer-programming-gui#downloads)
  * 上記リンク先のダウンロードセクションより、最新のインストーラ（Windows版もしくはPCに合った環境）をダウンロードしてPCにインストールしてください。

![RFPダウンロードページ](../img/tron_debug1.png)

---

## 3. 書き込み手順 (プロジェクト作成から書き込みまで)

PCとボードを接続し、プロジェクトを作成してプログラムを書き込むまでの全手順です。

### ① ハードウェアの接続
1. パソコンと EK-RA8P1 ボード上のオンボードデバッガ用USBポート **`DEBUG1`**（micro-USB または USB-C）をUSBケーブルで接続します。
2. ボードの電源が入っている（緑のLEDが点灯している）ことを確認します。

![EK-RA8P1ボード外観](../img/EK-RA8P1.png)

### ② プロジェクトの作成と設定

1. インストールした **Renesas Flash Programmer (RFP)** を起動します。
2. メニューの **[ファイル] ➡ [新しいプロジェクトの作成...]** を選択し、表示されるダイアログを以下のように設定します。

| 設定項目 | 設定値 |
| :--- | :--- |
| **マイクロコントローラ** | **`RA`** を選択 |
| **プロジェクト名** | 任意の名前（例: `RA8P1_Demo`） |
| **作成場所** | 任意のフォルダ（デフォルトのままでOK） |
| **通信ツール** | **`J-Link`** を選択 |
| **インターフェース** | **`SWD`** を選択 |

![新規プロジェクト作成画面](../img/tron_debug2.png)

3. 入力後、右下の **[作成(C)]** をクリックします。これでターゲットボードとの接続が確立されます。

### ③ プログラムファイル（SREC）の選択

1. プロジェクト画面が開いたら、中央の「プログラムファイルとユーザー鍵」枠にある **[ファイルの追加と削除(A)...]** または入力欄の右側のボタンをクリックします。
2. 本 `debug/` フォルダ配下から、書き込みたいプログラムの **`.srec` ファイル**（例: `tron_yolo_face_npu.srec`）を選択してロードします。
   * ※ファイルがロードされると、アドレス範囲やCRC-32値が画面に自動表示されます。

![ファイルロード完了画面](../img/tron_debug3.png)

### ④ [重要] 書き込み前のデバイス初期化（アドレスエラー防止）

YOLOやMobileNetなどの容量の大きいプログラムを書き込む際、マイコン側に残っている古いメモリ境界の仕切り設定と競合し、書き込み開始時に以下のエラーが発生して失敗することがあります。
> `エラー(E1000008): デバイスでアドレスエラーが発生しました。(Command: 13, Response: D2)`

このエラー（安全装置による書き込みブロック）を未然に防ぐため、**ファイルをロードした後、書き込みをスタートする前に、必ず以下の「デバイスの初期化」を実行してください。**

1. RFPの上部メニューバーにある **`ターゲットデバイス(D)`** をクリックします。
2. ドロップダウンメニューから **`デバイスの初期化(I)...`** を選択して実行します。

![デバイスの初期化メニュー選択](../img/tron_debug5.png)

3. 画面右側に緑色で「デバイスの初期化が成功しました」と表示されるのを待ちます。
   * （これによりマイコンの古いメモリ境界設定がクリアされ、新しいプログラムサイズに合わせた書き込みが可能になります）

### ⑤ 書き込みの実行

1. デバイスの初期化が完了したら、画面中央にある大きな **[スタート(S)]** ボタンをクリックします。
2. 自動でデバイス消去 ➡ 書き込み ➡ 照合（ベリファイ）が実行されます。
3. 書き込みが成功すると、画面右側に緑色の背景で **「操作が成功しました」** と表示されます。

![書き込み成功画面](../img/tron_debug4.png)

4. 書き込み完了後、ボード上の `RESET` ボタン（赤いスライドスイッチ付近にある黒い小さなタクトスイッチ）を押すか、USBケーブルを抜き差しすると、新しいプログラムが再起動して動作を開始します。

---

## 4. 代表的なプログラムの概要とデモ動画

書き込み完了後、以下の3つの代表的なプログラムの動作概要と実機デモ動画をご確認いただけます。それぞれオンチップのNPU（Neural Processing Unit）を用いることで、ミリ秒単位の超高速なAI推論処理を実行します。

### ① YOLO顔検出 NPU高速版 (`tron_yolo_face_npu.srec`)
* **動作概略**: カメラのリアルタイム動画像から人物の顔を検し、その位置に緑色のバウンディングボックス（顔枠）を重ね描きします。
* **特徴**: 通常CPU実行で約2秒（2,090ms）かかる重い推論処理を、 Ethos-U55 NPU へオフロードすることで **約16ms** へと劇的に高速化。60Hzの画面更新を阻害せずに滑らかに追従します。
* **実機デモ動画**:
  [![High-speed YOLO Face Detection with Ethos-U55 NPU](https://img.youtube.com/vi/cH7dd1agzxg/hqdefault.jpg)](https://youtu.be/cH7dd1agzxg)
  * [YouTubeリンク (https://youtu.be/cH7dd1agzxg)](https://youtu.be/cH7dd1agzxg)

### ② MobileNet画像分類 NPU高速版 (`tron_img_npu.srec`)
* **動作概略**: カメラに写った物体の特徴を分析し、それが何であるか（マグカップ、キーボードなど）を推論して、判定されたクラス名と確率（%）を画面にリアルタイム表示します。
* **特徴**: 通常CPU実行で約1.5秒（1,512ms）要する推論処理を、NPUによって **約17ms** に短縮し、チラつきのない快適な分類処理を実現しています。
* **実機デモ動画**:
  [![Ethos-U55 NPU Image Processing Demo with RTOS](https://img.youtube.com/vi/FbrsUrJ6Ovw/hqdefault.jpg)](https://youtu.be/FbrsUrJ6Ovw)
  * [YouTubeリンク (https://youtu.be/FbrsUrJ6Ovw)](https://youtu.be/FbrsUrJ6Ovw)

### ③ FOMO基板部品検出 NPU高速版 (`tron_edge_fomo_npu_type.srec`)
* **動作概略**: 電子基板上の極小の部品（Raspberry Pi Pico、Seeed Studio Xiao、nRF54L15など）をリアルタイムに同時識別し、個数と位置をラベル付きで検出・カウントします。
* **特徴**: 通常CPUで 278ms かかる推論を、NPUを用いて **約5ms** へと超高速化。複数オブジェクトの瞬間的なカウント追従を実現しています。
* **実機デモ動画**:
  [![PCB Object Detection using Ethos-U55 NPU](https://img.youtube.com/vi/_uKRamoLaNA/hqdefault.jpg)](https://youtu.be/_uKRamoLaNA)
  * [YouTubeリンク (https://youtu.be/_uKRamoLaNA)](https://youtu.be/_uKRamoLaNA)
