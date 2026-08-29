# Software License & Credits Notice

本リポジトリに含まれるプログラムおよび学習モデルには、コンポーネントごとに以下のライセンスが適用されます。

## 1. 独自開発アプリケーションコード (Your Application Code)
本プロジェクトのために独自に開発・改変されたアプリケーション部分（各 src/*/src/ 配下のソースコード等）には、**MIT License** が適用されます。

--------------------------------------------------------------------------
Copyright (c) 2026

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
--------------------------------------------------------------------------


## 2. リアルタイムOS (μT-Kernel 3.0)
`mtk3_bsp2/` ディレクトリ配下に含まれる μT-Kernel 3.0 のソースコードには、TRON Forumが規定する **T-License 2.2** が適用されます。
* **著作権者**: Copyright (C) 2006-2023 by Ken Sakamura.
* **ライセンス詳細**: T-License 2.2 の規定に基づき、ソースコードの改変・再配布が認められています。詳細は各ソースコードヘッダ of ライセンスコメントをご参照ください。


## 3. Board Support Package / FSP (Renesas Electronics)
`ra/` および `ra_gen/` ディレクトリ配下に含まれる自動生成コードおよびルネサスFSPドライバには、**Renesas FSP Software License** が適用されます。
* **著作権者**: Renesas Electronics Corporation
* **ライセンス規定**: ルネサス製マイクロコントローラ上での利用を前提として、無償でのコードの複製、改変、および再配布が認められています。


## 4. Edge Impulse SDK & Machine Learning Libraries (Google / Arm / Edge Impulse)
Edge Impulse からエクスポートされた C++ Inferencing SDK コアコードには **BSD 3-Clause Clear** が適用されます。
なお、TensorFlow Lite Micro および CMSIS ライブラリには、**Apache License 2.0** が適用されます。
* **著作権者**: Google LLC / Arm Limited / Edge Impulse Inc.
* **ライセンス詳細**: 
  - Edge Impulse SDK: BSD 3-Clause Clear License (https://github.com/edgeimpulse/inferencing-sdk-cpp/blob/master/LICENSE)
  - TensorFlow Lite Micro / CMSIS: Apache License 2.0 (http://www.apache.org/licenses/LICENSE-2.0)


## 5. データセット・クレジット (Dataset Attribution)
本プロジェクトの一部AIモデル（FOMO基板部品検出・ICチップ検出）の学習用データセットとして、以下のオープンソースデータセットを利用しています。

* **Roboflow 100 (Printed Circuit Board Dataset)**:
  * **ライセンス**: **CC BY 4.0** (Creative Commons Attribution 4.0 International)
  * **引用元・URL**: [Roboflow Universe - Printed Circuit Board Dataset](https://universe.roboflow.com/roboflow-100/printed-circuit-board)
