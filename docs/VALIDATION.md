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
verification in GitHub Actions run `34426008007` for preview 0.27.2. Portable
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

## Follower presentation and manual saves (0.26.5)

All 54 local CTest groups pass, including individual shiny metadata on real room
packets, supplied normal/shiny sprite decoding, follower lifecycle, and explicit
campaign, wager and release persistence. The ROM address audit validates 194 ROM
mappings and 65 RAM references across all four supported cartridge layouts.

Two native LeafGreen clients passed host-initiated manual checkpoints, cold
reconnect with private teams/money/positions, nearby trade invitations and host
loss returning the guest to the launcher. A separate 60-second hosting test
created no automatic checkpoints or native save calls. The former timer caused
1.2–1.4 second calls; those calls now occur only when saving explicitly. The
largest measured host world-update call before manual Save was 28.35 ms in the
diagnostic build (guest: 5.53 ms); this is not an overall frame-rate benchmark.

Native follower acceptance covers initial stationary placement, X petting,
half-size emotes, both settings transitions, faint replacement, an actual
Pokemon Center exit and walking from Route 1 into Viridian. All 413 captured
LeafGreen fade/black frames had zero overlay changes. Visible shiny Pokemon use
the supplied art and enter battle with the matching shiny identity. All 386
shiny PNGs and four animation PNGs match the supplied files byte-for-byte.

The same native follower acceptance also passed on FireRed US 1.0. A native
LeafGreen wager battle verified the winner received P101 net, the loser paid
P101, both original parties/positions were restored, and native result messages
were readable. No wager phase wrote a native game save.

## Follower animation row correction (0.26.6)

Send-out selects source cells 0 through 3; recall selects cells 5 through 7
without returning to the top row. Tests reject blank or opposite-row cells and
verify that finished effects select no cell. The supplied PNG remains unchanged.
The runtime refreshes the native framebuffer before composition, and the world
renderer clears its host-object layer before drawing one current cell per follower.

## Emote1 sequences (0.26.7 / FRMP 22)

Question uses top-row cells 1-4. Exclamation uses top-row cell 5 followed by
bottom-row cells 1-5. Each plays once before holding its final expression, then
clears. These two reactions participate in the twelve-reaction X interaction
cycle. Network validation accepts and transports all twelve kinds.

Emote2 pairs have a 16-tick animation duration: A, B, A, B at four ticks
per cell. Tests verify every pair clears before a third loop. All twelve reaction
kinds are carried in FRMP 22; the supplied source images are unchanged.

Emote3 uses top 1+2, top 3+4, top 5+bottom 5, bottom 1+2, and bottom 3+4.
All five pairs play A, B, A, B and clear before a third loop. Tests cover every
cell transition and the cross-row pair. Music uses Emote2 top 1+bottom 1 as
confirmed by the asset owner. Each X interaction selects one of the twelve
original expressions, with matching native dialogue and a Pokemon cry.

Final 0.26.7 validation: all 54 Windows CTest groups passed. Two connected
LeafGreen clients completed all twelve native X reactions. The final Windows
package audit found 1,335 installed files and no ROMs, personal saves, native
caches or test executables.

All twelve final native emote previews were reviewed at their active animation
frames, including both cross-row pairs. Apple Silicon and Intel preview 0.27.2
passed CI, package/architecture checks, and byte-for-byte original animation
and shiny-art checks before their downloads were published.

## Camp party interactions (0.26.8 / FRMP 23)

X selects the nearby camp party member in front of the trainer, independently
of the follower setting. The chosen member pauses without snapping mid-step,
faces the trainer, and resumes roaming after its reaction. Other party members
continue moving. Camp packets carry each member's reaction kind and sequence.
Tests cover proximity, exclusive targeting, pause/resume motion, invalid input,
and reaction replication to both current and late-joining clients.

### Session controls and ledge regression (0.26.8)

