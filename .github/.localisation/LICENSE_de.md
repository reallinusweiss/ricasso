# Multi-Lizenz-Hinweis für SwordigoDesktop (Multi-License Notice)

**Lizenzierungsrahmen von AevoraLabs (prev OpenSwordigo) und dem Lawncher Team**

> **Englisches Originaldokument**: [English (LICENSE.md)](../../LICENSE.md) | [Alle Übersetzungen (All Translations)](#translations--localisation)

Dieses Repository ist ein zusammengesetztes Open-Source-Projekt, dessen Komponenten unter zwei unterschiedlichen rechtlichen Bedingungen lizenziert sind:
1. **GNU General Public License v3.0 (GPLv3)** — Swordigo Runtime Environment (SRE), Swordigo-spezifische Tools, Spielfrontend und Editoren.
2. **MIT-Lizenz** — Generische Host-Infrastruktur, Android-Emulationsschichten und JNI-Bridges.

Alle Originalwerke in diesem Repository sind gemeinschaftlich im Besitz von **AevoraLabs (prev OpenSwordigo)** (`QuantumCreeper`, `Msinup`, `ManoK`) und dem **Lawncher Team** (`Raijin`, `Kiziyon`).

---

## 1. GNU General Public License v3.0 (GPLv3)
### Swordigo Runtime Environment (SRE), Spiel- und Editor-Komponenten

Die folgenden Subsysteme und Verzeichnisse sind unter den Bedingungen der **GNU General Public License, Version 3 (GPLv3)** lizenziert:

- **Swordigo Runtime Environment (SRE)** (`src/sre/`):
  - **`src/sre/sre13/`**: Swordigo 1.4.13 Gast-Laufzeitumgebung, Caver-Architektur-Hooks, rbmath Lua-Mathematikbibliothek sowie Konsolen- und Audio-Subsysteme.
  - **`src/sre/sre12/`**: Swordigo 1.4.12 Gast-Laufzeitumgebung, Core-Hooks und Mini-API.
  - **`src/sre/extras/`**: Erweiterte SRE-Funktionen, FFI-Schnittstellen, Speicher-Patches und Speicherdateisysteme.
  - **`src/sre/base/`**: SRE-Basiseingebettete Schnittstellen, benutzerdefinierte Laufzeit-ABI-Verbindungen und Plattform-Shims.
  - *Gemeinsam entwickelt und lizenziert vom Lawncher Team (`Raijin`, `Kiziyon`) und AevoraLabs (`QuantumCreeper`, `Msinup`, `ManoK`).*
- **Ruby & Ruby GG IDE Suite** (`src/ruby/`):
  - Qt6 Studio Editor, visueller Node-Editor `Graphy`, Viewport-Shader, Beleuchtungs- und Post-Processing-Pipeline, Caver-Visual-Engine und integrierte Tools.
- **Swordfare Launcher & Spiel-Overlay** (`src/launcher/`, `src/platform/`):
  - In-Game-HUD-Overlay, Mod-Manager, Spielstand-Editor-UI, Profil-Manager, Hintergrund-Videoplayer und Laufzeit-Frontend.
- **Swordigo Tools & Konverter** (`src/tools/`, `tools/`):
  - SCL/Scene-zu-Graph-Konverter, Boulder-Terrain-Generator, Rubymesh-Formate, glTF-Bridge und Asset-Compiler.

**Copyright © 2026 Lawncher Team & AevoraLabs.**
Lizenziert unter GPLv3. Siehe [`src/sre/LICENSE.md`](../../src/sre/LICENSE.md), [`src/ruby/LICENSE.md`](../../src/ruby/LICENSE.md) und [`src/platform/LICENSE.md`](../../src/platform/LICENSE.md).

---

## 2. MIT-Lizenz
### Generische Host-Infrastruktur, Android- und JNI-Bridges

Die generische Host-Umgebung, portable Laufzeit-Shims und Low-Level-Emulations-Plumbing, die keine spielspezifische Logik enthalten, sind unter der permissiven **MIT-Lizenz** lizenziert:

- **Generische Android-Emulation & JNI-Bridges** (`src/jni/`, `src/android/`):
  - POSIX-Android-Kompatibilitätsschichten, Asset-Manager, Logger und JNI-Marshalling-Bridges.
- **Binärer ELF-Loader & Architektur-Support** (`src/loader/`, `src/srehost/`):
  - Dynamischer ELF-Loader, Symbol-Relokationstabellen und Gast-Host-ABI-Grenzschichten.
- **Generische Engine-Plattform-Helfer** (Teile von `src/platform/`):
  - Generische Fenster-Wrapper, Timer-Abstraktionen und PVRTC/ASTC-Bilddekodierer.

**Copyright © 2026 AevoraLabs.**
*(Teile Copyright © 2023 Rinnegatamante — Swordigo Vita Port; Imagination Technologies Ltd. — PVR SDK)*

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

## 3. Drittanbieter-Komponenten und Lizenzierungshistorie

### Neulizenzierung von SRE
Das Lawncher Team und AevoraLabs haben die Codebasis des Swordigo Runtime Environment (SRE) und aller seiner Submodule (`sre13`, `extras`, `sre12` und `base`) gemeinsam auf die **GNU General Public License v3.0 (GPLv3)** umgestellt.

### Drittanbieter-Abhängigkeiten
- Drittanbieter-Open-Source-Komponenten in `src/sre/base/` (wie Lua 5.1, LuaSocket, LuaFileSystem, toml-c und RakNet) behalten ihre ursprünglichen Lizenzen (MIT, BSD, zlib).
- ufbx (`src/tools/ufbx/`) ist unter MIT / Public Domain doppelt lizenziert.

---

## Zusammenfassungsmatrix (Summary Matrix)

| Verzeichnis / Komponente | Lizenz | Exklusivität / Urheberrechtsinhaber |
| :--- | :--- | :--- |
| `src/sre/` (`sre12`, `sre13`, `extras`, `base`) | **GNU GPLv3** | Gemeinschaftlich **Lawncher Team** & **AevoraLabs** |
| `src/ruby/` (Ruby, Ruby GG Studio IDE) | **GNU GPLv3** | Exklusiv **AevoraLabs** |
| `src/launcher/`, `src/platform/` (Swordfare UI) | **GNU GPLv3** | Exklusiv **AevoraLabs** |
| `src/tools/`, `tools/` (Konverter & Compiler) | **GNU GPLv3** | Exklusiv **AevoraLabs** |
| `src/jni/`, `src/android/` (JNI-Bridges & Shims) | **MIT** | Exklusiv **AevoraLabs** |
| `src/loader/`, `src/srehost/` (Host ELF Loader) | **MIT** | Exklusiv **AevoraLabs** |
| Generische Plattform-Decoder (ASTC, PVRTC) | **MIT** | Ursprüngliche Autoren & **AevoraLabs** |

---

## 4. Gemeinschaftsvereinbarungen und Governance-Richtlinien

Alle Beiträge und die Nutzung von Online-Diensten unterliegen folgenden Vereinbarungen:
- **Contributor License Agreement (CLA)**: Beitragsbedingungen finden Sie in [`../CLA.md`](../CLA.md).
- **Projekt-Governance-Modell**: Die administrative Struktur finden Sie in [`../GOVERNANCE.md`](../GOVERNANCE.md).
- **Nutzungsbedingungen (Terms of Use)**: Die Regeln für den Mod-Store finden Sie in [`../TERMS_OF_USE.md`](../TERMS_OF_USE.md).
- **Verhaltenskodex (Code of Conduct)**: Den Community-Standard finden Sie in [`../CODE_OF_CONDUCT.md`](../CODE_OF_CONDUCT.md).

---

## Translations & Localisation

| Sprache (Language) | Dokumentdatei (Document File) |
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
