# Visible wild Pokemon

Visible land encounters are implemented using FireRed's local encounter table. Up to three entities appear on nearby valid grass tiles within the current viewport, with the map's encounter species weights and level ranges. They avoid occupied NPC/player tiles and incompatible collision/elevation. They periodically wander within view, expire, and despawn when distant, beyond the viewport margin or after a map transition.

Artwork uses animated party icons read from the player's ROM. Version 0.3.1 corrects their palette lookup and grounds their visible feet on the tile; earlier builds created encounter entities but failed to draw the icons. Each player owns their own encounter entities and battle state; wild entities are not synchronized or captured on another player's behalf.

On supported grass tiles, the hook replaces the ordinary random encounter trigger with contact against a visible entity. Contact after its spawn grace period calls FireRed's own CreateScriptedWildMon and StartWildBattle, preserving the real battle UI and game rules. Contact first calls FireRed's own Repel eligibility check; a repelled weak entity despawns without starting a battle. Other encounter modes retain the original game behavior.

F2 > Options > Visible wild toggles the feature. Disabling it clears visible entities and restores the original random encounter path.

Validated locally: controller movement into a spawned level 2 Pidgey on Route 1 produced FireRed's real “Wild PIDGEY appeared!” battle, with matching species and level. The setup used a labeled development fixture; movement and contact used ordinary controller input.

Version 0.3.1 additionally verifies the actual rendered pixels with independent follower/wild on/off checks and inspected Route 1 captures containing Pidgey and Rattata. Entity telemetry alone is not treated as visual proof.

Current scope is walking on grass with a valid conscious party. Fishing, water, caves and scripted encounters continue using FireRed's original system. Version 0.3.2 composes wild icons with native map priorities, sprite depth and window masks. Unusual field-effect combinations and additional encounter modifiers need broader compatibility work before this feature should be considered campaign-complete.
