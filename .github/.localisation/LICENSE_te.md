# SwordigoDesktop బహుళ-లైసెన్స్ ప్రకటన (Multi-License Notice)

**AevoraLabs (prev OpenSwordigo) మరియు Lawncher Team లైసెన్సింగ్ ఫ్రేమ్‌వర్క్**

> **అసలు ఆంగ్ల పత్రం**: [English (LICENSE.md)](../../LICENSE.md) | [అన్ని అనువాదాలు (All Translations)](#translations--localisation)

ఈ రిపోజిటరీ ఒక మిశ్రమ (composite) ఓపెన్-సోర్స్ ప్రాజెక్ట్, దీని భాగాలు రెండు నిర్దిష్ట నిబంధనల ప్రకారం లైసెన్స్ చేయబడ్డాయి:
1. **GNU General Public License v3.0 (GPLv3)** — Swordigo రన్‌టైమ్ ఎన్విరాన్మెంట్ (SRE), Swordigo-నిర్దిష్ట సాధనాలు, గేమ్ ఫ్రంటెండ్ మరియు ఎడిటర్లు.
2. **MIT License** — సాధారణ హోస్ట్ మౌలిక సదుపాయాలు, ఆండ్రాయిడ్ ఎమ్యులేషన్ పొరలు మరియు JNI వంతెనలు.

ఈ రిపోజిటరీలోని అసలు పనులు సంయుక్తంగా **AevoraLabs (prev OpenSwordigo)** (`QuantumCreeper`, `Msinup`, `ManoK`) మరియు **Lawncher Team** (`Raijin`, `Kiziyon`) యాజమాన్యంలో ఉంటాయి.

---

## 1. GNU General Public License v3.0 (GPLv3)
### Swordigo రన్‌టైమ్ ఎన్విరాన్మెంట్ (SRE), గేమ్ మరియు ఎడిటర్ భాగాలు

కింది సబ్‌సిస్టమ్‌లు మరియు డైరెక్టరీలు **GNU General Public License, Version 3 (GPLv3)** నిబంధనల క్రింద లైసెన్స్ చేయబడ్డాయి:

- **Swordigo Runtime Environment (SRE)** (`src/sre/`):
  - **`src/sre/sre13/`**: Swordigo 1.4.13 గెస్ట్ రన్‌టైమ్, Caver ఆర్కిటెక్చర్ హుక్స్, rbmath Lua మ్యాథ్ లైబ్రరీ, కన్సోల్ మరియు ఆడియో సబ్‌సిస్టమ్‌లు.
  - **`src/sre/sre12/`**: Swordigo 1.4.12 గెస్ట్ రన్‌టైమ్, కోర్ హుక్స్ మరియు Mini API.
  - **`src/sre/extras/`**: విస్తరించిన SRE సామర్థ్యాలు, FFI ఇంటర్‌ఫేస్‌లు, మెమరీ ప్యాచ్‌లు మరియు సేవ్ ఫైల్ సిస్టమ్‌లు.
  - **`src/sre/base/`**: SRE బేస్ ఇంజిన్ ప్లంబింగ్, కస్టమ్ రన్‌టైమ్ ABI గ్లూ మరియు ప్లాట్‌ఫారమ్ షిమ్‌లు.
  - *సంయుక్తంగా Lawncher Team (`Raijin`, `Kiziyon`) మరియు AevoraLabs (`QuantumCreeper`, `Msinup`, `ManoK`) చేత లైసెన్స్ పొందింది.*
- **Ruby మరియు Ruby GG IDE సూట్** (`src/ruby/`):
  - Qt6 స్టూడియో ఎడిటర్, `Graphy` విజువల్ నోడ్ ఎడిటర్, వ్యూపోర్ట్ షేడర్లు, లైటింగ్ & పోస్ట్-ప్రాసెసింగ్ పైప్‌లైన్, Caver విజువల్ ఇంజిన్ మరియు ఇంటిగ్రేటెడ్ టూల్స్.
- **Swordfare లాంచర్ మరియు గేమ్ ఓవర్‌లే** (`src/launcher/`, `src/platform/`):
  - ఇన్-గేమ్ HUD ఓవర్‌లే, మోడ్ మేనేజర్, సేవ్ ఎడిటర్ UI, ప్రొఫైల్ మేనేజర్, బ్యాక్‌గ్రౌండ్ వీడియో ప్లేయర్ మరియు రన్‌టైమ్ ఫ్రంటెండ్.
- **Swordigo టూలింగ్ మరియు కన్వర్టర్లు** (`src/tools/`, `tools/`):
  - SCL/Scene గ్రాఫ్ కన్వర్టర్లు, బోల్డర్ టెర్రైన్ జనరేటర్, rubymesh ఫార్మాట్లు, glTF వంతెన మరియు అసెట్ కంపైలర్లు.

**కాపీరైట్ © 2026 Lawncher Team & AevoraLabs.**
GPLv3 క్రింద లైసెన్స్ చేయబడింది. [`src/sre/LICENSE.md`](../../src/sre/LICENSE.md), [`src/ruby/LICENSE.md`](../../src/ruby/LICENSE.md), మరియు [`src/platform/LICENSE.md`](../../src/platform/LICENSE.md) చూడండి.

---

## 2. MIT License
### సాధారణ హోస్ట్ మౌలిక సదుపాయాలు, ఆండ్రాయిడ్ & JNI వంతెనలు

నిర్దిష్ట గేమ్ లాజిక్ లేని సాధారణ హోస్ట్ వాతావరణం, పోర్టబుల్ రన్‌టైమ్ షిమ్‌లు మరియు ఎమ్యులేషన్ ప్లంబింగ్ అనుమతినిచ్చే **MIT License** క్రింద లైసెన్స్ చేయబడ్డాయి:

- **సాధారణ ఆండ్రాయిడ్ ఎమ్యులేషన్ మరియు JNI వంతెనలు** (`src/jni/`, `src/android/`):
  - POSIX ఆండ్రాయిడ్ షిమ్ లేయర్‌లు, అసెట్ మేనేజర్‌లు, లాగర్‌లు మరియు JNI మార్షలింగ్ వంతెనలు.
- **బైనరీ ELF లోడర్ మరియు ఆర్కిటెక్చర్ మద్దతు** (`src/loader/`, `src/srehost/`):
  - డైనమిక్ ELF లోడర్, సింబల్ రీలోకేషన్ టేబుల్స్ మరియు గెస్ట్-హోస్ట్ ABI బౌండరీ గ్లూ.
- **సాధారణ ఇంజిన్ ప్లాట్‌ఫారమ్ సహాయకాలు** (`src/platform/` భాగాలు):
  - సాధారణ విండోయింగ్ ర్యాపర్లు, టైమర్ అబ్‌స్ట్రాక్షన్‌లు మరియు PVRTC/ASTC ఇమేజ్ డీకోడర్లు.

**కాపీరైట్ © 2026 AevoraLabs.**
*(కొన్ని భాగాలు కాపీరైట్ © 2023 Rinnegatamante — Swordigo Vita Port; Imagination Technologies Ltd. — PVR SDK)*

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

## 3. మూడవ పక్ష భాగాలు మరియు లైసెన్సింగ్ చరిత్ర

### SRE తిరిగి లైసెన్స్ చేయడం (Relicensing of SRE)
Lawncher Team మరియు AevoraLabs సంయుక్తంగా Swordigo రన్‌టైమ్ ఎన్విరాన్మెంట్ (SRE) మరియు దాని అన్ని సబ్‌మాడ్యూల్‌లను (`sre13`, `extras`, `sre12`, `base`) **GNU General Public License v3.0 (GPLv3)** కిందకు మార్చాయి.

### మూడవ పక్ష ఆధారితాలు
- `src/sre/base/` లోని మూడవ పక్ష ఓపెన్-సోర్స్ భాగాలు (Lua 5.1, LuaSocket, LuaFileSystem, toml-c, RakNet) వాటి అసలు లైసెన్స్‌లను (MIT, BSD, zlib) కలిగి ఉంటాయి.
- ufbx (`src/tools/ufbx/`) MIT / Public Domain ద్వంద్వ లైసెన్స్ కలిగి ఉంది.

---

## సారాంశ మాట్రిక్స్ (Summary Matrix)

| డైరెక్టరీ / భాగం | లైసెన్స్ | ప్రత్యేకత / కాపీరైట్ హోల్డర్లు |
| :--- | :--- | :--- |
| `src/sre/` (`sre12`, `sre13`, `extras`, `base`) | **GNU GPLv3** | సంయుక్తంగా **Lawncher Team** & **AevoraLabs** |
| `src/ruby/` (Ruby, Ruby GG Studio IDE) | **GNU GPLv3** | ప్రత్యేకంగా **AevoraLabs** |
| `src/launcher/`, `src/platform/` (Swordfare UI) | **GNU GPLv3** | ప్రత్యేకంగా **AevoraLabs** |
| `src/tools/`, `tools/` (కన్వర్టర్లు & కంపైలర్లు) | **GNU GPLv3** | ప్రత్యేకంగా **AevoraLabs** |
| `src/jni/`, `src/android/` (JNI వంతెనలు & షిమ్‌లు) | **MIT** | ప్రత్యేకంగా **AevoraLabs** |
| `src/loader/`, `src/srehost/` (హోస్ట్ ELF లోడర్) | **MIT** | ప్రత్యేకంగా **AevoraLabs** |
| సాధారణ ప్లాట్‌ఫారమ్ డీకోడర్లు (ASTC, PVRTC) | **MIT** | అసలు రచయితలు & **AevoraLabs** |

---

## 4. కమ్యూనిటీ ఒప్పందాలు మరియు పాలన విధానాలు

ప్రాజెక్ట్‌కు అన్ని రచనలు మరియు ఆన్‌లైన్ సేవల వినియోగం క్రింది ఒప్పందాలకు లోబడి ఉంటాయి:
- **రచయిత లైసెన్స్ ఒప్పందం (CLA)**: రచన నిబంధనలు మరియు 50/50 కాపీరైట్ నిలుపుదల కోసం [`../CLA.md`](../CLA.md) చూడండి.
- **ప్రాజెక్ట్ పాలన మోడల్ (Governance)**: పరిపాలనా నిర్మాణం కోసం [`../GOVERNANCE.md`](../GOVERNANCE.md) చూడండి.
- **వినియోగ నిబంధనలు (Terms of Use)**: ఆన్‌లైన్ మోడ్ స్టోర్ సేవల కోసం [`../TERMS_OF_USE.md`](../TERMS_OF_USE.md) చూడండి.
- **ప్రవర్తనా నియమావళి (Code of Conduct)**: కమ్యూనిటీ ప్రమాణాల కోసం [`../CODE_OF_CONDUCT.md`](../CODE_OF_CONDUCT.md) చూడండి.

---

## Translations & Localisation

| భాష (Language) | పత్ర ఫైల్ (Document File) |
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
