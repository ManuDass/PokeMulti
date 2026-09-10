# PokéMulti

Play Pokémon FireRed or LeafGreen together, with shared worlds, nearby trainers, friends, chat,
trades, agreed battle wagers, followers and camps.

**[Download PokéMulti for Windows](https://github.com/ManuDass/PokeMulti/releases/latest/download/PokeMulti-Setup.exe)**

Windows 10/11, 64-bit. Open the download, choose an install folder in the wizard,
then launch PokéMulti from your desktop or Start menu. No development tools or
extra asset downloads are required. Supply your own unmodified English FireRed or LeafGreen
US 1.0 or 1.1 ROM, either `.gba` or a ZIP containing exactly one `.gba`.
**No ROM is included.**

The [Releases page](https://github.com/ManuDass/PokeMulti/releases/latest) also
provides a portable Windows ZIP. Extract the whole folder and open `pokemulti.exe`.
GitHub's **Code → Download ZIP** contains source code, not the installed game.

**Mac preview:** [Apple Silicon (.dmg)](https://github.com/ManuDass/PokeMulti/releases/download/v0.27.2-mac-preview/PokeMulti-0.27.2-macOS-arm64.dmg) · [Intel (.dmg)](https://github.com/ManuDass/PokeMulti/releases/download/v0.27.2-mac-preview/PokeMulti-0.27.2-macOS-x86_64.dmg)

Open the DMG and drag PokéMulti to Applications. Mac builds are previews: gameplay
on physical Macs remains unverified, and updates are manual. Read the
[Mac preview notes](https://github.com/ManuDass/PokeMulti/releases/tag/v0.27.2-mac-preview)
for first-launch instructions and current limitations.

## Get started

1. Select your own supported ROM and enter an online username.
2. Create or select a world. **Play world** continues alone; **Host world** lets
   friends join. The host chooses a room key, reward rules and 2–32 player capacity.
3. Friends choose **Join a friend** and paste the host's invitation. Use matching
   game, ROM revision and application versions on every client. FireRed and
   LeafGreen use separate worlds; a mismatched ROM is refused before loading saves.
4. Play through your game's New Game/Continue menus as usual.

**Switch games:** Settings & ROM → Change ROM → Update ROM keeps your username
and friends. The world picker shows only worlds for that exact ROM.
LeafGreen uses green UI accents; FireRed uses red.

**Shiny rate:** while playing, open Options → World. The host can change the
world's chance from the game default (1 in 8,192) down to 1 in 1. The setting is
saved per world and applies to new encounters and random gifts for every player.


For internet play, the host forwards the selected **TCP port** (default 38475)
to their PC and allows the app through Windows Firewall. **Room → Invite friends**
provides internet, LAN and same-PC invitations. A public address alone does not
configure a router; carrier-grade NAT may prevent direct hosting. There is no
public directory or relay service.

## Worlds and multiplayer

The host world stores a separate checkpoint for each trainer. Supported story
events advance the campaign for the room, while each trainer keeps their team,
wallet and personal progression. The host controls eligible reward sharing;
money always stays individual. See [campaign rules](docs/CAMPAIGN.md).

The host's in-game **Save** requests checkpoints from connected trainers.
Saving is manual; save before leaving. Guests return to the
launcher if the host disconnects and resume from their latest completed
checkpoint when they reconnect. See [world saves](docs/WORLDS.md).

Face an adjacent trainer and press **X** to challenge them. Both players agree to
any wager in the game's original dialogue and menus. Press **T** to invite them
to trade; accepted trades use the upstairs Cable Club flow. Other trainers can
keep exploring during your wild and NPC battles.

Chat appears in the game frame and in tiled bubbles above trainers. Friends are
identified by stable local identities, with presence in the current room.
There is no global account or friend-discovery server.

## Controls

| Control | Action |
| --- | --- |
| WASD / arrows | Move |
| X | Confirm, interact, challenge a nearby trainer |
| Z | Cancel; run with Running Shoes |
| Enter | Game menu; send while chat is focused |
| Right Shift | Select |
| C / V | L / R |
| G | Set up / pack camp |
| T | Invite the facing adjacent trainer to trade |
| F2 | Show / hide sidebar |
| F11 | Fullscreen |
| F12 | Screenshot |

Click the chat field to type and the game to resume controls. Audio volume,
followers and visible wild Pokémon are in **Options**. Camp needs clear ordinary
outdoor grass; walking outside its boundary automatically packs it up. A
[Poké Ball Plus controller](docs/POKE_BALL_PLUS.md) is supported on Windows.

## Usernames and updates

**Edit** beside your username in the launcher changes your online name without changing
FireRed's narrative trainer name, identity or saved progress.

**Update program → Check for updates → Download & restart** installs future
stable GitHub releases. The updater verifies the download and program files,
preserves personal data, and rolls back replaced files if installation fails.
Close other sessions using that installation before updating.
Users on 0.25.0 or earlier need to install 0.26.2 once to obtain the updater.
See [updates](docs/UPDATES.md).

Worlds and identities live outside the installation, in
`%LOCALAPPDATA%\FireRedRecomp`. Back up the whole data folder, including
`identity.cfg` and `identity.key`. Installing, updating or uninstalling the
program preserves that folder. `--data-dir` selects a separate profile.

## Build from source

Install Visual Studio 2022 or newer with **Desktop development with C++**, a
Windows SDK and **C++ CMake tools for Windows**. Dependencies and required runtime
artwork are included. Then run:

```powershell
.\build_release.bat
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\package.ps1
.\run_release.bat
```

`build_release.bat` compiles and runs CTest. To build the setup wizard, install
[Inno Setup 7](https://jrsoftware.org/isinfo.php), then run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build_installer.ps1
```

Release tags run the same build, tests and packaging in GitHub Actions. Only
explicit installation files enter releases; ROMs, saves, private identities and
generated native game-code caches are excluded.

## Status and credits

**0.26.7 · FRMP 22 · experimental.** FireRed US 1.0 and LeafGreen US 1.1
boot, play, save and reload.
Local two-process checks cover room play, native battles and durable trades.
Full campaign coverage, FireRed US 1.1/LeafGreen US 1.0 gameplay and
cross-PC/WAN cable timing need more testing. [Validation notes](docs/VALIDATION.md) describe the evidence and limits.

Original 2D rendering is used throughout. The experimental voxel/paper mode has
been removed. Supplied fonts, chat tiles, follower sheets, camp frames and UI art
are preserved in the package.

The runtime dependency is **noncommercial**. Dependency licenses, attribution,
modified MPL source and TinyCC's corresponding source accompany releases.
See [third-party notices](docs/THIRD_PARTY.md) and [dependencies](dependencies.json).

