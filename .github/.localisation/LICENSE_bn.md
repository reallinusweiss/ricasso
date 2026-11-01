# SwordigoDesktop বহু-লাইসেন্স বিজ্ঞপ্তি (Multi-License Notice)

**AevoraLabs (prev OpenSwordigo) ও Lawncher Team লাইসেন্সিং ফ্রেমওয়ার্ক**

> **মূল ইংরেজি নথি**: [English (LICENSE.md)](../../LICENSE.md) | [সকল অনুবাদ (All Translations)](#translations--localisation)

এই রিপোজিটরিটি একটি যৌগিক (composite) ওপেন-সোর্স প্রকল্প যার বিভিন্ন উপাদান দুটি স্বতন্ত্র শর্তাবলীর অধীনে লাইসেন্সপ্রাপ্ত:
1. **GNU General Public License v3.0 (GPLv3)** — Swordigo রানটাইম এনভায়রনমেন্ট (SRE), Swordigo-নির্দিষ্ট টুলস, গেম ফ্রন্টএন্ড এবং এডিটর।
2. **MIT License** — সাধারণ হোস্ট অবকাঠামো, অ্যান্ড্রয়েড এমুলেশন স্তর এবং JNI ব্রিজ।

এই রিপোজিটরির সমস্ত মূল কাজ যৌথভাবে **AevoraLabs (prev OpenSwordigo)** (`QuantumCreeper`, `Msinup`, `ManoK`) এবং **Lawncher Team** (`Raijin`, `Kiziyon`) এর মালিকানাধীন।

---

## ১. GNU General Public License v3.0 (GPLv3)
### Swordigo রানটাইম এনভায়রনমেন্ট (SRE), গেম ও এডিটর উপাদানসমূহ

নিম্নলিখিত সাবসিস্টেম এবং ডিরেক্টরিগুলি **GNU General Public License, Version 3 (GPLv3)** এর শর্তাবলীর অধীনে লাইসেন্সপ্রাপ্ত:

- **Swordigo Runtime Environment (SRE)** (`src/sre/`):
  - **`src/sre/sre13/`**: Swordigo 1.4.13 গেস্ট রানটাইম, Caver আর্কিটেকচার হুক, rbmath Lua গণিত লাইব্রেরি, কনসোল ও অডিও সাবসিস্টেম।
  - **`src/sre/sre12/`**: Swordigo 1.4.12 গেস্ট রানটাইম, কোর হুক এবং Mini API।
  - **`src/sre/extras/`**: সম্প্রসারিত SRE মডিউল, FFI ইন্টারফেস, মেমরি প্যাচ এবং সেভ ফাইল সিস্টেম।
  - **`src/sre/base/`**: SRE বেস ইঞ্জিন প্লাম্বিং, কাস্টম রানটাইম ABI গ্লু এবং প্ল্যাটফর্ম শিম।
  - *যৌথভাবে Lawncher Team (`Raijin`, `Kiziyon`) এবং AevoraLabs (`QuantumCreeper`, `Msinup`, `ManoK`) কর্তৃক লাইসেন্সপ্রাপ্ত।*
- **Ruby ও Ruby GG IDE স্যুট** (`src/ruby/`):
  - Qt6 স্টুডিও এডিটর, `Graphy` ভিজ্যুয়াল নোড এডিটর, ভিউপোর্ট শেডার, আলো ও পোস্ট-প্রসেসিং পাইপলাইন, Caver ভিজ্যুয়াল ইঞ্জিন এবং ইন্টিগ্রেটেড টুলস।
- **Swordfare লঞ্চার ও গেম ওভারলে** (`src/launcher/`, `src/platform/`):
  - ইন-গেম HUD ওভারলে, মড ম্যানেজার, সেভ এডিটর UI, প্রোফাইল ম্যানেজার, ব্যাকগ্রাউন্ড ভিডিও প্লেয়ার এবং রানটাইম ফ্রন্টএন্ড।
- **Swordigo টুলস ও কনভার্টার** (`src/tools/`, `tools/`):
  - SCL/Scene থেকে গ্রাফ কনভার্টার, বোল্ডার ভূখণ্ড জেনারেটর, rubymesh ফরম্যাট, glTF ব্রিজ এবং অ্যাসেট কম্পাইলার।

**কপিরাইট © 2026 Lawncher Team ও AevoraLabs.**
GPLv3 এর অধীনে লাইসেন্সপ্রাপ্ত। দেখুন [`src/sre/LICENSE.md`](../../src/sre/LICENSE.md), [`src/ruby/LICENSE.md`](../../src/ruby/LICENSE.md), এবং [`src/platform/LICENSE.md`](../../src/platform/LICENSE.md)।

---

## ২. MIT License
### সাধারণ হোস্ট অবকাঠামো, অ্যান্ড্রয়েড ও JNI ব্রিজ

সাধারণ হোস্ট পরিবেশ, পোর্টেবল রানটাইম শিম এবং নিম্নস্তরের এমুলেশন প্লাম্বিং যা নির্দিষ্ট গেম লজিক ধারণ করে না তা শিথিল **MIT License** এর অধীনে লাইসেন্সপ্রাপ্ত:

- **সাধারণ অ্যান্ড্রয়েড এমুলেশন ও JNI ব্রিজ** (`src/jni/`, `src/android/`):
  - POSIX অ্যান্ড্রয়েড শিম স্তর, অ্যাসেট ম্যানেজার, লগার এবং JNI মার্শাল ব্রিজ।
- **বাইনারি ELF লোডার ও আর্কিটেকচার সমর্থন** (`src/loader/`, `src/srehost/`):
  - ডায়নামিক ELF লোডার, সিম্বল রিলোকেশন টেবিল এবং গেস্ট-হোস্ট ABI সীমানা স্তর।
- **সাধারণ ইঞ্জিন প্ল্যাটফর্ম সহায়ক** (`src/platform/` এর অংশ):
  - সাধারণ উইন্ডোয়িং র‍্যাপার, টাইমার অ্যাবস্ট্রাকশন এবং PVRTC/ASTC চিত্র ডিকোডার।

**কপিরাইট © 2026 AevoraLabs.**
*(কিছু অংশ কপিরাইট © 2023 Rinnegatamante — Swordigo Vita Port; Imagination Technologies Ltd. — PVR SDK)*

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

## ৩. তৃতীয় পক্ষের উপাদান ও লাইসেন্সিং ইতিহাস

### SRE-এর পুনরায় লাইসেন্সকরণ
Lawncher Team এবং AevoraLabs যৌথভাবে Swordigo রানটাইম এনভায়রনমেন্ট (SRE) এবং এর সমস্ত সাবমডিউল (`sre13`, `extras`, `sre12`, এবং `base`) এর লাইসেন্সকে **GNU General Public License v3.0 (GPLv3)** এ রূপান্তর করেছে।

### তৃতীয় পক্ষের নির্ভরতা
- `src/sre/base/` এ অবস্থিত তৃতীয় পক্ষের ওপেন-সোর্স উপাদানগুলি (যেমন Lua 5.1, LuaSocket, LuaFileSystem, toml-c, এবং RakNet) তাদের মূল লাইসেন্স (MIT, BSD, zlib) বজায় রাখে।
- ufbx (`src/tools/ufbx/`) MIT / Public Domain দ্বৈত লাইসেন্সপ্রাপ্ত।

---

## সারাংশ ম্যাট্রিক্স (Summary Matrix)

| ডিরেক্টরি / উপাদান | লাইসেন্স | একচেটিয়া / কপিরাইট ধারক |
| :--- | :--- | :--- |
| `src/sre/` (`sre12`, `sre13`, `extras`, `base`) | **GNU GPLv3** | যৌথভাবে **Lawncher Team** & **AevoraLabs** |
| `src/ruby/` (Ruby, Ruby GG Studio IDE) | **GNU GPLv3** | একচেটিয়াভাবে **AevoraLabs** |
| `src/launcher/`, `src/platform/` (Swordfare UI) | **GNU GPLv3** | একচেটিয়াভাবে **AevoraLabs** |
| `src/tools/`, `tools/` (কনভার্টার ও কম্পাইলার) | **GNU GPLv3** | একচেটিয়াভাবে **AevoraLabs** |
| `src/jni/`, `src/android/` (JNI ব্রিজ ও শিম) | **MIT** | একচেটিয়াভাবে **AevoraLabs** |
| `src/loader/`, `src/srehost/` (হোস্ট ELF লোডার) | **MIT** | একচেটিয়াভাবে **AevoraLabs** |
| সাধারণ প্ল্যাটফর্ম ডিকোডার (ASTC, PVRTC) | **MIT** | মূল লেখক ও **AevoraLabs** |

---

## ৪. কমিউনিটি চুক্তি ও পরিচালনা নীতি

প্রকল্পে সমস্ত অবদান এবং অনলাইন পরিকাঠামোর ব্যবহার নিম্নলিখিত চুক্তিসমূহের অধীন:
- **কন্ট্রিবিউটর লাইসেন্স চুক্তি (CLA)**: অবদানের শর্তাবলী এবং 50/50 কপিরাইট ধরে রাখার নিয়মের জন্য [`../CLA.md`](../CLA.md) দেখুন।
- **প্রকল্প পরিচালনা মডেল (Governance)**: প্রশাসনিক কাঠামো এবং সিদ্ধান্ত গ্রহণের জন্য [`../GOVERNANCE.md`](../GOVERNANCE.md) দেখুন।
- **ব্যবহারের শর্তাবলী (Terms of Use)**: অনলাইন মড স্টোর এবং নেটওয়ার্ক ব্যবহারের জন্য [`../TERMS_OF_USE.md`](../TERMS_OF_USE.md) দেখুন।
- **আচরণবিধি (Code of Conduct)**: সম্প্রদায়ের মানদণ্ডের জন্য [`../CODE_OF_CONDUCT.md`](../CODE_OF_CONDUCT.md) দেখুন।

---

## Translations & Localisation

| ভাষা (Language) | অনুবাদ নথি (Document File) |
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
