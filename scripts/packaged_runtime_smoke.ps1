param([Parameter(Mandatory=$true)][string]$Rom)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$sourceVersion = [regex]::Match([IO.File]::ReadAllText((Join-Path $projectRoot "CMakeLists.txt")), 'project\(PokeMulti VERSION ([0-9.]+)').Groups[1].Value
$packageRoot = Join-Path $projectRoot "dist\PokeMulti-$sourceVersion"
$romPath = (Resolve-Path -LiteralPath $Rom).Path
$checkRoot = Join-Path $projectRoot ('cache\packaged-runtime-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $checkRoot)
$runtime = Join-Path $packageRoot 'pokemulti_game.exe'
$save = Join-Path $checkRoot 'trainer.sav'
$png = Join-Path $checkRoot 'boot.png'
$stdout = Join-Path $checkRoot 'stdout.log'
$stderr = Join-Path $checkRoot 'stderr.log'
$packageFilesBefore = @(Get-ChildItem -LiteralPath $packageRoot -File -Recurse | ForEach-Object FullName | Sort-Object)
$oldBackend = $env:GBARECOMP_HEAL_BACKEND
$oldVideo = $env:SDL_VIDEODRIVER
$oldAudio = $env:SDL_AUDIODRIVER
try {
    $env:GBARECOMP_HEAL_BACKEND = 'auto-no-gcc'
    $env:SDL_VIDEODRIVER = 'dummy'
    $env:SDL_AUDIODRIVER = 'dummy'
    $arguments = '--rom "' + $romPath + '" --save "' + $save + '" --frames 600 --dump-png "' + $png + '" --quiet'
    $process = Start-Process -FilePath $runtime -ArgumentList $arguments -WorkingDirectory $packageRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $processHandle = $process.Handle
    if (!$process.WaitForExit(30000)) {
        if ($process.Path -eq $runtime) { $process.Kill() }
        throw 'Packaged runtime exceeded the 30-second boot check.'
    }
    if ($process.ExitCode -ne 0) { throw "Packaged runtime failed: $checkRoot" }
    $log = [IO.File]::ReadAllText($stdout) + [IO.File]::ReadAllText($stderr)
    if ($log -match 'compile FAILED' -or $log -notmatch 'native_calls=([1-9][0-9]*)') { throw "Bundled native compilation failed: $checkRoot" }
    if (!(Test-Path -LiteralPath $png)) { throw 'Boot image was not produced.' }
    $packageFilesAfter = @(Get-ChildItem -LiteralPath $packageRoot -File -Recurse | ForEach-Object FullName | Sort-Object)
    if (Compare-Object $packageFilesBefore $packageFilesAfter) { throw 'Runtime wrote private files into the package directory.' }
    Write-Output "Packaged runtime booted using its bundled compiler and headers: $checkRoot"
    Get-FileHash -LiteralPath $png -Algorithm SHA256
} finally {
    $env:GBARECOMP_HEAL_BACKEND = $oldBackend
    $env:SDL_VIDEODRIVER = $oldVideo
    $env:SDL_AUDIODRIVER = $oldAudio
}
