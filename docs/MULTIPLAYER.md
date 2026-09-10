# Multiplayer and friends

src/online/session.cpp implements a host-authoritative TCP room with a configurable 2-32 player limit. The wire protocol is FRMP version 20 with bounded packets/queues, validated UTF-8 identities and trainer fields, connection limits, room keys, heartbeat timeouts and explicit invitation acceptance. Winsock readiness events and a command wake event avoid timer-granularity delays on every serial word.

The host assigns room slots. Peers publish only semantic overworld state: map, tile, pixel position, elevation, facing, local avatar graphic index, active flag, animation image/flip/offset, follower species and path position, sequence and sample timestamp. Receivers draw from their own ROM. The game thread owns guest state; networking works on copies. World sessions also exchange bounded, authenticated checkpoint bundles containing each trainer's flash save and transaction receipts. No ROM, game assets, arbitrary memory or debugger commands are sent. See [world storage](WORLDS.md).

Story progression, NPC presentation and visible grass populations are shared. Encounters are owned by their triggering trainer, so other trainers keep moving. Each player retains their own party, inventory and money. Remote trainers do not consume original NPC slots. Matching map/elevation gates rendering; warps snap and clear history, while ordinary steps interpolate. Non-overworld callbacks, fades and invisible actors hide the extra entities. A stationary frozen actor remains visible. Different maps do not imply a broken connection.

## Party indicators (0.22.0 / FRMP 17)
One icon appears above the username for each occupied party slot, up to six. Egg slots use `UI Artwork/Party Icon_Egg.png`; other slots use `UI Artwork/Party Icon.png`, including fainted Pokemon. Empty parties have no icons. The row follows native party order and updates from the live count and native Egg flag, including after returning from menus and battle. Spectators use the battle presence's captured party metadata while that trainer is battling.

Both supplied PNGs are copied unchanged into the build and package. Their original RGBA pixels are placed at native resolution, with one transparent pixel between slots; the existing nearest-neighbor game viewport scales them. A two-pixel gap separates the row from the username. Nearby labels are placed as a group to avoid overlapping each other and trainer bodies; chat bubbles reserve that whole area. If an entire icon row cannot fit above a name at the top viewport edge, it is omitted rather than partially clipped. Native menus, dialogue and hardware windows retain compositor priority.

The room sends only the occupied-slot count and six Egg bits, not species, HP, individual Pokemon records or artwork for this indicator. Counts above six and Egg bits outside occupied slots are invalid. These indicator bytes were introduced in FRMP 17; the current release requires FRMP 19. Egg detection uses native GetMonData and caches each slot's header/checksum so steady movement does not repeatedly decrypt the party.

## Connected-map transitions (0.22.1)
The completed native field snapshot selects hidden NPC records by that snapshot's map, even when the previous frame still names the city or route being left. Incoming NPC movement also checks that the live native map matches the multiplayer frame before applying a shared pose. This prevents a Viridian NPC removal record from being sent as Route 22 data and rejected as an invalid local world update. Network validation continues to reject mixed-map records. Version 0.22.1 fixed that transition crash without changing FRMP 17. Current clients require FRMP 19.

## Movement presentation
State is sampled every guest frame, sent at a 16 ms network cadence and relayed immediately by the host. Membership snapshots do not add a second movement interval. An 80 ms timestamped history interpolates pixel positions; facing, animation and followers use that same playback time. Duplicate/stale sequence numbers are ignored. Stops hold their final position without extrapolation; map changes, large discontinuities and slot identity changes reset history. Clock wrap and stalled peers are bounded. All clients in a room must use protocol v20.

The camera transform is inverted from the local ROM's actual tile, signed sub-tile remainder and total pixel offsets. It is updated while moving. Avatar animation image indices and OAM flips come from the real sprite, replacing the old guessed direction frames.

## Friends UI
F2 toggles the integrated sidebar. Room, Friends and Options tabs frame the game in the same window. Host/Join/Leave and Add/Remove friend are implemented. Identity persists as a random 128-bit local ID. Friends are stored atomically with validated quoted UTF-8 text, with bounded file size and count. Online/offline refers to presence in the current room, not a global directory.

