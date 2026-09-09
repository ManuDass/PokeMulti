# Standalone friends package and repository cleanup - 0.25.0 (2026-09-09)

- Release rebuild and all 53 CTest groups passed (`cache/share-package-build.log`). Gameplay and launcher executable bytes remain identical to the accepted 0.25.0 build; these changes concern installation, documentation and validation scripts.
- The package now includes all 386 normal FireRed follower sheets, cartridge artwork and the controller mesh/diffuse texture. Every supplied asset is copied unchanged. The explicit follower list is `cmake/FollowerAssets.cmake`; original credits accompany it. The package has 942 installed files plus its integrity manifest.
- An extracted ZIP outside the repository passed ROM validation, profile/world creation, two-world selection, an eight-player host configuration, joining controls, launcher reload, production native compilation/gameplay, F12 capture and clean return to the launcher (`cache/share-package-standalone-final.log`). The test used a disposable profile and the owner's ROM by path; no ROM was packaged.
- The standalone controller preview loaded the supplied 2,462-triangle model and its texture, rendered both frame sizes, and passed `pokeball_native_smoke.py --view-only` (`cache/share-package-controller.log`). Its captured frame was inspected. Defender's scan of the extracted package found no threats (`cache/share-package-defender.log`).
- `scripts/smoke_test.ps1 -PackageDirectory` supports extracted-package checks. Game-window discovery now matches the launcher's actual child process, avoiding unrelated SDL windows. The package script rejects unlisted installation files, native caches, ROMs, private saves, world data, identities and test executables. All ZIP checksums and manifest hashes were verified; after the standalone run only the third-party documentation changed.
- Audit and cleanup manifests are under `backups/cleanup-2026-09-09`. Historical cache paths cited below are preserved in that folder's recovery archives after cleanup; consult the cleanup README for restoration. Source, supplied asset/reference folders, exported atlases, the current build and external user saves are retained.
- These checks establish a working local standalone distribution on this Windows PC, not a full playthrough on every friend's machine or a WAN stress test.

# Host worlds, controls and configurable rooms - 0.25.0 (2026-09-09)

- Release build and all 53 CTest groups passed (`cache/test-0.25.0-final.log`). The world-store test exercises 32 actual TCP clients, capacity rejection, per-world trainer isolation, identity credentials, durable upload receipts, corrupt-checkpoint rejection/recovery and reconnect downloads.
- `scripts/world_native_smoke.py` passed with the final checkpoint bundle in `cache/world-native-efbfe9553c02/acceptance.json`. Two native US 1.0 clients sent a nearby T trade invitation, used native acceptance/decline, typed a chat message containing T, automatically wrote separate saves to the host world, and requested guest saving through the original host Save routine. Both cold-reconnected with the same party personality/OT/HP, money and map coordinates. Normal host closure and an intentionally terminated test host both returned the guest out of gameplay.
- `scripts/story_retry_native_smoke.py --configuration Release-0.25.0 --departures` passed in `cache/story-retry-cc5e07abb931/acceptance.txt`. A real Route 22 loss triggered the remote spin/flight. Captured samples at 127, 550 and 1229 ms showed turns and lift of 0, 14 and 278 native pixels. Frames were inspected: the trainer left the viewport without painting over the frame UI. Native healing, private money, friend takeover and shared completion still passed. Fly/teleport/rope entry points are revision-matched; this run directly exercised blackout.
- Launcher captures and interaction tests create a second independent world, show both choices, expose an eight-player host limit and reward controls, and open the join/invite page using the supplied font. Tests keep their windows offscreen; native edit controls require a visible ancestor to render their actual values into the capture. Final packaged captures: `cache/ui-check-b96a107ea112488bb77bd46d4228a1d2`.
- Both root launchers passed against the packaged release: `cache/launch-0.25.0-player1-final.log` and `cache/launch-0.25.0-player2.log`. The first test verified that the selected world, host mode and capacity 8 reached the production game process. Both verified the exact 0.25.0 launcher/runtime paths, native compilation, F12 capture, clean game exit and return to world selection. The package audit contains 558 allowed files and no ROM, private saves, native caches or test executables (`cache/package-0.25.0.log`); Defender's packaged scan is clean (`cache/defender-0.25.0-package.txt`).
- Native validation exposed a 32-entry function-hook registry limit and borrowed temporary hook IDs. The registry now owns names and supports 128 entries. The new hook test registers 80 temporary names and verifies selective dispatch; native world and fixture hooks now register reliably.
- Defender reported no threats in the fresh build (`cache/defender-0.25.0-fifth.txt`) and rebuilt launcher (`cache/defender-0.25.0-launcher.txt`). No quarantined binaries or security settings were restored or bypassed.
- Crash recovery means the latest durable safe-field checkpoint, not an arbitrary in-battle frame. Native acceptance used two local US 1.0 processes; 32-client transport acceptance is not a 32-player WAN performance claim or a complete US 1.1 playthrough.

# Walk out to pack camp - 0.24.2 (2026-09-09)

- Release build succeeded and all 51 existing CTest groups passed (`cache/test-0.24.2.log`). The native camp smoke was updated for the new behavior; no new unit tests were added.
- Two native US 1.0 clients passed `scripts/camp_native_smoke.py` using disposable profiles. Evidence: `cache/camp-native-cf083abde3b64c019a9a12e87ba12533/verified.json`. The visitor walked out without clearing the owner's camp. The owner then crossed the outline; both clients removed the camp, native input remained unlocked, and the owner kept moving without a dialogue box.
- The same run checked six party members wandering and playing within the camp area, physical tent collision, indoor rejection, and original 2D rendering despite legacy settings. Native terrain collision is unchanged. The optional Route 1 ledge variant was not rerun for this patch.
- The before/after boundary screenshots were inspected: the tent, roaming party and ground outline disappeared after the owner left; the ordinary trainer/follower presentation returned without an overlapping native message box. The original supplied camp artwork and ground outline drawing are unchanged.
- Defender's scan of the fresh 0.24.2 build reported no threats (`cache/defender-0.24.2.txt`). The camp packet format remains FRMP 18. This acceptance used two local US 1.0 clients; it does not establish US 1.1 or WAN playthrough coverage.

# Wager quantity-box layout - 0.24.1 (2026-09-09)

- Release build succeeded and all 51 existing CTest groups passed (`cache/test-0.24.1.log`). No new unit tests were added for this presentation change.
- The existing two-player native field challenge smoke passed with `--case decline`, using disposable profiles and the owner's US 1.0 ROM. Final evidence: `cache/field-native-1332af752518/acceptance.txt`. It opened the wager picker, edited P100 to P101, sent the offer, declined on the second client and returned both clients to the field with their original party, P3000 wallet, no held money and no picker/arrow task remaining.
- The final `amount.png` was inspected in the running game's captured frame: original border, Wager label at left, compact Pokédollar amount at right, original red arrows centered above/below it inside the box. The native window creator adds one tile to its requested origin; final arrow positioning accounts for that inset. The native arrow animation phase is retained when number width changes.
- Defender's final build scan found no threats (`cache/defender-0.24.1-final.txt`). US 1.1 symbol addresses are revision-matched; this native UI check used US 1.0. The previous story retry and other gameplay code is unchanged; FRMP 18 is unchanged.
- The 0.24.1 package audit and both root batch launch checks are recorded in `cache/package-0.24.1.log`, `cache/launch-0.24.1-player1.log`, and `cache/launch-0.24.1-player2.log`.

# Shared story battle retries - 0.24.0 (2026-09-09)

- All 51 CTest groups passed, including the new encounter retry regression: failed-owner handoff, canonical staged positions, repeated losses, wrong/stale tokens, center-return fallback, disconnect, successful completion and actual host/guest socket snapshots. Final build results are in `cache/test-0.24.0-final.log`.
- `scripts/story_retry_native_smoke.py` passed against the owner's US 1.0 ROM in `cache/story-retry-ca96b9603dd8/acceptance.txt`. Two private native clients used disposable saves. Player 1 triggered the middle Route 22 row, Gary walked to (32,5), and the other player's attempted bypass was blocked. Native battle damage caused a real loss, the original blackout returned Player 1 to Viridian Center and healed 1 HP back to 20 HP. Only that trainer paid the P40 loss (3000 -> 2960).
- Player 2 then triggered the bottom row. Gary remained at (32,5), with both pre-battle relative movements replaced by native facing actions. Player 2 won the original battle, received the personal native payout (3000 -> 3144), and advanced Route 22 scene 1 -> 2 on both clients. Retry and ownership records cleared; Player 2 walked west past the trigger. Player 1 retained P2960. Screenshots of the waiting rival, healed trainer, retry and open route were inspected.
- The final adjacent-facing-tile guard was compiled after that native run; it narrows scriptless X interaction and does not change the coordinate-trigger path exercised above. Scriptless X retries, Route 22's late rival, other story branches and WAN timing have not all received native playthroughs. US 1.1 symbol addresses are revision-matched; native acceptance used US 1.0.
- Defender custom scans of the fresh 0.24.0 build reported no threats, including the rebuilt harness, before any native regression execution. The earlier 0.23.2 quarantine was not restored, and no exclusions or security settings were changed. Final scan: `cache/defender-0.24.0-final.txt`. This does not establish the earlier detection as a false positive.
- Both room clients must update to FRMP 18. Packages exclude all test harnesses, ROMs, private saves, native caches and reference downloads. Root batch launch checks are recorded separately in `cache/launch-0.24.0-player1.log` and `cache/launch-0.24.0-player2.log`.

# Battle dialogue and wager messages - 0.23.2 (2026-09-09)

- Release game and launcher build successfully. All 50 CTest groups pass: 49 in `cache/ctest-0.23.2.log`, and the CMake monolith guard in `cache/ctest-0.23.2-guard.log` after supplying its required Visual Studio/Ninja environment.
- Wager tests cover pending results, winner/loser perspectives, net winnings versus the returned deposit, maximum stake, spectator exclusion, refunds, and cancellation without a deposit. Existing ledger/socket/idempotent payment tests pass.
- Defender's custom scan of the rebuilt production `pokemulti_game.exe` reported no threats. Two ordinary game windows booted isolated copies of prior native test saves; the host opened a room. Evidence: `cache/battle-production-bc4eb4d7e83e`.
- **Full native battle regression remains unverified for this patch.** Defender detected `Trojan:Win32/Bearfoos.B!ml` in the diagnostic `fr_game_harness.exe` and quarantined it during the first test, before battle entry (`cache/field-native-7c0bdbcb23c2`). That executable was not restored or rebuilt to bypass the quarantine. The ordinary-window fallback ended before completing a match. No security exclusions/settings were changed. The detection has not been established as a false positive.
- The native test script now captures both introductions and checks name expansion, opposite IDs and result messages, in addition to the existing money/party/position assertions. Its new assertions await a runnable reviewed test harness.
- US 1.1 symbol mappings are checked against the reference map; native play here used the owner's US 1.0 ROM. FRMP17 is unchanged.

# Validation ? 2026-09-08

## Connected controller input (0.23.1, 2026-09-09)

The user confirmed connection and a moving live stick dot, and had not clicked the game after using Connect. The Connect button in 0.23.0 left `gameFocus=false`, so valid accessory input was intentionally blocked by the sidebar capture gate. The earlier native smoke manually clicked the game and missed that connection workflow. Connection completion now returns focus once, unless a subsequent mouse/key action or window switch superseded it. The controller page displays the input state and a Resume game button when paused. A separate neutral-gate defect required the stick to be inside 70% of the configured dead zone; arming now accepts the full movement dead zone, while motion release retains hysteresis.

