# Worlds and host saves — 0.25.0

The launcher lists worlds for the selected ROM. Select **Play world** for solo play, **Host world** to open it to friends, or **New world** for a separate adventure. Host setup includes the room key, TCP port, reward checkboxes and a **2–32 player limit**, including the host. Money and owned teams always remain individual. Solo play continues the same world's campaign without opening a listening socket.

Friends use **Join a friend** and **Paste invite**, or enter the host's IPv4 address, port and key. Hosting uses the existing direct, port-forwarded connection. Every client needs 0.26.5 and the same verified ROM. There is no hosted directory to configure.

## Saving and returning

The host owns the world's campaign, shared released Pokémon and separate trainer checkpoints. Guests receive their world-specific trainer before the game boots. First-time guests use the original New Game flow and choose their own starter.

Saving is entirely manual. There are no periodic, campaign, wager or release-triggered game saves. Saving waits for a safe, stationary field boundary outside dialogue, battles, active encounters, link trades and pending transactions. It uses the original native save routine. The host's **Enter → Save** requests checkpoints from all connected trainers; guests do not need to open Save themselves.

**Exit world** and closing the window do not start a save. They allow an already requested save up to five seconds to finish. Save from the game menu before leaving. If a trainer is busy, their most recent completed checkpoint remains available. A crash or lost connection therefore restores the latest checkpoint already received by the host, rather than an arbitrary in-battle frame. Guests return to the launcher when the host disconnects or shuts down, and cannot continue that world alone. Rejoining always downloads the host's copy.

## Files and migration

The default launcher data folder remains `%LOCALAPPDATA%\FireRedRecomp`. Custom `--data-dir` folders work too.

```
worlds/<world-id>/
  world.cfg
  campaigns/
  wagers-host.cfg
  released-world.cfg
  players/<trainer-id>/
    identity.sha256
    checkpoint.pmsv
    checkpoint.previous.pmsv
    runtime/trainer.sav
```

Each checkpoint bundles the 128 KiB cartridge save with wallet, pending-release and released-battle receipts. Hash validation, bounded transfers, atomic replacement and a previous valid checkpoint protect against incomplete writes. Incoming saves are restricted to the authenticated trainer's folder. Guest runtime caches are disposable and do not appear as owned worlds.

On first use, an existing legacy save for the selected ROM is copied into **My first world**, together with its relevant shared records. The original files remain intact. Existing friends who previously kept their saves on another PC start a new trainer slot when first joining a managed world; this release does not automatically import arbitrary guest saves.

Back up the complete world folder and the launcher's `identity.cfg` and `identity.key`. Those identity files reconnect a trainer to their existing record; changing only the displayed username does not create or claim another trainer. The room key remains the shared join key. Direct room traffic is not encrypted; identity binding prevents accidentally loading another trainer but is not a public-service account system.

## Keyboard controls

| Key | Action |
| --- | --- |
| WASD / arrows | Move |
| X | Confirm / interact; pet a facing follower or challenge a facing trainer |
| Z | Cancel; run with Running Shoes |
| Enter | Game menu; Send while editing chat |
| Right Shift | Select |
| C / V | L / R |
| G | Set up / pack camp |
| T | Invite the facing adjacent trainer to trade |
| Esc | Return focus from chat/sidebar to the game |
| F2 | Toggle sidebar |
| F11 | Fullscreen |
| F12 | Screenshot |

Click the chat input to type. Trade invitations use the native text box; the accepted trade itself continues through the existing upstairs Cable Club flow. Join/leave notices appear in chat and briefly above the game. Normal chat retains the supplied overhead bubble tiles.