Hosting now exposes **Invite friends**, with separate Internet and LAN/VPN/same-PC invitations. Internet copy uses a public IPv4 discovered asynchronously through the IPv4-only `https://api.ipify.org` endpoint, the listening TCP port (default 38475), and the room key. It rejects private, loopback, link-local, CGNAT and other non-public addresses. A failed lookup leaves Internet copy disabled; refresh and custom public IPv4/external port fields are available. No room key, username, ROM, save or game data goes to the lookup provider. HTTP proxies are bypassed so their egress addresses are not accidentally advertised. HTTPS uses normal certificate validation, bounded timeouts/body size, and rejects redirects/non-200 responses. Leaving or rehosting invalidates pending results.

Friends use the launcher's **Join a friend > Paste invite > Join adventure**. This imports the address, port and room key without modifying spaces in the key. LAN/VPN invitations list local adapters separately, and **Copy same-PC invite** explicitly uses 127.0.0.1. These are only usable on that network or PC. Joining itself does not trigger a public-address lookup.

Discovery does not establish inbound reachability. The dialog explains forwarding the chosen external TCP port to the host's local room port and allowing PokeMulti through Windows Firewall. CGNAT, multiple routers and VPN routing may require additional network configuration. A custom public port only changes the invitation; it does not change the listening port or configure the router. This remains direct IPv4 TCP, without a relay, automatic NAT traversal, DNS/IPv6 joins or central login service. The room key and gameplay transport are not encrypted by this protocol; use a trusted private network/VPN when confidentiality is needed.

## Battles and trades
Face an adjacent room trainer and press X (native A), or use the Room panel Battle action while nearby. Challenge choices, editable wager digits, terms and acceptance use the original game's text box and bordered menus. GBA directional input chooses a digit/value; A confirms and B cancels. Both parties must contain a conscious, non-egg Pokemon. Busy scenes defer prompts; the recipient can decline. Pending invitations expire after 60 seconds, and the sender can press X again to cancel. Leaving invalidates the native invitation safely.

Accepted Battle pairs exchange authenticated readiness stages before starting a native single link battle directly from their current field map. Outdoor and indoor maps use the same path. Each participant retains its native position and original 600-byte party, restored after the match. Deposits save before readiness; payouts/refunds save after normal field return. The virtual cable closes after both clients return and the wager result is committed. Facing a nearby trainer and pressing T sends a Trade invitation with native in-game consent. Accepted Trade pairs retain their original Cable Club flow. Field entry assigns opposite native LinkPlayer battler IDs before the normal player-data exchange, using relative cable order rather than absolute room slots. The original trainer-name encoding and save identities remain intact.

The game supplies actual 16-bit GBA multiplayer serial words. Transfers preserve master/slave ordering and four receive slots, with disconnected slots reading FFFF. Only paired peers can exchange serial data. Transfer intervals coordinate the two guest clocks. IRQ-disabled polled transfers work, and a peer port reset cancels pending transfers promptly; normal GBA SIO busy, readiness, error and interrupt behavior is exposed to FireRed.

For a trade, both trainers must speak to the upstairs Direct Corner attendant (right counter in the tested Viridian Center layout) and select Trade. FireRed handles monster selection, exchange and its own saving. Use opposite console seats for trading. Exit the in-game Cable Club room before disconnecting or closing a game. Disconnecting an active cable can trigger FireRed's own communication-error screen.

**Trade acceptance:** two real game processes exchanged Bulbasaur and Charmander through FireRed's trade menu and animation. The original trainer IDs stayed with their Pokemon. FireRed wrote both 128 KiB flash saves, and fresh processes loaded the exchanged party data. A full battle with two Pokemon per trainer also completed with opposite win/loss results, both saves written and both trainers back in the Colosseum with the link active. See VALIDATION.md.

## Story roadblocks (0.21.0 / FRMP 16)
NPC conversation ownership no longer suppresses adjacent story trigger tiles. The host grants permission before the local trainer steps onto one; other trainers stay outside that trigger while continuing to use the surrounding map. Native dialogue uses the game's message box. Original scripted requirements and shared campaign adapters determine route unlocks. All room clients must use FRMP 19 (0.25.x).

## Encounter retry checkpoints (0.24.0 / FRMP 18)