All 50 CTest groups pass (`cache/ctest-0.23.1.log`), including an off-center resting-stick regression. `scripts/pokeball_native_smoke.py` passes in `cache/pokeball-ui-fe7b4e57ae06/acceptance.txt`: connection completion without clicking the viewport, actual native trainer movement, native Start menu, stale/disconnect release, chat typing and background-window focus preservation, and the real Resume button. Visually inspected ready/paused/resumed screenshots include 940x650 with `scroll=0` and `chrome_clear=1`.

These native input reports are synthetic. A read-only attempt to obtain an additional input report from the user's existing physical connection returned Unreachable, so the patch does not claim a newly measured physical calibration or hardware gameplay test. Bluetooth decoding and subscriptions are unchanged. Bounded production `controller-status.txt` diagnostics now expose the input gates for a live accessory check after restart. Original supplied art and model are unchanged; FRMP 17 remains compatible.

## Poke Ball Plus (0.23.0, 2026-09-09)

All 50 CTest groups passed in `cache/ctest-0.23.0.log`, including new controller decoding, directions, dead zone/hysteresis, Start chord, taps, mapping, focus/reconnect neutral and native SDL input merging. The model importer rendered all 2,462 supplied triangles, with original UVs/texture and no cropped preview edges. Original model files were not modified.

`scripts/pokeball_native_smoke.py` passed using a private native game/profile: `cache/pokeball-ui-23416ed57800/acceptance.txt`. Injected controller reports moved the actual trainer from Viridian (26,27); both buttons opened the original Start menu (visually inspected screenshot `controller-start-menu.bmp`); Options/chat blocked movement; stale and disconnected input returned to released. The 940x650 and 1140x760 controller screenshots both report `scroll=0` and `chrome_clear=1`. Reports were synthetic, not received from physical hardware.

`fr_pokeball_tests --scan` exercised Windows BLE on this PC: `cache/pokeball-radio-check.log` reports a completed scan with zero matching devices, not an adapter failure. No physical ball connection, battery notification or radio reconnect was available to validate. Source references and setup instructions are in `POKE_BALL_PLUS.md`.

## Viridian western-exit crash (0.22.1, 2026-09-09)

Both user runtime logs ended with `Invalid local world state` near Viridian tile (2,18). An isolated diagnostic build of the pre-fix snapshot logic reproduced the crash after stepping west: report map `3,41` (Route 22) contained hidden NPC 1 from map `3,1` (Viridian), at (8,26). Evidence: `cache/route-exit-ad67e868ccd0/a/runtime.log`. The runtime was selecting hidden NPC records with the previous frame's map during capture.

The corrected capture selects the completed snapshot's map. NPC pose application also refuses a live/native map mismatch during transition. Host/world validation remains strict, and invalid local reports now include map/NPC diagnostics. The wire format stays FRMP 17.

All 49 CTest groups pass (`cache/ctest-0.22.1.log`), including a regression rejecting a hidden NPC tagged with the wrong map while allowing an empty initial destination snapshot and preserving separate map records. `cache/route-exit-cace638d7630/acceptance.txt` records two actual native US 1.0 clients walking through the reported exit: host west/east twice, guest west/east twice, then both crossing west together, for ten successful boundary crossings. Both processes remained alive and emitted no invalid-world error. The test uses isolated profiles and the owner's ROM; no production save was modified. `scripts/route_exit_probe.py` reproduces the traversal. Other connected-map pairs and US 1.1 were not individually walked in this acceptance run.

Release game SHA-256: `32CDD3774F57F6DAC6D106A9451D268B4C5FF155B13F21DE48F64DBF56BB281E`.


## Party and Egg indicators (0.22.0, 2026-09-09)

All 49 CTest groups pass (`cache/ctest-0.22.0.log`). Networking checks verify changing counts and ordered Egg masks across a host and three clients; malformed party counts disconnect their sender. Label checks compare every pixel/alpha value against both original PNGs, including alternating regular/Egg slots, and reject invalid or empty layouts.

`cache/party-icons-a74679205e2e/acceptance.txt` records two native US 1.0 clients displaying 1/6, 6/6, 6/3 and 0/3 parties. The Egg mask changes from `2a` (slots 2, 4, 6) to `22` (slots 2, 6), then zero, and both viewers follow it. Nearby username/icon groups avoid overlaps, chat reports zero actor/label overlap, and protected native menu pixels retain their original values. Screenshots of the mixed rows and chat were inspected. `scripts/party_icons_native_smoke.py` reproduces this using disposable profiles and the owner's ROM. The harness creates controlled party contents and changes the native Egg field; it does not replay a complete breeding/hatching sequence or each acquisition workflow.

A second run, `cache/party-icons-01a71367c58d/acceptance.txt`, also verifies the rows after one trainer walks four tiles away and captures the normal spaced layout alongside the crowded case.

Source and build PNG hashes are identical: Party Icon `5A97763433C647BC3B6025B5E2FC51B57EBDB7A9651FCD6585DDEC9685874A90`; Egg `C7D943A43258BA8FFBD3A4F0A1A323F7CEEEB5FA27DF6F6541FBFEC939C1977F`. Neither artwork file was modified. Current rendering is native 2D; all room clients require FRMP 17 for the two added presentation bytes.


## Shared story barriers (0.21.0, 2026-09-09)

All 49 CTest groups pass (`cache/ctest-0.21.0.log`). Added host-arbitration checks cover a busy NPC denying its neighboring scene, authenticated release, independent-map progress, simultaneous approaches, disconnect release without story completion, and rejection of a stale story condition after unlock. FRMP 16 rejects older clients that can skip a denied coordinate script.

Two isolated native US 1.0 clients, using only the owner's ROM and disposable profiles:

- `cache/story-gates-oldman-b48183ba760c/acceptance.txt`: the old man's conversation stays owned by A; B's upward input stops at (22,12), with the original message frame. B can move away while A remains in dialogue. Once A finishes, B enters the original road script and is walked back to (22,12). Shared parcel completion then permits B to cross to (22,10).
- `cache/story-gates-pewter-f9fac752d1e7/acceptance.txt`: simultaneous eastward approaches grant exactly one scene. The losing trainer remains at x=41. During the original escort the owner reaches (29,20), visibly away from the route, while another eastward attempt remains blocked. Completing the escort leaves Pewter's route condition at zero. Shared Brock clearance then permits the waiting trainer to cross to (43,21).
- `cache/story-gates-arrival-da40e50478d3/acceptance.txt`: a test-only collision bypass forces arrival directly on the busy Viridian event. The native wait script holds B at (22,11), without encounter ownership, despite further movement input. Releasing A's conversation resumes B's original ROM road script and walks B back. The subsequent shared unlock permits normal crossing.

`scripts/story_gate_native_smoke.py` reproduces those three cases with `--case oldman`, `pewter`, or `arrival`. It captures both native client windows and tile/lease diagnostics. Unlock stages inject the canonical parcel/Brock proof; they do not replay the entire delivery quest or Gym battle in this test. Actual movement, original NPC conversation/escort scripts, native wait/resume, and campaign projection run in the game. Screenshots were reviewed for barrier placement and in-game message composition.

The shared field collision hook also passes the native Camp regression: placement, six wandering party members, perimeter movement limits, free visitor movement, packing and indoor rejection (`cache/camp-native-c6e5330d20a14377b57d13878e407a10/verified.json`). The Release game SHA-256 is `FB3181B086BD1D340AF2063CD7A600D34ECC5251C2D7E11D8541CB748ED2E52D`.

Coverage is conditional coordinate events (`4000..40ff`) on their native elevation, plus existing native object collision. Weather and unconditional immediate events remain native. This does not establish a full-playthrough guarantee for every NPC, special-variable script, US 1.1 branch, WAN condition or unfinished-cutscene disconnect. Unknown quest progress remains local under the existing curated campaign adapter. Unit checks cover ownership release on disconnect; native disconnect recovery mid-escort has not been played through.


## 2D-only release (0.20.0, 2026-09-09)
The Release build passes all 49 remaining CTest groups (`cache/ctest-0.20.0.log`). Five retired renderer-only groups and their source files were archived with the removed feature; networking, native execution, saves, sprites, campaigns, camps, battles, chat and wagers remain tested.

The active runtime contains no alternate renderer, scene builder, capture target, settings reader or camera input handler. CMake no longer builds/links fr_voxel or its Direct3D 11/compiler dependency. The World sidebar page is removed, leaving Room/Friends/Options. F6 remains available only as inert test input, with no production handler. Existing voxel.cfg files are ignored without rewriting profiles.

Native 2D acceptance passed with two isolated US 1.0 players (`cache/camp-native-dc52d69b7f994e2999d6cce93080c9f7/verified.json`). Each profile started with enabled 3D, maximum curvature/tilt-shift and legacy camera settings. F6 had no effect, and voxel.cfg remained byte-for-byte unchanged. Captures show three sidebar tabs and original 2D scenery, both trainers, the supplied tent and all six camp party members. Tests also passed for camp walking limits, visitor movement, tent collision, wandering/play, synchronized packing, restored movement and indoor rejection. No alternate-renderer diagnostic files were created.

A native two-player field battle also entered successfully in 2D and returned both trainers to their exact original tiles after a disconnect. Both full party hashes were restored and both P101 deposits refunded and saved (`cache/field-native-d18bec7a1c68/acceptance.txt`). The first controller run pressed A again before its 30-frame diagnostic sample caught the open menu; the retained test now waits for printer/menu settling and the rerun passed. No production battle logic changed for this removal.

Both run.bat and run_player2.bat start the packaged 0.20.0 launcher and game; private-profile smoke tests passed ROM validation, Play/native compilation, settings reload and clean exit (`cache/launch-0.20.0-player1.log`, `cache/launch-0.20.0-player2.log`). The package game matches the build SHA-256 E10987FA2C326609394217007F9F11B76099742A89E8896177175855B5BBEC50. Its audit contains 554 files without retired renderer docs/licenses, ROMs, private saves, caches or test executables.

Removed source, dedicated preview tools and tests are backed up under `cache/source-before-0.20.0`, excluded from the release. The owner-supplied Voxel Renderer download, graphics, fonts, Camp sprites and cartridge art remain intact. Historical validation below refers to the versions named in its headings.


## Build and automated checks
Windows x64 Debug and Release build with MSVC 19.51, Ninja and static CRT. Both configurations pass all 36 CTest groups, including ROM/hash/profile validation, 18 ZIP archive cases, ARM/Thumb execution, PPU, DMA, timers, interrupts, audio, saves, code generation, BIOS HLE and networking.

The networking checks use four real loopback sessions. They cover membership, semantic movement, consent, ordered serial words, clock intervals, unrelated peer departure, disconnect cancellation and wrong keys. A sustained 1,024-transfer Debug run completed in 78 ms. This is a local throughput measurement, not an internet latency claim. Socket events wake the worker immediately for serial traffic.

Serial device regressions cover both roles, receive ordering, busy duration, IRQ acknowledgement, recovery from an error, fresh slave send values, scheduled transfer boundaries and polled transfers while serial interrupts are disabled. A long BIOS-copy regression verifies that timer interrupts continue throughout the HLE cycle budget instead of being delayed until its end.

