# Shared campaigns and personal trainers (0.25.0)

A campaign owns world changes. Each trainer owns their career and possessions. Both clients must run 0.25.0, using FRMP 19.

| Progress | Behavior |
| --- | --- |
| Starter and opening lab battle | Each trainer completes their own onboarding. |
| Oak parcel | One collection and delivery; all ready trainers get Pokedex and route access. |
| Supported rescues, Rocket operations and scene changes | The acting trainer runs the original encounter; the campaign records its completion and projects safe world changes. |
| Ordinary trainers | A campaign defeat suppresses another automatic ambush. Other trainers can talk to that NPC for their own battle. |
| Gym Leaders | A campaign clearance opens supported travel access. Each trainer still earns their own Badge, winnings and victory. |
| League | Native League sequence and championship stay personal. |
| Essential tools | Supported HMs, Ticket, Flute, Tea, Scope and quest keys reach everyone regardless of optional reward policy. A trainer still needs a Pokemon that knows the appropriate field move. |
| Optional TMs/items | Host policy enables one copy per trainer or claimant-only collection. Full pockets retry without marking a receipt. |
| Pokemon | Teams are always personal. Host policy controls whether supported fixed encounters/gifts are first-claim or individually available. |
| Money and XP | Earned only by the trainer who battles. Wagers retain their separate agreed transaction flow. |

## Story barriers

Conditional coordinate events reserve their scene with the host before a trainer walks onto the trigger. A conversation with a road-blocking NPC, or an escort that moves it away, does not disable that barrier for other trainers. Blocked approaches use the original in-game message frame. Movement elsewhere remains available after dismissing the notice.

Once the scene is available, its original ROM script handles dialogue, movement and requirements. Shared campaign state controls supported unlocks: parcel delivery opens Viridian's road and campaign Brock clearance opens Pewter's eastern route. Merely talking to the old man or following the guide does not supply those completions. A native script arriving without a walking collision check waits for ownership instead of silently losing its coordinate event.

The guard reads conditional trigger tiles and elevations from the loaded map rather than a hand-written list of barriers. Weather events and unconditional immediate scripts retain native behavior. Temporary/personal conditions use their local value with exclusive scene ownership; this does not turn unknown quest variables into shared campaign progress. Offline play retains native collision/scripts. Both clients need FRMP 19; older clients that could discard blocked events are incompatible.

## Failed battles and retries

A native trainer-battle blackout releases the acting trainer's scene ownership. Another trainer can then trigger that encounter while the first trainer heals at their own last Pokemon Center. A loss does not complete the room objective or open its story barrier. Original money loss, healing and party state remain personal.

Before battle, the room checkpoints the participating NPCs at the end of their approach. Following a failure, these NPCs stay at that encounter spot. A retry turns them toward the new trainer instead of repeating relative approach steps from their advanced position. Scripted dialogue, native battles and the successful departure still run in the ROM. Scriptless rivals such as Route 22 Gary can also restart their active coordinate encounter when spoken to from an adjacent tile.

The room retains these waiting poses for its current lifetime, including participant disconnections. A new room retains saved campaign progress, but does not resume an unfinished native script or its temporary actor poses. An active battle keeps its ownership until loss, completion, disconnect or departure to another field map. A lost attempt restores its uncommitted shared scene changes without copying money, Pokemon or inventory.

## Continuing a campaign

Playing or hosting a selected world resumes `campaigns/active.cfg` inside that world's folder. The selected ID points to a validated, atomically written `.cfg` containing canonical story proofs and named objective completions. The first room seeds its world from the host's native save; a resumed campaign restores its saved world before any new native baseline. A newcomer joins that world after choosing their own starter and completing the initial lab battle.

Choose **New world** in the launcher for an independent adventure. Each world owns its campaign and separate per-trainer checkpoints. Guests receive their trainer from that world before the game boots; they cannot continue it offline when its host leaves. See [world storage and recovery](WORLDS.md).

Campaign changes and personal rewards update during play. They persist when the host explicitly saves the world, together with each trainer's party, money, inventory and receipt flags. Guest saves wait through battles, scripts and cable transactions. An existing native save gets a `.before-campaign.bak` copy before its first campaign application. Keep the whole world folder together. There are no periodic or campaign-triggered game saves, and closing a session does not start a save.

## Presentation

Campaign updates, reward notices and camp rejection messages run through the original native text printer and message frame. Text wraps to the ROM font's measured width and uses original two-line pages. A/B advance and dismiss. Existing conversations, battles and cutscenes finish first. Only the recipient's game controls pause; other trainers continue playing. The room chat dock and the supplied overhead chat-bubble artwork retain their existing purpose.

**Room > Campaign journal** displays the most recent eight named milestones, with the completing trainer. The complete named journal is retained in the campaign file. Notifications already present when joining are not replayed as a backlog.

## Supported adapters and limits

`src/game/story.hpp` explicitly selects supported world flags and variables. `src/game/campaign.hpp` identifies personal native state and 27 journal milestones: parcel stages, Bill, Fuji, Silph, Rocket keys, Tea, the Cerulean rival/Rocket, Lostelle, six HMs, Gold Teeth and eight Gyms. Unknown script state stays local. This is a curated FireRed adapter, not automatic interpretation of every quest in an arbitrary ROM.

`src/game/world_campaign.inc` maps campaign clearance to Pokedex and selected route/field-move checks while preserving personal Badge flags, obedience, payouts and League progression. Gift-script checks consult campaign receipts when optional sharing is disabled; native receipts remain individual so changing policy later can deliver a previously unavailable copy. Unconditional Gym victory gift jumps also check the receipt.

`src/game/rewards.hpp` lists supported deliveries. Ordinary pickups, shops, experience, money and Oak's five introductory Poke Balls are not automatically duplicated. Pokemon availability uses the existing explicit list in `unique.hpp`; no party data is copied. Fossils, roaming Pokemon, purchased Pokemon and arbitrary event distributions require their own adapters.

These hooks are intended for the supported US FireRed revisions. Native acceptance runs use US 1.0. Every story branch, postgame requirement, disconnect during an unfinished native cutscene, and joining with substantially different personal progress has not been played through. Existing badges granted by earlier versions are not revoked. There is no cloud server or anti-cheat authority.

Adapter references are the original [FireRed script definitions](https://github.com/pret/pokefirered/tree/master/data/maps), [script interpreter](https://github.com/pret/pokefirered/blob/master/src/scrcmd.c), and [field-move callbacks](https://github.com/pret/pokefirered/blob/master/src/party_menu.c). ROM addresses are revision-matched locally; reference files and ROM data are excluded from packages.
