# Source organization

Runtime artwork is organized under `assets/`; `cmake/Assets.cmake` preserves the installed paths below. All source pixels and model data remain unchanged. Unused variants and reference packs remain local.

# UI assets

The owner supplied the Fonts folder for this project. The package includes PixelOperator.ttf, PixelOperator8.ttf, PixelOperator-Bold.ttf, PixelOperatorMono.ttf and the twelve PNGs from CHAT BUBBLE TILES. Launcher and frame text use these supplied fonts.

The font files identify Pixel Operator by Jayvee Enaguas (HarvettFox96), 2009-2018, released under Creative Commons Zero (CC0) 1.0: https://creativecommons.org/publicdomain/zero/1.0/. This attribution and license identification come from their embedded name tables. The originals are copied unchanged.

All twelve current owner-supplied bubble pieces are used as 16x16 RGBA PNGs. Every source pixel, including transparent pixel data and the Poke Ball corner, is retained. Nothing is resampled, recolored, redrawn or augmented with extra artwork. One-row frames use Thin Left/Middle/Right; two rows use Upper and Bottom tiles; taller frames repeat the middle row. Text has at least three pixels of padding and additional left inset to keep it clear of the corner artwork. Wrapped text uses compact line spacing and is centered vertically within whole tile rows. The single-line layout retains its original text position. No additional authorship or license information accompanied the bubble PNGs.

The executable reads the packaged Fonts directory. CMake copies the current source files byte for byte into build/package directories. tests/integration/verify_chat_tiles.py checks source/build hashes and compares rendered frame pixels with arrangements of those same PNGs. Only padded black text may differ. The PPU compositor preserves source 8-bit channels for UI artwork in normal rendering while retaining native visibility/window tests; original GBA sprites continue using RGB555.

## Standalone package assets

The friend package includes the 386 normal directional follower PNGs used by FireRed, under `LocalAssets/Followers`. The explicit list is `cmake/FollowerAssets.cmake`; every PNG is copied byte for byte from the owner-supplied Following Pokemon EX 2.5.1 folder. The original plugin metadata and credits are included as `licenses/Following-Pokemon-EX-credits.txt`. No plugin scripts, shiny/unused alternate packs, Essentials engine files or ROM are included.

The supplied `Cart Art/Fire Red Cart Art.png`, `Poké Ball Plus/ob0008_00_gadgets.dae`, and its `ob0008_00_obj_col.png` diffuse texture are also copied unchanged. The alternate model and unused material maps stay in the source asset folder. No texture edits or resampling are applied to packaged originals. The source folders remain available locally for future development.