## Actual owner-supplied ROM checks
The local ZIP validates as FireRed US 1.0. No ROM was downloaded. Native and instruction-reference runs of 600 boot frames produce identical PNG SHA-256: EB2189DDB97DF26B6D70D6F41C86A0F3C7E33804C77782DD69374154D3233DD1. The check was repeated after the BIOS timing and native-cache startup changes.

Ordinary controller input completes the opening sequence, starts a new game, navigates the player's house and uses FireRed's Save menu. The runtime writes a 128 KiB flash save and backup. A fresh process selects Continue and restores the saved downstairs position. This verifies an in-game save independently of development save states; it does not establish full-campaign compatibility.

A development-only fixture creates a two-Pok?mon party and places the player on Route 1. Ordinary movement into a spawned level-2 Pidgey starts FireRed's actual wild battle. A separate fixture with Repel active rejected 17 lower-level visible encounters without starting a battle. Fixtures and their generated saves are excluded from distribution.

## Two-player visual evidence
Two separate connected game processes were placed on different tiles in the same Pok?mon Center: A at (4,7), B at (9,4). Both views visibly contain the local and remote trainer, at the corresponding positions. The original same-tile preview was insufficient visual proof and is superseded by this check.

Local evidence: `cache/runtime-check/two-trainers-a.png` and `cache/runtime-check/visual-b-profile/live.bmp`. The B final PNG was captured after A exited and must not be used as two-player evidence. The earlier claim that this also established follower rendering was incorrect: the palette bug below prevented icons from being drawn. Follower state and encounter contact were verified separately from their visual appearance.

## Cable Club acceptance
Two actual native game processes enter the original Trade Center, remain connected through the map transition, move independently to opposite trading seats and display each other's parties in the original trade menu. BIOS service timing and serial scheduling fixes were required to reach this point.

The trade completed through the normal selection, consent and exchange animation. A's Bulbasaur (original trainer ID 0x11223344) moved to B; B's Charmander (original trainer ID 0x55667788) moved to A. Each second party member remained with its owner. Both games wrote their 128 KiB cartridge saves. Separate cold processes used Continue, without fixtures or runtime snapshots, restored the Pok?mon Center and retained the exchanged species and original trainer IDs. Evidence is under `cache/trade-reload/`, including final telemetry and PNGs for both sides.

The Colosseum check has entered a real linked battle and exchanged move selections and damage. The final full run completed a battle with two Pokemon per trainer using ordinary Fight, move and replacement-party inputs. A lost after both party members fainted; B won with its Rattata remaining. The games reported outcomes 2 and 1 respectively, wrote both flash saves, healed the parties and returned to the Colosseum with two peers, remote-link state active and no game link error. The capture immediately before test shutdown is `cache/battle-evidence/returned-a.txt` / `returned-b.txt`, with matching BMPs, at frame 21960. The controller records PASS in `cache/battle-controller.log`.

Polled transfers while IRQs are disabled and prompt cancellation after peer port reset are covered by regressions. Those behaviors were necessary for the post-battle save/reconnect path. Closing one QA process then disconnects its peer; final post-shutdown telemetry is not the evidence for the successful return.

The trade was repeated after the serial changes, returned to party selection and wrote both saves. Both saves then passed another cold Continue check in `cache/trade-final-reload/`, preserving the exchanged species and original trainer IDs.

## Native UI checks
The packaged launcher smoke check passes with ZIP loading, profile persistence, enabled Play/Host/Join actions, Settings, rendering and clean restart. Clicking Play launches the actual game as a child, compiles a native block, closes cleanly and re-enables Play. Latest capture directory: `cache/ui-check-97055be11d684759b0080fbed1b68a03`.

The friends UI smoke check uses two real game processes and exercises Host, Join, Add Friend and Leave. Latest capture: `cache/online-ui-7c01da1af8fe475f8c3cf75605ede930/friends.png`.

## Package check
The explicit install allowlist contains only runtime/toolchain/documentation/license files and the second-trainer launch shortcut. A separate manifest records their sizes and SHA-256 values. The package audit rejects any file outside that allowlist, including private runtime diagnostics. The runtime smoke check also asserts that running the game creates no extra files inside the package. Compilation diagnostics now go under the save's native-cache directory.

A cold 600-frame run from the package, forcing bundled compiler/header lookup and ignoring the development source tree, compiled and executed native blocks and matched the boot PNG hash above. Latest successful package smoke: `cache/packaged-runtime-7ffd943f9da14809be93c1a49c2f6f20`. The first attempt exposed a forwarding header that depended on the development tree; installation now supplies the actual shared ABI header. Repeatable check: `scripts/packaged_runtime_smoke.ps1 -Rom <local-ROM-or-ZIP>`.

## Limits
No US 1.1 ROM was available for end-to-end validation. Its identity and revision metadata are supported, but all gameplay evidence above is US 1.0. Native execution uses locally compiled x64 blocks with instruction fallback; it is not a complete static translation. Full campaign progression, audio hardware, controllers, cross-PC/VPN latency, reconnect during gameplay, unusual field-effect combinations and non-grass visible encounters still need broader testing. Friends presence is room-local, without a public login/relay service.

All screenshots, local symbols, extracted runtime data, compiled game blocks and QA saves stay in ignored cache directories and are excluded from the package.

## 0.3 motion and integrated interface
Both Debug and Release pass 36 CTest groups. New behavior checks cover timestamp jitter, stops, synchronized turn poses, follower timing, stale/duplicate updates, warps, reused room slots and timestamp/sequence wrap. Four-session network checks now assert pixel/pose payloads and clearing presence after leaving.

The initial Windows graphics replay exposed a camera update interrupted between its tile and pixel writes. Sampling now occurs at CB2_Overworld entry, observing the preceding completed field update. The corrected two-process Direct3D 11 replay is under cache/motion-verified. Both players walk right/up/left/down while their peer observes; camera scrolling and stopping are included. The independent scripts/verify_motion.py check matches all 2,869 rendered pose samples to the sending process, with zero facing/frame/flip mismatches and zero source camera discontinuities. Maximum remote changes for presentation intervals under 60 ms were 2 pixels in A and 3 pixels in B, within elapsed-frame bounds. No 16-pixel reversals remain in this replay. This is local-PC evidence, not a WAN performance claim.

The UI shares one SDL window with the game, with Room/Friends/Options tabs, nonblocking connection operations and invitation cards. The launcher hides during play and restores after the game exits. The development-only UI replay drives the actual widgets; no test input files or guest-state controls are supported by the installed product. Captures remain private. Historical 0.2 UI evidence above describes the prior window layout.

Final 0.3 UI replay: cache/online-ui-v3-cfbde7a17ec8492e9af3616022a54550. Host/join, text-field input capture, game focus, saved friend add/remove and presence, compact resizing, sidebar toggling, persistent world options, battle invitation decline, trade invitation acceptance, cable disconnect and Leave all passed through the actual widgets. Leaving clears stale roster/presence immediately.

Final packaged launcher check: cache/ui-check-bfbcdf4b7b4548f3ac3712d9811ce788. It verified ZIP/profile flow, one visible game window while the launcher is hidden, the product F12 screenshot path, native compilation, clean shutdown and launcher restoration. The bundled-toolchain cold boot in cache/packaged-runtime-5a34cd0fbbf14cac83764cc584890696 retained the reference boot hash above. Version 0.3 installs 530 explicitly allowed files plus its hash manifest; no local game art, ROM, saves, generated native code or development harness is included.

## 0.3.1 visible Pokemon correction
The shared party-icon renderer read inline palette colors as pointers, so range checks discarded every follower and wild icon. The fix reads one of the three inline 32-byte palettes, aligns visible feet to the tile ground line, and keeps wild spawning/wandering within view. No game graphics are added to the package.

Release passes all 36 CTest groups. Five isolated native runtime replays use the owner-provided US 1.0 ZIP, offline development checkpoints and ordinary recorded movement in the integrated interface. The development renderer compares RGB pixels immediately before and after world overlays: follower off = 0 changed pixels, follower on = 235, wild off = 0, wild on = 580 with three entities, and both on = 815 with three entities. Each uses its own profile/save and persisted options. Captures were inspected: Bulbasaur is visible behind the trainer in the Pokemon Center; the Route 1 view contains a following Bulbasaur, Pidgey and two Rattata. This supersedes earlier state-only visibility claims.

Evidence: `cache/visible-pokemon-0.3.1-final/verified.json` and each case's `result.png` / `game-ui.bmp`. The development-only pixel diagnostics and fixture captures remain outside installation. US 1.1 gameplay and a separate two-player follower rendering replay have not been repeated for this patch.

A further native replay walked into a spawned Route 1 Pidgey (species 16, level 2) and reached the actual wild battle screen after the rendering fix. Log and inspected capture: `cache/visible-pokemon-0.3.1-contact/run.log` and `battle.png`.

## 0.3.2 scene composition
Release passes all 37 CTest groups. The new sprite_composition group checks unchanged native output with no added sprites, opaque dialogue/menu BG0, foreground/background priorities and transparent tiles, front/back and equal-depth OBJ ordering, elevation, per-pixel windows, forced blank, alpha/brightness, scanline-latched masks and reset invalidation. The existing PPU, motion, native runtime and networking groups also pass.

Two actual connected game processes use private profiles, the owner's US 1.0 ZIP and offline Pokemon Center checkpoints. Ordinary controller input moves trainers behind, in front of and onto each other while both followers are active. The native frame and composed frame are captured separately. Every covered candidate pixel is compared directly: 212 attempted remote-sprite pixels under the open Start menu remain identical to the original game pixels; crossing captures preserve nearer native sprites and let nearer remote sprites cover them. Equal-ground ordering agrees between clients, including overlapping followers. Native OAM depth capture recognizes all subsequent visible submissions after the initial snapshot frame; no continuing metadata misses occur in the replay.

Fresh-profile evidence is under `cache/composition-multiplayer-0.3.2-011e7839e9f64ab38881335a33fab2d1`, with separate captures and pixel CSVs for both clients. Repeatable development check: `python scripts/composition_smoke.py --rom <local-ROM-or-ZIP> --configuration Release-0.3.2`. Synthetic parties/checkpoints, screenshots and pixel traces remain outside distribution. The separately inspected follower/wild on/off replays are under `cache/composition-0.3.2-quick`.

Priority-based roof/foreground and window behavior is covered by the compositor regressions. This does not implement every original grass, reflection or shadow field-effect object for remote players. Those effects and US 1.1 gameplay still need broader compatibility coverage.

The final full composition replay also opens actual NPC dialogue at the upstairs counter. All 32 added-sprite pixels beneath its text box match the original native image; adjacent NPC/follower intersections remain layered. Both client views were inspected. The equal-position check additionally compares the captured keys for over 100 shared trainer pixels per client and confirms the same room-slot ordering on both sides (`tie-order-verified.json`).

The 0.3.2 package passes the ZIP/profile/Play/UI/native-launch check (`cache/ui-check-b6f672a9e16242d38a97c1122f201e5d`). Its bundled-compiler cold boot (`cache/packaged-runtime-3d95f04663b240f3a33941322a01facc`) retains the reference boot hash. A separate packaged Route 1 run exactly matches the verified development follower/three-wild-Pokemon framebuffer (`cache/composition-0.3.2-product/result.png`, SHA-256 `8b182c11a243c875fe656a473b24271e4f144e77b7ada176fd994469b8aec289`).


