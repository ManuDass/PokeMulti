# Validation

The Windows release is built and tested both locally and from a clean GitHub
checkout. CTest runs 54 groups covering ROM/ZIP validation, profile preservation,
updater integrity and rollback, room protocols, chat, story ownership and retries,
world checkpoints, rewards, wagers, camps, sprites and the underlying runtime.

Packaged launcher checks cover profile creation and rename, world selection,
host settings, native game startup, clean shutdown and return to the launcher.
Two packaged sessions also passed host/join, rejected-key recovery and host-loss
return checks. Native acceptance checks passed private checkpoints, cold reconnect,
trade invitations and abrupt host-loss handling. A real 0.26.0-to-0.26.2 update
verified every installed file and restarted with the same disposable profile.
The Windows setup wizard was installed into a separate folder; installed files
matched the package manifest, the game launched successfully, and uninstalling
preserved an extra user file. These checks used disposable profiles.

Release packages contain only explicit installation targets. Audits reject ROMs,
private saves, identities, native game-code caches and test executables. Included
artwork is copied unchanged. The 0.26.1 package and installer passed a Microsoft
Defender scan. Reorganized source files retain their original content hashes.

Previous local gameplay checks established FireRed US 1.0 boot/save/reload,
visible encounters, multiplayer rendering, real Cable Club battles and trades,
host-owned checkpoints and supported shared-story retries.

These results do not establish complete campaign coverage. US 1.1 gameplay,
cross-PC/WAN cable timing, unusual field effects and full playthroughs need more
testing. Native compilation retains instruction fallback. Crashes can lose work
since the last completed safe checkpoint.

Mac builds are undergoing separate native compilation and test checks. They are
not covered by the Windows results. No ROM or personal save is sent to CI.

Build instructions are in the repository README. Developer acceptance checks
are grouped under `tests/integration`; historical planning and session logs are
maintained locally, outside the public source tree.
