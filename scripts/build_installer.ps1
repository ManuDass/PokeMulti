param([string]$Compiler='')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$version=[regex]::Match([IO.File]::ReadAllText((Join-Path $root 'CMakeLists.txt')),'project\(PokeMulti VERSION ([0-9.]+)').Groups[1].Value
$package=Join-Path $root "dist\PokeMulti-$version"
if (!(Test-Path -LiteralPath (Join-Path $package 'package-manifest.json'))) { throw 'Run scripts/package.ps1 first.' }
$manifest=[IO.File]::ReadAllText((Join-Path $package 'package-manifest.json'),[Text.Encoding]::UTF8) | ConvertFrom-Json
foreach ($entry in $manifest) {
    $path=[IO.Path]::GetFullPath((Join-Path $package $entry.path))
    if (!$path.StartsWith($package+'\',[StringComparison]::OrdinalIgnoreCase) -or (Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() -ne $entry.sha256) { throw 'Package integrity check failed.' }
}
if (@(Get-ChildItem -LiteralPath $package -Recurse -File).Count -ne @($manifest).Count+1) { throw 'Unexpected files in installer input.' }
if (!$Compiler) {
    $candidates=@($env:POKEMULTI_ISCC,(Join-Path $env:LOCALAPPDATA 'Programs\PokeMulti-BuildTools\InnoSetup7\ISCC.exe'),(Join-Path $env:ProgramFiles 'Inno Setup 7\ISCC.exe'),(Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'))
    $Compiler=$candidates | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -First 1
}
if (!$Compiler) { throw 'Install Inno Setup 7 or pass -Compiler with the ISCC.exe path.' }
& $Compiler ("/DAppVersion=$version") ("/DPackageDir=$package") ("/DOutputDir="+(Join-Path $root 'dist')) (Join-Path $root 'installer\PokeMulti.iss')
if ($LASTEXITCODE -ne 0) { throw 'Windows installer build failed.' }
Write-Output (Join-Path $root 'dist\PokeMulti-Setup.exe')
