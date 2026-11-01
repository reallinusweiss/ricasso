# SwordigoDesktop મલ્ટી-લાયસન્સ નોટિસ (Multi-License Notice)

**AevoraLabs (prev OpenSwordigo) અને Lawncher Team લાયસન્સિંગ ફ્રેમવર્ક**

> **મૂળ અંગ્રેજી દસ્તાવેજ**: [English (LICENSE.md)](../../LICENSE.md) | [બધા અનુવાદો (All Translations)](#translations--localisation)

આ રિપોઝીટરી એક સંયુક્ત ઓપન-સોર્સ પ્રોજેક્ટ છે જેના ઘટકો બે વિશિષ્ટ કાનૂની શરતો હેઠળ લાઇસન્સ પ્રાપ્ત છે:
1. **GNU General Public License v3.0 (GPLv3)** — Swordigo રનટાઇમ એન્વાયર્નમેન્ટ (SRE), Swordigo-વિશિષ્ટ સાધનો, રમત ફ્રન્ટએન્ડ અને સંપાદકો.
2. **MIT License** — સામાન્ય હોસ્ટ ઈન્ફ્રાસ્ટ્રક્ચર, એન્ડ્રોઇડ ઇમ્યુલેશન સ્તરો અને JNI બ્રિજ.

આ રિપોઝીટરીના તમામ મૂળ કાર્યો સંયુક્ત રીતે **AevoraLabs (prev OpenSwordigo)** (`QuantumCreeper`, `Msinup`, `ManoK`) અને **Lawncher Team** (`Raijin`, `Kiziyon`) ની માલિકી હેઠળ છે.

---

## ૧. GNU General Public License v3.0 (GPLv3)
### Swordigo રનટાઇમ એન્વાયર્નમેન્ટ (SRE), રમત અને સંપાદક ઘટકો

નીચેની સબસિસ્ટમ્સ અને ડિરેક્ટરીઓ **GNU General Public License, Version 3 (GPLv3)** ની શરતો હેઠળ લાઇસન્સ પ્રાપ્ત છે:

- **Swordigo Runtime Environment (SRE)** (`src/sre/`):
  - **`src/sre/sre13/`**: Swordigo 1.4.13 ગેસ્ટ રનટાઇમ, Caver આર્કિટેક્ચર હુક્સ, rbmath Lua ગણિત લાયબ્રેરી, કન્સોલ અને ઑડિયો સબસિસ્ટમ્સ.
  - **`src/sre/sre12/`**: Swordigo 1.4.12 ગેસ્ટ રનટાઇમ, કોર હુક્સ અને Mini API.
  - **`src/sre/extras/`**: વિસ્તૃત SRE ક્ષમતાઓ, FFI ઇન્ટરફેસ, મેમરી પેચ અને સેવ ફાઇલ સિસ્ટમ્સ.
  - **`src/sre/base/`**: SRE બેઝ એન્જિન પ્લમ્બિંગ, કસ્ટમ રનટાઇમ ABI ગ્લુ અને પ્લેટફોર્મ શિમ્સ.
  - *સંયુક્ત રીતે Lawncher Team (`Raijin`, `Kiziyon`) અને AevoraLabs (`QuantumCreeper`, `Msinup`, `ManoK`) દ્વારા લાઇસન્સ પ્રાપ્ત.*
- **Ruby અને Ruby GG IDE સ્યુટ** (`src/ruby/`):
  - Qt6 સ્ટુડિયો એડિટર, `Graphy` વિઝ્યુઅલ નોડ એડિટર, વ્યૂપોર્ટ શેડર્સ, લાઇટિંગ અને પોસ્ટ-પ્રોસેસિંગ પાઇપલાઇન, Caver વિઝ્યુઅલ એન્જિન અને સંકલિત સાધનો.
- **Swordfare લૉન્ચર અને ગેમ ઓવરલે** (`src/launcher/`, `src/platform/`):
  - ઇન-ગેમ HUD ઓવરલે, મોડ મેનેજર, સેવ એડિટર UI, પ્રોફાઇલ મેનેજર, બેકગ્રાઉન્ડ વિડિઓ પ્લેયર અને રનટાઇમ ફ્રન્ટએન્ડ.
- **Swordigo ટૂલ્સ અને કન્વર્ટર** (`src/tools/`, `tools/`):
  - SCL/Scene થી ગ્રાફ કન્વર્ટર, બોલ્ડર ભૂપ્રદેશ જનરેટર, rubymesh ફોર્મેટ્સ, glTF બ્રિજ અને એસેટ કમ્પાઇલર.

**કૉપિરાઇટ © 2026 Lawncher Team & AevoraLabs.**
GPLv3 હેઠળ લાઇસન્સ પ્રાપ્ત. [`src/sre/LICENSE.md`](../../src/sre/LICENSE.md), [`src/ruby/LICENSE.md`](../../src/ruby/LICENSE.md), અને [`src/platform/LICENSE.md`](../../src/platform/LICENSE.md) જુઓ.

---

## ૨. MIT License
### સામાન્ય હોસ્ટ ઈન્ફ્રાસ્ટ્રક્ચર, એન્ડ્રોઇડ અને JNI બ્રિજ

રમત-વિશિષ્ટ તર્ક ન ધરાવતું સામાન્ય હોસ્ટ વાતાવરણ, પોર્ટેબલ રનટાઇમ શિમ્સ અને ઇમ્યુલેશન પ્લમ્બિંગ ઉદાર **MIT License** હેઠળ લાઇસન્સ પ્રાપ્ત છે:

- **સામાન્ય એન્ડ્રોઇડ ઇમ્યુલેશન અને JNI બ્રિજ** (`src/jni/`, `src/android/`):
  - POSIX એન્ડ્રોઇડ શિમ સ્તરો, એસેટ મેનેજર, લોગર્સ અને JNI માર્શલિંગ બ્રિજ.
- **બાઇનરી ELF લોડર અને આર્કિટેક્ચર સપોર્ટ** (`src/loader/`, `src/srehost/`):
  - ડાયનેમિક ELF લોડર, સિમ્બોલ રિલોકેશન ટેબલ અને ગેસ્ટ-હોસ્ટ ABI સીમા ગુંદર.
- **સામાન્ય એન્જિન પ્લેટફોર્મ હેલ્પર્સ** (`src/platform/` ના ભાગો):
  - સામાન્ય વિન્ડોઇંગ રેપર્સ, ટાઈમર એબ્સ્ટ્રેક્શન્સ અને PVRTC/ASTC ઈમેજ ડીકોડર્સ.

**કૉપિરાઇટ © 2026 AevoraLabs.**
*(કેટલાક ભાગો કૉપિરાઇટ © 2023 Rinnegatamante — Swordigo Vita Port; Imagination Technologies Ltd. — PVR SDK)*

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

## ૩. તૃતીય-પક્ષ ઘટકો અને લાઇસન્સિંગ ઇતિહાસ

### SRE નું પુનઃલાઇસન્સિંગ (Relicensing of SRE)
Lawncher Team અને AevoraLabs એ સંયુક્ત રીતે Swordigo રનટાઇમ એન્વાયર્નમેન્ટ (SRE) અને તેના તમામ સબમોડ્યુલ્સ (`sre13`, `extras`, `sre12`, `base`) ને **GNU General Public License v3.0 (GPLv3)** હેઠળ ઓપન-સોર્સ કર્યા છે.

### તૃતીય-પક્ષ નિર્ભરતાઓ
- `src/sre/base/` માં રહેલા તૃતીય-પક્ષ ઓપન-સોર્સ ઘટકો (Lua 5.1, LuaSocket, LuaFileSystem, toml-c, RakNet) તેમના મૂળ લાઇસન્સ (MIT, BSD, zlib) જાળવી રાખે છે.
- ufbx (`src/tools/ufbx/`) MIT / Public Domain હેઠળ ડ્યુઅલ-લાઇસન્સ પ્રાપ્ત છે.

---

## સારાંશ મેટ્રિક્સ (Summary Matrix)

| ડિરેક્ટરી / ઘટક | લાઇસન્સ | વિશિષ્ટતા / કૉપિરાઇટ ધારકો |
| :--- | :--- | :--- |
| `src/sre/` (`sre12`, `sre13`, `extras`, `base`) | **GNU GPLv3** | સંયુક્ત રીતે **Lawncher Team** & **AevoraLabs** |
| `src/ruby/` (Ruby, Ruby GG Studio IDE) | **GNU GPLv3** | વિશિષ્ટ રીતે **AevoraLabs** |
| `src/launcher/`, `src/platform/` (Swordfare UI) | **GNU GPLv3** | વિશિષ્ટ રીતે **AevoraLabs** |
| `src/tools/`, `tools/` (કન્વર્ટર અને કમ્પાઇલર) | **GNU GPLv3** | વિશિષ્ટ રીતે **AevoraLabs** |
| `src/jni/`, `src/android/` (JNI બ્રિજ અને શિમ્સ) | **MIT** | વિશિષ્ટ રીતે **AevoraLabs** |
| `src/loader/`, `src/srehost/` (હોસ્ટ ELF લોડર) | **MIT** | વિશિષ્ટ રીતે **AevoraLabs** |
| સામાન્ય પ્લેટફોર્મ ડીકોડર (ASTC, PVRTC) | **MIT** | મૂળ લેખકો અને **AevoraLabs** |

---

## ૪. સમુદાય કરારો અને શાસન નીતિઓ

પ્રોજેક્ટમાં તમામ યોગદાન અને ઑનલાઇન સેવાઓનો ઉપયોગ નીચેના કરારોને આધીન છે:
- **યોગદાનકર્તા લાઇસન્સ કરાર (CLA)**: યોગદાન શરતો માટે [`../CLA.md`](../CLA.md) જુઓ.
- **પ્રોજેક્ટ શાસન મોડેલ (Governance)**: વહીવટી માળખા માટે [`../GOVERNANCE.md`](../GOVERNANCE.md) જુઓ.
- **ઉપયોગની શરતો (Terms of Use)**: ઑનલાઇન મોડ સ્ટોર સેવાઓ માટે [`../TERMS_OF_USE.md`](../TERMS_OF_USE.md) જુઓ.
- **આચારસંહિતા (Code of Conduct)**: સમુદાયના ધોરણો માટે [`../CODE_OF_CONDUCT.md`](../CODE_OF_CONDUCT.md) જુઓ.

---

## Translations & Localisation

| ભાષા (Language) | દસ્તાવેજ ફાઇલ (Document File) |
| :--- | :--- |
| **English (Official)** | [`../../LICENSE.md`](../../LICENSE.md) |
| **हिन्दी (Hindi)** | [`LICENSE_hi.md`](LICENSE_hi.md) |
| **বাংলা (Bengali)** | [`LICENSE_bn.md`](LICENSE_bn.md) |
| **తెలుగు (Telugu)** | [`LICENSE_te.md`](LICENSE_te.md) |
| **தமிழ் (Tamil)** | [`LICENSE_ta.md`](LICENSE_ta.md) |
| **મરાઠી (Marathi)** | [`LICENSE_mr.md`](LICENSE_mr.md) |
| **ગુજરાતી (Gujarati)** | [`LICENSE_gu.md`](LICENSE_gu.md) |
| **Español (Spanish)** | [`LICENSE_es.md`](LICENSE_es.md) |
| **Français (French)** | [`LICENSE_fr.md`](LICENSE_fr.md) |
| **简体中文 (Chinese)** | [`LICENSE_cn.md`](LICENSE_cn.md) |
| **Deutsch (German)** | [`LICENSE_de.md`](LICENSE_de.md) |
| **日本語 (Japanese)** | [`LICENSE_ja.md`](LICENSE_ja.md) |
| **Русский (Russian)** | [`LICENSE_ru.md`](LICENSE_ru.md) |
| **Português (Portuguese)** | [`LICENSE_pt.md`](LICENSE_pt.md) |
