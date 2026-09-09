# ROM support

Unmodified English FireRed US v1.0 and v1.1 are allowlisted. End-to-end gameplay validation used the owner's US v1.0 image; v1.1 has identity/layout coverage but still needs a full gameplay run.

| Revision | Header | SHA-1 |
| --- | --- | --- |
| US v1.0 | BPRE / 0 | 41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc |
| US v1.1 | BPRE / 1 | dd5945db9b930750cb39d00c84da8571feebf417 |

Identity metadata: [FireRed revision hashes](https://github.com/pret/pokefirered). SHA-1 is an exact known-image lookup; SHA-256 detects changes to the selected local ROM. Both hashes cover decompressed ROM bytes, so loading a GBA directly or from ZIP produces the same identity and save location.

Validation also requires exactly 16 MiB, the title/game code, maker code, fixed marker and header checksum. Unsupported or modified images do not unlock gameplay. There is no hash bypass, ROM downloader or unsupported-ROM execution switch.

## ZIP loading
The native picker, --rom argument and validator accept .gba and .zip. A ZIP must contain exactly one .gba member. Stored and Deflate compression are supported. The archive is bounded to 64 MiB, the output to 32 MiB and the directory to 1,024 entries. Encryption, split archives, ZIP64, ambiguous multiple-ROM archives, unsupported compression, corrupt CRC and inconsistent headers are rejected.

The selected member is decompressed into bounded process memory. Archive filenames are never used as extraction paths; no ROM is extracted to disk. The supplied archive stays where its owner put it.

The local v1.0 ROM's SHA-256 was 3d0c79f1627022e18765766f6cb5ea067f6b5bf7dca115552189ad65a5c3a8ac. No v1.1 ROM was supplied for a real-game run.

LeafGreen, translations and ROM hacks require separate verified integration. No ROM, BIOS, game source, art, music or Essentials assets are included in the distributable. Never share the whole workspace, which contains private reference material and runtime caches.