- `release-0268-build.log`: 54/54 test groups passed, including identical-checkpoint receipts, reconnect save baselines, ending hosting without resetting in-memory world authority, solo saves and socket disconnects.
- `session-0268-acceptance.log` / `world-native-cfe7a6b8621e`: real native host Save waits for guest dialogue to close, then commits separate trainer files. The actual End session button returns the guest to the launcher while the host keeps walking and saving; the actual Exit world button then closes the game. Reminder placement and both UI states inspected in screenshots.
- Route 1 reproduction: elevation changes reseeded the follower from `(128,239)` to `(112,256)`, then pulled it diagonally backward. The fix retains its cardinal trail, records the native jump height and finishes that arc even if the trainer stops. Socket and interpolation tests include the follower height.
- New portable native acceptance scripts cover camp X interactions, ledge movement and session controls, using isolated synthetic worlds and caller-supplied local ROM/state paths.

Final ledge acceptance passed in both LeafGreen (`world-native-fc126135331c`) and FireRed (`world-native-db28c2ee23b7`): straight movement bounded by elapsed native frames, consistent downward facing, native jump height, no temporary scene-plane change, and complete landing after the trainer stops. Jump screenshots from both clients were inspected. The Windows package audited 1,335 files and the installer compiled successfully. No ROM or user data is included.


Final camp acceptance (`world-native-ced21af14dda`) passed on 0.26.8: native X targeted different non-lead party members with Party follower disabled, both clients received each reaction, and packing removed the camp. Original PNGs remain unchanged.

Mac preview 0.27.3 passed Apple Silicon and Intel CI at `3210b00` (run 34428881969), architecture checks, original-art byte comparisons, and package privacy checks; both downloads were published. Physical Mac gameplay remains unverified.

The first Windows release CI attempt exposed an existing test setup race: seeing the host's NPC did not prove the guest's `started=true` report had arrived. The shared-world test now waits for a guest-only actor before racing encounter claims. This changes test synchronization only; runtime files and the v0.26.8 tag remain unchanged. All production feature checks above passed before the release retry.


Windows v0.26.8 published successfully after all CI checks passed (run 34428962875, attempt 2). The corrected setup fixture also passed 60 consecutive local runs. The shipped 0.26.4 updater downloaded the public 0.26.8 ZIP, verified the full manifest, applied the update, restarted with the same isolated profile, and preserved both profile data and the nested synthetic world-save hash (`cache/release-0268-public-update.log`). Installer, portable ZIP and SHA-256 checksums are publicly available. Main now links both published Mac 0.27.3 previews; no ROM, real save or credential was published.


## Doorway follower fix (0.26.9)

The native Center exit reproduced a side seed at `(400,432)` while the trainer was already at `(416,417)`, followed by diagonal/backward interpolation. Arrival now anchors at the reciprocal native doorway `(416,416)` and uses the actual short path length. Seeds cannot invent diagonal segments. Regression tests include exact-door, partial-step and side-facing arrivals, plus existing ledge and connected-route checks.

All 54 test groups passed after the fix. Native two-client exit checks passed for LeafGreen (`world-native-2ab0295e7e79`) and FireRed (`world-native-01ef9ca81327`): constant door column, correct facing/spacing, no snap, and zero overlay changes during fades. Host and guest screenshots were inspected. Original assets and save logic were not edited. FRMP remains 23.

The versioned 0.26.9 build also passed all 54 tests. Final native acceptance (world-native-798b48b529af) additionally confirmed continued normal trailing after leaving the doorway. The package audit excluded ROMs, real saves, native caches and test executables.


Windows v0.26.9 passed release CI (34435799950) and published its installer,
portable ZIP and checksums. The shipped 0.26.4 updater downloaded the public
0.26.9 release, verified and applied every manifest entry, restarted with the
same isolated profile, and preserved the nested synthetic world-save hash
(`cache/door-0269-public-update.log`).

Mac preview 0.27.4 passed both architecture builds and tests (34435596988,
`b7c7f57`). Both app bundles passed architecture, original-art and private-data
audits; packaged launcher previews were inspected before publishing the DMG
and ZIP downloads. Physical Mac gameplay remains unverified. The tracked source
audit found no ROMs, real saves or credentials among 3,373 files.
