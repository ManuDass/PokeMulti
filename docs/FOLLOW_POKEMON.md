# Party followers

Followers are implemented in src/game/world.cpp. The first conscious, non-egg party member follows one walking tile behind the local trainer. The party reader decrypts FireRed's records, verifies their checksum and respects the game's substructure permutations. Reordering, fainting and party changes are reflected automatically.

Stock FireRed party icons provide the species artwork: two 32x32 animation frames and their palette are read directly from the selected local ROM. These are animated party icons, not complete directional walking sheets. No Essentials sprites are copied.

A bounded history of actual pixel positions supplies the position 16 pixels behind the trainer along the walked path. Followers do not occupy original object-event slots or block movement. They hide during invalid overworld states, battles, fades, warps, cycling and surfing. Warps clear the history; a follower reappears after walking at least one tile. Remote followers use the peer's replicated species and follower position, sampled with the trainer's movement timeline.

F2 > Options > Party follower toggles the feature and saves the preference in world.cfg. Remote graphics always come from the local ROM.

Since 0.3.2, followers share the native composition order with trainers, NPCs and map layers. Ground position controls depth, with stable room-slot tie ordering; jumps do not change the depth anchor. Captured foreground priorities and window masks cover sprites correctly under roofs and game UI.

Current limits: followers do not have independent pathfinding, and unusual field-effect combinations need further coverage. Abrupt map connections are handled as map changes, without cross-map rendering. In 0.3.1, corrected inline palette addressing makes the icon visible, and transparent-padding detection places its feet at ground level. Paired on/off framebuffer checks and image inspection verified a local Bulbasaur after walking through a Pokemon Center and on Route 1. Earlier follower-state checks did not establish visible rendering. See VALIDATION.md for the evidence and its scope.

Essentials 21.1 and Following Pokemon EX 2.5.1 informed hide-on-transfer and step-history behavior only. Their Ruby engine state, scripts and graphics are not included.
