# Game hook evidence

As of 0.21.0, the experimental 3D scene builders, sprite capture path and renderer hooks are removed. Historical 3D notes below describe retired versions; current rendering uses the original framebuffer and the existing 2D host sprite compositor.

Hooks are restricted to the exact ROM hashes in ROM_SUPPORT.md. Address/name/size metadata was read from the independent [pokebot-gen3 symbol data](https://github.com/40Cakes/pokebot-gen3/tree/5dd898f830775d448b06db6f5cd65b930540f146/modules/data/symbols). Its player/map readers were consulted for factual layouts, without copying their implementation. Instruction/structure checks used the owner's local ROM.

## Shared RAM layouts
- gMain at 030030F0, callback2 at +4.
- gPlayerAvatar at 02037078; object index +5.
- gObjectEvents at 02036E38, 36-byte stride; coordinates include the seven-tile border.
- gSprites at 0202063C, 68-byte stride; camera offsets at 02021BC8/02021BCA.
- gMapHeader at 02036DFC; save pointers at 03005008 and 0300500C.
- Party count at 02024029, player records at 02024284, enemy records at 0202402C; party stride 100 bytes.
- Palette fade state at 02037AB8; active bit checked at +7.

## US 1.0 ROM addresses
| Function/data | Address |
| --- | --- |
| CB2_Overworld | 080565B4 |
| TryStandardWildEncounter | 080833B0 |
| CreateScriptedWildMon | 080A029C |
| StartWildBattle | 0807F704 |
| Object graphics info pointers | 0839FDB0 |
| Object sprite palettes | 083A5158 |
| Wild encounter headers | 083C9CB8 |
| Party icon palettes/table/indices | 083D3740 / 083D37A0 / 083D3E80 |

The party-icon palette base is `gMonIconPalettes`: three inline palettes of 16 RGB555 colors (32 bytes each), totaling 0x60 bytes. Species palette indices select `base + index * 32`. This is distinct from the six-entry `SpritePalette` descriptor table at 083D4038 (US 1.0) / 083D40A8 (US 1.1). Interpreting inline colors as descriptor pointers caused invisible icons before 0.3.1.

For the listed functions US 1.1 adds 0x14, and for the listed ROM data it adds 0x70, verified against that revision's symbol metadata. This is not a general offset rule for arbitrary symbols.

Map, sprite and palette pointers are range checked against ROM/RAM. Party records are decrypted with personality/OT ID, checksum verified, and reordered by personality modulo 24. Original object-event flags determine valid local visibility and movement.

Development-only FR_TEST_HARNESS fixtures call actual guest functions to create synthetic parties and warp to a test map. This target is not installed. It does not enable a remote memory/debug interface. Fixture output and frame captures remain in ignored cache directories.

## Pixel movement and animation (0.3)
Local disassembly of SetSpritePosToMapCoords (08063B1C, US 1.0) establishes the camera transform: subtract save-block camera coordinates, total pixel offsets (0300506C/X, 03005068/Y), and signed tile remainder (03005060/X, 03005064/Y), then compensate by one tile in the remainder's direction. Sprite creation adds 8 horizontally and 16 plus signed center-to-corner Y vertically. Inverting this transform yields map pixel position throughout a step, rather than its destination tile.

Sprite pos1 is +32/+34, pos2 is +36/+38, signed center offsets +40/+41, animation table +8, animation number +42 and command index +43. AnimCmd_frame (080079FC, US 1.0) reads the image index as a halfword at a four-byte animation command. End/jump commands retain the preceding image. OAM attribute 1 bits 12/13 provide horizontal/vertical flip. Raw pointers and frame pixels stay local; only bounded semantic pose fields cross the network.

A development replay covering right/up/left/down movement verified pixel deltas against the camera fields every frame. Its diagnostics live under ignored cache/motion-probe and cache/motion-ui-check.

Presentation state is sampled at CB2_Overworld entry, when the preceding field update is complete. PPU frame callbacks may interrupt CameraUpdate between its tile and pixel writes; sampling there produced a one-frame 16-pixel reversal in a hardware-rendered replay. Player pose and camera are now captured together at the field boundary.

## Native scene composition (0.3.2)
The completed RGB frame alone cannot distinguish a roof, a menu and a trainer. An opt-in PPU context now retains its original two leading color candidates, native sprite order, window mask and blend registers at each scanline, and latches that data with the framebuffer. Added ROM-derived sprite pixels are inserted into that context before resolving color effects. Native guest pixels and emulation memory are not rewritten. The host context is omitted from snapshots and invalidated on reset/load.

Local US 1.0 disassembly verifies BuildOamBuffer at 08006BA8, AddSpriteToOamBuffer at 08008A64, and CopyMatricesToOamBuffer at 08006EB8. Their US 1.1 symbol addresses add 0x14. Submission receives the Sprite pointer in R0 and a pointer to the current OAM count in R1. Each emitted OAM entry, including subsprites, is associated with its source sprite's subpriority and ground coordinate. The final count is consumed at CopyMatricesToOamBuffer entry, immediately after submission and before the stack-local count is reused. Four bounded submission generations are matched against actual OAM attribute bytes during scanline composition; unknown guest sprites retain precedence within their priority.

Sprite subpriority is byte +67; native coordinate-offset enable is byte +62 bit 1. Ground position uses pos1 Y minus signed center-to-corner Y plus the enabled camera offset, excluding jump/bob pos2. Local and remote trainers use the same room-slot tie breaker; followers and wild icons have separate stable tie ranges.

SetObjectSubpriorityByElevation (080682F8) and UpdateObjectEventElevationAndPriority (080681EC) select sElevationToSubpriority at 083A706C and sElevationToPriority at 083A707C. The former combines the elevation base with the ground row using an eight-bit wrap and a one-point object offset. These tables add 0x70 in US 1.1. All addresses are restricted to supported ROM identities; no arbitrary revision offset is inferred.

The native PPU path is unchanged when no host sprites are submitted. BG/OBJ priority, transparent texels, per-line hardware windows, blanking, alpha and brightness are retained. The integrated SDL sidebar is composed outside the bounded 240x160 game image. Independent follower pathfinding, and creating every original grass/shadow field effect for remote objects are separate work.


## Names and chat (0.4.0)

FRMP 4 sends chat separately from motion and cable traffic. The host supplies the authenticated slot, ID, username and global sequence; clients cannot supply their own author. Each receiver timestamps new messages with its own monotonic clock. Presence records include the room sequence at join, so leaving/rejoining cannot revive earlier speech. History is bounded to 100 messages, text to 128 UTF-8 bytes, and send rate to two messages per second per player.

Name and bubble images are rasterized once per text change with Pixel Operator, then composed within the 240x160 viewport. Name pixels have a one-pixel black glyph outline and transparent background. Bubble corners stay fixed while center/edges tile around word-wrapped text. Placement follows the speaker and considers all visible player bodies/names and earlier bubbles in stable room-slot order. Annotations use OBJ priority 1 above the field, below native BG0 interfaces and priority-zero objects, and still honor latched hardware windows and color effects. Character/Pokemon sprites retain their elevation and ground-depth ordering. The frame chat dock is rendered separately from the game image.


In 0.4.1, PNG tiles retain their original 16x16 RGBA pixels. Thin tiles supply single-row frames, and regular corners/edges/center supply two or more rows. There is no generated pointer. Host composition carries optional original RGBA alongside the RGB555 value, preserving exact PNG channels when the UI object wins without native lighting; original native priority, windows, blanking and effect handling remain in place. Text stays at least three pixels from the frame edges and clear of the original corner icon.


## Directional followers (0.5.0)
The local supplied four-column/four-row sheet uses rows down, left, right, up. Exact duplicated 2x texels can be collapsed losslessly; foot placement uses a stable sheet anchor. FollowerPath evaluates a point 16 path pixels behind the trainer, faces its own movement segment, holds its pose between repeated samples, and uses trainer facing while idle. Follower facing and animation are explicit FRMP 5 fields. MotionTimeline preserves the segment direction during interpolation, preventing backward-facing travel after a turn.

Species filenames are derived from validated-ROM gSpeciesNames at 08245EE0 (US 1.1 adds 0x70), with the pack's NIDORANfE/NIDORANmA aliases. Missing local sheets use the original ROM party icon. Reference packs and cartridge label art remain local and outside the package allowlist.

## Shared world and story (0.6.0)
FRMP 6 sends bounded semantic NPC/map/spawn state, host-arbitrated encounter leases and allowlisted story flag/variable transactions. There are no ROM pointers, extracted art, RAM blocks, party records or save blobs on this channel. Story updates use per-key revisions; the host rejects stale writes and unknown IDs; canonical personal-achievement proofs never overwrite native trainer achievements. Encounter requests include a story precondition when triggered by a map scene, so an old scene cannot restart after its completion commits. Queues retain existing protocol size limits. A late claim response is cancelled on timeout.

Native hooks use US 1.0 entries UpdateObjectEventCurrentMovement 08063DB8, GetInteractedObjectEventScript 0806CFF4, TryRunCoordEventScript 0806DD80, CheckTrainer 08081B84 and TryRunOnFrameMapScript 08069C74 (US 1.1 +14). Spectators mirror native NPC coordinates, direction and animation; the original game still renders native NPCs, collision and field composition. Story-created NPCs are spawned/despawned locally through native helpers while retaining the receiver's original story bits. Only the encounter owner runs its original dialogue/battle and movement scripts. Other native actors may have independent owners, including on other maps. Reported NPCs are keyed by map and local object ID, not volatile sprite slots.

Shared flags and map-scene variables are reviewed explicitly in src/game/story.hpp. The supported save layout has flags at SaveBlock1+0EE0 and script variables at +1000. Updates wait for ScriptContext status SHUTDOWN (03000EA8 == 2), unlocked field controls (03000F9C == 0), normal overworld callback and no fade/recap. Native map load/redraw helpers refresh changed doors and objects. Personal state and received-item flags are not copied. Native AddBagItem (0809A084) handles selected personal story rewards; the associated claim bit changes only when delivery succeeds.

Quest Log states at 0203ADFA (2/3) and 03005E88 (1/3) suppress live overlays and world participation. The original historical native sprites and grayscale palette remain responsible for playback; current party composition is never used to invent a historical follower.

Factual layout/behavior cross-checks: [object definitions](https://github.com/pret/pokefirered/blob/master/include/global.fieldmap.h), [script contexts](https://github.com/pret/pokefirered/blob/master/src/script.c), [flags](https://github.com/pret/pokefirered/blob/master/include/constants/flags.h), [variables](https://github.com/pret/pokefirered/blob/master/include/constants/vars.h), and the previously recorded exact-ROM symbol metadata. These references supplied addresses/behavior facts; their code and game assets are not bundled.

Cable Club, Wireless Club and Union Room attendants keep their native per-player service sessions. Both trainers must be able to use these services concurrently for link battle/trade entry; they do not take a shared story-encounter lease.

The common service entries are US 1.0 081A8CF6/081A8CFC/081A8D02 and US 1.1 081A8D6E/081A8D74/081A8D7A; these script addresses have their own verified revision adjustment. Native linked scenes at 03003F64 bypass live overworld overlays and synchronization hooks.

In 0.6.1, the wild authority/claim gate is separate from story readiness: a usable party Pokemon is sufficient for shared grass spawns and exclusive wild contact. That version required Pokedex/onboarding for shared story and NPC scripts; 0.17.0 starts after the personal starter/lab battle. This preserves Route 1 encounters on the first trip after obtaining a starter.

## 0.7.0 reward and wager integration

The owner's session policy is limited to TMs, selected story gifts and explicit one-off Pokemon encounters. Money cannot be shared. Gift hooks wrap ScriptGiveMon and GiveMonToPlayer and commit only successful original-game receipts under a unique encounter lease. Special scripts use a +0x78 revision offset; the hooked functions use +0x14 in US 1.1. Tests execute the owned US 1.0 ROM, not US 1.1 gameplay.

Native wallet transactions call SetMoney (0809FD70) and TrySavingData (080DA364) on the game thread. The latter returns SAVE_STATUS_OK (1). SaveBlock1 + 0x3D24 is the original unused 16-byte field; the wager marker records a transaction hash and deposit/completion phase. Acknowledgements wait for the runtime's dirty flash buffer to clear after atomic disk replacement. Linked battle outcomes use BattleMainCB2, BATTLE_TYPE_LINK and gBattleOutcome. Ordinary trainer/wild outcomes cannot settle wagers.

Semantic addresses and original function behavior were checked against the upstream pret/pokefirered symbol maps and primary sources: src/script_pokemon_util.c, src/money.c, src/save.c, include/global.h and include/constants/battle.h. No game implementations or ROM data are transmitted or packaged.

## 0.8.0 camps
`world_camp.inc` checks gMapHeader's mapType at +0x17 and cave byte at +0x15. Only town/city/route types qualify. The General primary tileset is 0x082D4A94 (US 1.1 +0x70). Ordinary full lawn IDs 0x001, 0x008, 0x009, 0x010, 0x011 were visually checked from the owned ROM; 0x00D and its tall-grass variants, decorative edges, flowers and paths are excluded. MB_NORMAL by itself is insufficient. Current metatiles come from gBackupMapData (0x02031DFC), with ROM layout width+15, height+14 and origin offset (7,7), retaining collision and elevation checks. Every footprint cell and the connected roaming ground must pass.

GetCollisionAtCoords at 0x080636AC (US 1.1 +0x14) returns COLLISION_OBJECT_EVENT for active tent tiles; original checks continue elsewhere. GetMonData obtains each non-egg party species without changing the party. Camp input filtering holds native keys released; host chat/UI input remains independent. The original object-depth compositor handles tent/party occlusion, native windows and UI. The exact three user PNG frames are loaded with WIC and composited at native size. Maps and source pixels are never patched.

Layout/behavior reference: https://github.com/pret/pokefirered/blob/master/include/global.fieldmap.h , https://github.com/pret/pokefirered/blob/master/src/fieldmap.c , https://github.com/pret/pokefirered/blob/master/include/constants/metatile_labels.h , https://github.com/pret/pokefirered/blob/master/src/event_object_movement.c .

## 0.9.0 battle presence and audio
CreateBattleStartTask (0807F690 US 1.0, code revision offset for US 1.1) latches the field trainer before native transitions. The existing visible-wild path explicitly latches after StartWildBattle because guarded guest calls skip hook recursion. Link, Old Man and Pokedude demonstrations are excluded. After BattleMainCB2 (08011100), gBattlersCount (02023BCC), gBattlerPositions (02023BD6) and gBattleMons (02023BE4, 88-byte stride; species +0, HP +40, max HP +44) identify the actual active battlers. Initialization waits for valid native data. Party menus retain the last scene; native faint/switch data updates it. Returning to a real active field clears it.

Addresses/layouts follow the supported ROM and the primary pret/pokefirered battle_setup.c, battle_main.c and include/pokemon.h references: https://github.com/pret/pokefirered . No Pokemon stats, battle outcome or player save position are changed by battle presentation. The harness alone provides an isolated low-HP party fixture and a money-display diagnostic; neither is compiled into the shipped runtime.

The product shell supplies an optional 0-100 volume callback to HostWindow. Its existing signed 16-bit sample gain applies the percentage before SDL audio delivery. No guest sound registers change. Wallet display decodes the actual save-block money/key during battle as well as exploration; safe wager save/debit/credit gates are unchanged.

## 0.10.0 camp walking
Camp input is held only while a room placement is pending. GetCollisionAtCoords compares its ObjectEvent pointer against gObjectEvents[gPlayerAvatar.objectEventId] (02036E38 + 36 * byte[0203707D]); only the camper's native object is restricted to its accepted walking mask. The original function handles ordinary collisions. Walking to a different permitted tile no longer packs camp. A complete one-tile ordinary-lawn loop around the tent is required, and the selected connected component contains the trainer at setup. Native party wandering uses the same immutable mask with dynamic actor avoidance.

Camp ground lines use the existing native composition frame and a key below same-plane OBJ and BG1, above BG2 ground. Only visible BG2/BG3 pixels are eligible. The two-pixel light green perimeter encloses the tent plus walking ground and respects camera transforms, native windows, foreground and menus. It never modifies map tiles, source art, OAM, VRAM or save data.

## 0.11.0 voxel presentation
The read-only adapter reads gMapHeader / live map grid, metatile definitions, BG VRAM and palettes for procedural geometry. Native object events and existing room timelines supply actor poses. The PPU adds latched OAM attr2 to host CompositionSample metadata to separate battle art from health bars; this metadata is neither serialized nor visible to the guest. gBattlerSpriteIds at 02023D44 and live OBJ tiles/palettes provide complete battle images. Battle cards render above scenery and below native UI at original pixel dimensions. No voxel code writes guest maps, flags, party records, money or saves. Camera-relative controls apply only in controllable field scenes.

## 0.12.0 released Pokemon
`ReleaseMon` (US 1.0 08093218, US 1.1 0809322C) is the native successful deletion boundary; canceled/refused releases never call it. `sCursorArea`/`sCursorPosition`/`sIsMonBeingMoved` (02039820/21/22), `GetBoxedMonPtr` (0808BD30 / 0808BD44), the native party, and the held Pokemon at `gStorage + 20A0` identify the actual individual. The adapter journals semantic Pokemon fields before calling the original function.

`GetSubstruct` (0803F940 / 0803F954) resolves native block permutation. The adapter validates the native checksum, retains individual fields, and rebuilds a native enemy with the original personality and stats. `BoxMonToMon` (0803E774 / 0803E788) initializes its party state; `PlayCry_Normal` (08071DF0 / 08071E04) plays the locally owned native cry. No ROM art, audio or memory pointers are sent to the room.

Map navigation reads `gMapGroups` (083526A8 / 08352718), bounded native layout/collision/behavior data, door warps and edge connections. Crowd positions reserve separate cells and keep the entrance corridor open. Room authority owns every release, its pose and encounter claim; actor interpolation shares the existing environment composition rules. Resting persisted actors explicitly outlive the live-player interpolation timeout.

Before `TrySavingData(0)`, release/recapture autosaves call `SaveMapView` (080590D8, US 1.1 +14) and `SaveQuestLogData` (08112450, US 1.1 +14), matching the native save dialog preparation. `TrySavingData` serializes party/objects itself. A release is not offered until the native flash is flushed. Pending records are reconciled against native ownership after an interrupted save; room claims remain exclusive until their local battle receipt is resolved.

Interface references: [native storage types](https://github.com/pret/pokefirered/blob/master/include/pokemon_storage_system_internal.h), [Pokemon data layout](https://github.com/pret/pokefirered/blob/master/include/pokemon.h), [map layout/connection types](https://github.com/pret/pokefirered/blob/master/include/global.fieldmap.h), [native save interface](https://github.com/pret/pokefirered/blob/master/include/load_save.h). Hook/call addresses were checked against both existing revision symbol maps and the owner's local ROM; no game-source files were copied into the project.

## Shared campaign adapters (0.17.0)

Canonical progress now persists separately from native trainer saves. Trainer defeats, Badges, League completion and supported reward receipts can serve as world proofs while their native fields stay personal. Supported travel permission queries consult the campaign only at known original field-move/route checks. `FlagGet` (0806E6D0) also provides claimant-only NPC script receipt checks when optional sharing is off. `ScrCmd_goto` (08069FB0) guards eight unconditional Gym TM gift jumps against duplicate delivery.

The parcel adapter synchronizes Viridian Mart's 4057 stages and projects original Pokedex access after completion. Viridian's original old-man transition subscript updates his saved template when the shared delivery opens the road. Starter choice, opening lab battle and League sequence stay personal. Hidden Rocket item flags are not sufficient proof of possession: Lift Key and Silph Scope use native `CheckBagHasItem`.

`world_dialogue.inc` allocates a small native message script with `AllocZeroed` (08002BB0), encodes and wraps measured native font glyphs, and uses `ScriptContext_SetupScript` (08069AE4). Native message/wait/close instructions supply the game's selected box, font, page controls and rendering layers. Allocation is freed only after the script shuts down. Campaign updates and camp rejections queue behind native dialogue/battles; no game-frame popup is used for these notices. `SaveMapView` (080590D8) and `SaveQuestLogData` (08112450) prepare each field checkpoint before `TrySavingData` (080DA364). Campaign, wager and release checkpoints share this preparation so cold loads retain the correct map tiles.

These addresses are US 1.0. Supported US 1.1 function offsets, script destinations and font tables use their revision-specific adjustments. See CAMPAIGN.md and VALIDATION.md for tested coverage.


## Native field challenges and Camp menu (0.19.0)
`world_field_symbols.inc` contains separately checked US 1.0/1.1 addresses; later UI routines do not use the small early-code revision offset. The interacted-script hook recognizes a facing adjacent remote trainer. Allocated native scripts use lockall/message/waitmessage/multichoice/releaseall; guest menu functions render choices and an exact six-digit wager amount. Allocation cleanup waits for the script to finish. Invalidated invitations destroy only their own active multichoice task/window.

Normal Start menus insert an unused Retire action between Bag and Player, replace its label/help/callback for CAMP or PACK UP, and preserve Safari Retire. Eight-row menus use the ordinary seven-entry window height with 13-pixel row/cursor spacing; the original font and artwork are retained.

Field battles call the original field transition and native link battle engine, with single-battle flags and saved return callback. Before entry they restore wired serial/timer handlers and SerialCB: indoor RFU checks can otherwise leave the wireless interrupt vector installed. Return/error hooks preserve map coordinates, restore the full private party, and re-enter the normal field callback. The wallet commits results only after safe return. US 1.1 symbols are mapped but this new gameplay flow has been exercised on the owner's US 1.0 ROM.

## Conditional story collision (0.21.0)

The shared field collision hook uses `GetCollisionAtCoords` (`080636ac`, US 1.1 `080636c0`) after native terrain/object/elevation collision. Only voluntary local-player movement is guarded; original scripted actor movements and Camp restrictions are preserved. Coordinate events come from `gMapHeader.events`, using the native 16-byte structure, elevation-zero wildcard, and byte-cast index comparison. Non-null conditional scripts with variables `4000..40ff` participate; weather and immediate scripts do not.

The host arbitrates an encounter reservation before crossing an active trigger. It stays live through the step until `TryRunCoordEventScript` consumes it; abandoned reservations expire after 120 frames. Canonical/local mismatches wait for normal campaign application. A denied coordinate event that arrives without preflight returns a native delay/goto script, then resumes its original ROM script after acquiring ownership, or ends if the canonical condition has cleared. Allocation failure also keeps movement closed while retrying. No event is intentionally discarded to permit walking past it.

Reference: original [coordinate-event handling](https://github.com/pret/pokefirered/blob/master/src/field_control_avatar.c), [collision](https://github.com/pret/pokefirered/blob/master/src/event_object_movement.c), and the Viridian/Pewter map scripts. Revision addresses are checked against local symbol maps; native acceptance uses the owner's US 1.0 ROM.

## Party indicators (0.22.0)
The field snapshot reads `gPlayerPartyCount` at `02024029`. Egg bits use native `GetMonData(..., MON_DATA_IS_EGG=45)` at US 1.0 `0803fbe8`, US 1.1 `0803fbfc`, for occupied records at `02024284 + slot*100`. A cached personality/OT/header/checksum key invalidates slot metadata after party changes. The normal semantic player and battle-presence packets carry the count and Egg mask; no asset or Pokemon record is added to the room payload. Existing priority-one annotations render each unchanged supplied PNG above the outlined username, behind native priority-zero menus.

## Connected-map snapshot boundary (0.22.1)
`captureNpcs` runs inside `completedWorldHook`, before `frame()` assigns `local=completedPlayer`. Hidden-object reporting must therefore use `sharedMap(completedWorld.group, completedWorld.map)`; the previous implicit lookup used `local` and could append a Viridian removal record to a new Route 22 snapshot. The observed failing pair was report `3,41` with hidden NPC 1 still tagged `3,1`. No new ROM hook or map-specific exception is required. `npcMovementHook` additionally requires its freshly read player map to match `local` before applying a cached NPC pose. Detailed invalid-report errors now identify offending map/NPC data without dumping saves or party records.

## Client accessory input (0.23.0)

The optional `HostShell::controller_keys` callback contributes active-low GBA buttons before the common UI input-capture gate. It does not hook or write native game state. Poke Ball Plus reports are decoded on a Windows BLE worker; the game thread reads a mutex-protected snapshot, applies freshness/focus and neutral rearming, then merges the resulting keys with existing keyboard/SDL input. The usual native `game::filterInput` still enforces camp, battle and story locks. No wire changes. Test-only report injection is compiled only into `fr_game_harness`.

## Field-battle trainer identity (0.23.2)

Native Cable Club seats call `SetLocalLinkPlayerId` before the link-player exchange. The direct field transition omitted this, leaving both `gLinkPlayers[].id` values at zero on fresh sessions (or retaining a previous seat). `GetBattlerMultiplayerId` searches four entries and returns four when the opponent ID is absent; battle placeholders then copy the unused fifth name field without an EOS terminator. This explains why the VS screen, which indexes cable players directly, could look correct while dialogue contained blanks and symbols.

Field entry now calls the original `SetLocalLinkPlayerId` at US 1.0 `080096f8` / US 1.1 `0800970c`, before `CreateBattleStartTask`. The lower member of the paired room slots uses native ID 0, the higher ID 1, matching `Session::cableControl` for any pair. Native link code still exchanges trainer names, IDs, language and gender normally; saved names and OT data are not rewritten.

References: [Cable Club seat initialization](https://github.com/pret/pokefirered/blob/master/src/cable_club.c), [native link-player initialization](https://github.com/pret/pokefirered/blob/master/src/link.c), [GetBattlerMultiplayerId](https://github.com/pret/pokefirered/blob/master/src/pokemon.c), and [battle name placeholders](https://github.com/pret/pokefirered/blob/master/src/battle_message.c). Revision addresses match the local symbol maps.

Test builds can trace text at `BattlePutTextOnWindow` (`080d87bc` / `080d87d0`), including bounded encoded text and the five native player name/ID records. This trace is excluded from production builds.


## Failed story battle retry (0.24.0)

`CreateBattleStartTask` checkpoints visible tracked scene actors before the native battle transition. The original `CB2_WhiteOut` entry (US 1.0 `080566a4`, US 1.1 `080566b8`) abandons ownership and restores uncommitted shared script values, then continues the ROM's original blackout, private money loss, healing and warp. A later safe field boundary commits successful native script changes normally.

`ScriptMovement_StartObjectMovementScript` (`08097434` / `08097448`) records participants in the first attempt. On a retry, previously staged actors use the ROM's own short facing-in-place movement instead of replaying the pre-battle relative approach; native `waitmovement` still completes. After battle begins, the original exit movements are preserved. `ObjectEventTurn` (`0805f218` / `0805f22c`) turns the current trainer toward the encounter. Ownership handoff adopts authoritative coordinates as well as native movement state before a script can run. Waiting actors remain visible despite their original spawn-hide flag.

Reference: [Route 22 scripts](https://github.com/pret/pokefirered/blob/master/data/maps/Route22/scripts.inc) advance Gary seven tiles for the top/middle trigger and eight for the bottom trigger. Reapplying that movement to his existing live object caused the reported overshoot. Scriptless retry interaction validates the actual facing tile and selects the nearest still-active original coordinate branch. [Native trainer battle completion](https://github.com/pret/pokefirered/blob/master/src/battle_setup.c) and [blackout/healing](https://github.com/pret/pokefirered/blob/master/src/overworld.c) remain authoritative for the individual battle outcome.
