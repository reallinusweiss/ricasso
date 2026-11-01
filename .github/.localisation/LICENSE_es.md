# Aviso de Licencia Múltiple de SwordigoDesktop (Multi-License Notice)

**Marco de Licenciamiento de AevoraLabs (prev OpenSwordigo) y Lawncher Team**

> **Documento original en inglés**: [English (LICENSE.md)](../../LICENSE.md) | [Todas las traducciones (All Translations)](#translations--localisation)

Este repositorio es un proyecto de código abierto compuesto cuyos componentes están regulados por dos términos legales distintos:
1. **GNU General Public License v3.0 (GPLv3)** — Entorno de ejecución de Swordigo (SRE), herramientas específicas de Swordigo, interfaz de juego y editores.
2. **Licencia MIT** — Infraestructura de host genérica, capas de emulación de Android y puentes JNI.

Todas las obras originales de este repositorio pertenecen conjuntamente a **AevoraLabs (prev OpenSwordigo)** (`QuantumCreeper`, `Msinup`, `ManoK`) y **Lawncher Team** (`Raijin`, `Kiziyon`).

---

## 1. GNU General Public License v3.0 (GPLv3)
### Entorno de Ejecución de Swordigo (SRE), Componentes de Juego y Editores

Los siguientes subsistemas y directorios están licenciados bajo los términos de la **GNU General Public License, Versión 3 (GPLv3)**:

- **Swordigo Runtime Environment (SRE)** (`src/sre/`):
  - **`src/sre/sre13/`**: Entorno de ejecución invitado de Swordigo 1.4.13, ganchos de arquitectura Caver, biblioteca matemática Lua rbmath y subsistemas de consola y audio.
  - **`src/sre/sre12/`**: Entorno de ejecución invitado de Swordigo 1.4.12, ganchos principales y Mini API.
  - **`src/sre/extras/`**: Módulos extendidos de SRE, interfaces FFI, parches de memoria y sistemas de archivos de guardado.
  - **`src/sre/base/`**: Plomería base del motor SRE, pegamento ABI de tiempo de ejecución personalizado y adaptadores de plataforma.
  - *Desarrollado y licenciado conjuntamente por Lawncher Team (`Raijin`, `Kiziyon`) y AevoraLabs (`QuantumCreeper`, `Msinup`, `ManoK`).*
- **Suite de Edición Ruby y Ruby GG IDE** (`src/ruby/`):
  - Editor Qt6 Studio, editor de nodos visual `Graphy`, shaders de visualización, tubería de iluminación y posprocesamiento, motor visual Caver y herramientas integradas.
- **Lanzador Swordfare y Superposición en el Juego (Overlay)** (`src/launcher/`, `src/platform/`):
  - Overlay HUD dentro del juego, gestor de mods, interfaz de guardado, gestor de perfiles, reproductor de video de fondo y frontend de ejecución.
- **Herramientas y Convertidores de Swordigo** (`src/tools/`, `tools/`):
  - Convertidores de SCL/Scene a gráfico, generador de terreno boulder, formatos rubymesh, puente glTF y compiladores de activos.

**Copyright © 2026 Lawncher Team & AevoraLabs.**
Licenciado bajo GPLv3. Consulte [`src/sre/LICENSE.md`](../../src/sre/LICENSE.md), [`src/ruby/LICENSE.md`](../../src/ruby/LICENSE.md) y [`src/platform/LICENSE.md`](../../src/platform/LICENSE.md).

---

## 2. Licencia MIT
### Infraestructura de Host Genérica, Emulación de Android y Puentes JNI

El entorno host genérico, los adaptadores de ejecución portátiles y la plomería de emulación de bajo nivel que no contienen lógica de juego específica están licenciados bajo la permisiva **Licencia MIT**:

- **Emulación Genérica de Android y Puentes JNI** (`src/jni/`, `src/android/`):
  - Capas de compatibilidad POSIX para Android, administradores de activos, registradores y puentes de serialización JNI.
- **Cargador Binario ELF y Soporte de Arquitectura** (`src/loader/`, `src/srehost/`):
  - Cargador dinámico ELF, tablas de reubicación de símbolos y pegamento de interfaz ABI invitado-host.
- **Ayudantes Genéricos de Plataforma del Motor** (partes de `src/platform/`):
  - Contenedores genéricos de ventanas, abstracciones de temporizadores y decodificadores de imágenes PVRTC/ASTC.

**Copyright © 2026 AevoraLabs.**
*(Partes Copyright © 2023 Rinnegatamante — Port de Swordigo Vita; Imagination Technologies Ltd. — PVR SDK)*

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

## 3. Componentes de Terceros e Historial de Licencias

### Cambio de Licencia de SRE
Lawncher Team y AevoraLabs han cambiado conjuntamente la licencia del código base de Swordigo Runtime Environment (SRE) y todos sus submódulos (`sre13`, `extras`, `sre12`, y `base`) a **GNU General Public License v3.0 (GPLv3)**.

### Dependencias de Terceros
- Las dependencias de terceros en `src/sre/base/` (como Lua 5.1, LuaSocket, LuaFileSystem, toml-c y RakNet) conservan sus licencias de código abierto originales (MIT, BSD, zlib).
- ufbx (`src/tools/ufbx/`) cuenta con licencia dual MIT / Dominio Público.

---

## Matriz Resumen (Summary Matrix)

| Directorio / Componente | Licencia | Exclusividad / Titulares del Copyright |
| :--- | :--- | :--- |
| `src/sre/` (`sre12`, `sre13`, `extras`, `base`) | **GNU GPLv3** | Conjuntamente **Lawncher Team** & **AevoraLabs** |
| `src/ruby/` (Ruby, Ruby GG Studio IDE) | **GNU GPLv3** | Exclusivamente **AevoraLabs** |
| `src/launcher/`, `src/platform/` (Swordfare UI) | **GNU GPLv3** | Exclusivamente **AevoraLabs** |
| `src/tools/`, `tools/` (Convertidores y Compiladores) | **GNU GPLv3** | Exclusivamente **AevoraLabs** |
| `src/jni/`, `src/android/` (Puentes JNI y Shims) | **MIT** | Exclusivamente **AevoraLabs** |
| `src/loader/`, `src/srehost/` (Cargador ELF Host) | **MIT** | Exclusivamente **AevoraLabs** |
| Decodificadores de Plataforma Genéricos (ASTC, PVRTC) | **MIT** | Autores originales & **AevoraLabs** |

---

## 4. Acuerdos Comunitarios y Políticas de Gobernanza

Todas las contribuciones y el uso de la infraestructura en línea están sujetos a los siguientes acuerdos:
- **Acuerdo de Licencia de Contribuidor (CLA)**: Para términos de contribución, consulte [`../CLA.md`](../CLA.md).
- **Modelo de Gobernanza del Proyecto**: Para la estructura administrativa, consulte [`../GOVERNANCE.md`](../GOVERNANCE.md).
- **Términos de Uso**: Para las reglas de la tienda de mods en línea, consulte [`../TERMS_OF_USE.md`](../TERMS_OF_USE.md).
- **Código de Conducta**: Para las normas comunitarias, consulte [`../CODE_OF_CONDUCT.md`](../CODE_OF_CONDUCT.md).

---

## Translations & Localisation

| Idioma (Language) | Archivo de Documento (Document File) |
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
