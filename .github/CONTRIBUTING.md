# Contributing to AevoraLabs (prev OpenSwordigo)

Thank you for your interest in contributing to **SwordigoDesktop**, the **Ruby GG Studio IDE**, the **Swordfare Launcher**, and the **Swordigo Runtime Environment (SRE)**!

We welcome contributions from developers, modders, 3D artists, reverse-engineers, and documentation writers. To keep our codebase robust, stable, and legally clear, all contributions must follow the guidelines outlined below.

---

## 1. Quick Start: Understand the Licensing Tiers

Before writing or editing code, identify which **licensing tier** your target directory belongs to:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            REPOSITORY DIRECTORY                             │
├───────────────────────┬─────────────────────────────┬───────────────────────┤
│    Tier 1: SRE Core   │  Tier 2: Studio & Tooling   │ Tier 3: Host Plumbing │
│       src/sre/        │  src/ruby/, src/launcher/,  │  src/jni/, src/loader │
│                       │   src/platform/, src/tools/ │   src/android/        │
├───────────────────────┼─────────────────────────────┼───────────────────────┤
│       GNU GPLv3       │       GNU GPLv3             │     MIT License       │
│ Joint Lawncher Team & │ Reciprocal copyleft.        │ Strictly permissive.  │
│ AevoraLabs custodians │ Modifications MUST remain   │ General shims and     │
│ 50/50 dual ownership. │ GPLv3 (cannot down-license).│ portable loaders.     │
└───────────────────────┴─────────────────────────────┴───────────────────────┘
```

> **Contributor License Agreement**: By opening a pull request or submitting code to this repository, you agree to the terms of the [AevoraLabs (prev OpenSwordigo) Contributor License Agreement (CLA.md)](./CLA.md). You retain copyright in your own original work while granting the project the perpetual, irrevocable right to distribute and maintain the composite software.

---

## 2. Technical Standards & Code Requirements

### 2.1 Language & Toolchain Standards
* **Host C++ Code ([`../src/ruby/`](../src/ruby/), [`../src/launcher/`](../src/launcher/), [`../src/platform/`](../src/platform/), [`../src/tools/`](../src/tools/))**:
  * Written in standard **C++17**.
  * Use modern RAII patterns, smart pointers (`std::unique_ptr`, `std::shared_ptr`), and Qt6 idioms where appropriate.
  * Avoid raw `new`/`delete` calls outside dedicated low-level graphics allocations.
* **Guest SRE Code ([`../src/sre/`](../src/sre/))**:
  * Written in **C99** and freestanding assembly (`.S`).
  * Must be cross-compilable using `aarch64-linux-gnu-gcc` with `-nostdlib -fPIC -fno-stack-protector`.
  * Do not call standard glibc functions directly in guest code; use the freestanding shims provided in [`../src/sre/base/include/`](../src/sre/base/include/).
* **Python Tooling ([`../tools/`](../tools/), [`../src/tools/rubyforge/`](../src/tools/rubyforge/))**:
  * Written in modern **Python 3.10+**.
  * Keep code free from transient bytecode cache (`__pycache__/`, `*.pyc`).

### 2.2 Clean-Room Engineering Rules
* **No Direct Decompiled Code Copies**: While reverse engineering disassembly in Ghidra/IDA is standard for understanding game structures, all SRE shims, hooks, and loaders must be **clean-room implementations** written in clean C/C++.
* **No Proprietary Game Assets in Pull Requests**: Do **not** commit Touch Foo's original commercial `.pod` models, `.pvr` textures, `.wav`/`.mp3` sound files, or `.scene` level descriptors to the Git repository. Use procedural test meshes, synthetic fixtures, or programmatic tests.

---

## 3. Contribution Workflow

### 3.1 Branching & Commit Guidelines
1. **Fork and Branch**:
   ```bash
   git clone https://github.com/TheAevoraLabs/SwordigoDesktop.git
   cd SwordigoDesktop
   git checkout -b feature/your-feature-name
   ```
2. **Conventional Commits**:
   Use clear, conventional commit prefixes:
   * `feat:` — New user-facing feature, node type, or tool.
   * `fix:` — Bug fix or crash resolution.
   * `refactor:` — Code reorganization without functional change.
   * `perf:` — Performance optimization (e.g. JIT fast-path or VBO batching).
   * `docs:` — Documentation, README, or license updates.
   * `test:` — Adding or updating test cases.
3. **Commit Sign-Off**:
   Include a Git sign-off indicating agreement with the [CLA](./CLA.md):
   ```bash
   git commit -s -m "feat(ruby): add custom node category to Graphy canvas"
   ```

### 3.2 Building and Verifying Locally
Before opening a pull request, compile all targets and execute the test suite:

```bash
# Configure build
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
# Build all binaries
cmake --build build -j$(nproc)

# Run full automated test suite
ctest --test-dir build --output-on-failure
```

Ensure all tests pass with zero warnings or linker errors.

---

## 4. Pull Request Checklist

When submitting your Pull Request, verify that:
- [ ] Your PR description clearly explains the problem solved or feature added.
- [ ] Code compiles cleanly without new compiler warnings.
- [ ] Changes respect the directory licensing tier (GPLv3 in [`../src/sre/`](../src/sre/) and [`../src/ruby/`](../src/ruby/), MIT in [`../src/jni/`](../src/jni/)).
- [ ] Commit does not mix binary artifacts, `.so` files, zip archives, or `__pycache__` directories.
- [ ] Automated unit and regression tests in [`../tests/`](../tests/) pass.
- [ ] You acknowledge and accept the [AevoraLabs CLA](./CLA.md).

---

## 5. Community & Governance

For questions, architectural discussions, or collaboration on reverse engineering:
* Read our [Governance Model](./GOVERNANCE.md) to understand how decisions are made.
* Review our [Code of Conduct](./CODE_OF_CONDUCT.md) for community standards.
* Review our [Terms of Use](./TERMS_OF_USE.md) for Online Mod Store and network infrastructure usage rules.
* Check root [`../LICENSE.md`](../LICENSE.md) for overall project licensing terms.
* Join the AevoraLabs developer discussions and modding channels.
