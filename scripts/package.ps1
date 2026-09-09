param([string]$Configuration='', [string]$Version='', [string]$OutputDirectory='')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$sourceVersion = [regex]::Match([IO.File]::ReadAllText((Join-Path $projectRoot "CMakeLists.txt")), 'project\(PokeMulti VERSION ([0-9.]+)').Groups[1].Value
if (!$Version) { $Version = $sourceVersion }
if (!$Configuration) {
    $Configuration = if (Test-Path -LiteralPath (Join-Path $projectRoot "build\Release-$Version\CMakeCache.txt")) { "Release-$Version" } else { 'Release' }
}
$vswhere = Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsPath) { throw 'Visual Studio C++ CMake tools were not found.' }
$cmake = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$buildRoot = Join-Path $projectRoot ("build\"+$Configuration)
$distributionRoot = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $projectRoot 'dist' }
$packageRoot = Join-Path $distributionRoot ("PokeMulti-"+$Version)
if (!(Test-Path -LiteralPath (Join-Path $buildRoot 'pokemulti_game.exe'))) { throw 'Build Release first.' }
$buildVersion = [regex]::Match([IO.File]::ReadAllText((Join-Path $buildRoot 'CMakeCache.txt')), 'CMAKE_PROJECT_VERSION:STATIC=([^\r\n]+)').Groups[1].Value
if ($buildVersion -ne $Version) { throw "Build version $buildVersion does not match package version $Version. Rebuild the requested release first." }
& $cmake --install $buildRoot --prefix $packageRoot
if ($LASTEXITCODE -ne 0) { throw 'CMake install failed.' }
$resolvedPackage = [IO.Path]::GetFullPath($packageRoot)
$installedFiles = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($installedPath in [IO.File]::ReadAllLines((Join-Path $buildRoot 'install_manifest.txt'))) {
    [void]$installedFiles.Add([IO.Path]::GetFullPath($installedPath))
}
$manifest = @()
foreach ($file in Get-ChildItem -LiteralPath $resolvedPackage -File -Recurse) {
    $relative = $file.FullName.Substring($resolvedPackage.Length + 1).Replace('\','/')
    if ($relative -eq 'package-manifest.json') { continue }
    if (!$installedFiles.Contains($file.FullName)) { throw "Unexpected file outside the install allowlist: $relative" }
    if ($relative -match '(?i)(^|/)(cache|native-cache|roms|userdata|generated|worlds|guest-cache|backups|migration-backups|Following Pokemon EX|Pokemon Essentials[^/]*)(/|$)' -or
        $relative -match '(?i)\.(gba|gbc|nds|rom|sav|state|sa1|ss0|pmsv|bak|lock|key)(\.|$)' -or
        $relative -match '(?i)(^|/)(profile|identity|friends|wager-wallet|wagers-host|released-world)\.cfg$' -or
        $relative -match 'fr_game_harness|fr_rom_disasm|fr_tests|launch_player2|run_player2') { throw "Private/test data in install: $relative" }
    if ($file.Length -eq 16777216) {
        $stream = [IO.File]::OpenRead($file.FullName)
        try {
            $header = New-Object byte[] 192
            [void]$stream.Read($header,0,$header.Length)
            if ([Text.Encoding]::ASCII.GetString($header,172,4) -eq 'BPRE') { throw "ROM content in install: $relative" }
        } finally { $stream.Dispose() }
    }
    $manifest += [ordered]@{path=$relative; bytes=$file.Length; sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
}
$manifestPath = Join-Path $resolvedPackage 'package-manifest.json'
[IO.File]::WriteAllText($manifestPath,($manifest | ConvertTo-Json -Depth 4),[Text.UTF8Encoding]::new($false))
$archive = Join-Path $distributionRoot ("PokeMulti-"+$Version+".zip")
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archiveStream=[IO.File]::Open($archive,[IO.FileMode]::Create,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
$zip=[IO.Compression.ZipArchive]::new($archiveStream,[IO.Compression.ZipArchiveMode]::Create,$false)
try {
    foreach ($file in Get-ChildItem -LiteralPath $resolvedPackage -File -Recurse) {
        $relative=$file.FullName.Substring($resolvedPackage.Length+1).Replace('\','/')
        [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$file.FullName,("PokeMulti-$Version/"+$relative),[IO.Compression.CompressionLevel]::Optimal)
    }
} finally { $zip.Dispose();$archiveStream.Dispose() }
Write-Output "Package: $resolvedPackage"
Write-Output "Archive: $archive"
Write-Output "Audited $($manifest.Count) files; no ROMs, private saves, native caches or test executables."
