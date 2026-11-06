# Credits and project origin

- **Touch Foo / Ville Mäkynen** — the original Swordigo game. The game and its original assets are separate from this compatibility project.
- **AevoraLabs (formerly OpenSwordigo)** — SwordigoDesktop / Swordfare, the desktop compatibility infrastructure, tools and frontend. The upstream notices name QuantumCreeper, Msinup and ManoK.
- **Lawncher Team** — SRE runtime work, jointly credited with AevoraLabs. The upstream notices name Raijin and Kiziyon.
- **Rinnegatamante and other upstream contributors** — prior porting work and components identified in the original notices and Git history.
- **Windows client adaptation** — Windows build and runtime compatibility fixes and input persistence, developed by the Windows port maintainer with OpenAI Codex assistance. Upstream authorship and ownership notices are retained.
- **Ricasso** — the starter, packaging, cleanup and this reduced distribution, by reallinusweiss. This part is vibe-coded: written with AI tools (Claude and OpenAI Codex).

The launcher/UI images in `src/assets/` (backgrounds, profile icons, window icons, splash) are original artwork generated procedurally by `tools/windows/make_original_assets.ps1`. Images taken from the game (logo, key art, app icon, character renders, level backgrounds, UI atlas) were removed from this tree. The sword-in-block icon (`icon_app.png`, `launcer_icon.png`) is upstream project artwork.

Upstream: https://github.com/TheAevoraLabs/SwordigoDesktop

Baseline: `072db6cc335c1c0bad41b965986738eb3840ab6d`.

Third-party components include Dynarmic and its dependencies, SDL, Dear ImGui, Qt, OpenAL Soft, Lua and the other libraries identified in their individual license files. This list is not a replacement for the original notices. Vendored source keeps its license files; Windows binary packages carry additional dependency notices.
