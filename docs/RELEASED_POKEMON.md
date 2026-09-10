# Released Pokemon

Use the original PC **Release** command. Canceling the confirmation, an Egg, or a release refused by the original game does not create a wild Pokemon. Log off the PC when finished.

A successful release is saved before it enters the shared world. The Pokemon appears beside the trainer, hops and cries, then walks through the building's exit. Outside, it chooses a separate waiting spot around the entrance. The central doorway stays clear; several releases form a scattered crowd instead of stacking.

When the releasing trainer reaches the outdoors, the waiting Pokemon hops, cries, and runs to the nearest reachable, unoccupied tall-grass tile. The route follows walkable terrain, doors and connected maps, so grass can be on another route. A Pokemon waits if no valid route is available. Changing floors inside the building does not count as leaving.

The released individual stays in the grass across map changes. Contact starts a normal native wild battle. Any trainer can encounter and catch it. A host-authorized claim prevents two trainers from taking the same Pokemon; escaping or defeating it returns it after a short cooldown, while capture removes it. The individual retains its personality, level, experience, moves, IVs/EVs, held item and other Pokemon data. It recovers HP and PP for a wild encounter.

Releases and recaptures update the running session immediately. The host must manually save the world to retain them and the corresponding trainer data. Personal money, party and items remain personal. The shared wild population is stored in the hosting trainer's profile (`released-world.cfg`) and resumes when that profile hosts again. Solo releases have a separate local population (`released-solo.cfg`). A disconnected encounter remains reserved until its trainer reconnects and the local battle/save receipt is resolved.

The shared state drives the native 2D presentation. Both room clients should launch **0.22.1 / FRMP 17**. Artwork and cries are read from the player's own local ROM/assets; the network carries only individual Pokemon data and semantic poses.
