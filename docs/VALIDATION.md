# Validation

The Windows release is built and tested both locally and from a clean GitHub
checkout. CTest runs 54 groups covering ROM/ZIP validation, profile preservation,
updater integrity and rollback, room protocols, chat, story ownership and retries,
world checkpoints, rewards, wagers, camps, sprites and the underlying runtime.

Packaged launcher checks cover profile creation and rename, world selection,
host settings, native game startup, clean shutdown and return to the launcher.
Two packaged sessions also passed host/join, rejected-key recovery and host-loss
return checks. Native acceptance checks passed private checkpoints, cold reconnect,
trade invitations and abrupt host-loss handling. A real 0.26.0-to-0.26.3 update
verified every installed file and restarted with the same disposable profile.
The Windows setup wizard was installed into a separate folder; installed files
matched the package manifest, the game launched successfully, and uninstalling
preserved an extra user file. These checks used disposable profiles.

Release packages contain only explicit installation targets. Audits reject ROMs,
private saves, identities, native game-code caches and test executables. Included
artwork is copied unchanged. The 0.26.1 package and installer passed a Microsoft
Defender scan. Reorganized source files retain their original content hashes.

Previous local gameplay checks established FireRed US 1.0 boot/save/reload,
visible encounters, multiplayer rendering, real Cable Club battles and trades,
host-owned checkpoints and supported shared-story retries.

LeafGreen US v1.1 was booted from the supplied ZIP into a new game, without
reusing a FireRed savestate. Native checks cover visible wild encounters, camp
placement/movement/packing, Route 22 loss and friend takeover through shared
completion, private world checkpoints, cold reconnects, chat, trade invitations
and host-loss return. The full LeafGreen wager battle passed opponent-name and
dialogue checks, restored both teams and positions, and displayed the committed
winner/loser money results. FireRed US v1.0 passed the world-persistence regression.

The hook audit verifies 188 ROM addresses and 61 RAM references against four
published symbol layouts. Forged LeafGreen headers still fail full-image hash
validation. Real packaged clients verified matching LeafGreen joins and visible
FireRed/LeafGreen mismatch rejection before checkpoint transfer. The supplied
LeafGreen art matches the installed file byte-for-byte.

These results do not establish complete campaign coverage. FireRed US v1.1 and
LeafGreen US v1.0 real-game acceptance,
cross-PC/WAN cable timing, unusual field effects and full playthroughs need more
testing. Native compilation retains instruction fallback. Crashes can lose work
since the last completed safe checkpoint.

The separate macOS preview branch passed native Intel and Apple Silicon builds,
CTest, ROM-free launcher and packaged-app smoke checks, and ad-hoc signature
verification in GitHub Actions run `34416681074`. Both LeafGreen cartridge
previews were visually reviewed, and the packaged artwork matches the supplied
PNG byte-for-byte. These checks do not validate ROM gameplay on physical Macs
or establish full Windows feature parity. Mac updates remain manual and
Poké Ball Plus Bluetooth support is not included in the preview. No ROM or
personal save is sent to CI.

Build instructions are in the repository README. Developer acceptance checks
are grouped under `tests/integration`; historical planning and session logs are
maintained locally, outside the public source tree.
