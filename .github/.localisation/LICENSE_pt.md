# Aviso de Licença Múltipla do SwordigoDesktop (Multi-License Notice)

**Estrutura de Licenciamento da AevoraLabs (prev OpenSwordigo) e Lawncher Team**

> **Documento Original em Inglês**: [English (LICENSE.md)](../../LICENSE.md) | [Todas as Traduções (All Translations)](#translations--localisation)

Este repositório é um projeto de código aberto composto cujos componentes são regidos por dois termos jurídicos distintos:
1. **GNU General Public License v3.0 (GPLv3)** — Ambiente de execução do Swordigo (SRE), ferramentas específicas do Swordigo, interface de jogo e editores.
2. **Licença MIT** — Infraestrutura genérica do host, camadas de emulação do Android e pontes JNI.

Todas as obras originais deste repositório pertencem conjuntamente à **AevoraLabs (prev OpenSwordigo)** (`QuantumCreeper`, `Msinup`, `ManoK`) e à **Lawncher Team** (`Raijin`, `Kiziyon`).

---

## 1. GNU General Public License v3.0 (GPLv3)
### Ambiente de Execução do Swordigo (SRE), Componentes de Jogo e Editores

Os seguintes subsistemas e diretórios estão licenciados sob os termos da **GNU General Public License, Versão 3 (GPLv3)**:

- **Swordigo Runtime Environment (SRE)** (`src/sre/`):
  - **`src/sre/sre13/`**: Ambiente de execução convidado do Swordigo 1.4.13, ganchos de arquitetura Caver, biblioteca matemática Lua rbmath e subsistemas de console e áudio.
  - **`src/sre/sre12/`**: Ambiente de execução convidado do Swordigo 1.4.12, ganchos principais e Mini API.
  - **`src/sre/extras/`**: Recursos estendidos do SRE, interfaces FFI, patches de memória e sistemas de arquivos de salvamento.
  - **`src/sre/base/`**: Encanamento base do motor SRE, cola ABI de tempo de execução personalizada e shims de plataforma.
  - *Desenvolvido e licenciado conjuntamente pela Lawncher Team (`Raijin`, `Kiziyon`) e AevoraLabs (`QuantumCreeper`, `Msinup`, `ManoK`).*
- **Suíte de Edição Ruby e Ruby GG IDE** (`src/ruby/`):
  - Editor Qt6 Studio, editor visual de nós `Graphy`, shaders de visualização, pipeline de iluminação e pós-processamento, motor visual Caver e ferramentas integradas.
- **Launcher Swordfare e Sobreposição no Jogo (Overlay)** (`src/launcher/`, `src/platform/`):
  - Overlay HUD no jogo, gerenciador de mods, interface do editor de save, gerenciador de perfis, reprodutor de vídeo de fundo e frontend de execução.
- **Ferramentas e Conversores Swordigo** (`src/tools/`, `tools/`):
  - Conversores de SCL/Scene para grafo, gerador de terreno boulder, formatos rubymesh, ponte glTF e compiladores de recursos.

**Copyright © 2026 Lawncher Team & AevoraLabs.**
Licenciado sob a GPLv3. Consulte [`src/sre/LICENSE.md`](../../src/sre/LICENSE.md), [`src/ruby/LICENSE.md`](../../src/ruby/LICENSE.md) e [`src/platform/LICENSE.md`](../../src/platform/LICENSE.md).

---

## 2. Licença MIT
### Infraestrutura Genérica do Host, Emulação do Android e Pontes JNI

O ambiente host genérico, os adaptadores de execução portáteis e o encanamento de emulação de baixo nível que não contêm lógica específica do jogo são licenciados sob a permissiva **Licença MIT**:

- **Emulação Genérica do Android e Pontes JNI** (`src/jni/`, `src/android/`):
  - Camadas de compatibilidade POSIX para Android, gerenciadores de recursos, registradores e pontes de serialização JNI.
- **Carregador Binário ELF e Suporte de Arquitetura** (`src/loader/`, `src/srehost/`):
  - Carregador dinâmico ELF, tabelas de realocação de símbolos e cola de limite ABI convidado-host.
- **Ajudantes Genéricos de Plataforma do Motor** (partes de `src/platform/`):
  - Invólucros genéricos de janelas, abstrações de temporizadores e decodificadores de imagem PVRTC/ASTC.

**Copyright © 2026 AevoraLabs.**
*(Partes Copyright © 2023 Rinnegatamante — Porta do Swordigo Vita; Imagination Technologies Ltd. — PVR SDK)*

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

## 3. Componentes de Terceiros e Histórico de Licenciamento

### Relicenciamento do SRE
A Lawncher Team e a AevoraLabs atualizaram conjuntamente a licença da base de código do Swordigo Runtime Environment (SRE) e todos os seus submódulos (`sre13`, `extras`, `sre12` e `base`) para a **GNU General Public License v3.0 (GPLv3)**.

### Dependências de Terceiros
- Componentes de código aberto de terceiros em `src/sre/base/` (como Lua 5.1, LuaSocket, LuaFileSystem, toml-c e RakNet) mantêm suas licenças originais (MIT, BSD, zlib).
- ufbx (`src/tools/ufbx/`) possui licença dupla MIT / Domínio Público.

---

## Matriz de Resumo (Summary Matrix)

| Diretório / Componente | Licença | Exclusividade / Detentores dos Direitos |
| :--- | :--- | :--- |
| `src/sre/` (`sre12`, `sre13`, `extras`, `base`) | **GNU GPLv3** | Conjuntamente **Lawncher Team** & **AevoraLabs** |
| `src/ruby/` (Ruby, Ruby GG Studio IDE) | **GNU GPLv3** | Exclusivamente **AevoraLabs** |
| `src/launcher/`, `src/platform/` (Interface Swordfare) | **GNU GPLv3** | Exclusivamente **AevoraLabs** |
| `src/tools/`, `tools/` (Conversores e Compiladores) | **GNU GPLv3** | Exclusivamente **AevoraLabs** |
| `src/jni/`, `src/android/` (Pontes JNI e Shims) | **MIT** | Exclusivamente **AevoraLabs** |
| `src/loader/`, `src/srehost/` (Carregador ELF Host) | **MIT** | Exclusivamente **AevoraLabs** |
| Decodificadores de Plataforma Genéricos (ASTC, PVRTC) | **MIT** | Autores originais & **AevoraLabs** |

---

## 4. Acordos Comunitários e Políticas de Governança

Todas as contribuições e o uso dos serviços online do projeto estão sujeitos aos seguintes acordos:
- **Contrato de Licença de Colaborador (CLA)**: Para termos de contribuição, consulte [`../CLA.md`](../CLA.md).
- **Modelo de Governança do Projeto**: Para a estrutura administrativa, consulte [`../GOVERNANCE.md`](../GOVERNANCE.md).
- **Termos de Uso**: Para as regras da loja online de mods, consulte [`../TERMS_OF_USE.md`](../TERMS_OF_USE.md).
- **Código de Conduta**: Para os padrões da comunidade, consulte [`../CODE_OF_CONDUCT.md`](../CODE_OF_CONDUCT.md).

---

## Translations & Localisation

| Idioma (Language) | Arquivo de Documento (Document File) |
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
