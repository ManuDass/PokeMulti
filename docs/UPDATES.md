# Launcher usernames and updates

Choose **Edit** in the launcher header to change your online name. This
changes the label used in rooms, chat, overhead names and friends lists. Your
stable trainer identity, world ownership, party, money and the game's narrative
trainer name are unchanged. Friends see the new name the next time you share a
room; their saved friend entry is refreshed without adding a second friend.

Choose **Update program**, then **Check for updates**. When a newer stable release
is available, choose **Download & restart**. Close other sessions using that same
installation before updating. The launcher stays open while the package downloads
and is verified, then restarts after installation. Failed downloads leave the
installed program alone; failed replacements roll back files already replaced.
Updates require a writable installation or extracted release folder. The setup
wizard's default per-user install location supports updates without elevation.

The updater reads the public
[GitHub Releases API](https://docs.github.com/en/rest/releases/releases#get-the-latest-release)
for [ManuDass/PokeMulti](https://github.com/ManuDass/PokeMulti/releases/latest).
It accepts the versioned Windows ZIP from that repository over HTTPS, checks
GitHub's SHA-256 release-asset digest, rejects unsafe/private-data paths, and
verifies every installed file against the package manifest. Stable versions are
compared numerically; prereleases and downgrades are not installed.

ROMs, profiles, identities, worlds, saves and extra user files are not replaced.
Work files and the previous program files are kept under the selected profile's
`updates/job-...` directory. `update-last.txt` records the installation result.
This is a file-replacement rollback, not a snapshot of live gameplay; updates are
installed only after the launcher exits and other sessions have closed.

Users on 0.25.0 or earlier need to install the latest version once to obtain the updater.
Subsequent releases can be installed from inside the launcher. No GitHub login
is required to download public updates. Offline/network-limit messages leave
the current program usable.

## Publishing an update

Keep `project(PokeMulti VERSION ...)`, README, START_HERE, and the root release
launcher on the intended version. Build with `build_release.bat`, run the tests,
and package with `powershell -ExecutionPolicy Bypass -File scripts/package.ps1`.
The setup wizard is built by `scripts/build_installer.ps1` using Inno Setup 7.
Edit `docs/RELEASE_NOTES.md` for the version being released. Push the source and a
matching `vX.Y.Z` tag, then publish `PokeMulti-X.Y.Z.zip` as a stable GitHub release
asset. The tag and ZIP filename must match the version. The launcher uses
published release assets, not GitHub's source-code ZIPs or individual commits.
The included release workflow builds and publishes the package when a version
tag is pushed. It publishes `PokeMulti-Setup.exe`, the versioned ZIP and checksums,
and refuses to overwrite an existing release. Never attach a ROM, profile folder
or generated native cache.
