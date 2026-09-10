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

## ROM switching and shiny settings (0.26.4)

GitHub Actions run `34420844227` passed the clean Windows build, all 54 test
groups, audited packaging and installer publication. The updater shipped in
0.26.3 downloaded the public 0.26.4 release, verified its archive and full
manifest, installed it into an isolated copy, and restarted with the same data
directory. Every installed file matched and disposable user data was preserved.

The real Windows file chooser passed FireRed → LeafGreen → FireRed switching,
cancellation, per-ROM world filtering and unchanged username/identity/friend data.
Launcher captures verify the green LeafGreen top bar and controls. The game UI
was reviewed in both themes, including World options at the minimum 940×650 size
with no options scrollbar or overlap with the game frame.

Two native clients using each supplied ROM (FireRed US 1.0 and LeafGreen US 1.1)
passed live host shiny changes and disabled guest editing. At 1 in 1, each client
generated 32 shiny random wild Pokémon and 32 shiny nature-selected Pokémon
through the native creator, with valid data and preserved fixed-personality
records. Restoring 1 in 8,192 restored native generation. Unit tests additionally
cover rate bounds, per-world persistence, reconnecting clients, trait constraints
and every Unown form. The ROM audit now checks unmapped direct calls, and map
ordering is enforced at compile time.

The hook audit verifies 194 ROM addresses and 61 RAM references against four
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
verification in GitHub Actions run `34421232551` for preview 0.27.1. Portable
tests also cover profile-preserving ROM switches, shiny traits, world isolation
and host/guest shiny settings. Both LeafGreen cartridge
previews were visually reviewed, and the packaged artwork matches the supplied
PNG byte-for-byte. These checks do not validate ROM gameplay on physical Macs
or establish full Windows feature parity. Mac updates remain manual and
Poké Ball Plus Bluetooth support is not included in the preview. No ROM or
personal save is sent to CI.

Build instructions are in the repository README. Developer acceptance checks
are grouped under `tests/integration`; historical planning and session logs are
maintained locally, outside the public source tree.