An authenticated encounter owner sends one bounded NPC checkpoint before a native trainer battle. A failed release retains the participating actor IDs and canonical staged poses, while making the encounter available to another trainer. Waiting NPC poses cannot be overwritten by an arriving client's old spawn coordinates. Normal battle reports retain the lease; a new field map or participant disconnect abandons it. Successful completion clears its retry state, and a changed story condition invalidates obsolete retries. Stale tokens and foreign owners cannot release or overwrite a later attempt.

`EncounterCheckpoint` is packet 36. `EncounterRelease` carries a failed/retry byte, and `EncounterSnapshot` includes bounded retry keys and actor IDs. Older protocols are rejected. Only semantic poses and supported shared story values cross the room; ROM scripts and each trainer's possessions remain local.

## Validation boundaries
Four actual loopback sessions test room membership, movement, invitation consent, serial words, disconnects and wrong keys. Two actual game processes test Host/Join and adding a friend through the native UI. Distinct remote avatars were captured from both views using separate tile positions.

Cross-PC latency, loss, reconnect during a transfer, wider party/move combinations, and a full gameplay campaign require further end-to-end checks. No public internet service is deployed by this project. The current cable implementation waits on individual serial exchanges, so higher network round-trip latency can substantially slow linked battles and trades; WAN play is not performance-validated.

## Session reward choices (0.7.0)

The host sets a three-bit policy before starting each room. Welcome carries that policy before any story baseline. Unknown policy bits are rejected; joining clients cannot set or change it. Every session has its own choices: shared TM availability, shared story-item availability and per-trainer special-Pokemon availability. Defaults are TMs/items enabled and special Pokemon disabled. There is no money-sharing bit, code path or UI option.

Item receipt proofs reach the campaign independently of policy; native receipt flags remain personal. Checked categories permit one-copy automatic native Bag delivery. Unchecked categories skip delivery and native gift-script checks consult the campaign claim, preventing a second collection. Essential quest tools always remain available. Gym victory scripts also guard their unconditional TM gift jump against duplicate delivery. Special encounters use host-arbitrated unique leases and monotonic claim/hide flags when unchecked. The receipt commits after the original gift/catch routine succeeds, before nickname dialogue can create a disconnect window. The Dojo's two choices share a single claim. When checked, special claim/hide flags remain personal; parties are never copied or synchronized.

The explicit unique list is in src/game/unique.hpp. Fossil restoration, roaming encounters and purchases are outside that list. This is an experimental per-room policy, not a persistent public event server.

## Wager picker (0.24.1)

The amount picker uses the original native item-sale quantity-box presentation with a Wager label on the left and the money amount on the right. The ROM's unmodified animated red scroll arrows sit above/below the money column. Native small-font text includes the original Pokédollar glyph. Left/right selects the decimal place (highlighted in red), up/down changes the amount, A offers and B cancels. Leading zeroes are hidden unless needed to show the selected place. Both arrows, their native task, tile/palette resources and the picker window are released when it closes. Wager amounts, wallet limits, agreement and settlement rules are unchanged; FRMP 18 remains compatible.

## Agreed battle wagers

After applying a payout or refund, a native field message reports the personal result. A P100 winner sees "You won P100!" and that their P100 stake was returned; the loser sees "You lost your P100 wager." A refunded deposit is described as a refund, and an unreserved wager never claims that money was returned. The receipt is announced once per running client before acknowledging settlement to the host. Free battles have no wager message.


An invitation includes an immutable amount and host nonce; acceptance identifies that exact invitation. Zero remains free. A nonzero wager waits for both original games to reserve their deposits before pairing the cable. The native linked battle must start on both clients and report opposite win/loss outcomes before the host commits a winner. Draws or conflicting outcomes refund both deposits. An unrelated player cannot submit a participant's events.

Native money remains encrypted using each save's original key. SetMoney runs on the game thread at a safe overworld boundary. Deposit and payout acknowledgements follow the in-memory change. The host explicitly saves the world to persist the native trainer data and host ledger; wager phases do not trigger game saves. A transaction marker occupies FireRed's unused 16-byte field at SaveBlock1 + 0x3D24; a profile sidecar and host ledger retain recovery state. A duplicate snapshot or acknowledgement cannot debit or credit again. Stakes are limited to 499,999 and payouts must fit the native 999,999 wallet. No money is discarded if a payout is temporarily blocked by the wallet cap.

