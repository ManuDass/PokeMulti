# Architecture

FireRed remains the game. The launcher validates a user-supplied ROM; the runtime executes its ARM/Thumb code, reads its graphics and encounter data, and uses its own battle, story and save routines. Essentials and Following Pokemon EX were inspected as behavioral references. Their code and graphics are not runtime dependencies.

## Execution
The Windows C++20 application uses a pinned, modified gbarecomp runtime and ARM recompilation core. Its hardware layer supplies memory, PPU, DMA, timers, IRQs, audio, input and cartridge flash. A BIOS image is not required: high-level BIOS services include mGBA-derived routines under MPL-2.0 and local boot/IRQ/interrupt-wait integration. Their timing is approximate. Long service cycle budgets are consumed in short slices so timer and serial interrupts continue during transfers and map decompression.

The initial static dispatch table is empty. Missing ARM/Thumb functions are translated into C locally, compiled to x64 DLLs by the included TinyCC toolchain, and installed into dispatch tables. Uncompiled or rejected regions use instruction fallback. This is native compilation with fallback, not a completed static recompilation. Native-call and fallback diagnostics appear in runtime.log. First visits can stutter while code is compiled. Cached functions load on demand; startup does not scan and load every previously compiled function.

Generated C and DLLs are ROM-derived private cache data, scoped by ROM SHA-1, platform, backend and runtime ABI. They are excluded from installation. The launcher-only legacy probe in firered_tool is a separate limited diagnostic, not the game backend.

## Frontend and ownership
firered_recomp is a Direct2D/DirectWrite native launcher. It validates identity, stores a local profile, and launches firered_game with an explicit ROM and save path. The game process owns the guest CPU and all guest-memory access. The launcher hides during play and restores after the child exits. A pinned Dear ImGui SDL2/SDLRenderer2 shell draws the game texture and online sidebar in one SDL window. F2 toggles that sidebar. UI, input focus, rendering and guest state stay on the game thread; bounded host/join operations run asynchronously. No second friends window or cross-thread embedded window is used.

Each profile owns its identity, friends, settings, flash save and native cache. A save lock prevents concurrent use of the same save. Flash writes are debounced and atomically replaced; the prior file is retained as .bak. FireRed's Start > Save writes actual cartridge save data. Runtime snapshots exist for developer validation; they are not the normal player save flow.

## Game integration
Version-scoped hooks observe the real map, player, object events, camera, fade and party. ROM identity must match an allowlisted revision before addresses are used. Rendered peers and followers are additional nonblocking sprites, not scripted NPC replacements. Visible encounter contact invokes FireRed's original monster creation and wild battle functions.

Only the game thread reads guest state. The networking worker receives copied semantic state and serial cable words. There is no multiplayer RAM read/write API, ROM transfer, remote code execution command, or exposed generic runtime debugger in the product CLI.

## Validation boundary
Hardware checks use original synthetic programs. Gameplay checks use the owner's local ROM and ignored artifacts. Normal new-game/save/reload tests are distinguished from development-only party/map fixtures. See VALIDATION.md for measured results and limitations, and THIRD_PARTY.md for licenses and provenance.
