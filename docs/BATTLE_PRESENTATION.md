# Battle presentation (0.22.1)

## The battling player

The battling trainer sees FireRed's original 2D battle background, full-size front/back Pokemon sprites, health bars, effects and menus. The original entry/exit animations and palette fades handle transitions. There is no alternate environment renderer or view toggle.

## Other players in the overworld

Battle presence captures the trainer's rendered pixel position, the visible partner's current trail position and the contacted wild Pokemon's current rendered position. No preset arrangement replaces these positions. The opponent stays at its encounter position. The partner and trainer turn toward the fight; if the trainer is in the way, a bounded walkable-ground search plans a short route around the actors. Walking uses one cardinal pixel per game frame with the correct directional animation, followed by the existing small combat hops. A cramped encounter keeps its current positions instead of teleporting onto blocked ground.

The same poses and animation states reach other trainers in the 2D overworld. The initial remote transition starts from the spectator's most recently displayed trainer/follower pose. Actual native battlers still control species, fainting and replacements. After battle, the trainer/partner reverse their cosmetic walking path before normal field control resumes; the native player location and save data are not moved by staging. A map-changing return, such as a blackout, clears the old scene instead of walking across maps.

Battle layouts and walking frames use FRMP 17 in the current release. Both clients should launch 0.22.1. This release is locally validated with native US 1.0 singles; broader campaign/battle variants, US 1.1 gameplay and WAN conditions remain compatibility work.