A disconnect cancels an unfinished wager; host restart changes unfinished records to refunds. Committed winners remain committed. Pending deposits must reconnect to the same host for coordinated recovery. The profile, its native save and its wager records must be kept together. This is a trusted-friends room model, without anti-cheat or a public authoritative economy.

Wager payouts and refunds apply at a safe normal-field boundary. Field battles return there automatically; no Cable Club exit is required. Save the world from the host game menu to keep those changes. Closing or crashing restores the last manual checkpoint.

## Camps (0.8.0)
World reports include an optional bounded camp: a unique placement ID, map/elevation, a fixed 4 x 3 footprint, animation tick and up to six species/position/direction/pose records. CampSnapshot (26) carries all four camp slots and host decisions. Only a client's authenticated slot may publish its camp. The host arbitrates intersecting placements, checks known player/NPC occupancy, fixes accepted geometry and species, and removes reservations on leave, inactive reports or stale presence. Rejected placement IDs cannot silently become accepted later.

Clients inspect the supported ROM's current outdoor map and live lawn tiles before requesting a reservation. Terrain validation is local, like the existing wild population model; this is a trusted room protocol, not an anti-cheat server. No party stats or save payloads are sent. Remote party poses use the same 80 ms timestamped interpolation as trainers. Camps are scene-scoped, temporary presentation and collision state; no party, wallet, item or story rewards are modified.

## Battle presentation and wild movement (0.9.0)
World reports include a bounded battle presence with a fixed trainer anchor, tick and up to four actual battler positions/species/visibility/facing records. BattleSnapshot (27) distributes all slots. Authenticated clients can update only their own battle; inactive overworld reports retain it, while normal field return, stale presence and disconnect clear it. Camp and battle state are mutually exclusive. Native linked rooms are excluded.

Remote scenes use the same timestamped 80 ms motion history, map/elevation gating, native terrain compositor and outlined usernames as ordinary trainers. Their normal walking avatar/follower is suppressed to prevent duplicate actors. Battler tiles are chosen from nearby unoccupied walkable terrain; constrained alcoves retain a compact encounter anchor. This is presentation only, not a change to the owning game's battle engine or save coordinates.

Wild records now include facing 1-4 and animation frame 0-3. Authority publishes the directional sheet pose through the full 32-frame tile step; followers and wilds use the same local sprite loader. Session rewards and player capacity are configured in the launcher's Host world screen; connected players inspect reward rules in Options. Money remains personal. The wallet is readable during battles; wager saving still waits for the original safe overworld boundary.

## Camp walking boundaries (0.10.0)
FRMP 10 adds eleven 11-bit rows (22 wire bytes) to each camp. They represent a fixed connected lawn area inside the five-tile radius, excluding the tent footprint. Validation requires an open two-tile front entrance, valid map coordinates, enough connected lawn for the party and trainer, and party poses inside the area. The tent may back onto a ledge or trees; its twelve footprint tiles must still be clear short grass. The host locks this mask along with the accepted tent geometry; moving actors cannot change it. Snapshots and late joins receive identical masks. No ROM data or source graphics are transmitted.

As of 0.24.2, the mask confines camping Pokemon only. The trainer can cross it with normal native movement. Once the owning trainer's ground position leaves the mask, the client silently packs up and publishes an empty camp; the existing CampSnapshot clears the tent, party poses and boundary for everyone. Merely attempting a blocked step does not pack up, and visitors leaving do not affect the owner's camp. Native terrain and tent collisions remain in effect. Manual packing remains available. FRMP 18 is unchanged. The outer perimeter encloses the union of the walking mask and tent footprint; shared tile edges and enclosed holes have no line. Its two-native-pixel stroke uses the local ground layer, beneath native and host objects, BG1 foreground and BG0 UI, with normal window/effect masking.

## Persistent campaigns (0.17.0)

FRMP 13 adds bounded CampaignSnapshot packets: campaign ID, sequence and named objective completions with authenticated actor names. The host atomically stores validated story state and its journal under the selected world's `campaigns` folder. Rehosting restores this baseline before any native host-state proposal. Choosing New world creates a separate adventure and trainer slots without modifying other worlds. Joining trainers receive the current world after their personal starter boundary.

The campaign's canonical proof of a trainer/Gym victory is separate from each trainer's native victory and badge flags. The original trainer card, obedience, payouts, Gym challenges and League sequence remain personal. Supported travel permission checks consult campaign access. Script encounters retain their actor leases; passive players do not execute the other trainer's dialogue.

