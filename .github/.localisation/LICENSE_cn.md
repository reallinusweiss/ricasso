# SwordigoDesktop 多重许可声明 (Multi-License Notice)

**AevoraLabs (prev OpenSwordigo) 与 Lawncher Team 许可框架 (Licensing Framework)**

> **英文原版文件**: [English (LICENSE.md)](../../LICENSE.md) | [所有翻译版本 (All Translations)](#translations--localisation)

本代码仓库是一个复合型开源项目，各组件分别遵循两类不同的许可条款：
1. **GNU 通用公共许可证第 3 版 (GPLv3)** — Swordigo 运行时环境 (SRE)、Swordigo 专用工具、游戏前端和关卡编辑器。
2. **MIT 许可证** — 通用宿主基础设施、Android 模拟层和 JNI 桥接。

本仓库内的原创作品归 **AevoraLabs (prev OpenSwordigo)** (`QuantumCreeper`, `Msinup`, `ManoK`) 与 **Lawncher Team** (`Raijin`, `Kiziyon`) 共同持有，详见下文。

---

## 1. GNU 通用公共许可证第 3 版 (GPLv3)
### Swordigo 运行时环境 (SRE)、专用游戏与编辑器组件

以下子系统及目录在 **GNU General Public License, Version 3 (GPLv3)** 条款下获得许可：

- **Swordigo 运行时环境 (SRE)** (`src/sre/`)：
  - **`src/sre/sre13/`**：Swordigo 1.4.13 客体运行时、Caver 架构挂钩、rbmath Lua 数学库和控制台/音频子系统。
  - **`src/sre/sre12/`**：Swordigo 1.4.12 客体运行时、核心挂钩和 Mini API。
  - **`src/sre/extras/`**：扩展 SRE 模块、FFI 接口、内存补丁和存档文件系统。
  - **`src/sre/base/`**：SRE 基础引擎、自定义运行时 ABI 胶水与平台兼容垫片。
  - *由 Lawncher Team (`Raijin`, `Kiziyon`) 与 AevoraLabs (`QuantumCreeper`, `Msinup`, `ManoK`) 共同开发并授权许可。*
- **Ruby 与 Ruby GG IDE 套件** (`src/ruby/`)：
  - Qt6 Studio 编辑器、`Graphy` 可视化节点编辑器、视口着色器、光照与后处理管线、Caver 视觉引擎及集成工具。
- **Swordfare 启动器与游戏内悬浮窗 (Overlay)** (`src/launcher/`, `src/platform/`)：
  - 游戏内 HUD 悬浮层、模组管理器 (Mod Manager)、存档编辑器 UI、配置文件管理器、视频背景播放器以及运行时前端。
- **Swordigo 工具链与转换器** (`src/tools/`, `tools/`)：
  - SCL/Scene 转图表转换器、boulder 地形生成器、rubymesh 格式、glTF 桥接器及资产编译器。

**版权所有 © 2026 Lawncher Team & AevoraLabs。**
基于 GPLv3 许可。详见 [`src/sre/LICENSE.md`](../../src/sre/LICENSE.md)、[`src/ruby/LICENSE.md`](../../src/ruby/LICENSE.md) 和 [`src/platform/LICENSE.md`](../../src/platform/LICENSE.md)。

---

## 2. MIT 许可证
### 通用宿主基础设施、Android 与 JNI 桥接

不包含游戏专用逻辑的通用宿主环境、可移植运行时垫片以及底层模拟桥接采用宽松的 **MIT 许可证**：

- **通用 Android 模拟与 JNI 桥接** (`src/jni/`, `src/android/`)：
  - POSIX Android 兼容层、资产管理器、日志记录器和 JNI 封送处理桥接。
- **二进制 ELF 加载器与架构支持** (`src/loader/`, `src/srehost/`)：
  - 动态 ELF 加载器、符号重定位表以及客体-宿体 ABI 边界胶水层。
- **通用引擎平台辅助模块** (`src/platform/` 的部分组件)：
  - 通用窗口包装器、定时器抽象及 PVRTC/ASTC 图像解码器。

**版权所有 © 2026 AevoraLabs。**
*(部分版权 © 2023 Rinnegatamante — Swordigo Vita Port；Imagination Technologies Ltd. — PVR SDK)*

特此免费授予任何获得本软件副本和相关文档文件（“软件”）的人无限制地处理本软件的许可，包括但不限于使用、复制、修改、合并、发布、分发、再许可和/或出售软件副本的权利，并允许向其提供软件的人在满足以下条件的情况下这样做：

上述版权声明和本许可声明应包含在软件的所有副本或重要部分中。

本软件按“原样”提供，不提供任何形式的明示或暗示保证，包括但不限于对适销性、特定用途适用性和非侵权性的保证。在任何情况下，作者或版权所有者均不对任何索赔、损害或其他责任负责，无论是在合同诉讼、侵权诉讼还是其他诉讼中，由软件或软件的使用或其他交易引起或与之相关。

---

## 3. 第三方组件与许可历史

### SRE 代码库重新许可
Lawncher Team 与 AevoraLabs 已共同将 Swordigo 运行时环境 (SRE) 及其所有子模块（`sre13`、`extras`、`sre12` 和 `base`）的代码库许可证全面更新为 **GNU General Public License v3.0 (GPLv3)**。

### 第三方集成依赖项
第三方集成组件保留其原始开源许可：
- `src/sre/base/` 中的上游依赖项（如 Lua 5.1、LuaSocket、LuaFileSystem、toml-c 和 RakNet）保留其原始许可（MIT, BSD, zlib）。
- ufbx (`src/tools/ufbx/`) 遵循 MIT / Public Domain 双重许可。

---

## 许可总览矩阵 (Summary Matrix)

| 目录 / 组件 | 许可证 | 专有性 / 版权所有者 |
| :--- | :--- | :--- |
| `src/sre/` (`sre12`, `sre13`, `extras`, `base`) | **GNU GPLv3** | 由 **Lawncher Team** (`Raijin`, `Kiziyon`) 与 **AevoraLabs** (`QuantumCreeper`, `Msinup`, `ManoK`) 共同持有 |
| `src/ruby/` (Ruby, Ruby GG Studio IDE) | **GNU GPLv3** | 独家归 **AevoraLabs** 所有 |
| `src/launcher/`, `src/platform/` (Swordfare UI) | **GNU GPLv3** | 独家归 **AevoraLabs** 所有 |
| `src/tools/`, `tools/` (转换器与编译器) | **GNU GPLv3** | 独家归 **AevoraLabs** 所有 |
| `src/jni/`, `src/android/` (JNI 桥接与垫片) | **MIT** | 独家归 **AevoraLabs** 所有 |
| `src/loader/`, `src/srehost/` (宿主 ELF 加载器) | **MIT** | 独家归 **AevoraLabs** 所有 |
| 通用平台解码器 (ASTC, PVRTC) | **MIT** | 上游作者 & **AevoraLabs** |

---

## 4. 社区协议与治理政策

所有向本项目提交的贡献以及对在线基础设施的使用均须遵守以下补充协议：
- **贡献者许可协议 (CLA)**：有关贡献条款及 50/50 版权保留规则，请参见 [`../CLA.md`](../CLA.md)。
- **项目治理模型**：有关管理架构与决策权，请参见 [`../GOVERNANCE.md`](../GOVERNANCE.md)。
- **使用条款**：有关在线模组商店及网络服务使用规则，请参见 [`../TERMS_OF_USE.md`](../TERMS_OF_USE.md)。
- **行为准则**：有关社区行为规范，请参见 [`../CODE_OF_CONDUCT.md`](../CODE_OF_CONDUCT.md)。

---

## Translations & Localisation

| 语言 (Language) | 翻译文档 (Document File) |
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
