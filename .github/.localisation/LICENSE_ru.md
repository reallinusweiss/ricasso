# Уведомление о мульти-лицензировании SwordigoDesktop (Multi-License Notice)

**Система лицензирования AevoraLabs (prev OpenSwordigo) и Lawncher Team**

> **Оригинальный документ на английском языке**: [English (LICENSE.md)](../../LICENSE.md) | [Все переводы (All Translations)](#translations--localisation)

Данный репозиторий представляет собой составной проект с открытым исходным кодом, компоненты которого лицензируются на двух различных правовых условиях:
1. **GNU General Public License v3.0 (GPLv3)** — Среда выполнения Swordigo (SRE), специализированные инструменты Swordigo, игровой интерфейс и редакторы.
2. **Лицензия MIT** — Общая хост-инфраструктура, уровни эмуляции Android и мосты JNI.

Все оригинальные разработки в данном репозитории находятся в совместной собственности **AevoraLabs (prev OpenSwordigo)** (`QuantumCreeper`, `Msinup`, `ManoK`) и **Lawncher Team** (`Raijin`, `Kiziyon`).

---

## 1. GNU General Public License v3.0 (GPLv3)
### Среда выполнения Swordigo (SRE), компоненты игры и редактора

Следующие подсистемы и каталоги лицензированы на условиях **GNU General Public License, Version 3 (GPLv3)**:

- **Swordigo Runtime Environment (SRE)** (`src/sre/`):
  - **`src/sre/sre13/`**: Гостевая среда выполнения Swordigo 1.4.13, хуки архитектуры Caver, математическая библиотека Lua rbmath, подсистемы консоли и звука.
  - **`src/sre/sre12/`**: Гостевая среда выполнения Swordigo 1.4.12, основные хуки и Mini API.
  - **`src/sre/extras/`**: Расширенные модули SRE, интерфейсы FFI, исправления памяти и файловые системы сохранений.
  - **`src/sre/base/`**: Базовый движок SRE, кастомный клей среды выполнения ABI и платформенные шимы.
  - *Лицензировано совместно Lawncher Team (`Raijin`, `Kiziyon`) и AevoraLabs (`QuantumCreeper`, `Msinup`, `ManoK`).*
- **Студийный пакет Ruby и Ruby GG IDE** (`src/ruby/`):
  - Редактор Qt6 Studio, визуальный редактор узлов `Graphy`, шейдеры вьюпорта, конвейер освещения и постобработки, визуальный движок Caver и встроенные инструменты.
- **Лаунчер Swordfare и внутриигровой оверлей** (`src/launcher/`, `src/platform/`):
  - Внутриигровой HUD-оверлей, менеджер модов, интерфейс редактора сохранений, менеджер профилей, фоновый видеоплеер и интерфейс выполнения.
- **Инструменты и конвертеры Swordigo** (`src/tools/`, `tools/`):
  - Конвертеры графов из SCL/Scene, генератор рельефа boulder, форматы rubymesh, мост glTF и компиляторы ресурсов.

**Copyright © 2026 Lawncher Team & AevoraLabs.**
Лицензировано под GPLv3. См. [`src/sre/LICENSE.md`](../../src/sre/LICENSE.md), [`src/ruby/LICENSE.md`](../../src/ruby/LICENSE.md) и [`src/platform/LICENSE.md`](../../src/platform/LICENSE.md).

---

## 2. Лицензия MIT
### Общая хост-инфраструктура, эмуляция Android и мосты JNI

Общая среда хоста, переносимые шимы среды выполнения и низкоуровневая эмуляция, не содержащие логики самой игры, лицензируются на условиях **Лицензии MIT**:

- **Общая эмуляция Android и мосты JNI** (`src/jni/`, `src/android/`):
  - Слои совместимости POSIX Android, менеджеры ресурсов, логгеры и мосты маршалинга JNI.
- **Двоичный загрузчик ELF и поддержка архитектуры** (`src/loader/`, `src/srehost/`):
  - Динамический загрузчик ELF, таблицы перемещения символов и клей границы ABI гость-хост.
- **Общие платформенные помощники движка** (части `src/platform/`):
  - Общие оболочки оконной системы, абстракции таймеров и декодеры изображений PVRTC/ASTC.

**Copyright © 2026 AevoraLabs.**
*(Частично Copyright © 2023 Rinnegatamante — Swordigo Vita Port; Imagination Technologies Ltd. — PVR SDK)*

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

## 3. Сторонние компоненты и история лицензирования

### Релицензирование SRE
Команда Lawncher Team и AevoraLabs совместно перевели лицензию кодовой базы Swordigo Runtime Environment (SRE) и всех её подмодулей (`sre13`, `extras`, `sre12` и `base`) на **GNU General Public License v3.0 (GPLv3)**.

### Сторонние зависимости
- Сторонние компоненты в `src/sre/base/` (такие как Lua 5.1, LuaSocket, LuaFileSystem, toml-c и RakNet) сохраняют свои исходные лицензии (MIT, BSD, zlib).
- ufbx (`src/tools/ufbx/`) имеет двойную лицензию MIT / Public Domain.

---

## Сводная матрица (Summary Matrix)

| Каталог / Компонент | Лицензия | Исключительность / Правообладатели |
| :--- | :--- | :--- |
| `src/sre/` (`sre12`, `sre13`, `extras`, `base`) | **GNU GPLv3** | Совместно **Lawncher Team** & **AevoraLabs** |
| `src/ruby/` (Ruby, Ruby GG Studio IDE) | **GNU GPLv3** | Исключительно **AevoraLabs** |
| `src/launcher/`, `src/platform/` (Swordfare UI) | **GNU GPLv3** | Исключительно **AevoraLabs** |
| `src/tools/`, `tools/` (Конвертеры и компиляторы) | **GNU GPLv3** | Исключительно **AevoraLabs** |
| `src/jni/`, `src/android/` (Мосты JNI и шимы) | **MIT** | Исключительно **AevoraLabs** |
| `src/loader/`, `src/srehost/` (Хост-загрузчик ELF) | **MIT** | Исключительно **AevoraLabs** |
| Платформенные декодеры (ASTC, PVRTC) | **MIT** | Авторы оригиналов & **AevoraLabs** |

---

## 4. Соглашения сообщества и политика управления

Все вклады в проект и использование сетевых сервисов регулируются следующими соглашениями:
- **Лицензионное соглашение участника (CLA)**: Условия участия см. в [`../CLA.md`](../CLA.md).
- **Модель управления проектом**: Структуру управления см. в [`../GOVERNANCE.md`](../GOVERNANCE.md).
- **Условия использования**: Правила магазина модов см. в [`../TERMS_OF_USE.md`](../TERMS_OF_USE.md).
- **Кодекс поведения**: Стандарты сообщества см. в [`../CODE_OF_CONDUCT.md`](../CODE_OF_CONDUCT.md).

---

## Translations & Localisation

| Язык (Language) | Файл документа (Document File) |
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
