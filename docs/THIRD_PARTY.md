# Dependencies and distribution

This project vendors generic runtime/compiler/library code. The standalone package excludes ROMs, BIOS images, generated cartridge translations and Nintendo game source. It includes the owner-supplied artwork used by the application: fonts, chat tiles, icons, camp frames, 386 normal follower sheets, the cartridge label and the controller preview model/texture. See UI_ASSETS.md and the included follower credits. Full Essentials/reference packs and their scripts are excluded.

| Dependency | Pin | Terms |
| --- | --- | --- |
| [mstan/gbarecomp](https://github.com/mstan/gbarecomp) | a1b7b337221cf407d531c12b4d641fcd81c7a976 | PolyForm Noncommercial 1.0.0 |
| [mstan/arm-recomp-core](https://github.com/mstan/arm-recomp-core) | 763b922f4912708d2704e7833f18addfbf8ddf33 | MIT |
| [SDL2](https://github.com/libsdl-org/SDL) | 2.32.10 / 5d249570393f7a37e037abf22cd6012a4cc56a71 | zlib |
| [zlib](https://github.com/madler/zlib) | 1.3.1 / 51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf | zlib |
| [toml++](https://github.com/marzer/tomlplusplus) | 3.4.0 / 30172438cee64926dc41fdd9c11fb3ba5b2ba9de | MIT |
| [TinyCC](https://www.nongnu.org/tinycc/) | official 0.9.27 Win64 binary + corresponding source | LGPL 2.1; see included file notices |

gbarecomp includes mGBA-derived BIOS HLE under MPL-2.0, and JRickey/gba-recomp ports under MIT OR Apache-2.0. Their attribution and license texts are included. mGBA itself is not vendored; the actual modified covered bios_hle source files are supplied in the package.

**The current runtime dependency restricts use to noncommercial purposes.** Read the included PolyForm license before redistributing or using the build. This is a material dependency term, not a license for Nintendo assets.

TinyCC is used as a separate local compiler process. Its complete 0.9.27 source and notices accompany the package, together with the shipped compiler. Binary SHA-256: 34a721949a2583fdff725312da092fa0f5f1f284b702e6f811c6954714faabb2. Source archive SHA-256: de23af78fca90ce32dff2dd45b3432b2334740bb9bb7b05bf60fdbfc396ceb9c.

## Local runtime modifications
- Source-only build integration with an empty BIOS dispatch stub and empty game dispatch corpus.
- BIOSless entry, boot/IRQ/interrupt-wait services, sliced long-service cycle budgets, portable leading-zero count and unsupported-service failure.
- In-memory ROM input, explicit save paths, atomic Windows save replacement/backup, UTF-8 paths.
- Game-ready/input/frame/render callbacks and developer snapshot output.
- Measured native/fallback dispatch, interpreter bridge frame yielding, on-demand native-cache loading, private diagnostic paths and bundled TinyCC/ABI headers.
- Multiplayer SIO adapter, timed transfers, IRQ-disabled polling, port-reset cancellation and IRQ delivery.
- Diagnostic wording corrected for empty/static dispatch and BIOSless operation.
- Optional latched native composition context for host sprites, preserving game layer priorities, OAM order, windows and color effects without guest-memory changes.

The bundled license-source directory contains the modified MPL-covered files and corresponding TinyCC source. The workspace contains the full vendored generic runtime used to build. No third-party source is represented as wholly original project code.

Dear ImGui 1.91.9b, MIT, commit f5befd2d29e66809cd1110a152e375a7f1981f06 from https://github.com/ocornut/imgui supplies the integrated SDL2 sidebar renderer. The unmodified core and SDL backends are vendored; its MIT notice accompanies the package. Launcher and frame text use the included owner-supplied Pixel Operator fonts. The twelve bubble tiles are also included for chat; see UI_ASSETS.md for their provenance.
