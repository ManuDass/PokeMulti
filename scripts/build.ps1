param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vswherePath = Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswherePath)) { throw 'Install Visual Studio 2022 or newer with Desktop development with C++ and CMake tools.' }
$vsPath = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsPath) { throw 'Visual Studio C++ tools were not found.' }
$cmakePath = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (!(Test-Path -LiteralPath $cmakePath)) { throw 'Add C++ CMake tools for Windows using Visual Studio Installer.' }
$sourceVersion = [regex]::Match([IO.File]::ReadAllText((Join-Path $projectRoot 'CMakeLists.txt')), 'project\(PokeMulti VERSION ([0-9.]+)').Groups[1].Value
$buildName = if ($Configuration -eq 'Release') { "Release-$sourceVersion" } else { $Configuration }
$buildRoot = Join-Path $projectRoot "build\$buildName"
$configureScript = Join-Path $projectRoot "build\configure_$Configuration.cmd"
New-Item -ItemType Directory -Force -Path (Join-Path $projectRoot 'build') | Out-Null
$lines = @(
    '@echo off',
    ('call "{0}\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul' -f $vsPath),
    'if not "%errorlevel%"=="0" exit /b %errorlevel%',
    ('"{0}" -S "{1}" -B "{2}" -G Ninja -DCMAKE_BUILD_TYPE={3} -DBUILD_TESTING=ON' -f $cmakePath,$projectRoot,$buildRoot,$Configuration),
    'if not "%errorlevel%"=="0" exit /b %errorlevel%',
    ('"{0}" --build "{1}" --parallel' -f $cmakePath,$buildRoot),
    'if not "%errorlevel%"=="0" exit /b %errorlevel%',
    ('"{0}\ctest.exe" --test-dir "{1}" --output-on-failure' -f (Split-Path $cmakePath),$buildRoot),
    'exit /b %errorlevel%'
)
[IO.File]::WriteAllLines($configureScript, $lines, [Text.Encoding]::Default)
& $env:ComSpec /d /c ('"{0}"' -f $configureScript)
exit $LASTEXITCODE