Native reward receipts and story access changes are included in the host's next manual save, using the original save routine at a safe field boundary. Notices use native script/text-printer commands, validated and wrapped ROM text, and queue behind existing encounters. See [CAMPAIGN.md](CAMPAIGN.md) for coverage and save semantics.

FRMP 14 keeps the camp packet layout but updates camp validity for compact clearings. Earlier clients reject these valid camps under their all-sides walkway rule, so mixed-version rooms are rejected at handshake. All players must launch 0.18.0 together.


## Field battle protocol (0.19.0)
FRMP 15 snapshots carry each pair's activity and readiness stage (0 waiting, 1 field frozen, 2 native battle starting, 3 returned). Only authenticated participants can advance their own stage, one step at a time. Repeated acknowledgements cannot reset a pair. InvitationCancel identifies the recipient's current nonce; unrelated players cannot cancel another pair. Native disconnect/error recovery restores the private party and returns to the same field map, with unfinished wagers handled by the existing durable refund ledger.

Camp is now in the native Start menu between Bag and the player name. G sets up/packs away camp while the game has input focus. The original art, ground rules and roaming radius are unchanged. Eight-entry menus use 13-pixel row spacing so Exit fits above native help text without scaling the font.

## Membership and departures (0.25.0 / FRMP 19)

Only the host creates join/leave events from accepted connections. They appear once in the chat transcript and briefly in the game frame header, without becoming player speech bubbles. T invites a trade; chat is focused by clicking its input.

Fly, teleport, Escape Rope and blackout hooks publish an authenticated departure anchored to the trainer's last field or battle position. Same-map spectators see the trainer turn repeatedly, then rise offscreen over 1.4 seconds. Ordinary doorway travel keeps its normal presentation. The game's terrain and UI compositor still controls clipping.

## Shiny rate

Options → World exposes a host-owned chance of 1 in N, for N from 1 to 8,192.
The default is the cartridge's original 1-in-8,192 behavior, with no personality
or RNG override. A custom setting affects newly generated wild Pokémon,
starters and random gifts on every client. It does not recolor existing teams,
stored Pokémon, trades, released Pokémon, already-created roamers or eggs, and
it preserves the cartridge's restrictions on preset/no-shiny NPC Pokémon.

The host can Apply a new rate or restore the default during gameplay. Guests
see the current setting but cannot edit it. The value persists atomically in
the world folder's `shiny-rate.cfg`, survives restarting or reconnecting, and
does not carry into another world. FRMP 20 sends it before a joining trainer's
checkpoint; old clients must update.

Native hooks adjust only a new creator's personality variable before its
encrypted data is initialized. OT identity is unchanged, and the original
shiny predicate remains intact, so caught Pokémon stay shiny after a save,
trade or a later rate change. Nature, gender and ability parity are retained.
For a new Unown, its letter takes priority when that letter/nature/OT combination
cannot be shiny under the native rules; only then is a compatible nature used.
The calculation follows the [original generation rules](https://github.com/pret/pokefirered/blob/master/src/pokemon.c)
and [shiny constants](https://github.com/pret/pokefirered/blob/master/include/constants/pokemon.h).

## Followers and shiny presentation (0.26.7 / FRMP 23)

Visible wild Pokemon receive a shiny identity at spawn. The map authority sends
that identity to all clients, and the native encounter keeps it even if the host
changes the rate before contact. Existing spawns and owned Pokemon are not
rerolled. Followers, camp party members, released Pokemon and spectator battle
poses select the supplied normal or shiny animated sheet for their individual.

A follower starts on an available neighboring tile without requiring a step.
Walking history is translated across connected routes and towns. Actual recalls,
send-outs, lead changes and faint replacements use the supplied ComeInOut cells.
Face an adjacent follower and press X for a cry, native text-box response and an
animated heart, happy or music emote. Emotes, send-out and recall effects display at half source size with
nearest-neighbor sampling. PNGs are unchanged. Effects use normal world object
composition; native UI, foreground terrain and transition fades retain priority.

All players must update because FRMP 23 includes shiny state and all twelve follower reaction kinds.
Save files remain compatible. Saving is manual; the host's Save includes guests
once they reach a safe field boundary. Exit does not create another checkpoint.
