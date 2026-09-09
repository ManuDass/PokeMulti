# PokÃ©Multi project status

## Current release: 0.23.1

- Controller connection returns input to the game; a visible Resume game button and input status explain paused controls. The neutral-stick gate accepts the configured movement dead zone. Native regression covers connection without a viewport click, later chat/window focus, and explicit Resume.

- Client-local Poke Ball Plus Bluetooth LE discovery, selected-device connection, input and battery notifications, disconnect, native Start chord, per-profile controls and a live tester in Options > Controller. No virtual controller driver.
- Original supplied Collada geometry/UVs/texture load locally for a draggable UI preview; gameplay stays native 2D. The controller page fits at 940x650.
- Software and native input tests pass; physical accessory connection awaits a live ball (the local scan found none awake).
Gameplay is 2D only. The paper/voxel renderer, World tab, camera controls, shaders and graphics dependency have been removed from the active build. Historical renderer work below describes retired releases. Existing gameplay, native Camp/challenge UI, saves and FRMP 17 room behavior continue. Conditional story roadblocks now retain collision while another trainer owns the scene, with native deferred-event handling for nonwalking arrivals.

## Scope
Preserve playable FireRed while adding native runtime integration, separate saves, two-to-four-player overworld rooms, friends, followers, visible encounters, PvP and trading. Load only the owner's verified ROM, directly or from ZIP. Do not distribute a ROM, BIOS, extracted game assets, generated game translation, or Essentials reference packs.

## Implemented
- Connected-map NPC snapshots use their captured map; old-map NPC poses cannot be applied during entry, fixing the Viridian western-exit crash (0.22.1).
- Live overhead party rows using the supplied Pokemon/Egg icons in native party order, shared with room viewers (0.22.0).
- Native nearby X challenges, in-game wager/consent menus, field PvP, restored parties and safe wager settlement (0.19.0).
- Native Start > Camp / Pack up between Bag and trainer name; G shortcut.
- Native red/cream Windows launcher, strict ROM validation, profiles, settings and ZIP loading.
- Generic ARM/Thumb/hardware runtime with on-demand local x64 compilation and instruction fallback.
- BIOSless service/IRQ integration, renderer, audio, controller/keyboard input and cartridge saves with backups.
- Revision-scoped real-game map, player, party and encounter hooks.
- Four-person rooms, host-assigned slots, timestamped pixel/pose state, completed-field snapshots and buffered remote sprites.
- Single-window modern retro game frame with an integrated Room/Friends/Options sidebar.
- Persistent local friends, room presence, invitation acceptance and virtual serial cable transport.
- Nonblocking party-icon followers and visible grass encounters entering the original battle engine.
- Debug/Release builds, synthetic device checks and actual local-ROM gameplay/UI checks.

## Step 4 acceptance
The launcher-only milestone has been replaced by a functioning experimental runtime. US 1.0 reaches the title, starts a normal new game, walks through the player's house, saves through the game menu and continues from a fresh process. Controller contact with a visible wild entity opens its matching real battle. Two connected processes show distinct trainers on different tiles of the same map.

A two-way trade has completed and both cold saves restored the exchanged parties. A full two-player battle produced matching win/loss results, wrote both saves and returned both trainers to the Colosseum with the link active. The explicit package and packaged-launch checks pass. Native execution is hybrid, not a fully static recompilation. Full campaign compatibility, broader visible-encounter terrains, sprite occlusion and US 1.1/cross-PC runs also remain.

## Remaining acceptance work
1. Broaden completed battle coverage to additional teams, moves and connection conditions.
2. Expand trade checks beyond the completed two-way starter exchange and cold-save restoration.
3. Validate door/route/elevation/disconnect behavior on two PCs.
4. Extend encounter modifiers/terrain compatibility and follower occlusion.
5. Broaden native/interpreter differential checks through a full campaign and US 1.1.
6. Keep the explicit package audit and bundled-toolchain boot checks in the release workflow.

## Reference material
The owner supplied Following Pokemon EX 2.5.1 and Pokemon Essentials 21.1. Their movement/transfer and encounter concepts were inspected. In 0.5.0 the runtime reads the owner-supplied directional follower sheets locally; the reference packs are excluded from distribution. FireRed behavior and native visuals still come from the owner's ROM.

See docs/ARCHITECTURE.md, docs/VALIDATION.md and docs/THIRD_PARTY.md for implementation, evidence and dependency terms.

## PokÃ©Multi 0.5.0
WASD aliases preserve arrow/controller input and respect UI typing focus. The supplied Program_Icon.png is copied unchanged and scaled with nearest sampling. The launcher presents a draggable, depth-buffered GBA cartridge using the owner's Cart Art image, with a local image picker and ROM-derived fallback. Followers use four directional rows, their own trail heading, and a shared FRMP 5 pose timeline. Existing profile paths and saves are retained. LeafGreen/other ROM execution remains future work.

## PokÃ©Multi 0.6.0
Shared story with personal teams/inventory/rewards; authoritative NPCs, scripted NPC appearance/movement and exclusive grass-wild encounters; live overlays suppressed during Quest Log playback; supplied Pixel Operator fonts throughout both interfaces. Selected story rewards remain independently claimable when a shared event removes the giver. Full-campaign and cross-PC compatibility testing remain ongoing, and LeafGreen/other ROM support is still future work.

## PokeMulti 0.7.0
The host chooses shared availability of gym/story TMs, selected story items and supported special Pokemon before opening each session. All clients receive the same fixed policy. TMs and story items default on; special Pokemon default to first claimant. Money is always individual and is excluded from the policy. Special encounters use exclusive native gift/catch receipts and shared claim/hide state without copying parties.

