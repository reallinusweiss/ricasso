# SwordigoDesktop பல உரிம அறிவிப்பு (Multi-License Notice)

**AevoraLabs (prev OpenSwordigo) மற்றும் Lawncher Team உரிமக் கட்டமைப்பு**

> **அசல் ஆங்கில ஆவணம்**: [English (LICENSE.md)](../../LICENSE.md) | [அனைத்து மொழிபெயர்ப்புகளும் (All Translations)](#translations--localisation)

இந்தக் களஞ்சியம் ஒரு கூட்டு (composite) திறந்த மூலத் திட்டமாகும், இதன் கூறுகள் இரண்டு தனித்தனி விதிமுறைகளின் கீழ் உரிமம் பெற்றுள்ளன:
1. **GNU General Public License v3.0 (GPLv3)** — Swordigo இயக்க நேர சூழல் (SRE), Swordigo-குறிப்பிட்ட கருவிகள், விளையாட்டு முன்முனை மற்றும் தொகுப்பாளர்கள்.
2. **MIT License** — பொதுவான ஹோஸ்ட் உள்கட்டமைப்பு, Android முன்மாதிரி அடுக்குகள் மற்றும் JNI பாலங்கள்.

இந்த களஞ்சியத்தின் அனைத்து அசல் படைப்புகளும் **AevoraLabs (prev OpenSwordigo)** (`QuantumCreeper`, `Msinup`, `ManoK`) மற்றும் **Lawncher Team** (`Raijin`, `Kiziyon`) ஆகியோரால் கூட்டாக உரிமம் பெற்றுள்ளன.

---

## 1. GNU General Public License v3.0 (GPLv3)
### Swordigo இயக்க நேர சூழல் (SRE), விளையாட்டு மற்றும் தொகுப்பாளர் கூறுகள்

பின்வரும் துணை அமைப்புகளும் கோப்பகங்களும் **GNU General Public License, Version 3 (GPLv3)** இன் விதிமுறைகளின் கீழ் உரிமம் பெற்றுள்ளன:

- **Swordigo Runtime Environment (SRE)** (`src/sre/`):
  - **`src/sre/sre13/`**: Swordigo 1.4.13 விருந்தினர் இயக்க நேரம், Caver கட்டமைப்பு கொக்கிகள் (hooks), rbmath Lua கணித நூலகம், கன்சோல் மற்றும் ஆடியோ அமைப்புகள்.
  - **`src/sre/sre12/`**: Swordigo 1.4.12 விருந்தினர் இயக்க நேரம், முக்கிய கொக்கிகள் மற்றும் Mini API.
  - **`src/sre/extras/`**: விரிவாக்கப்பட்ட SRE தொகுதிகள், FFI இடைமுகங்கள், நினைவக திருத்தங்கள் மற்றும் சேமிப்பு கோப்பு முறைமைகள்.
  - **`src/sre/base/`**: SRE அடிப்படை எஞ்சின் குழாய்கள், தனிப்பயன் இயக்க நேர ABI பசை மற்றும் தள ஷிம்கள்.
  - *Lawncher Team (`Raijin`, `Kiziyon`) மற்றும் AevoraLabs (`QuantumCreeper`, `Msinup`, `ManoK`) ஆகியோரால் கூட்டாக உரிமம் அளிக்கப்பட்டது.*
- **Ruby மற்றும் Ruby GG IDE தொகுப்பு** (`src/ruby/`):
  - Qt6 ஸ்டுடியோ எடிட்டர், `Graphy` காட்சி முனை எடிட்டர், காட்சிப்பகுதி ஷேடர்கள், லைட்டிங் & பிந்தைய செயலாக்க பைப்லைன், Caver காட்சி இயந்திரம் மற்றும் ஒருங்கிணைந்த கருவிகள்.
- **Swordfare துவக்கி மற்றும் விளையாட்டு மேலடுக்கு** (`src/launcher/`, `src/platform/`):
  - விளையாட்டுக்குள் HUD மேலடுக்கு, மோட் மேலாளர், சேமிப்பு எடிட்டர் UI, சுயவிவர மேலாளர், பின்னணி வீடியோ பிளேயர் மற்றும் இயக்க நேர முன்முனை.
- **Swordigo கருவிகள் மற்றும் மாற்றிகள்** (`src/tools/`, `tools/`):
  - SCL/Scene வரைபட மாற்றிகள், போல்டர் நிலப்பரப்பு ஜெனரேட்டர், rubymesh வடிவங்கள், glTF பாலம் மற்றும் சொத்து தொகுப்பாளர்கள்.

**பதிப்புரிமை © 2026 Lawncher Team & AevoraLabs.**
GPLv3 இன் கீழ் உரிமம் பெற்றது. [`src/sre/LICENSE.md`](../../src/sre/LICENSE.md), [`src/ruby/LICENSE.md`](../../src/ruby/LICENSE.md), மற்றும் [`src/platform/LICENSE.md`](../../src/platform/LICENSE.md) ஐப் பார்க்கவும்.

---

## 2. MIT License
### பொதுவான ஹோஸ்ட் உள்கட்டமைப்பு, Android மற்றும் JNI பாலங்கள்

குறிப்பிட்ட விளையாட்டு தர்க்கத்தைக் கொண்டிருக்காத பொதுவான ஹோஸ்ட் சூழல் மற்றும் குறைந்த-நிலை முன்மாதிரி கூறுகள் அனுமதிக்கப்பட்ட **MIT License** இன் கீழ் உரிமம் பெற்றுள்ளன:

- **பொதுவான Android முன்மாதிரி மற்றும் JNI பாலங்கள்** (`src/jni/`, `src/android/`):
  - POSIX Android ஷிம் அடுக்குகள், சொத்து மேலாளர்கள், பதிவர்கள் மற்றும் JNI மார்ஷலிங் பாலங்கள்.
- **பைனரி ELF ஏற்றி மற்றும் கட்டமைப்பு ஆதரவு** (`src/loader/`, `src/srehost/`):
  - டைனமிக் ELF ஏற்றி, சின்ன இடமாற்ற அட்டவணைகள் மற்றும் விருந்தினர்-ஹோஸ்ட் ABI எல்லை பசை.
- **பொதுவான இயந்திர இயங்குதள உதவியாளர்கள்** (`src/platform/` இன் பகுதிகள்):
  - பொதுவான விண்டோயிங் ரேப்பர்கள், டைமர் சுருக்கங்கள் மற்றும் PVRTC/ASTC பட டிகோடர்கள்.

**பதிப்புரிமை © 2026 AevoraLabs.**
*(சில பகுதிகள் பதிப்புரிமை © 2023 Rinnegatamante — Swordigo Vita Port; Imagination Technologies Ltd. — PVR SDK)*

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

## 3. மூன்றாம் தரப்பு கூறுகள் மற்றும் உரிம வரலாறு

### SRE-ஐ மறுஉரிமம் செய்தல் (Relicensing of SRE)
Lawncher Team மற்றும் AevoraLabs ஆகியவை Swordigo இயக்க நேர சூழல் (SRE) மற்றும் அதன் அனைத்து துணை தொகுதிகளையும் (`sre13`, `extras`, `sre12`, `base`) கூட்டாக **GNU General Public License v3.0 (GPLv3)** இன் கீழ் வெளியிட்டுள்ளன.

### மூன்றாம் தரப்பு சார்புகள்
- `src/sre/base/` இல் உள்ள மூன்றாம் தரப்பு சார்புகள் (Lua 5.1, LuaSocket, LuaFileSystem, toml-c, RakNet) தங்களின் அசல் உரிமங்களை (MIT, BSD, zlib) தக்க வைத்துக் கொள்கின்றன.
- ufbx (`src/tools/ufbx/`) MIT / பொது டொமைன் இரட்டை உரிமம் கொண்டது.

---

## சுருக்க அணி (Summary Matrix)

| அடைவு / கூறு | உரிமம் | பிரத்தியேகத்தன்மை / பதிப்புரிமை வைத்திருப்பவர்கள் |
| :--- | :--- | :--- |
| `src/sre/` (`sre12`, `sre13`, `extras`, `base`) | **GNU GPLv3** | கூட்டாக **Lawncher Team** & **AevoraLabs** |
| `src/ruby/` (Ruby, Ruby GG Studio IDE) | **GNU GPLv3** | பிரத்தியேகமாக **AevoraLabs** |
| `src/launcher/`, `src/platform/` (Swordfare UI) | **GNU GPLv3** | பிரத்தியேகமாக **AevoraLabs** |
| `src/tools/`, `tools/` (மாற்றிகள் & தொகுப்பாளர்கள்) | **GNU GPLv3** | பிரத்தியேகமாக **AevoraLabs** |
| `src/jni/`, `src/android/` (JNI பாலங்கள் & ஷிம்கள்) | **MIT** | பிரத்தியேகமாக **AevoraLabs** |
| `src/loader/`, `src/srehost/` (ஹோஸ்ட் ELF ஏற்றி) | **MIT** | பிரத்தியேகமாக **AevoraLabs** |
| பொதுவான தள டிகோடர்கள் (ASTC, PVRTC) | **MIT** | அசல் ஆசிரியர்கள் & **AevoraLabs** |

---

## 4. சமூக ஒப்பந்தங்கள் மற்றும் நிர்வாகக் கொள்கைகள்

திட்டத்திற்கான அனைத்து பங்களிப்புகளும் ஆன்லைன் சேவைகளும் பின்வரும் ஒப்பந்தங்களுக்கு உட்பட்டவை:
- **பங்களிப்பாளர் உரிம ஒப்பந்தம் (CLA)**: பங்களிப்பு விதிமுறைகளுக்கு [`../CLA.md`](../CLA.md) ஐப் பார்க்கவும்.
- **திட்ட நிர்வாக மாதிரி (Governance)**: நிர்வாக அமைப்பிற்கு [`../GOVERNANCE.md`](../GOVERNANCE.md) ஐப் பார்க்கவும்.
- **பயன்பாட்டு விதிமுறைகள் (Terms of Use)**: ஆன்லைன் மோட் ஸ்டோர் விதிகளுக்கு [`../TERMS_OF_USE.md`](../TERMS_OF_USE.md) ஐப் பார்க்கவும்.
- **நடத்தை நெறிமுறை (Code of Conduct)**: சமூக நெறிமுறைகளுக்கு [`../CODE_OF_CONDUCT.md`](../CODE_OF_CONDUCT.md) ஐப் பார்க்கவும்.

---

## Translations & Localisation

| மொழி (Language) | ஆவணக் கோப்பு (Document File) |
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
