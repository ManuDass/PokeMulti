param([Parameter(Mandatory=$true)][string]$Helper)
$ErrorActionPreference='Stop'
. $Helper
$testRoot=Join-Path ([IO.Path]::GetTempPath()) ('PokeMulti-updater-test-'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($testRoot) | Out-Null
function Assert([bool]$condition,[string]$message) { if (!$condition) { throw $message } }
function Reject([scriptblock]$operation,[string]$message) { $caught=$false;try { & $operation } catch { $caught=$true };Assert $caught $message }
function Fixture([string]$name,[string]$contents) {
    $folder=Join-Path $testRoot $name; [IO.Directory]::CreateDirectory((Join-Path $folder 'tools')) | Out-Null
    $records=@()
    foreach ($relative in @('pokemulti.exe','pokemulti_game.exe','tools/update.ps1')) {
        $path=Join-Path $folder $relative;[IO.File]::WriteAllText($path,$contents+$relative)
        $records+=@{path=$relative;bytes=(Get-Item -LiteralPath $path).Length;sha256=(Hash-File $path)}
    }
    [IO.File]::WriteAllText((Join-Path $folder 'package-manifest.json'),($records | ConvertTo-Json),$script:Utf8)
    return $folder
}
function Zip-Fixture([string]$folder,[string]$name,[string]$extra='') {
    $path=Join-Path $testRoot ($name+'.zip');$z=[IO.Compression.ZipFile]::Open($path,[IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in Get-ChildItem -LiteralPath $folder -Recurse -File) {
            $relative=$file.FullName.Substring($folder.Length+1).Replace('\','/')
            [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($z,$file.FullName,('PokeMulti-0.26.0/'+$relative))
        }
        if ($extra) { $entry=$z.CreateEntry($extra);$stream=$entry.Open();try {$stream.WriteByte(1)}finally{$stream.Dispose()} }
    } finally { $z.Dispose() }
    return $path
}
try {
    Assert ((Version-Number '0.26.0') -gt (Version-Number '0.9.9')) 'Version ordering is lexical.'
    foreach ($bad in @('v0.26.0','0.26','0.26.0-beta','01.2.3','../0.26.0')) { Reject { Version-Number $bad } 'Bad version accepted.' }
    foreach ($bad in @('../outside','dir/../../outside','/absolute','C:/escape','dir/file:stream','dir\file','a/CON.txt','a/space ','a/./file')) { Reject { Child-Path $testRoot $bad } "Unsafe path accepted: $bad" }
    foreach ($private in @('worlds/owner/checkpoint.pmsv','roms/game.gba','profile.cfg','identity.key','friends.cfg','native-cache/game.dll')) { Reject { Assert-ProgramPath $private } 'Private data path accepted.' }
    $new=Fixture 'new' 'new-content-';$zip=Zip-Fixture $new 'valid'
    $expanded=Join-Path $testRoot 'expanded';Expand-VerifiedPackage $zip $expanded '0.26.0' (Hash-File $zip)
    Assert ((Hash-File (Join-Path $expanded 'pokemulti.exe')) -eq (Hash-File (Join-Path $new 'pokemulti.exe'))) 'Valid package failed extraction.'
    Reject { Expand-VerifiedPackage $zip (Join-Path $testRoot 'bad-digest') '0.26.0' ('0'*64) } 'Incorrect package digest accepted.'
    foreach ($extra in @('PokeMulti-0.26.0/../../escape.txt','PokeMulti-0.26.0/POKEMULTI.EXE','PokeMulti-0.26.0/extra.txt','PokeMulti-0.26.0/worlds/data.pmsv')) {
        $id=[guid]::NewGuid().ToString('N');$bad=Zip-Fixture $new $id $extra
        Reject { Expand-VerifiedPackage $bad (Join-Path $testRoot $id) '0.26.0' (Hash-File $bad) } 'Unsafe or unlisted ZIP entry accepted.'
    }
    Assert (!(Test-Path -LiteralPath (Join-Path $testRoot 'escape.txt'))) 'ZIP traversal wrote outside staging.'
    $installed=Fixture 'installed' 'old-content-';[IO.Directory]::CreateDirectory((Join-Path $installed 'worlds')) | Out-Null
    [IO.File]::WriteAllText((Join-Path $installed 'worlds/checkpoint.pmsv'),'private-progress')
    [IO.File]::WriteAllText((Join-Path $installed 'personal.gba'),'private-ROM')
    [IO.File]::WriteAllText((Join-Path $installed 'identity.key'),'private-identity')
    Install-StagedPackage $expanded $installed (Join-Path $testRoot 'backup-success')
    Assert ((Hash-File (Join-Path $installed 'pokemulti.exe')) -eq (Hash-File (Join-Path $new 'pokemulti.exe'))) 'Update did not install.'
    Assert ([IO.File]::ReadAllText((Join-Path $installed 'worlds/checkpoint.pmsv')) -eq 'private-progress') 'Update modified a world save.'
    Assert ([IO.File]::ReadAllText((Join-Path $installed 'identity.key')) -eq 'private-identity') 'Update modified trainer identity.'
    Assert ([IO.File]::ReadAllText((Join-Path $installed 'personal.gba')) -eq 'private-ROM') 'Update modified a user ROM.'
    $rollback=Fixture 'rollback' 'original-';$before=Hash-File (Join-Path $rollback 'pokemulti.exe')
    $lock=[IO.File]::Open((Join-Path $rollback 'pokemulti_game.exe'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try { Reject { Install-StagedPackage $expanded $rollback (Join-Path $testRoot 'backup-rollback') } 'Locked running file did not reject update.' } finally { $lock.Dispose() }
    Assert ((Hash-File (Join-Path $rollback 'pokemulti.exe')) -eq $before) 'Rollback did not restore the first changed program.'
    Assert ([IO.File]::ReadAllText((Join-Path $rollback 'pokemulti_game.exe')) -eq 'original-pokemulti_game.exe') 'Locked file was changed.'
    [IO.File]::AppendAllText((Join-Path $expanded 'pokemulti.exe'),'tampered')
    Reject { Install-StagedPackage $expanded $rollback (Join-Path $testRoot 'backup-tamper') } 'Changed staging files were accepted.'
    Write-Output 'PASS: versions, unsafe paths, private-data exclusion, checksums, ZIP validation, installation, personal-data preservation and locked-file rollback.'
} finally {
    $resolved=(Resolve-Path -LiteralPath $testRoot).Path
    $parent=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')
    if ($resolved.StartsWith($parent+'\',[StringComparison]::OrdinalIgnoreCase) -and [IO.Path]::GetFileName($resolved).StartsWith('PokeMulti-updater-test-')) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