## 0.4.0 usernames and room chat

All 39 Release CTest groups pass, including supplied-font/tile rendering and four-player chat tests. Actual TCP clients exercise attributed ordered delivery, one local echo, UTF-8 text, invalid/control text, the 128-byte limit, host-enforced rate limits, rejected forged server packets, bounded 100-message history, host loss and rapid reconnect without stale speech. Existing movement, serial/cable and sprite composition checks remain green.

The final two-native-client widget replay is `cache/chat-ui-0.4.0-75ed7e4a23004531b69e35fe0732f8ca`. It covers T/Enter/Esc input focus, both users' visible outlined names, short and wrapped nine-tile bubbles, free placement around both player bodies/names, eight-second expiry with retained history, compact/wide layouts, and unchanged native pixels under the Start menu. Its menu capture protects 6,649 proposed added pixels. Both clients' screenshots, transcripts and annotation/PPU diagnostics are retained privately. Run `python scripts/chat_ui_smoke.py --rom <your-ROM-or-ZIP> --configuration Release-0.4.0` to repeat with the local development checkpoints.

Existing Room/Friends/Options controls pass the full native UI replay in `cache/online-ui-v3-75002f6045dc4cd9ac2dbfc3ae616459` (the script's historical directory prefix is unchanged). Host/join, friends presence/add/remove, focus/resizing, settings, invitation accept/decline, cable disconnect and Leave are verified through the actual widgets.

Character/follower crossings and equal-ground ties also pass on 0.4.0: `cache/composition-multiplayer-0.3.2-c7fde50a7b424778b7f9069d6bfa5aa5` (historical script prefix). Native NPC/character depth is retained, the Start menu protects 390 proposed pixels, and NPC dialogue protects 32. No ongoing native-order attribution misses occurred beyond initial snapshot recovery.

These checks use isolated local processes and profiles; they do not establish WAN latency or a campaign-complete playthrough. Captures, checkpoint states, ROM data, user saves and generated caches remain outside the package. Only the two explicitly selected UI fonts and nine owner-supplied bubble PNGs are added to the distribution asset allowlist.


The packaged 0.4.0 launcher passes ZIP validation, profile creation/reload, Play/native compilation, screenshot and launcher hide/restore checks in `cache/ui-check-76bf5a46499944df921cb27e8ea75fca`. The packaged bundled-compiler cold boot passes in `cache/packaged-runtime-77a8b18a0ec64fe69a108fe72a4b3244` and retains reference PNG SHA-256 `EB2189DDB97DF26B6D70D6F41C86A0F3C7E33804C77782DD69374154D3233DD1`.


## 0.4.1 original revised tile assets

All twelve revised source PNGs are reloaded at their original 16x16 resolution and RGBA colors. The earlier 8x8 sampling and generated pointer have been removed. Single-line frames use the original Thin Left/Middle/Right files, two-row frames use the original Upper and Bottom files, and taller frames repeat the original middle tiles. The original Poke Ball corner remains intact.

All 39 Release CTest groups pass. The composition test now verifies exact non-RGB555 PNG colors and continued native-menu occlusion. `scripts/verify_chat_tiles.py` compares rendered 1x3, 1x4, two-row, three-row and long-word frames (including accented letters and descenders) against direct arrangements of the current source PNGs. Every non-text RGBA pixel matches the originals, every corner-icon pixel is retained, and all text pixels have at least three pixels of edge clearance. Source/build file SHA-256 values match for all twelve tiles. The pixel report and images are in `build/Release-0.4.1/chat-label-fixtures/verified.json` and adjacent PNGs.

The two-native-client chat replay passes in `cache/chat-ui-0.4.1-cd284f9102c045e3bf3d4ead29af9404`: usernames, thin and wrapped frames, attributed room messages, typing focus, expiry/history, compact/wide layouts and protected Start-menu pixels. Its Start-menu capture preserves 7,484 covered proposed pixels. The screenshot clearly shows the full-size original Poke Ball in both thin and larger frames. Test profiles and captures remain outside distribution.


The final multiline spacing adjustment uses nine-pixel line advance and centers the text within whole tile rows. The three-line sample fits a 32-pixel frame with four pixels of bottom clearance; the two/four-line samples have balanced top/bottom clearance. Pixel verification checks that balance as well as the three-pixel minimum. The accepted single-line Hi! rendering is byte-for-byte identical to its saved pre-adjustment sample. The final two-player replay above passes after this adjustment.


## PokeMulti 0.5.0
The Release-0.5.0 build passes all 42 CTest groups. Added checks feed SDL's actual held-key state for WASD and arrows, simultaneous input, releases and UI capture; verify follower corners, reversals, idle facing, warps, directional sheet rows and lossless recovery of source pixels; and exercise cartridge identity/colors, depth/rotation, logo dimensions and transparent label edges without an added backing/border. The final preview-only refinements passed the focused cartridge regression again.

Two native clients passed `scripts/composition_smoke.py` with local directional Bulbasaur/Charmander art. Source pose traces cover all four follower directions with zero backward-facing movement frames. Native trainer/NPC depth and equal-ground ties remain consistent; 390 Start-menu pixels and 32 dialogue pixels protected from host sprites were exercised. Evidence: `cache/composition-multiplayer-0.3.2-a10bfae2bc5e441689c5deaff7d78226` (the script retains its historical directory prefix).

Both FRMP 5 clients passed the real chat-widget regression, including T/Enter/Esc focus, short and wrapped speech, history, expiry, resize and native menu protection. Evidence: `cache/chat-ui-0.5.0-1f583c714dc44c808824808c24df60e0`. All twelve chat PNGs still pass exact RGBA/non-text pixel, corner, padding and thin/multiple-row checks.

The launcher reads the supplied `Cart Art/Fire Red Cart Art.png` directly and maps it onto its red 3D shell. Dragging changes the perspective. The local file and logo are not edited. An isolated installation without access to Cart Art generated a 240x160 title image through an instruction-reference ROM boot in its artwork cache and created no game save; evidence: `cache/art-fallback-55d022eea67545618ec6034543f69a2c`.

The current package contains no ROMs, saves, private caches, cartridge label art or follower reference pack. Program_Icon.png and the selected UI fonts/chat tiles are explicit owner-supplied UI assets. The default data folders retain their historical names, preserving existing profiles and saves. FireRed US 1.0/1.1 remains the supported runtime; other game cartridge color metadata does not establish runtime compatibility.

## PokeMulti 0.6.0
Release-0.6.0 passes all 43 CTest groups. The new shared_world group covers per-NPC authority, guest encounter handoff, simultaneous real socket claims with exactly one winner, lease cleanup on disconnect, consumed-wild tombstones, story compare-and-set conflicts, reward policy boundaries and playback gating. Old-protocol rejection and existing cable/chat tests remain covered.

Two native clients share wandering NPC and wild state in `cache/world-native-0.6.0-1f391ecfb42f4db1991bba0c5e359ff2/first-pass`. A host-side badge change reaches the guest, a guest-side Cerulean scene change reaches the host, and the host's TM claim bit does not change the guest's. These story changes are explicit development-harness semantic injections, not claims of completing those battles.

The repeatable `scripts/world_sync_smoke.py` uses separate native processes, copied offline development checkpoints and isolated saves. The final reward/service run is `cache/shared-native-0.6.0-33d42855e2a44257bedc021c28a30444`: the guest receives TM39 through the native Bag routine, a second claim does not duplicate it, and the host continues walking while the guest speaks to a receptionist. Both then enter the same Cable Club service dialogue concurrently without a story lease. The shared wild run is `cache/shared-native-0.6.0-0bda182853ea43e692987ce2b08b8244`: the guest contacts a shared spawn and enters the original wild battle while the host remains controllable. The rival run is `cache/shared-native-0.6.0-61b7ae10c6ce488282108905d104e119`: native coordinate triggers make Cerulean's rival approach the guest at (23,6), both clients display the native rival at (23,5), and the spectator keeps moving without receiving the dialogue lock. Use the retained pre-shutdown capture subfolders, not the last rolling status after peers close.

A cold Continue of an isolated copy of the existing local save observes 268 actual Quest Log playback frames in `cache/recap-profile-copy-0.6.0-c934130b56604ef9ba5f78e645cfda5b/recap.csv`. Every playback frame has zero bytes changed by live overlays, with a current party present. Source profiles and cartridge saves were not modified. Earlier synthetic saves without playable history were insufficient for this check.

The actual two-client chat/UI replay passes with supplied Pixel Operator fonts, T/Enter/Esc focus, wrapping, history, expiry, resize and native menu protection: `cache/chat-ui-0.5.0-6ff1ca2b6ac24a79bdcf5e0876fbb6b6` (historical script prefix). The launcher supplied-font/profile/ZIP test passes in `cache/ui-check-b2596cd30cd147878ede6c101f04fa0e`. Source bubble tiles, icon and cartridge art are unchanged.

This verifies concrete native scenes and the shared-state mechanism, not every quest or full-campaign completion. Personal onboarding, inventory, money and gift claims remain local; selected shared-story rewards have explicit private delivery rules. Cross-PC/VPN latency, every map puzzle, full story playthrough, US 1.1 gameplay, and non-grass encounter presentation still require broader compatibility testing. All ROM data, reference assets, private save copies and QA captures remain outside the distribution allowlist.

Final 0.6.0 verification: all 43 CTest groups pass after the encounter-handoff and native-link review. The final native rival replay passes in `cache/shared-native-0.6.0-6f8390e9925d4f3b9290865ad24d7271`. The packaged launcher Play/native-start/return test passes in `cache/ui-check-e0a7c300783249f48488e48c4d5d68dd`. Bundled-toolchain cold boot in `cache/packaged-runtime-b40adc5972084a5b8c75ba08ddd28bc8` retains the reference SHA-256 EB2189DDB97DF26B6D70D6F41C86A0F3C7E33804C77782DD69374154D3233DD1. All twelve supplied chat tile RGBA checks pass unchanged. Package 0.6.0 contains 548 explicitly allowed files plus its manifest; standard run scripts target that package, and the Player 2 shortcut retains a separate profile/save.

The original native Cable Club handshake and entry also pass with two processes in `cache/native-link-0.6.0-b398c762363b468e9736aeda8c977dfc`. Both trainers enter the original Trade Center with remote link players present and no native link error. `scripts/native_link_entry_smoke.py` repeats this check. Common Direct Corner, Union Room and Wireless Club service scripts are per-player; the original engine takes over its linked scenes once gReceivedRemoteLinkPlayers (03003F64) is set. Shared story/NPC overlays do not interfere with native battle/trade room actors.

## PokeMulti 0.6.1: early Route 1 wild encounters

The 0.6.0 shared-world gate incorrectly required the personal Pokedex flag for wild authority and encounter claims as well as story synchronization. Both existing player saves had that flag unset, and both latest field diagnostics showed starters on Route 1 with zero spawns. An isolated two-native-client reproduction on 0.6.0 failed after 4,800 rendered frames with ready=0, starter=0, and no wild population: `cache/shared-native-0.6.0-ff6f86192da043679e307c974ba1b489`.

Wild authority now requires an active field and a usable party Pokemon, independently of the Pokedex/story baseline. The host arbitrates early wild claims exactly once; consumed IDs cannot return from stale reports. Story/NPC authority retains the personal onboarding boundary. Reports from players without usable Pokemon cannot take wild authority or claim an encounter.

All 43 Release-0.6.1 CTest groups pass. Added authority and real-socket regressions cover an unseeded room, no-party rejection, identical shared spawn identities, simultaneous wild claims with one winner, stale consumed snapshots, and ownership handoff before Pokedex acquisition.

The patched native replay is `cache/shared-native-0.6.1-cd8d7b3c9f184cc2a0b28e1bc9890bf5`: both clients show the same shared Route 1 population with their personal Pokedex flags cleared and no story baseline applied. Pidgey and Rattata are visibly rendered in the retained `shared-wild` captures. The guest contacts Pidgey and reaches BattleMainCB2 (08011100), with a fully displayed native wild battle retained under `guest-wild-battle`; the host remains free to move. Repeat with `python scripts/world_sync_smoke.py --rom <local-ROM-or-ZIP> --configuration Release-0.6.1 --wild --pre-pokedex`.

The post-onboarding native story/reward/service regression also passes in `cache/shared-native-0.6.1-cd4cbc93799042e0859953a4ae8c390a`. Shared story updates, private single-copy TM39 delivery, independent movement, and concurrent Cable Club service dialogue remain functional. Original user saves and supplied art were not modified. These checks use isolated local native clients and do not establish full-campaign or WAN compatibility.

## PokeMulti 0.7.0: session rewards, individual money and layout

Release-0.7.0 passes all 44 CTest groups. The reward tests cover all eight permitted checkbox combinations, policy propagation to guests, fresh-room changes, exclusive special claims, duplicate denial and shared hide flags. Money has no policy bit. The agreed_wagers group uses real sockets for exact invitation terms, consent, deposits, participant authentication, native-result agreement and settlement. It also tests draw/conflict refunds, late deposit receipts, duplicate acknowledgements, host restart, wallet limits and transient Windows read locks during account-file replacement.

Original US 1.0 game execution with isolated test saves verifies Eevee's original gift script. With default special sharing off, only one of two simultaneous trainers receives Eevee: cache/rewards-native-unique-e39de80bbfc14e31a48670645d277d2e. With it on, both receive their own Eevee: cache/rewards-native-shared-0dfb7db0982e41e98de2ed5c092a9db5. Native gift/hide flags and parties are retained in the after-gift evidence folders. This verifies that scene, not every listed legendary battle.

Four native clients in cache/layout-0.7.0-5842e351715a45ae81ca135e81d87597 receive the actual UI-selected policy 6: story items and special Pokemon on, TMs off. A guest cannot edit the host settings. A semantic test badge/claim update synchronizes, but requesting rewards grants no TM39 while disabled; all four native balances remain 3000. Room and Options have zero vertical overflow at 940x650. The final-layout captures also verify Host and Join have zero overflow after eliminating the seven-pixel host-page excess. Supplied fonts remain active; original logo, tiles and label art are not altered.

Native wager reserve/refund: cache/rewards-native-refund-144ea38be63244d4a3d6c4b1d660586f. Both games save P100 deposits (3000 -> 2900), then save refunds to 3000. Fresh processes load the actual flash saves without checkpoint files and retain the independent balances and completed transaction receipts. Diagnostics are cleared before that cold run to avoid mistaking previous output for fresh execution. Host ledger restart and duplicate-event cases are covered separately by the automated ledger/socket test.

The early Route 1 native regression passes in cache/shared-native-0.7.0-dba13e99f397471e95214efbe68ce639: shared visible grass Pokemon before Pokedex, native wild battle for the claimant and continued movement for the other trainer. The 0.6.1 grass fix remains intact.

The enabled-TM/native service regression also passes on 0.7.0 in cache/shared-native-0.7.0-7ad143c540754bd2b1530c6a9f6954cf: native Bag delivery supplies one TM39, a second claim does not duplicate it, story changes reach both games and one trainer can move while the other uses an NPC service. All twelve chat tile PNGs pass exact RGBA/non-text pixel checks with at least three pixels of padding and unchanged corner art.

The full linked-battle test also exposed an invalid save boundary: a correct money payout saved inside the linked Colosseum restored its link actors without a usable ordinary avatar. The final wallet gate requires a normal active player, no remote-link scene, no script/fade and no Quest Log playback. A committed payout remains pending until the trainer uses the original Cable Club exit. Cold-load validation now waits for stable normal-avatar state as well as correct money; a black link-room scene cannot pass it.

The socket test now waits for both peers' asynchronous port-mode updates before beginning its IRQ-disabled transfer. This removes a test race in which a pending reset correctly cancelled a newly submitted transfer. The final full Release suite passes all 44 groups.

Final native wager acceptance passes in cache/rewards-native-battle-f733028556d446fc965a91a89d2eaac8. Both wallets reserve P100 from P3000. The original Colosseum battle produces host win / guest loss; the committed result remains pending while linked. Both trainers take their original room exits, return to Viridian's Pokemon Center and automatically save P3100 / P2900 with no held balance. The cold run uses those actual flash saves without checkpoints and verifies stable normal avatars, map 5,5, correct balances and completed receipts after 240 additional overworld frames. Screenshots under cold-load show the playable Pokemon Center rather than a link scene. Test navigation used observed native actor positions and ordinary key input; no battle outcome or wallet result was injected. Source test replay also includes the doorway's downward exit interaction and original confirmation/escort dialogue.

Final packaged native boot passes in cache/packaged-runtime-55c6545dacfe4d75b63e0b79f493b78c with the unchanged reference PNG SHA-256 EB2189DDB97DF26B6D70D6F41C86A0F3C7E33804C77782DD69374154D3233DD1. Packaged launcher ZIP/profile/Play/return checks pass in cache/ui-check-af61d739608843c1939b95065918d584. The release contains 548 explicitly allowed files plus its manifest. Binaries match Release-0.7.0; source fonts, logo and all twelve chat PNGs are unchanged. ROMs, private saves, cartridge labels, follower reference packs, compiled ROM caches and harness executables remain excluded. Normal run scripts and the independent Player 2 shortcut target 0.7.0.

## PokeMulti 0.8.0: temporary camps
Release-0.8.0 passes all 45 CTest groups, including camp. The new group checks every supported map-type/metatile combination, rejects cave/indoor/tall-grass/path/collision/elevation cases, checks all twelve tent footprint tiles, simulates a six-member party for 10,000 frames with bounded continuous movement and playful interactions, arbitrates overlapping tents, forbids relocation of accepted tents, and exercises real-socket full-party publication, late joins, packing and disconnect cleanup.

Final native acceptance: `cache/camp-native-dc40a444a1bd4945bd4d448d8b3b3ab3/verified.json`. Two isolated US 1.0 clients use the actual Camp and Pack up buttons at 940 x 650. The same three-frame tent and six party species appear in both games, with two distinctly positioned trainers. Camper movement is held; the visitor remains controllable and stops at the tent footprint. Party poses wander and play; packing removes the camp from both games and restores movement. An indoor setup attempt is rejected. Both sidebars have zero vertical overflow. Final names have no AFK tag. The provided font, username outline, room chat, normal followers and wild population remain visible.

Exact rendering check in `cache/camp-native-3aea1fbb00854a6eb1e487b6a9a87ea8/camp-in-both-clients`: all 1,545 opaque source-frame-1 pixels match the native host capture; all 1,548 opaque source-frame-2 pixels match the guest capture at the camera-adjusted tent location. There is no redrawing, downsampling or color alteration of tent art. All three source PNGs are byte-identical to the packaged files.

Packaged runtime validation: `cache/packaged-runtime-daef17c5df384427ad2079da0dea6fad`. Bundled native compilation boots successfully; the reference boot image remains SHA-256 EB2189DDB97DF26B6D70D6F41C86A0F3C7E33804C77782DD69374154D3233DD1. The release allowlist now has 551 files plus its manifest, adding only the three owner-provided camp PNGs. ROMs, private saves, reference packs and compiled ROM caches remain excluded. Camps are transient and never alter party contents, wallet balances or cartridge saves. Cross-PC latency and US 1.1 camp gameplay remain outside this local acceptance run.

## PokeMulti 0.9.0: battle presence, wild walking and frame controls
Final Release-0.9.0 passes all 46 CTest groups, including the new battle_presence group. It checks bounded battler records, faint/replacement poses, hop bounds, real-socket spectators and late joins, field-inactive trainer retention, cleanup and directional wild data. Existing camp, composition, chat, world, reward and wager tests also pass.

Two isolated original US 1.0 clients: `cache/battle-native-464ccc9c46ba4b71994cd771d6d66c50/verified.json`. Contact with a shared Pidgey enters the guest's actual native battle while the host remains on Route 1 and can move from (12,14) to (11,14). The spectator retains Leaf's trainer, Bulbasaur and Pidgey, facing and hopping. The fixture starts Bulbasaur at 1 HP with Splash; the original enemy attack causes the faint, rather than an injected outcome. Native party selection sends Charmander and the spectator changes to species 4. Original battle victory returns the guest to the field and clears the battle scene on the host. Captures retain entry, faint, native party menu, replacement and return states. The native replacement path is checked; additional switch combinations are covered by semantic socket tests, not a full native party matrix.

Both clients' wild-motion.csv traces use all four facings, all four sheet frames and directional-sheet rendering for every sampled wild. A harness-only SetMoney command changes the isolated guest balance to 4321 during battle; its frame wallet updates while the host stays at 3000. This is a display check, not an injected wager settlement. Wager settlement tests remain unchanged.

`final-layout/verified.json` records fresh-process restoration of the UI-selected volume 24 and six layouts: sidebar shown at 940x650, 1140x760, 1280x720 and 1600x900; hidden at 940x650 and 1280x720. Every viewport including its border clears the Camp/wallet row and status row. Options has zero vertical overflow in all four shown-sidebar sizes. Full-window captures confirm the minimum size retains 2x integer gameplay scaling and an unobscured wallet/Camp row. Session rewards appears in Options with the Pokemon label; active-room rules remain read-only.

Native game tests use isolated local profiles and local multiplayer. Full-campaign battle variants, WAN performance and US 1.1 gameplay remain broader compatibility work. Original user artwork and saves were not modified.

Packaged 0.9.0 native boot passes in `cache/packaged-runtime-f6ca3be7c97e4702bd465bd911888a7c` with the unchanged reference image SHA-256 EB2189DDB97DF26B6D70D6F41C86A0F3C7E33804C77782DD69374154D3233DD1. Packaged launcher ZIP/profile/Play/native-start/return passes in `cache/ui-check-dc1bc5260bc141918abe4a98f8096e37`. The allowlist contains 551 files plus its manifest; binaries match the tested release build. All 20 supplied font/logo/chat/tent files are byte-identical to their source assets. Run scripts point to 0.9.0; both room clients must upgrade for FRMP 9.

## PokeMulti 0.10.0: walking around camp and a visible ground perimeter
All 46 Release-0.10.0 CTest groups pass. Camp regressions require a complete walking loop on all four sides of the tent, reject one-sided patches and invalid area bits, verify shared player/party bounds, connected terrain, unchanged accepted masks, true outer edges without a tile grid or inner holes, and identical masks for real-socket guests and late joins. The existing 10,000-frame six-Pokemon simulation remains bounded and playful. Composition checks cover ground visibility, native trainer occlusion, same-priority BG1 foreground and BG0 menus.

Final two-native-client acceptance is `cache/camp-native-d5d74ff138ab4a40b07e2085a69f7c15/verified.json`. The original Route 3 map has enough ordinary lawn for a tent at (62,12), a complete walking loop and 48 connected walking cells within its five-tile radius. The camper walks from (64,11) to (68,11), then (68,13), while the visitor moves independently to (66,13) and stops at the tent. Holding Right for 90 frames at the radius edge keeps the camper at (68,13), with camp still active. Pack up removes the shared tent/party/perimeter and the same direction moves the trainer to the previously blocked lawn at (69,13). Six Pokemon continue to wander and play. An indoor placement attempt is rejected. All test profiles are isolated from the user's saves.

`perimeter-verified.json` checks 508 host-view and 1,111 guest-view line pixels from actual native captures: every proposed perimeter pixel is on BG2/BG3 ground with native object windows enabled, and every visible pixel has RGB (158,232,172). The stroke is two native pixels, displayed as four pixels at the tested 2x game scale. The guest's `camp-in-both-clients/b-game-ui.bmp` shows the tent inside the enclosing boundary, with Pokemon on multiple sides and both trainers present. No interior tile grid is drawn. Native/host sprites, foreground scenery and UI remain above the line. Original fonts, chat tiles, logo and tent PNGs are unchanged.

FRMP 10 transmits the bounded fixed walking mask; both players must use 0.10.0. Full campaign, US 1.1 gameplay and WAN conditions remain outside this local acceptance check.

Packaged 0.10.0 boot passes in `cache/packaged-runtime-a2118b8b7f154462bc25f18287ed1e7c`, retaining reference SHA-256 EB2189DDB97DF26B6D70D6F41C86A0F3C7E33804C77782DD69374154D3233DD1. Packaged ZIP/profile/Play/native-start/return passes in `cache/ui-check-0ccd4d8597974370b71fdc323c4108b1`. Final binaries match the tested build, all 20 supplied font/logo/chat/tent files match their source hashes, and the archive contains 551 allowlisted files plus its manifest. The normal launch scripts target 0.10.0.

## PokeMulti 0.11.0: client-only voxel rendering
All 47 Release-0.11.0 CTest groups pass, including the new real Direct3D rendering test. It covers isolated settings persistence, off/default behavior, camera-relative input and unchanged menu directions, setting clamps, projection/curvature, all seven cameras, water/AA/resolution/distance modes, visible lighting/shadow/grid/tilt effects, and native HUD precedence. A 12x24 source sprite retains its 1:2 aspect across all cameras with and without world curvature. Its full-resolution pixel mask remains identical across all four scenery resolution settings. A battle card retains its exact 24x48 displayed pixels at 2x scale even when the entire terrain is a raised wall.

Final battle/camp acceptance: `cache/voxel-native-6818bfae44684f5aba110f6aef023052/verified.json`. Two native clients share the same campsite, six party members and perimeter with Voxel enabled on only one. Guest movement remains independent. F6 switches off/on during camp and off during a real wild battle. Native menus/health bars remain above sprites; the final `voxel-battle/a-game-ui.png` shows the unchanged map environment with both original battle Pokemon in front of it. No replacement arena is created. Default Voxel tab scroll is zero at 940x650, and Camp/wallet/game viewport chrome remains clear.

Camera acceptance: `cache/voxel-cameras-c886c56b64dc4bc692a12e37ad1e31ee/verified.json`. Separate profiles load a rotated 50-degree camera and a first-person camera; forward input follows the first-person heading while native menus retain their layout. Turning off the host's renderer leaves the guest's renderer active. Original game saves and supplied image/font assets remain untouched. Additional first-person panel spacing was tightened after this capture.

This validates local US 1.0 native clients and actual GPU output. It does not establish complete campaign coverage, authored FireRed building models, all battle-animation variants, US 1.1 gameplay, or performance on other hardware. First/third-person cameras and procedural GBA shapes remain experimental.

Packaged native boot passes in `cache/packaged-runtime-477697a98b0a404bb8538ddceb2f1215`, with the unchanged 2D reference SHA-256 EB2189DDB97DF26B6D70D6F41C86A0F3C7E33804C77782DD69374154D3233DD1. Packaged launcher/ZIP/profile/Play/native startup/return passes in `cache/ui-check-f338b6286084411eb2c0020bde4e2d5b`. All 20 supplied font/logo/chat/tent files match both their source and the prior release. Executables match the tested build. The release contains 555 allowlisted files plus its manifest.

Bulbasaur source check: `cache/voxel-source-check/verified.json`. Its original ROM back sprite contains 1,178 opaque pixels. All 4,712 corresponding displayed pixels at 2x match in the final voxel battle capture, including the lowest rows. The apparent lower edge is not terrain clipping. The regression still verifies battle sprites remain in front of terrain, with native UI above them.


## PokeMulti 0.12.0: released Pokemon and the Center crowd

All 48 Release-0.12.0 CTest groups pass. The new released_pokemon group checks semantic individual data, invalid/egg rejection, idempotent offers, durable exclusive claims, escape and re-encounter, capture tombstones, native catcher identity changes, cross-map door/grass paths, blocked terrain, 30 distinct crowd positions with a clear doorway, real-socket contention and host restart. Existing sprite composition, motion, camp, chat, battle presence and GPU voxel tests continue to pass.

Native acceptance uses two isolated US 1.0 clients and the owner's local ZIP ROM: `cache/released-native-97447aff7b78496180841acd0eac1d7f`. `native-pc-crowd-verified.json` records a canceled original PC release creating nothing, then three confirmed original releases (Pikachu, Eevee and Squirtle). After logging off, all three leave the native Center and wait on separate outdoor tiles (24,27), (23,26), (29,26), keeping the doorway at (26,26) clear. `crowd-2d-fixed.png` and `crowd-voxel-fixed.png` show the persistent crowd in both renderers. The idle display remains visible beyond live-player timeout. Native auto-save preparation includes SaveMapView and SaveQuestLogData; `native-cold-save-fixed.png` confirms a proper Center floor after a fresh-process Continue.

After Aster exits, the released Pokemon travel from Viridian City (3,1) into the nearest reachable Route 1 (3,19) grass, occupying different tiles. The watch captures retain the map transition and shared phases. Leaf contacts the released Pikachu and enters an original wild battle with species 25, level 10 and the original personality 391536684. Native Bag/Master Ball capture and the original nickname prompt complete normally. `native-capture` records outcome 7, that personality in Leaf's actual party, a 131072-byte native save, an empty resolved battle receipt and the host's durable Caught state. The Master Balls and starting party are isolated test fixtures; the release, battle, capture and saves use native game operations.

`native-capture-cold` verifies that both original game and host population survive fresh-process Continue: the caught personality remains in Leaf's party and its host record stays consumed. Remaining released individuals remain available. Recovery compares personality/species/IV identity because the native catch routine changes trainer fields. A completed capture saves even if the room disconnects; its receipt settles the reserved claim on reconnect.

This is local two-client US 1.0 acceptance, not a full campaign, every indoor exit, WAN-latency or US 1.1 gameplay certification. No user profile/save or supplied image/font asset is changed by these tests. FRMP 11 requires both room clients to use 0.12.0.

Final package acceptance: native boot passes in `cache/packaged-runtime-ed3b0c2c434d47ba8f666dff9e037f3f` with unchanged reference SHA-256 EB2189DDB97DF26B6D70D6F41C86A0F3C7E33804C77782DD69374154D3233DD1. Packaged ZIP/profile/Play/native-start/return passes in `cache/ui-check-a51c5c416d93479092cad3e215797bab`. All 20 supplied font/logo/chat/tent assets remain byte-identical to source and 0.11.0. The 556-file install allowlist excludes private/test data, and packaged executables match the tested build. The normal launcher and Player 2 shortcut target 0.12.0. The final cold capture screenshot shows the restored Route 1 field after native Quest Log playback, the two remaining released Pokemon and no duplicate Pikachu.


## PokeMulti 0.13.0: native battle foreground and continuous overworld staging

Release-0.13.0 passes all 48 CTest groups. Expanded battle_presence coverage checks original encounter positions, one-pixel trainer/partner movement, return paths, blocked ground, sub-tile opponent avoidance, already-clear encounters, cramped-space fallback and walking frames over real sockets. Exact US 1.0/1.1 callback boundaries keep Bag, party, summary and title screens outside battle transition retention. Existing motion, composition, follower, camp, releases, chat, rewards, wagers, native runtime and GPU renderer groups pass.

Final native acceptance uses two isolated US 1.0 clients: `cache/battle-transition-a5cc3071c734409cac6eea153ea18ccd/verified.json`. The guest enters and escapes an original wild battle while the host remains an overworld spectator, both using voxel rendering. The trainer moves at most one pixel per update and returns along the walking path; the wild opponent remains at its original (176,160) position. In this encounter the partner already has a clear fighting position, so only the trainer needs to walk. Diagonal partner movement is covered by the semantic regression. There are zero unintended 2D fallback frames across 1,677 sampled entry/battle/exit frames, including 230 held transition frames. Explicit full-screen Bag and party visits remain native and usable; those intentional menu frames are excluded from the fallback assertion.

`foreground-verified.json` compares all 76,948 visible foreground pixels in the final captured battle against the original PPU pixels, with zero mismatches. The full-size Charmander back sprite and Rattata front sprite remain at native battle-screen positions over the voxel environment, clear of the health panels. The native-moves capture in `cache/battle-transition-c434880a34314ef782ae4caff79cec7f` additionally shows the move-selection panel without obscuring Charmander; that earlier build has the same foreground implementation, before restricting transition retention to allow full-screen menus. No source sprite is redrawn, downsampled or replaced with overworld art. Native Bag and party screenshots in the final acceptance directory verify the full-screen menu boundary.

These are local native singles and socket regressions, not full campaign, every battle variant, US 1.1 gameplay or WAN certification. Original user profiles, saves and supplied assets remain untouched. FRMP 12 requires both clients to use 0.13.0.

Packaged 0.13.0 boot passes in `cache/packaged-runtime-dd48ffce2de44d43967d97bb5b369a06` with the unchanged reference SHA-256 EB2189DDB97DF26B6D70D6F41C86A0F3C7E33804C77782DD69374154D3233DD1. Packaged launcher/ZIP/profile/Play/native-start/return passes in `cache/ui-check-daf0761f0a19452bae83e3ba4a149a76`. Executables match the native-tested build. All 20 supplied logo/font/chat/tent assets remain byte-identical to source and 0.12.0. The package contains 557 allowlisted files plus its manifest. The normal and Player 2 launch shortcuts select 0.13.0.


## PokeMulti 0.14.0: banded scenery, rounded foliage and closed shorelines

The final Release-0.14.0 build passes all 49 CTest groups (`cache/ctest-0.14.0.log`). New geometry coverage checks roof/facade texture separation, measured silhouettes, empty roof padding, complete badge reliefs, bounded geometry, circular foliage chords, cap/rim source samples, native tree trunks, transparent metatile continuation, four-sided shoreline closure including corners and model plots, cropped shoreline UV bands, and map/menu callback boundaries. GPU coverage also verifies that animated map texture refreshes do not replace a cached tree texture; existing sprite aspect, native battle foreground, camp, composition, networking and runtime tests pass.

The private owner-ROM survey (`cache/voxel-map-survey`) uses the production profile matcher and Direct3D renderer, not hand-built demonstration meshes. It renders 134 building instances across twelve Kanto layouts using original ROM images and foreground masks. `cliff-closed.png` records the corrected Cerulean inner bank: dry ground and model plots continue to the recessed water plane. The user's ROM art and supplied graphic/font assets are not rewritten.

Native doorway acceptance: `cache/voxel-field-6b1dfb520ddf4fa4a9a44428afbb56c3/doors-verified.json`. Entering/exiting Viridian Center sampled 898 rendered frames, zero 2D fallbacks and 201 held transition frames; the retained native palette fade spans black to full brightness. `bushes-native.png` shows the revised trees and foreground-signature bush variants beside Viridian houses. Center and Mart retain their complete front emblems.

Native battle acceptance before the final shoreline-only meshing change: `cache/battle-transition-a1a23523ed5e410ba7ba7528d88cc3ec/verified.json`. Two clients passed original encounter, move selection, Bag/party visits, escape and spectator return. There were zero unintended 2D fallbacks over 1,702 sampled battle/transition frames, with 231 held frames; explicit native full-screen menu visits are excluded. The trainer moved at most one pixel per update and the opponent remained at its encounter position. `foreground-verified.json` compares 76,948 visible original battle/HUD pixels with zero mismatches. The final shoreline change affects below-ground bank geometry only; the final GPU regressions recheck protected battle foreground and sprite sizing.

Dedicated profiles cover the surveyed Kanto buildings and General-tileset trees/bushes. This is not full-campaign, every-tileset or pixel-identical Gen 1/FireRed parity: other structures retain fallback geometry. Native gameplay acceptance above uses the owner's US 1.0 ZIP; US 1.1 address offsets are covered in code-level callback tests. FRMP 12 is unchanged.


## PokeMulti 0.15.0: reference terrain, signs, fences and tree ground

The final production build passes all 50 CTest groups (`cache/ctest-0.15.0-final.log`). New reference-rule coverage verifies native ledge priority and six-pixel height, cropped side bands, internal-face culling, two-pixel sign thickness and original drawn width, constant per-voxel texture samples, independent six-pixel fence components, volume repetition and uniform-body rim sampling. Tree-ground regression cases reject a nearby/frequent trail in favour of the source art's grass, ignore painted cast shadow, and exclude raised or recessed donors. Existing native battle foreground, sprite aspect, camp, composition, room, save, runtime, reward and wager checks pass.

The owner-ROM Direct3D survey reuses the production classifier and profile matcher across twelve Kanto layouts. `cache/voxel-map-survey/scenery-0.png` through `scenery-6.png` cover the Viridian ledge/sign, side/back views, fence lines, and the corrected grass beside the actual route trail. Round-tree floor selection follows the supplied reference's background matching; FireRed's original apron colours replace GB light/white background classes. No source graphic assets or ROM pixels are rewritten.

Native field evidence is in `cache/voxel-field-ba2ade3d086f4632b495e3af0669778b`. Viridian Center entry and exit sampled 2,297 frames with zero 2D fallbacks and 201 held transition frames. These doorway captures preceded the ground-donor-only correction; transition handling and terrain geometry are identical to the final build. Native signs/fences are also captured there.

The scope and source mappings are documented in `docs/VOXEL_REFERENCE.md`. These checks cover the reported scenery and local US 1.0 native gameplay, not all FireRed tilesets, every battle variant or full Gen 1 visual parity. FRMP 12 is unchanged, and original user profiles and saves remain untouched.

Final native battle acceptance: `cache/battle-transition-513504024a04403793d8e09e42510cb7/verified.json`. Two isolated clients pass encounter entry, move selection, native Bag/party menus, escape and spectator return. Across 1,704 entry/battle/return samples there are zero unintended 2D fallbacks and 230 held transition frames; intentional full-screen menus are excluded. The trainer advances at most one pixel per update, the partner remains at its already-clear position, and the wild opponent remains at its original encounter position. `foreground-verified.json` confirms all 76,948 visible original battle/HUD pixels with zero mismatches.


## PokeMulti 0.16.0: tilt-shift passes and connected surroundings

The final build passes all 51 CTest groups (`cache/ctest-0.16.0-final.log`). The GPU test compares a fixed grayscale scene at blur levels 0-3: more than 1,000 edge pixels change visibly, edge contrast decreases with strength, the central focus band stays within one channel step, and disabling the effect restores the exact original frame. A native HUD pixel remains exact with maximum blur. Existing sprite aspect, battle foreground, composition, camp, networking, reward, save and runtime groups pass.

`voxel_surroundings` verifies north/south/east/west signed offsets, recursive neighboring bodies, cycle/bounds checks, native border phase at negative coordinates, body priority over filler, invalid connections, and ground support after world-origin translation. Native current-map cells still come from RAM; additional tilesets come from the original ROM.

Native acceptance: `cache/voxel-field-0befac225e3d48589e25a051ce8249f2/verified.json`. The actual UI slider selects and persists levels 1, 2, 3 and Off while the host's settings remain unchanged. `blur-level-0.png` and `blur-level-3.png` show the original scene with increasing edge blur. `voxel-settings.png` shows Route 1 before leaving Viridian; `route-crossed.png` shows the real crossing with Viridian still behind the player. All 541 sampled crossing frames stay in 3D. `cliff-border.png` shows native Cerulean border terrain and its connected surroundings.

The final disabled-mode optimization is checked separately in `cache/voxel-field-ce854f25cecf437f8858727da42d457b/toggle-verified.json`: disabling the renderer, changing to Pallet, and enabling it again produces the fresh padded Pallet scene; the other client remains enabled.

Acceptance uses isolated local US 1.0 profiles. It covers the reported blur and map edges, a real route seam and the native border rules; it is not a full campaign or every camera/tileset screenshot certification. Source graphics, the owner's ROM and real save/profile files are unchanged. Room protocol remains FRMP 12.

## PokeMulti 0.17.0: persistent campaigns and native notices

Final source checks: all 52 CTest groups pass (`cache/ctest-0.17.0-final.log`, 14.40 seconds). New `shared_campaign` coverage exercises persistent campaign creation/resume, retained older campaigns, deduplicated milestones, all optional reward policies, personal achievement boundaries, native text width/control-byte safety, authenticated completion actors and socket host restart before a new baseline. Existing network, chat, wager, save, release, camp, composition, voxel and runtime groups pass.

Native US 1.0 two-client evidence is under `cache/campaign-native-c40340c7e9af4a75950195f822f7166b`:

- `parcel-verified.json`: original Mart collection and Oak delivery run once on Leaf. Aster can move during collection, remains outside during delivery, receives Pokedex access, and subsequently enters the Mart without repeating the task. Story starts after the personal starter/lab battle, without requiring the Pokedex first. Initial hidden Rocket drops do not award unearned keys.
- `careers-verified.json`: both original Brock battles complete separately. First victory gives Leaf a native Boulder Badge and P1400; Aster keeps no Badge or winnings and receives one shared TM39. Aster's later victory awards their own Badge and money without another TM39. Original Fly dialogue supplies both native Bags. The initial controller only advanced the giver's text; the recipient's queued notice was then dismissed to complete the check, and delivery ordering was improved before the final build.
- `cold-verified.json`: separate native saves load from a cold boot without runtime snapshots, retaining the campaign, personal Badges, wallets and one-copy reward receipts. Host policy changes to zero without resetting the campaign.
- `policy-verified.json`: original Warden dialogue makes HM Strength available to both trainers with every optional sharing checkbox off. The original Silph President gift goes only to Leaf; Aster's later conversation does not award another Master Ball or falsely mark Aster's private receipt. Setup supplies Gold Teeth and the rescued Silph state; this does not claim a complete Safari Zone or Silph playthrough.
- `trainers-verified.json`: Leaf beats Route 3's Janice; Aster can walk through the defeated NPC's sight without an ambush, then talk to Janice for a separate original battle and personal winnings. The test controller needed a longer directional hold to walk after turning.
- `messages-verified.json` and `message-pixels.json`: the actual Camp button opens native text, repeated rejection reopens it, native A/B dismiss, and the Room journal uses native multi-page text. The captured text-box interior is pixel-identical in 2D and voxel views. The compact 940 x 650 Host/Room pages report no scrolling and keep the wallet/Camp row clear.

`cache/campaign-road-67ce42f91f1448a0beba381dc5d4824c/verified.json` separately exercises the live route projection: an injected completed parcel proof runs the original old-man positioning subscript, opens the road without map reentry and commits derived travel access back to the campaign. Full original parcel dialogue was exercised by the two-client check above.

A visual cold-load check caught stale saved map tiles in the first checkpoint implementation. Campaign and wager saves now use the same original SaveMapView/SaveQuestLogData preparation already used by release saves before TrySavingData.

All native gameplay checks use isolated cache profiles and the owner's local US 1.0 ROM, with labeled party/map setup fixtures where described. Real trainer saves, reference packs and supplied artwork were not edited. This coverage does not certify every campaign/postgame branch, US 1.1 gameplay, WAN play or disconnect recovery during an unfinished native cutscene. Current rooms use FRMP 13.

Corrected checkpoint acceptance: `map-save-verified.json` compares native cold-Continue terrain fingerprints, map/position, personal wallets, Badges, HM/TM receipts and the individual Master Ball receipt. `personal-save-verified.json` additionally confirms that a new original Janice victory checkpoints automatically when her campaign clearance already exists, without a manual save command. The corrected map was also visually inspected after cold loading.


## 0.18.0 paper view and compact camping

All 53 Release-0.18.0 CTest groups pass (`cache/ctest-0.18.0.log`). Paper coverage includes exact source pixels, unchanged aspect at 25/40/55/75-degree tilts and curvature extremes, correct ground anchors, two-triangle cards, base-layer door retention, flat plateau art, floor pixels excluded from furniture cards, invisible border handling, continuous exit mats, and black indoor surrounds. Camera tests reject legacy orbit/first-person restoration while preserving saved overhead tilts. Existing battle sprites/HUD, tilt-shift, camp, multiplayer, rewards, saves and runtime groups pass.

The reported Route 1 clearing is covered at trainer (8,13), with a tent at (4,11) backing onto a ledge. Native two-client acceptance in `cache/camp-native-ad78026d03df487db7fa1bc62bd97b71` verifies six party members, identical ground masks, camper movement, visitor collision, radius enforcement, playful movement, packing and indoor rejection. This acceptance used the initial 0.17.1 bugfix binary; the same camp implementation is included in 0.18.0. Paper-native acceptance also verifies the tent/party/outline in the new renderer.

Native paper camera acceptance: `cache/paper-cameras-5037cc2e92314fa18559dbb9ed8e515d`. Both profiles migrate correctly; horizontal drag cannot rotate, vertical drag respects 25/75-degree bounds, movement follows map directions, settings persist independently, and one player toggling leaves the other unchanged.

Native paper gameplay acceptance: `cache/paper-native-f42913a2f4eb42609d78fa7c14a977ba`. The World tab, client-local F6 toggle, tent, six Pokemon, ground perimeter, guest movement, native menus, actual wild battle, original battle sprites/health bars and battle toggle pass. Later visual corrections were followed by the complete CTest suite and focused native scene captures.

Visual review found and corrected tall plateau cards, floor pixels attached to cards, a separated exit-carpet border, outdoor sky in interiors, and base-layer doors removed by foreground-only building masks. Reviewed final door/tree captures: `cache/paper-scenes-1fadf69b`; final interior floor: `cache/paper-scenes-d52ce3dd`. Convenient PNG copies are in `cache/paper-release-review`. They are direct screenshot format conversions; no source artwork was repainted or resampled.

The active renderer submits flat ground and image cards. The older extrusion builders remain reference/test code and are not called by the game renderer. All native acceptance uses isolated cache profiles and the owner's US 1.0 ROM ZIP. Real trainer saves and provided asset folders were not modified. This is not a certification of every map, indoor furnishing, campaign branch, US 1.1 gameplay or WAN conditions. FRMP 14 requires matching 0.18.0 clients.

Controlled 720-frame Route 1 probes discard 180 warm-up frames and use identical daytime, resolution and dummy SDL settings. Old voxel (`cache/voxel-performance-Release-0.17.0-41e8e7a5`): mean 74.83 ms, median 68.85 ms, p95 110.07 ms. Paper (`cache/voxel-performance-Release-0.18.0-f55faa33`): mean 33.68 ms, median 30.82 ms, p95 62.32 ms. Mean frame time falls 55.0%. The 2D controls measure 26.34/25.56 ms respectively. Reported submitted triangles fall from 6,131,794 to 7,580. No JIT compilation occurs in the measured windows. Dummy SDL contributes substantial software UI cost, so these are comparative measurements rather than desktop FPS claims; terrain refresh spikes remain visible in the p95 result.


## 0.18.0 launcher version correction

The root release batches already selected 0.18.0, but the launcher badge, F3 panel and log still contained literal 0.17.0 text. These now use a header generated from CMake's project version; the runtime reports the same version in its log. The debug batch now rebuilds before launch instead of reusing any existing executable. Player 2 batches forward optional arguments while retaining their separate default profile. Packaging derives the current version and rejects mismatched build versions before installation.

Actual batch-to-launcher-to-game checks passed for `run.bat`, `run_player2.bat` and the packaged `launch_player2.bat`, using unique cache profiles and the owner's ROM ZIP. Checks verify executable paths, both version logs, native compilation, clean game exit, launcher restoration and profile reload. Evidence: `cache/launcher-run-bat-smoke.log`, `cache/launcher-player2-bat-smoke.log`, `cache/launcher-packaged-player2-smoke.log`. Visually inspected launcher capture: `cache/ui-check-f07ff6ed8f4143dfbde138b6c31738b0/menu.bmp`; badge reads 0.18.0. A negative packaging check rejected the old 0.17.0 build without changing packaged executables (`cache/launcher-stale-package-check.log`). Debug rebuild-on-launch was changed but not exercised in these Release acceptance checks.


## 0.18.1 curvature, house cutouts and Camera controls

All 53 CTest groups pass (`cache/ctest-0.18.1.log`). New Direct3D checks sample ground contact across a 256px card, including a base between ground-grid rows, at all four curvature levels and 25/55/75-degree tilts. Source-pixel checks cover base-layer walls/windows, enclosed window colours that match the ground, and exterior transparency. Scenery uses upright strips matching the ground grid; both use the same interpolated curvature height. Buildings remove only exterior-connected ground colours instead of mistaking the native foreground layer for object transparency.

Native before/after screenshots reproduce and correct the reported Pallet house/lab holes and Route 1 detached ledges at maximum curvature. Before: `cache/paper-scenes-953d5796`, `cache/paper-scenes-1baf4442`, `cache/paper-scenes-4feb5659`. After: `cache/paper-scenes-d0319cc0`, `cache/paper-scenes-303f20fb`, `cache/paper-scenes-5c95d7db`. Those native captures used the same rendering changes in the intermediate 0.18.0 build. Final 0.18.1 native UI evidence is in `cache/paper-ui-final-b68b11fe`; the four curvature levels and tilt-shift on/off were exercised through actual sliders and their saved settings inspected. Camera opens by default and contains Tilt, Zoom, Curvature and Tilt shift. Compact 940x650 layout reports zero sidebar scrolling and a clear game viewport. Screenshot copies are in `cache/paper-0.18.1-review`. Original supplied assets and real trainer saves were not changed.


## 0.18.2 flat indoor scenery

Non-outdoor scenes retain the complete composed native room image on the ground plane. The renderer skips all indoor scenery-card extraction, masks and ground donors; the native adapter also skips unused indoor object-profile matching. Character/Pokemon actor cards, camera settings, native collision and gameplay remain independent.

All 53 Release-0.18.2 CTest groups pass (`cache/ctest-0.18.2.log`). New coverage checks exact indoor room pixels and animated updates despite conflicting base layers/object profiles. Actor proportions are checked indoors and outdoors across 25-75 degree tilts and curvature off/maximum.

Inspected native captures: Center `cache/paper-scenes-9a0df5a8`, Mart `cache/paper-scenes-18130b17`, house `cache/paper-scenes-1fc8269c`; each reports zero environment models. Native movement and the separate upright Bulbasaur follower were inspected in `cache/flat-center-walk-2f0b1e64`. Maximum curvature at 75 degrees remains continuous (`cache/paper-scenes-4ce3fd58`), and Pallet's outdoor standing houses remain intact (`cache/paper-scenes-e07ff38c`). PNG screenshot copies are in `cache/paper-0.18.2-review`. All captures use isolated test profiles; supplied artwork and real trainer saves were not edited.


## 0.18.3 Internet invitations

All 54 Release-0.18.3 CTest groups pass (`cache/ctest-0.18.3.log`). `connection_invites` covers public/private/reserved IPv4 validation, invitation parsing and exact key preservation, custom ports, failed/invalid provider responses, refresh recovery and stale-result rejection when changing rooms. These tests use injected providers and do not require Internet access. A separate `fr_connection_tests --live` run successfully returned a valid public IPv4 from the actual HTTPS endpoint; the test output omits that address.

Two isolated native clients exercised the actual Host, Copy internet invite, Copy LAN invite, Copy same-PC invite, Paste invite and Join adventure controls. Both reached two connected peers. The joining client did not start a lookup. Invalid/private custom addresses disabled Internet copy; a valid custom address with a different external port copied exactly. Leaving cleared the public address. Minimum-size Room layout had zero scroll and clear game chrome. The expanded help originally grew below the screen; the final centered/bounded modal was checked with both help and LAN sections expanded. Final evidence: `cache/invite-0.18.3-final/ui-checks.txt` and the corresponding PNG captures. Dummy SDL clipboards were transferred explicitly between the two test clients; copy and paste still used the actual production widgets.

This verifies endpoint discovery, invitation handling and local connectivity. It does not verify inbound connections from another household or change any router/firewall settings. No relay, UPnP, automatic NAT traversal or transport encryption was added.

The packaged release audits 559 allowlisted files. Root `run.bat` and `run_player2.bat` both launched `dist/PokeMulti-0.18.3/pokemulti.exe` and its sibling `pokemulti_game.exe`; native compilation, ZIP ROM validation, profile creation/reload and clean exit passed. Logs: `cache/launch-host-0.18.3.log` and `cache/launch-player2-0.18.3.log`. These launcher tests used isolated profiles.


## Native field challenges and camping (0.19.0, 2026-09-09)
MSVC Release build passes all 54 CTest groups (`cache/ctest-0.19.0.log`). New protocol tests cover authenticated sequential battle readiness, invitation cancellation ownership, and no debit/pairing for cancelled invitations. Raw chat tests use the shared FRMP 15 protocol constant.

The native Start menu was inspected with eight entries: Camp appears immediately between Bag and the trainer name, and Exit remains fully visible above help text. Camp/Pack up invokes the existing grass validation and supplied animated tent. G packs camp; indoor placement produces an original FireRed message. Evidence: `cache/field-0.19.0-9bfcb4c9/menu-fixed.png`, `amount-final.png`; original tent setup/packing was also exercised on Route 3.

`scripts/field_battle_smoke.py` drives two isolated harness clients with the owner's local US 1.0 ROM, original native A/B/directional controls and private fixture saves. It supports outdoor/indoor matches, free battles, exact P101 wagers, refusal, invitation cancellation and active-battle disconnect recovery. It checks full 600-byte party hashes, exact return map/tiles and saved balances. Fixtures and native test executables remain excluded from packages.

An initial indoor test exposed RFU serial interrupt vectors left by the map; restoring wired serial/timer handlers before battle corrected the startup. Its failed connection restored both trainers and refunded both saved deposits. Earlier outdoor testing completed a real P101 battle and saved P3101/P2899 at the original Route 3 tiles (`cache/field-0.19.0-3e473f79/acceptance.txt`). Final native checks below supersede the initial build where stated.

Final native evidence:
- `cache/field-0.19.0-60f407df/acceptance.txt`: complete indoor P100 battle, native outcomes 1/2, original 600-byte party hashes restored, original Center ground-floor tiles (7,6)/(7,7), saved balances P3100/P2900 and no held funds.
- `cache/field-cold-3d87ec77e8/acceptance.txt`: fresh processes use native Continue from copies of those cartridge saves, without runtime snapshots or fixtures; both exact wallets, parties and positions persist.
- `cache/field-native-1309a25f5265/acceptance.txt`: outdoor P101 invitation cancelled through X and the native menu while the recipient's prompt is open; both return to idle field, no party changes, no deposits and P3000 each.
- `cache/field-native-6b00f56e75f8/acceptance.txt`: final binary enters the outdoor native battle, then disconnects; both trainers return to their original Route 3 tiles with original party hashes and saved P101 refunds (P3000 each, held 0, receipt 2, dirty 0).

Both root launch batches now target 0.19.0. Separate packaged UI smoke tests verify run.bat and run_player2.bat start the actual dist/PokeMulti-0.19.0 launcher and game, compile native game code, close cleanly and re-enable Play. Logs: `cache/launch-0.19.0-player1.log` and `cache/launch-0.19.0-player2.log`. The packaged game executable matches the Release build SHA-256. The package audit contains 559 files and excludes ROMs, private saves, caches and test binaries.

These are two-process local checks with the owner's US 1.0 ROM. WAN timing, US 1.1 gameplay and broader party/move combinations remain outside this acceptance run; the underlying serial exchange can still slow battles on higher-latency connections.

## 0.26.0 launcher, updater and Windows installer

- Release build: all 54 CTest groups pass, including profile identity/world
  preservation on username changes and updater traversal/hash/manifest checks.
- Updater fault injection: a locked second program file rolls back the first;
  corrupt/tampered archives are rejected, and user files remain unchanged.
- Packaged UI smoke: welcome, ROM validation, cartridge drag, multiple worlds,
  edit username, updates page, host capacity, join and profile reload pass.
  Screenshots were inspected for clipping and layout errors.
- Packaged and wizard-installed launchers both started the bundled game, compiled
  native blocks, closed cleanly and returned to world selection, using isolated
  profiles and a local ROM that is excluded from distribution.
- Installer installed all 944 manifest-verified files into a custom test folder;
  uninstall removed program files and retained an extra user file. Real player
  profiles and world saves were not opened by these tests.
- Git source audit: no ROMs, saves, checkpoints, identities, private keys, native
  game caches or credential patterns in staged source. Runtime artwork and
  dependency notices remain included.

These tests do not establish macOS support. Mac port and signing work are separate.