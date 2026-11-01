# SwordigoDesktop बहु-परवाना सूचना (Multi-License Notice)

**AevoraLabs (prev OpenSwordigo) आणि Lawncher Team परवाना रूपरेषा (Licensing Framework)**

> **मूळ इंग्रजी दस्तऐवज**: [English (LICENSE.md)](../../LICENSE.md) | [सर्व भाषांतरे (All Translations)](#translations--localisation)

हे भांडार (repository) एक संमिश्र ओपन-सोर्स प्रकल्प आहे ज्याचे घटक दोन स्वतंत्र कायदेशीर अटींनुसार परवानाकृत आहेत:
1. **GNU General Public License v3.0 (GPLv3)** — Swordigo रनटाइम एन्व्हायर्नमेंट (SRE), Swordigo-विशिष्ट साधने, गेम फ्रंटएंड आणि संपादके.
2. **MIT License** — सामान्य होस्ट पायाभूत सुविधा, Android इम्युलेशन स्तर आणि JNI ब्रिजेस.

या भांडारातील सर्व मूळ कामे संयुक्तपणे **AevoraLabs (prev OpenSwordigo)** (`QuantumCreeper`, `Msinup`, `ManoK`) आणि **Lawncher Team** (`Raijin`, `Kiziyon`) यांच्या मालकीची आहेत.

---

## १. GNU General Public License v3.0 (GPLv3)
### Swordigo रनटाइम एन्व्हायर्नमेंट (SRE), गेम आणि संपादक घटक

खालील सबसिस्टीम आणि डिरेक्टरी **GNU General Public License, Version 3 (GPLv3)** च्या अटींनुसार परवानाकृत आहेत:

- **Swordigo Runtime Environment (SRE)** (`src/sre/`):
  - **`src/sre/sre13/`**: Swordigo 1.4.13 गेस्ट रनटाइम, Caver आर्किटेक्चर हुक्स, rbmath Lua गणित लायब्ररी, कन्सोल आणि ऑडिओ सबसिस्टीम.
  - **`src/sre/sre12/`**: Swordigo 1.4.12 गेस्ट रनटाइम, कोर हुक्स आणि Mini API.
  - **`src/sre/extras/`**: विस्तारित SRE क्षमता, FFI इंटरफेस, मेमरी पॅचेस आणि सेव्ह फाईल सिस्टीम.
  - **`src/sre/base/`**: SRE बेस इंजिन प्लंबिंग, सानुकूल रनटाइम ABI ग्लू आणि प्लॅटफॉर्म शshims.
  - *संयुक्तपणे Lawncher Team (`Raijin`, `Kiziyon`) आणि AevoraLabs (`QuantumCreeper`, `Msinup`, `ManoK`) द्वारे परवानाकृत.*
- **Ruby आणि Ruby GG IDE संच** (`src/ruby/`):
  - Qt6 स्टुडिओ एडिटर, `Graphy` व्हिज्युअल नोड एडिटर, व्ह्यूपोर्ट शेडर्स, लाइटिंग आणि पोस्ट-प्रोसेसिंग पाइपलाइन, Caver व्हिज्युअल इंजिन आणि एकात्मिक साधने.
- **Swordfare लाँचर आणि गेम आच्छादन (Overlay)** (`src/launcher/`, `src/platform/`):
  - इन-गेम HUD ओव्हरले, मॉड मॅनेजर, सेव्ह एडिटर UI, प्रोफाइल मॅनेजर, बॅकग्राउंड व्हिडिओ प्लेयर आणि रनटाइम फ्रंटएंड.
- **Swordigo साधने आणि कन्व्हर्टर्स** (`src/tools/`, `tools/`):
  - SCL/Scene ते ग्राफ कन्व्हर्टर्स, बोल्डर भूप्रदेश जनरेटर, rubymesh स्वरूप, glTF ब्रिज आणि मालमत्ता संकलक.

**कॉपीराइट © 2026 Lawncher Team & AevoraLabs.**
GPLv3 अंतर्गत परवानाकृत. [`src/sre/LICENSE.md`](../../src/sre/LICENSE.md), [`src/ruby/LICENSE.md`](../../src/ruby/LICENSE.md), आणि [`src/platform/LICENSE.md`](../../src/platform/LICENSE.md) पहा.

---

## २. MIT License
### सामान्य होस्ट पायाभूत सुविधा, Android आणि JNI ब्रिजेस

विशिष्ट गेम तर्क नसलेले सामान्य होस्ट वातावरण, पोर्टेबल रनटाइम शshims आणि निम्न-स्तरीय इम्युलेशन प्लंबिंग उदारमतवादी **MIT License** अंतर्गत परवानाकृत आहेत:

- **सामान्य Android इम्युलेशन आणि JNI ब्रिजेस** (`src/jni/`, `src/android/`):
  - POSIX Android शshims स्तर, मालमत्ता व्यवस्थापक, लॉगर आणि JNI मार्शलिंग ब्रिजेस.
- **बायनरी ELF लोडर आणि आर्किटेक्चर समर्थन** (`src/loader/`, `src/srehost/`):
  - डायनॅमिक ELF लोडर, चिन्ह पुनर्स्थापना सारण्या आणि गेस्ट-होस्ट ABI सीमा गोंद.
- **सामान्य इंजिन प्लॅटफॉर्म मदतनीस** (`src/platform/` चे भाग):
  - सामान्य विंडोइंग रॅपर्स, टायमर ॲब्स्ट्रॅक्शन आणि PVRTC/ASTC इमेज डीकोडर्स.

**कॉपीराइट © 2026 AevoraLabs.**
*(काही भाग कॉपीराइट © 2023 Rinnegatamante — Swordigo Vita Port; Imagination Technologies Ltd. — PVR SDK)*

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

## ३. तृतीय-पक्ष घटक आणि परवाना इतिहास

### SRE चे पुनर्मालिकीकरण (Relicensing of SRE)
Lawncher Team आणि AevoraLabs ने संयुक्तपणे Swordigo रनटाइम एन्व्हायर्नमेंट (SRE) कोडबेस आणि त्याचे सर्व सबमॉड्यूल (`sre13`, `extras`, `sre12`, `base`) **GNU General Public License v3.0 (GPLv3)** अंतर्गत मुक्त स्रोत केले आहेत.

### तृतीय-पक्ष अवलंबित्व
- `src/sre/base/` मधील तृतीय-पक्ष ओपन-सोर्स घटक (Lua 5.1, LuaSocket, LuaFileSystem, toml-c, RakNet) त्यांचे मूळ परवाने (MIT, BSD, zlib) कायम ठेवतात.
- ufbx (`src/tools/ufbx/`) MIT / Public Domain दुहेरी परवानाकृत आहे.

---

## सारांश मॅट्रिक्स (Summary Matrix)

| डिरेक्टरी / घटक | परवाना | विशिष्टता / कॉपीराइट धारक |
| :--- | :--- | :--- |
| `src/sre/` (`sre12`, `sre13`, `extras`, `base`) | **GNU GPLv3** | संयुक्तपणे **Lawncher Team** & **AevoraLabs** |
| `src/ruby/` (Ruby, Ruby GG Studio IDE) | **GNU GPLv3** | विशेषतः **AevoraLabs** |
| `src/launcher/`, `src/platform/` (Swordfare UI) | **GNU GPLv3** | विशेषतः **AevoraLabs** |
| `src/tools/`, `tools/` (कन्व्हर्टर्स आणि कंपायलर) | **GNU GPLv3** | विशेषतः **AevoraLabs** |
| `src/jni/`, `src/android/` (JNI ब्रिजेस आणि शshims) | **MIT** | विशेषतः **AevoraLabs** |
| `src/loader/`, `src/srehost/` (होस्ट ELF लोडर) | **MIT** | विशेषतः **AevoraLabs** |
| सामान्य प्लॅटफॉर्म डीकोडर (ASTC, PVRTC) | **MIT** | मूळ लेखक आणि **AevoraLabs** |

---

## ४. समुदाय करार आणि प्रशासन धोरणे

प्रकल्पातील सर्व योगदाने आणि ऑनलाइन सेवांचा वापर खालील करारांच्या अधीन आहे:
- **योगदानकर्ता परवाना करार (CLA)**: योगदानाच्या अटींसाठी [`../CLA.md`](../CLA.md) पहा.
- **प्रकल्प प्रशासन मॉडेल (Governance)**: प्रशासकीय संरचनेसाठी [`../GOVERNANCE.md`](../GOVERNANCE.md) पहा.
- **वापराच्या अटी (Terms of Use)**: ऑनलाइन मॉड स्टोअर सेवांसाठी [`../TERMS_OF_USE.md`](../TERMS_OF_USE.md) पहा.
- **आचारसंहिता (Code of Conduct)**: समुदाय मानकांसाठी [`../CODE_OF_CONDUCT.md`](../CODE_OF_CONDUCT.md) पहा.

---

## Translations & Localisation

| भाषा (Language) | दस्तऐवज फाईल (Document File) |
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
