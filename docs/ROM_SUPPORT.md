# ROM support

PokéMulti accepts unmodified English FireRed and LeafGreen US v1.0/v1.1.
Select a `.gba` file or a ZIP containing exactly one `.gba` in the launcher.
**Settings & ROM → Change ROM** switches cartridges. The launcher shows the matching
cartridge color and supplied label artwork. Your online identity stays the same;
world selection shows only worlds created with the selected ROM. Switching back
to the original ROM restores that game's world list without migrating saves.

## Multiplayer compatibility

Every player in a room needs the **same game and exact ROM revision**. FireRed
and LeafGreen rooms are separate, as are v1.0 and v1.1. The handshake checks the
ROM SHA-256 before authenticating a world player or transferring a checkpoint.
A mismatch returns an explanation to the launcher's join screen.

The editions share an engine and much of the story, but their encounter tables,
version-specific scripts and ROM addresses differ. Shared world behavior and
host-owned checkpoints have not been validated across editions. Native cartridge
trading compatibility does not establish compatibility for this shared-world
protocol. Keep application versions matched too.

## Hook layouts

`src/game/rom_layout.hpp` maps each address used by our adapters separately for
all four supported identities. There is no global relocation offset: early code,
late code, scripts and graphics tables move by different amounts. RAM symbols
used by these adapters have matching addresses and sizes in all four layouts.
RAM structure offsets are shared by the matching FR/LG engine definitions.

Only factual symbol names and addresses are included, never ROM bytes or game
implementation. Identity hashes come from
[pret/pokefirered](https://github.com/pret/pokefirered). Symbol facts were checked
against the generated `.sym` files at
[pokebot-gen3 commit 5dd898f](https://github.com/40Cakes/pokebot-gen3/tree/5dd898f830775d448b06db6f5cd65b930540f146/modules/data/symbols),
which are built from the decompilation. Download these metadata files locally
and run `python tools/verify_rom_layout.py --symbols PATH` to audit the mapping.
The four filenames are `pokefirered.sym`, `pokefirered_rev1.sym`,
`pokeleafgreen.sym` and `pokeleafgreen_rev1.sym`.

ROM-backed acceptance tests require the caller's own ROM and a local savestate
created with that exact ROM. Never load a FireRed savestate into LeafGreen.
ROMs, savestates, extracted assets and personal saves stay outside public builds.

## Identity and archive checks

| Game | Revision | Header | SHA-1 |
| --- | --- | --- | --- |
| FireRed | US v1.0 | BPRE / 0 | 41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc |
| FireRed | US v1.1 | BPRE / 1 | dd5945db9b930750cb39d00c84da8571feebf417 |
| LeafGreen | US v1.0 | BPGE / 0 | 574fa542ffebb14be69902d1d36f1ec0a4afd71e |
| LeafGreen | US v1.1 | BPGE / 1 | 7862c67bdecbe21d1d69ce082ce34327e1c6ed5e |

Validation requires exactly 16 MiB, the corresponding title/game code, maker
code, fixed marker, header checksum and full-image hash. Loading the same image
as GBA or ZIP produces the same identity. There is no hash bypass.

ZIP input supports Stored and Deflate compression, up to 64 MiB compressed,
32 MiB output and 1,024 directory entries. Encryption, split archives, ZIP64,
multiple GBA members, corrupt CRCs and inconsistent headers are rejected. The
selected member is decompressed into bounded memory; archive filenames are
never used as extraction paths.

Real-game acceptance uses locally supplied FireRed US v1.0 and LeafGreen US
v1.1. FireRed US v1.1 and LeafGreen US v1.0 have symbol/layout coverage but still
need equivalent real-game acceptance. Translations and ROM hacks are unsupported.
