# AevoraLabs (prev OpenSwordigo) Project Governance Model

**Version 1.0 — Adopted September 2026**

This document establishes the organizational structure, decision-making processes, role definitions, and project stewardship guidelines for the **AevoraLabs (prev OpenSwordigo) Organization** and the **SwordigoDesktop** project.

---

## 1. Principles of Governance

AevoraLabs is governed by a **collaborative and trust-centered partnership** between **AevoraLabs** and the **Lawncher Team**. Our governance framework is designed to:
* **Foster Community Innovation**: Enable modders, engine developers, tool authors, and reverse-engineers to contribute freely under clear rules.
* **Maintain Architectural Integrity**: Ensure technical decisions maintain performance, binary stability, clean-room standards, and cross-platform compatibility.
* **Protect Intellectual Property**: Safeguard community code, reverse-engineered runtime interfaces, and project assets through unambiguous licensing tiers and clear decision-making authority.

---

## 2. Leadership Structure & Roles

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          SOLE DECISION-MAKING BODY                          │
├──────────────────────────────────────┬──────────────────────────────────────┤
│            AevoraLabs                │            Lawncher Team             │
│        (Core Maintainers)            │      (Joint SRE Custodians)          │
└──────────────────────────────────────┴──────────────────────────────────────┘
                                       │
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                            Community Contributors                           │
│                     (Engineers, Modders, QA, Tool Authors)                  │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.1 The Sole Decision-Making Body
The project is stewarded jointly by two recognized teams:

1. **AevoraLabs & Its Members**:
   - The primary administrative and development body for the overall repository, architecture, and toolchain.
   - Holds sole authority over project repositories, release pipelines, official branding, and composite licensing.
   - Responsible for the Ruby and Ruby GG Studio IDE ([`../src/ruby/`](../src/ruby/)), Swordfare launcher and game overlay ([`../src/launcher/`](../src/launcher/), [`../src/platform/`](../src/platform/)), host loaders, bridges ([`../src/jni/`](../src/jni/), [`../src/loader/`](../src/loader/)), and companion tools.

2. **Lawncher Team**:
   - The joint reverse-engineering and runtime development team.
   - Holds joint intellectual property rights and custodial authority over the Swordigo Runtime Environment ([`../src/sre/`](../src/sre/) — `sre12`, `sre13`, `extras`, `base`, licensed under GNU GPLv3).
   - Co-decides all architectural, ABI, memory layout, and runtime decisions relating to SRE.

Together, **AevoraLabs and Lawncher Team constitute the sole decision-making authority** for the project.

### 2.2 Contributors
Any community member who submits issues, pull requests, patches, documentation, or tooling improvements. Contributors retain dual copyright over their individual contributions in accordance with the [Contributor License Agreement](./CLA.md), while granting the project perpetual and irrevocable distribution rights.

---

## 3. Decision-Making Procedures

### 3.1 Subsystem Ownership & Consensus
* **Repository & Tools ([`../src/ruby/`](../src/ruby/), [`../src/launcher/`](../src/launcher/), [`../src/platform/`](../src/platform/), [`../src/tools/`](../src/tools/), [`../src/jni/`](../src/jni/), [`../src/loader/`](../src/loader/))**:
  - Maintained and decided directly by AevoraLabs and its members.
* **SRE Subsystem ([`../src/sre/`](../src/sre/))**:
  - Managed jointly under GNU GPLv3. Any architectural changes, ABI updates, hooking redesigns, or distribution modifications for SRE require mutual consensus between AevoraLabs and Lawncher Team.

### 3.2 Review and Merges
* All pull requests require review and approval from AevoraLabs or Lawncher Team maintainers before merging.
* Merges proceed once automated test suites pass and technical review criteria are satisfied.

---

## 4. Relicensing, Commercialization & Trust Policy

1. **Mutual Trust Model**: While our legal agreements establish strict, enforceable copyright boundaries, our day-to-day operations rely on mutual trust between the maintainers and our contributors.
2. **Relicensing & Packaging Authority**: AevoraLabs and Lawncher Team retain full authority over release packaging, binary builds, and distribution models for their respective subsystems without requiring individual approval from historic contributors.
3. **No Project Hostage-Taking**: Because all contributions are granted perpetually and irrevocably under the [CLA](./CLA.md), the repository can never be frozen, obstructed, or subjected to retroactive takedowns by departing contributors.
4. **Contributor Recognition**: All contributors are permanently recorded in project commit history, documentation, and release credits.

---

## 5. Amendments to Governance

Amendments to this governance model require the joint agreement of AevoraLabs and the Lawncher Team.

---

## 6. Related Community Policies

* **[Contributor License Agreement (CLA)](./CLA.md)**: Intellectual property terms and dual-ownership model.
* **[Contributing Guidelines](./CONTRIBUTING.md)**: Code standards, review process, and checklist.
* **[Code of Conduct](./CODE_OF_CONDUCT.md)**: Community standards and enforcement.
* **[Terms of Use](./TERMS_OF_USE.md)**: Mod Store terms and online infrastructure acceptable use.