Agreed battle wagers save a deposit from each player's own native wallet, wait for matching original linked-battle outcomes, then save a winner payout or coordinated refund. Persistent receipts and the host ledger handle duplicates and interrupted sessions. The network uses FRMP 7. The supplied-font sidebar is refitted for four trainers and compact Host/Join/Options pages, with reward and wager details in dialogs. Original user artwork remains unchanged.

Native validation covers both Eevee policies, host-selected settings in four clients, disabled TM delivery, individual wallets, saved refunds, a complete original linked battle, safe Cable Club exit payouts and playable cold reloads. Current detailed evidence and final release checks are recorded in docs/VALIDATION.md. Full campaign, non-grass/roaming special mechanics and WAN gameplay remain broader compatibility work.

## PokeMulti 0.8.0
Temporary multiplayer camps add the owner's unchanged three-frame tent, checked ordinary outdoor lawn placement, connected roaming space for the complete non-egg party, playful neighbor interactions, native tent collision, and Camp/Pack up controls. Campers can chat while their trainer stays still. Names have no AFK tag. No healing, fetching, cooking, shared inventory or money changes are included.

## PokeMulti 0.9.0
Options contains session rewards (the Pokemon checkbox label is simplified) and saved game volume. An always-visible personal wallet and Camp button occupy a reserved row above integer-scaled gameplay. Wilds use directional walking sheets and synchronized poses. Remote wild/NPC battle scenes retain the trainer and show native active battlers facing and hopping, including faint/replacement updates, through FRMP 9. Original artwork, camp mechanics and safe wager saving are retained.

## PokeMulti 0.10.0
Campers can walk around their tent inside the same connected five-tile lawn area as their Pokemon. Placement requires a walking loop around every side. A shared, fixed two-pixel light green perimeter encloses the tent and walking area, drawn only at ground depth with no tile grid. Native boundary collision confines only the camper; packing releases the limit. FRMP 10 synchronizes bounded camp masks including late joins. No source artwork is altered.

## PokeMulti 0.11.0
Client-local Voxel tab and Direct3D 11 adaptation of supplied Dramaless Shape 2.0.4. Native map geometry, camera/lighting/water/quality settings, projected room sprites and camps, protected UI and battle cards. Gen 1 authored structure profiles do not transfer directly; FireRed shapes and first/third-person views remain experimental.

## PokeMulti 0.12.0
- Confirmed native PC releases preserve the individual Pokemon and commit a native save before sharing it.
- Jump/cry, indoor exit path, separate crowd positions around the entrance, and departure when the owner leaves the building.
- Reachable tall-grass routing through doors and map connections, with persistent populations and exclusive native re-encounters.
- Native map/quest-log save preparation, recovery journals, and host-owned captured-ID tombstones prevent accidental duplicates across retries/restarts.
- Shared 2D/voxel release presentation and compact feedback in the existing frame status row; FRMP 11 requires matching room clients.


## 0.13.0 battle presentation
- Preserve native battle sprites and HUD at original screen coordinates over voxel scenery.
- Retain complete 3D scenes through battle transition callbacks.
- Anchor shared overworld encounters to actual rendered positions and walk around obstructions without preset-position snapping.
- Walk back after battle, preserve follower continuity and transmit walking phases with FRMP 12.


## 0.14.0 scenery and map transitions
- Adapt Dramaless Shape's silhouette/banded building mesher and circular foliage hulls to native FireRed art.
- Separate roof and facade texture bands; retain Center/Mart emblems and profiled roof fixtures.
- Build complete trees and identify bush variants by original foreground tile references, preserving their native grass/path floor.
- Decode every upper-layer texel after transparent pixels; keep custom tree textures independent of animated map updates.
- Hold complete voxel scenes through native building/map fades, with original palette fading and native fullscreen menus.
- Dedicated profiles cover the surveyed Kanto structures and General-tileset trees/bushes. Other structures still require authored profiles for full parity.


## 0.15.0 reference terrain, signs and fences
- Remove colour-based object guesses and the generic procedural dome/box fallback.
- Translate Dramaless Shape's 8px terrain bands, repeated-volume measurement, shallow ledges, two-voxel signs, six-voxel per-cell posts and ground-height support.
- Identify native fence variants by their original foreground references; preserve original textures and component silhouettes on every exposed face.
- Share gameplay classification with the owner-ROM visual survey and verify front/side/back views.
- Record source mappings and remaining specialized-profile scope in the archived VOXEL_REFERENCE.md in cache/source-before-0.20.0.
- Match tree floors to native background colours across the map, excluding the tree silhouette and painted shadow; retain grass around route trails.


## 0.16.0 tilt shift and surrounding environment
- Use the supplied reference's horizontal and vertical Gaussian passes, including all three strength presets.
- Load real connected maps and each area's original tilesets before crossing a seam.
- Fill the remaining surroundings with native border blocks, preserving signed offsets and border phase.
- Preserve native world coordinates, actor support, live map edits, source assets and client-local settings.
- Verify actual slider persistence, measurable blur, native route crossing and original HUD pixels.

## PokeMulti 0.17.0
Persistent host campaigns distinguish shared narrative/world access from individual starter choice, Gym Badges, League career, Pokemon, XP and money. Ordinary trainer victories suppress repeat ambushes while permitting personal challenges. Essential tools remain available under every optional reward policy; original native receipts and gift guards prevent duplicate item delivery. Shared changes checkpoint safely through the original native save routine. The Room journal, campaign/reward notices and camp rejections use FireRed's own message box and text printer. FRMP 13 requires matching clients. See docs/CAMPAIGN.md for the explicit adapter coverage and remaining full-campaign validation.
