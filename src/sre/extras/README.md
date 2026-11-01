# SRE Extras

Extended capabilities for SwordigoDesktop's SRE (Swordigo Runtime Emulator).

Provides advanced modding APIs when loaded alongside `libsre12.so` **or**
`libsre13.so` (the module is ABI-aware):

- **Mini.MemoryAddress** — Raw memory access userdata
- **FFI** — Full libffi-backed foreign function interface
- **Raijin FFI** — Signature-based call dispatch
- **Mod hooks** — Button lifecycle, filesystem, saves

## ABI checker (version-dependent offsets)

No engine-version-specific constant is hardcoded in this module anymore.
The host passes a `SreExtrasAbi` block (part of `SreExtrasInit`) for the
RUNNING binary — `swordi_abi`, the CppString layout
(`cppstring_data_off` = 0, `cppstring_rep_len` = 24 for GNU COW
std::string) and the resolved `SceneObject::ComponentWithInterface` guest
vaddr — and the module publishes it into the global `g_sre_extras_abi`
after `sre_extras_abi_validate()` clamps it to known-good values. All
consumers (mod_saves string reads, memory.c component lookup / CppString
length) read that global.

Host wiring: `src/main.cpp` SRE-Extras block (loads for both ABIs, fills
`abi` + Lua API, wires `g_sre_extras_miniLL_open_memory` into the loaded
SRE module). When the module is absent, `libsre12.so` falls back to
`sre/sre12/sre_extras_stubs.c` and `libsre13.so` to
`sre/sre13/sre13_extras_stubs.c` — same safe Mini/ffi stub surface.

## Build

```bash
cmake -S . -B build
cmake --build build
```

Requires `aarch64-linux-gnu-gcc` cross-compiler.

## License

Licensed under the GNU General Public License v3.0 (GPLv3). See `LICENSE` for details.
