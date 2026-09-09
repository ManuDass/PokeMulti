# Integration checks

These checks use disposable profiles under the ignored `cache` folder. They
require Windows and the developer test harness, except the packaged launcher and
runtime checks. Supply your own supported ROM where requested; no ROM or live
player save belongs in the repository.

From the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests\integration\smoke_test.ps1 -Packaged -PlayGame -Rom "C:\Games\FireRed.gba"
powershell -NoProfile -ExecutionPolicy Bypass -File tests\integration\packaged_runtime_smoke.ps1 -Rom "C:\Games\FireRed.gba"
```

Native multiplayer checks exercise dedicated harness fixtures and may need a
locally prepared test checkpoint. Run unit and room-protocol checks with CTest
first; those require no ROM. Historical one-off probes and export tooling are
kept only in the maintainer's local recovery archive.
