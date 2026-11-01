# SwordigoDesktop マルチライセンス通知 (Multi-License Notice)

**AevoraLabs (prev OpenSwordigo) & Lawncher Team ライセンスフレームワーク**

> **英語の原版ドキュメント**: [English (LICENSE.md)](../../LICENSE.md) | [すべての翻訳版 (All Translations)](#translations--localisation)

本リポジトリは、2つの異なる法的条件に基づいてライセンス供与されたコンポーネントから構成されるオープンソースプロジェクトです：
1. **GNU General Public License v3.0 (GPLv3)** — Swordigo ランタイム環境 (SRE)、Swordigo 専用ツール、ゲームフロントエンド、およびエディタ。
2. **MIT License** — 汎用ホストインフラストラクチャ、Android エミュレーションレイヤー、および JNI ブリッジ。

本リポジトリ内のすべてのオリジナル成果物は、**AevoraLabs (prev OpenSwordigo)** (`QuantumCreeper`, `Msinup`, `ManoK`) および **Lawncher Team** (`Raijin`, `Kiziyon`) が共同で権利を保持しています。

---

## 1. GNU General Public License v3.0 (GPLv3)
### Swordigo ランタイム環境 (SRE)、ゲームおよびエディタコンポーネント

以下のサブシステムおよびディレクトリは、**GNU General Public License, Version 3 (GPLv3)** の条件の下でライセンスされています：

- **Swordigo Runtime Environment (SRE)** (`src/sre/`):
  - **`src/sre/sre13/`**: Swordigo 1.4.13 ゲストランタイム、Caver アーキテクチャフック、rbmath Lua 数学ライブラリ、コンソールおよびオーディオサブシステム。
  - **`src/sre/sre12/`**: Swordigo 1.4.12 ゲストランタイム、コアフック、および Mini API。
  - **`src/sre/extras/`**: 拡張 SRE 機能、FFI インターフェース、メモリパッチ、およびセーブファイルシステム。
  - **`src/sre/base/`**: SRE 基本エンジン配管、カスタムランタイム ABI グルー、およびプラットフォームシム。
  - *Lawncher Team (`Raijin`, `Kiziyon`) および AevoraLabs (`QuantumCreeper`, `Msinup`, `ManoK`) による共同開発・ライセンス供与。*
- **Ruby & Ruby GG IDE スイート** (`src/ruby/`):
  - Qt6 Studio エディタ、`Graphy` ビジュアルノードエディタ、ビューポートシェーダー、ライティング＆ポストプロセスパイプライン、Caver ビジュアルエンジン、および統合ツール群。
- **Swordfare ランチャー & ゲームオーバーレイ** (`src/launcher/`, `src/platform/`):
  - ゲーム内 HUD オーバーレイ、Mod マネージャー、セーブデータエディタ UI、プロファイルマネージャー、背景ビデオプレーヤー、およびランタイムフロントエンド。
- **Swordigo ツール & コンバーター** (`src/tools/`, `tools/`):
  - SCL/Scene からグラフへのコンバーター、ボルダー地形ジェネレーター、rubymesh フォーマット、glTF ブリッジ、およびアセットコンパイラー。

**Copyright © 2026 Lawncher Team & AevoraLabs.**
GPLv3 に基づきライセンス供与。詳細は [`src/sre/LICENSE.md`](../../src/sre/LICENSE.md)、[`src/ruby/LICENSE.md`](../../src/ruby/LICENSE.md)、および [`src/platform/LICENSE.md`](../../src/platform/LICENSE.md) を参照してください。

---

## 2. MIT License
### 汎用ホストインフラストラクチャ、Android & JNI ブリッジ

ゲーム固有のロジックを含まない汎用ホスト環境、ポータブルランタイムシム、および低レベルのエミュレーション配管は、許諾型の **MIT License** の下でライセンスされています：

- **汎用 Android エミュレーション & JNI ブリッジ** (`src/jni/`, `src/android/`):
  - POSIX Android シムレイヤー、アセットマネージャー、ロガー、および JNI マーシャリングブリッジ。
- **バイナリ ELF ローダー & アーキテクチャサポート** (`src/loader/`, `src/srehost/`):
  - 動的 ELF ローダー、シンボル再配置テーブル、およびゲスト・ホスト間 ABI 境界グルー。
- **汎用エンプラットフォームヘルパー** (`src/platform/` の一部):
  - 汎用ウィンドウラッパー、タイマー抽象化、および PVRTC/ASTC 画像デコーダー。

**Copyright © 2026 AevoraLabs.**
*(一部の著作権 © 2023 Rinnegatamante — Swordigo Vita Port; Imagination Technologies Ltd. — PVR SDK)*

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

## 3. サードパーティ製コンポーネントとライセンス履歴

### SRE の再ライセンス化
Lawncher Team および AevoraLabs は、Swordigo ランタイム環境 (SRE) コードベースとそのすべてのサブモジュール (`sre13`、`extras`、`sre12`、`base`) を共同で **GNU General Public License v3.0 (GPLv3)** に再ライセンスしました。

### サードパーティ依存関係
- `src/sre/base/` 内のサードパーティオープンソースコンポーネント (Lua 5.1、LuaSocket、LuaFileSystem、toml-c、RakNet など) は、それぞれの元のライセンス (MIT, BSD, zlib) を保持します。
- ufbx (`src/tools/ufbx/`) は MIT / パブリックドメインのデュアルライセンスです。

---

## 要約マトリックス (Summary Matrix)

| ディレクトリ / コンポーネント | ライセンス | 独占権 / 著作権保持者 |
| :--- | :--- | :--- |
| `src/sre/` (`sre12`, `sre13`, `extras`, `base`) | **GNU GPLv3** | 共同保持: **Lawncher Team** & **AevoraLabs** |
| `src/ruby/` (Ruby, Ruby GG Studio IDE) | **GNU GPLv3** | 独占保持: **AevoraLabs** |
| `src/launcher/`, `src/platform/` (Swordfare UI) | **GNU GPLv3** | 独占保持: **AevoraLabs** |
| `src/tools/`, `tools/` (コンバーター & コンパイラー) | **GNU GPLv3** | 独占保持: **AevoraLabs** |
| `src/jni/`, `src/android/` (JNI ブリッジ & シム) | **MIT** | 独占保持: **AevoraLabs** |
| `src/loader/`, `src/srehost/` (ホスト ELF ローダー) | **MIT** | 独占保持: **AevoraLabs** |
| 汎用プラットフォームデコーダー (ASTC, PVRTC) | **MIT** | 原作者 & **AevoraLabs** |

---

## 4. コミュニティ協定とガバナンスポリシー

すべての貢献およびオンラインサービスの利用には、以下の協定が適用されます：
- **コントリビューターライセンス契約 (CLA)**: 貢献条件については [`../CLA.md`](../CLA.md) を参照してください。
- **プロジェクトガバナンスモデル**: 管理体制については [`../GOVERNANCE.md`](../GOVERNANCE.md) を参照してください。
- **利用規約 (Terms of Use)**: Mod ストアの利用規約については [`../TERMS_OF_USE.md`](../TERMS_OF_USE.md) を参照してください。
- **行動規範 (Code of Conduct)**: コミュニティ基準については [`../CODE_OF_CONDUCT.md`](../CODE_OF_CONDUCT.md) を参照してください。

---

## Translations & Localisation

| 言語 (Language) | ドキュメントファイル (Document File) |
| :--- | :--- |
| **English (Official)** | [`../../LICENSE.md`](../../LICENSE.md) |
| **हिन्दी (Hindi)** | [`LICENSE_hi.md`](LICENSE_hi.md) |
| **বাংলা (Bengali)** | [`LICENSE_bn.md`](LICENSE_bn.md) |
| **తెలుగు (Telugu)** | [`LICENSE_te.md`](LICENSE_te.md) |
| **தமிழ் (Tamil)** | [`LICENSE_ta.md`](LICENSE_ta.md) |
| **मराठी (Marathi)** | [`LICENSE_mr.md`](LICENSE_mr.md) |
| **ગુજરાતી (Gujarati)** | [`LICENSE_gu.md`](LICENSE_gu.md) |
| **Español (Spanish)** | [`LICENSE_es.md`](LICENSE_es.md) |
| **Français (French)** | [`LICENSE_fr.md`](LICENSE_fr.md) |
| **简体中文 (Chinese)** | [`LICENSE_cn.md`](LICENSE_cn.md) |
| **Deutsch (German)** | [`LICENSE_de.md`](LICENSE_de.md) |
| **日本語 (Japanese)** | [`LICENSE_ja.md`](LICENSE_ja.md) |
| **Русский (Russian)** | [`LICENSE_ru.md`](LICENSE_ru.md) |
| **Português (Portuguese)** | [`LICENSE_pt.md`](LICENSE_pt.md) |
