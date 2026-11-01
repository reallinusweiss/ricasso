# Avis de Multi-Licence SwordigoDesktop
 
**Cadre de Licence du Projet AevoraLabs (prev OpenSwordigo) & Lawncher Team**

> **Document original en anglais** : [English (LICENSE.md)](../../LICENSE.md) | [Toutes les Traductions (All Translations)](#translations--localisation)

Ce dépôt constitue un projet composite open-source dont les composants sont régis par deux régimes juridiques distincts :
1. **GNU General Public License v3.0 (GPLv3)** — Swordigo Runtime Environment (SRE), outils spécifiques à Swordigo, interface de jeu et éditeurs.
2. **Licence MIT** — Infrastructure hôte générique, couches d'émulation Android et ponts JNI.

Les œuvres originales de ce dépôt sont concédées sous licence par **AevoraLabs (prev OpenSwordigo)** (`QuantumCreeper`, `Msinup`, `ManoK`) et **Lawncher Team** (`Raijin`, `Kiziyon`) comme détaillé ci-dessous.

---

## 1. GNU General Public License v3.0 (GPLv3)
### Swordigo Runtime Environment (SRE), Composants de Jeu et Éditeurs

Les sous-systèmes et répertoires suivants sont concédés sous les termes de la **GNU General Public License, Version 3 (GPLv3)** :

- **Swordigo Runtime Environment (SRE)** (`src/sre/`) :
  - **`src/sre/sre13/`** : Environnement d'exécution invité Swordigo 1.4.13, points d'ancrage de l'architecture Caver, bibliothèque mathématique Lua rbmath et sous-systèmes console/audio.
  - **`src/sre/sre12/`** : Environnement d'exécution invité Swordigo 1.4.12, points d'ancrage centraux et mini-API.
  - **`src/sre/extras/`** : Extensions SRE étendues, interfaces FFI, correctifs mémoire et systèmes de fichiers de sauvegarde.
  - **`src/sre/base/`** : Plomberie de base du moteur SRE, liant ABI d'exécution personnalisé et adaptateurs de plateforme.
  - *Créé et concédé conjointement sous licence par Lawncher Team (`Raijin`, `Kiziyon`) et AevoraLabs (`QuantumCreeper`, `Msinup`, `ManoK`).*
- **Suite d'Édition Ruby et Ruby GG IDE** (`src/ruby/`) :
  - Éditeur Studio Qt6, éditeur de nœuds visuel `Graphy`, shaders de vue 3D, pipeline d'éclairage et de post-traitement, moteur visuel caver et outils intégrés.
- **Lanceur Swordfare et Overlay en Jeu** (`src/launcher/`, `src/platform/`) :
  - Overlay HUD en jeu, gestionnaire de mods, interface d'édition de sauvegarde, gestionnaire de profils, lecteur vidéo d'arrière-plan et interface d'exécution.
- **Outillage et Convertisseurs Swordigo** (`src/tools/`, `tools/`) :
  - Convertisseurs SCL/Scene vers graphe, générateur de terrain boulder, formats rubymesh, passerelle glTF et compilateurs d'assets.

**Copyright © 2026 Lawncher Team & AevoraLabs.**
Licencié sous GPLv3. Voir [`src/sre/LICENSE.md`](../../src/sre/LICENSE.md), [`src/ruby/LICENSE.md`](../../src/ruby/LICENSE.md), et [`src/platform/LICENSE.md`](../../src/platform/LICENSE.md).

---

## 2. Licence MIT
### Infrastructure Hôte Générique, Émulation Android et Ponts JNI

L'environnement hôte générique, les adaptateurs d'exécution portables et la plomberie d'émulation de bas niveau ne contenant pas de logique propre au jeu sont concédés sous la licence permissive **MIT** :

- **Émulation Android Générique et Ponts JNI** (`src/jni/`, `src/android/`) :
  - Couches d'adaptation POSIX Android, gestionnaires d'assets, journalisation et ponts de sérialisation JNI.
- **Chargeur de Binaires ELF et Support d'Architecture** (`src/loader/`, `src/srehost/`) :
  - Chargeur dynamique ELF, tables de relocalisation de symboles et liant d'interface ABI invité-hôte.
- **Assistants de Plateforme Génériques** (parties de `src/platform/`) :
  - Wrappers de fenêtrage génériques, abstractions de minuteurs et décodeurs d'images PVRTC/ASTC.

**Copyright © 2026 AevoraLabs.**
*(Certaines parties Copyright © 2023 Rinnegatamante — Port Swordigo Vita ; Imagination Technologies Ltd. — PVR SDK)*

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

## 3. Composants Tiers et Historique de Licence

### Changement de Licence pour SRE
L'équipe Lawncher et AevoraLabs ont conjointement basculé la licence de l'environnement d'exécution Swordigo (SRE) vers la **GNU General Public License v3.0 (GPLv3)** pour l'ensemble des sous-modules : `sre13`, `extras`, `sre12` et `base`.

### Dépendances Tierces
Les dépendances tierces conservent leurs licences open-source d'origine respectives :
- Lua 5.1, LuaSocket, LuaFileSystem, toml-c et RakNet dans `src/sre/base/` conservent leurs licences d'origine (MIT, BSD, zlib).
- ufbx (`src/tools/ufbx/`) est sous double licence MIT / Domaine Public.

---

## Matrice Récapitulative (Summary Matrix)

| Répertoire / Composant | Licence | Exclusivité / Titulaires du Copyright |
| :--- | :--- | :--- |
| `src/sre/` (`sre12`, `sre13`, `extras`, `base`) | **GNU GPLv3** | Conjointement détenu par **Lawncher Team** (`Raijin`, `Kiziyon`) & **AevoraLabs** (`QuantumCreeper`, `Msinup`, `ManoK`) |
| `src/ruby/` (Ruby, Studio IDE Ruby GG) | **GNU GPLv3** | Exclusivement **AevoraLabs** |
| `src/launcher/`, `src/platform/` (UI Swordfare) | **GNU GPLv3** | Exclusivement **AevoraLabs** |
| `src/tools/`, `tools/` (Convertisseurs & Compilateurs) | **GNU GPLv3** | Exclusivement **AevoraLabs** |
| `src/jni/`, `src/android/` (Ponts JNI & Shims) | **MIT** | Exclusivement **AevoraLabs** |
| `src/loader/`, `src/srehost/` (Chargeur ELF Hôte) | **MIT** | Exclusivement **AevoraLabs** |
| Décodeurs de Plateforme Génériques (ASTC, PVRTC) | **MIT** | Auteurs d'origine & **AevoraLabs** |

---

## 4. Accords Communautaires et Politiques de Gouvernance

Toutes les contributions et l'utilisation de l'infrastructure en ligne du projet sont soumises aux accords suivants :
- **Contrat de Licence Contributeur (CLA)** : Voir [`../CLA.md`](../CLA.md) pour les conditions de contribution.
- **Gouvernance du Projet** : Voir [`../GOVERNANCE.md`](../GOVERNANCE.md) pour la structure administrative.
- **Conditions d'Utilisation** : Voir [`../TERMS_OF_USE.md`](../TERMS_OF_USE.md) pour l'infrastructure réseau et mod store.
- **Code de Conduite** : Voir [`../CODE_OF_CONDUCT.md`](../CODE_OF_CONDUCT.md) pour les normes communautaires.

---

## Translations & Localisation

| Langue (Language) | Fichier de Document (Document File) |
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
