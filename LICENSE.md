# SwordigoDesktop Multi-License Notice

**AevoraLabs (prev OpenSwordigo) & Lawncher Team Licensing Framework**

> **Translations & Localisation**: [हिन्दी (Hindi)](.github/.localisation/LICENSE_hi.md) | [Español (Spanish)](.github/.localisation/LICENSE_es.md) | [Français (French)](.github/.localisation/LICENSE_fr.md) | [简体中文 (Chinese)](.github/.localisation/LICENSE_cn.md) | [All 13 Languages...](#translations--localisation)

This repository is an open-source composite project consisting of components licensed under two distinct terms:
1. **GNU General Public License v3.0 (GPLv3)** — Swordigo Runtime Environment (SRE), Swordigo-specific tools, game frontend, and editors.
2. **MIT License** — Generic host infrastructure, Android emulation layers, and JNI bridges.

Original works across this repository are held by **AevoraLabs (prev OpenSwordigo)** (`QuantumCreeper`, `Msinup`, `ManoK`) and **Lawncher Team** (`Raijin`, `Kiziyon`) as detailed below.

---

## 1. GNU General Public License v3.0 (GPLv3)
### Swordigo Runtime Environment (SRE), Game & Editor Components

The following subsystems and directories are licensed under the terms of the **GNU General Public License, Version 3 (GPLv3)**:

- **Swordigo Runtime Environment (SRE)** (`src/sre/`):
  - **`src/sre/sre13/`**: Swordigo 1.4.13 guest runtime, Caver architecture hooks, rbmath Lua math library, console/audio subsystems, and guest engines.
  - **`src/sre/sre12/`**: Swordigo 1.4.12 guest runtime, core hooks, and mini API.
  - **`src/sre/extras/`**: Extended SRE capabilities, FFI interfaces, memory patches, and save file systems.
  - **`src/sre/base/`**: SRE base engine plumbing, custom runtime ABI glue, and platform shims.
  - *Jointly authored & licensed by Lawncher Team (`Raijin`, `Kiziyon`) and AevoraLabs (`QuantumCreeper`, `Msinup`, `ManoK`).*
- **Ruby & Ruby GG IDE Suite** (`src/ruby/`):
  - Qt6 Studio Editor, `Graphy` visual node editor, viewport shaders, lighting and post-processing pipeline, caver visual engine, and integrated tools.
- **Swordfare Launcher & Game Overlay** (`src/launcher/`, `src/platform/`):
  - In-game HUD overlay, mod manager, save editor UI, profile manager, video background player, and runtime frontend.
- **Swordigo Tooling & Converters** (`src/tools/`, `tools/`):
  - SCL/Scene to graph converters, boulder terrain generator, rubymesh formats, glTF bridge, and asset compilers.

**Copyright © 2026 Lawncher Team & AevoraLabs.**
Licensed under GPLv3. See [`src/sre/LICENSE.md`](src/sre/LICENSE.md), [`src/ruby/LICENSE.md`](src/ruby/LICENSE.md), and [`src/platform/LICENSE.md`](src/platform/LICENSE.md).

---

## 2. MIT License
### General Host Infrastructure, Android & JNI Bridges

The generic host environment, portable runtime shims, and low-level emulation plumbing that do not contain game-specific logic are licensed under the permissive **MIT License**:

- **General Android Emulation & JNI Bridges** (`src/jni/`, `src/android/`):
  - POSIX Android shim layers, asset managers, loggers, and JNI marshalling bridges.
- **Binary ELF Loader & Architecture Support** (`src/loader/`, `src/srehost/`):
  - Dynamic ELF loader, symbol relocation tables, and guest-host ABI boundary glue.
- **Generic Engine Platform Helpers** (portions of `src/platform/`):
  - Generic windowing wrappers, timer abstractions, and PVRTC/ASTC image decoders.

**Copyright © 2026 AevoraLabs.**
*(Portions Copyright © 2023 Rinnegatamante — Swordigo Vita Port; Imagination Technologies Ltd. — PVR SDK)*

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

## 3. Third-Party Vendored Components & Licensing History

### Relicensing of SRE
The Lawncher Team and AevoraLabs have jointly updated the Swordigo Runtime Environment (SRE) codebase license to **GNU General Public License v3.0 (GPLv3)** across all submodules: `sre13`, `extras`, `sre12`, and `base`.

### Third-Party Vendored Dependencies
Third-party vendored components retain their respective upstream licenses:
- Standard Lua 5.1, LuaSocket, LuaFileSystem, toml-c, and RakNet in `src/sre/base/` retain their original open-source licenses (MIT, BSD, zlib).
- ufbx (`src/tools/ufbx/`) is dual-licensed MIT / Public Domain.

---

## Summary Matrix

| Directory / Component | License | Exclusivity / Copyright Holders |
| :--- | :--- | :--- |
| `src/sre/` (`sre12`, `sre13`, `extras`, `base`) | **GNU GPLv3** | Jointly held by **Lawncher Team** (`Raijin`, `Kiziyon`) & **AevoraLabs** (`QuantumCreeper`, `Msinup`, `ManoK`) |
| `src/ruby/` (Ruby, Ruby GG Studio IDE) | **GNU GPLv3** | Exclusively **AevoraLabs** |
| `src/launcher/`, `src/platform/` (Swordfare UI) | **GNU GPLv3** | Exclusively **AevoraLabs** |
| `src/tools/`, `tools/` (Converters & Compilers) | **GNU GPLv3** | Exclusively **AevoraLabs** |
| `src/jni/`, `src/android/` (JNI Bridges & Shims) | **MIT** | Exclusively **AevoraLabs** |
| `src/loader/`, `src/srehost/` (Host ELF Loader) | **MIT** | Exclusively **AevoraLabs** |
| Generic Platform Decoders (ASTC, PVRTC) | **MIT** | Upstream authors & **AevoraLabs** |

---

## 4. Community Agreements & Governance Policies

All contributions and usage of online project infrastructure are subject to the following companion agreements:
- **Contributor License Agreement**: See [`.github/CLA.md`](.github/CLA.md) for contribution terms and 50/50 copyright retention rules.
- **Project Governance**: See [`.github/GOVERNANCE.md`](.github/GOVERNANCE.md) for project stewardship and decision-making authority.
- **Terms of Use**: See [`.github/TERMS_OF_USE.md`](.github/TERMS_OF_USE.md) for Online Mod Store and network infrastructure terms of use.
- **Code of Conduct**: See [`.github/CODE_OF_CONDUCT.md`](.github/CODE_OF_CONDUCT.md) for community standards.

---

## Translations & Localisation

This license agreement is officially localized into multiple Indian and international languages under [`.github/.localisation/`](.github/.localisation/):

| Language Family | Language | Localised Title | Document File |
| :--- | :--- | :--- | :--- |
| **Official** | English | Multi-License Notice | [`LICENSE.md`](LICENSE.md) |
| **Indian Languages** | हिन्दी (Hindi) | बहु-लाइसेंस सूचना | [`.github/.localisation/LICENSE_hi.md`](.github/.localisation/LICENSE_hi.md) |
| | বাংলা (Bengali) | বহু-লাইসেন্স বিজ্ঞপ্তি | [`.github/.localisation/LICENSE_bn.md`](.github/.localisation/LICENSE_bn.md) |
| | తెలుగు (Telugu) | బహుళ-లైసెన్స్ ప్రకటన | [`.github/.localisation/LICENSE_te.md`](.github/.localisation/LICENSE_te.md) |
| | தமிழ் (Tamil) | பல உரிம அறிவிப்பு | [`.github/.localisation/LICENSE_ta.md`](.github/.localisation/LICENSE_ta.md) |
| | मराठी (Marathi) | बहु-परवाना सूचना | [`.github/.localisation/LICENSE_mr.md`](.github/.localisation/LICENSE_mr.md) |
| | ગુજરાતી (Gujarati) | મલ્ટી-લાયસન્સ નોટિસ | [`.github/.localisation/LICENSE_gu.md`](.github/.localisation/LICENSE_gu.md) |
| **International Languages** | Español (Spanish) | Aviso de Licencia Múltiple | [`.github/.localisation/LICENSE_es.md`](.github/.localisation/LICENSE_es.md) |
| | Français (French) | Avis de Multi-Licence | [`.github/.localisation/LICENSE_fr.md`](.github/.localisation/LICENSE_fr.md) |
| | 简体中文 (Chinese) | 多重许可声明 | [`.github/.localisation/LICENSE_cn.md`](.github/.localisation/LICENSE_cn.md) |
| | Deutsch (German) | Multi-Lizenz-Hinweis | [`.github/.localisation/LICENSE_de.md`](.github/.localisation/LICENSE_de.md) |
| | 日本語 (Japanese) | マルチライセンス通知 | [`.github/.localisation/LICENSE_ja.md`](.github/.localisation/LICENSE_ja.md) |
| | Русский (Russian) | Уведомление о мульти-лицензировании | [`.github/.localisation/LICENSE_ru.md`](.github/.localisation/LICENSE_ru.md) |
| | Português (Portuguese) | Aviso de Licença Múltipla | [`.github/.localisation/LICENSE_pt.md`](.github/.localisation/LICENSE_pt.md) |
