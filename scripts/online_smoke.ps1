param([Parameter(Mandatory=$true)][string]$Rom,[ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
python (Join-Path $PSScriptRoot 'online_ui_smoke.py') --rom $Rom --configuration $Configuration
if ($LASTEXITCODE -ne 0) { throw 'Integrated online UI smoke check failed.' }
