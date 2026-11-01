# GNU GENERAL PUBLIC LICENSE, Version 3 (GPLv3)

**Copyright © 2026 Lawncher Team & AevoraLabs (prev OpenSwordigo).**

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

---

### Copyright Holders & Authorship Attribution
The **Swordigo Runtime Environment (SRE)** and all its submodules (`sre13`, `extras`, `sre12`, and `base`) are licensed under the GNU General Public License, Version 3 (GPLv3), jointly authored and maintained by:

- **Lawncher Team**:
  - `Raijin`
  - `Kiziyon`
- **AevoraLabs (prev OpenSwordigo)**:
  - `QuantumCreeper`
  - `Msinup`
  - `ManoK`

---

### Scope of this License
This license applies to all modules, components, headers, source files, and assembly routines contained within `src/sre/`, specifically including without limitation:

1. **`src/sre/sre13/`**:
   - The Swordigo 1.4.13 guest runtime environment.
   - Caver architecture hooks (`Camera`, `CameraController`, `CaverShell`, `Component`, `CharControllerComponent`, `EntityComponent`, `GameSceneController`, `GameState`, `GameViewController`, `ModelLibrary`, `PhysicsObjectState`, `PlayerProfile`, `ProgramState`, `RenderingContext`, `Scene`, `SceneObject`, `TextureLibrary`).
   - `rbmath` raymath Lua extensions and C bindings.
   - SRE13 console, audio subsystem, recovery engine, safety hooks, and scene shifter.
   - In-game touch UI and button controllers.

2. **`src/sre/sre12/`**:
   - The Swordigo 1.4.12 guest runtime environment (`sre_init`, `sre_lua`, `sre_mini_api`, `sre_scene_update`, `sre_gui_native`, `sre_frame_loop`, `sre_vfs`, etc.).

3. **`src/sre/extras/`**:
   - Extended SRE capabilities (`sre_extras`, `sre_ffi`, `mod_fs`, `mod_saves`, `memory`, `raijin_ffi`).

4. **`src/sre/base/`**:
   - SRE base engine plumbing, custom ABI interfaces, and platform runtime glue (`sre_base.c`, `sre_setjmp.S`, `sre_host_abi.h`).

---

### Third-Party Vendored Components
- Upstream vendored components within `src/sre/base/` (such as standard Lua 5.1, LuaSocket, LuaFileSystem, toml-c, and RakNet) retain their original upstream licenses (MIT, BSD, zlib).
- All custom patches, wrappers, glue code, ABI integrations, and Swordigo-specific modifications applied to these libraries are Copyright © Lawncher Team and AevoraLabs under GNU GPLv3 terms.
