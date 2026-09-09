param(
    [ValidateSet('Check','Stage','Apply')][string]$Mode='Check',
    [string]$CurrentVersion='', [string]$Installation='', [string]$DataDirectory='',
    [string]$WorkDirectory='', [int]$ParentPid=0
)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$script:Utf8=[Text.UTF8Encoding]::new($false)
$script:Repository='ManuDass/PokeMulti'

function Version-Number([string]$value) {
    if ($value -notmatch '^(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})$') { throw 'Invalid release version.' }
    return [Version]$value
}
function Assert-NoLinks([string]$path) {
    $item=[IO.Path]::GetFullPath($path)
    while ($item) {
        if (Test-Path -LiteralPath $item) {
            if ((Get-Item -LiteralPath $item -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Update paths cannot contain links or junctions.' }
        }
        $item=[IO.Path]::GetDirectoryName($item)
    }
}
function Child-Path([string]$root,[string]$relative) {
    if (!$relative -or $relative.Contains('\') -or $relative.Contains(':') -or $relative.StartsWith('/') -or $relative -match '[\x00-\x1f]') { throw 'Unsafe package path.' }
    foreach ($part in $relative.Split('/')) {
        if (!$part -or $part -eq '.' -or $part -eq '..' -or $part.EndsWith('.') -or $part.EndsWith(' ') -or $part -match '[<>"|?*]' -or $part -match '^(?i:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)') { throw 'Unsafe package filename.' }
    }
    $base=[IO.Path]::GetFullPath($root).TrimEnd('\')
    $full=[IO.Path]::GetFullPath((Join-Path $base $relative))
    if (!$full.StartsWith($base+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Package path escapes its directory.' }
    Assert-NoLinks $full
    return $full
}
function Assert-ProgramPath([string]$path) {
    if ($path -match '(?i)(^|/)(worlds|saves|roms|userdata|backups|updates|guest-cache|native-cache|\.git|\.update[^/]*)(/|$)' -or
        $path -match '(?i)\.(gba|gb|gbc|nds|rom|sav|sa1|ss0|state|pmsv|key|lock|bak)(\.|$)' -or
        $path -match '(?i)(^|/)(profile|identity|friends|world|campaign-current|wager-wallet|wagers-host|released-world)\.cfg$') { throw 'An update contains private-data paths.' }
}
function Hash-File([string]$path) { return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Write-Result([string]$kind,[string]$message) {
    $message=$message.Replace("`r",' ').Replace("`n",' ')
    [IO.File]::WriteAllText((Join-Path $WorkDirectory 'result.txt'),$kind+"`n"+$message,$script:Utf8)
}
function Get-Https([Uri]$uri,[string]$destination,[long]$limit) {
    [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
    for ($redirect=0;$redirect -lt 6;$redirect++) {
        if ($uri.Scheme -ne 'https' -or $uri.Port -ne 443 -or $uri.UserInfo -or ($uri.Host -notin @('api.github.com','github.com','release-assets.githubusercontent.com','objects.githubusercontent.com'))) { throw 'Untrusted update download address.' }
        $request=[Net.HttpWebRequest]::Create($uri)
        $request.UserAgent='PokeMulti-Updater'; $request.Accept='application/vnd.github+json'
        $request.Headers.Add('X-GitHub-Api-Version','2026-03-10')
        $request.AllowAutoRedirect=$false; $request.Timeout=30000; $request.ReadWriteTimeout=30000
        $response=$null
        try {
            $response=$request.GetResponse()
            $code=[int]$response.StatusCode
            if ($code -in @(301,302,303,307,308)) { $uri=[Uri]::new($uri,$response.Headers['Location']); continue }
            if ($code -ne 200 -or $response.ContentLength -gt $limit) { throw 'Invalid or oversized update download.' }
            $input=$response.GetResponseStream(); $output=[IO.File]::Create($destination)
            try {
                $buffer=New-Object byte[] 65536; [long]$size=0
                while (($read=$input.Read($buffer,0,$buffer.Length)) -gt 0) {
                    $size+=$read; if ($size -gt $limit) { throw 'Update download exceeds the size limit.' }
                    $output.Write($buffer,0,$read)
                }
                if ($response.ContentLength -ge 0 -and $size -ne $response.ContentLength) { throw 'The update download was incomplete.' }
                $output.Flush($true)
            } finally { $output.Dispose(); $input.Dispose() }
            return
        } catch [Net.WebException] {
            if ($_.Exception.Response -and [int]$_.Exception.Response.StatusCode -eq 404) { throw 'No published update is available yet.' }
            if ($_.Exception.Response -and [int]$_.Exception.Response.StatusCode -in @(403,429)) { throw 'GitHub is limiting update checks. Please try again later.' }
            throw 'Could not reach GitHub. Check your connection and try again.'
        } finally { if ($response) { $response.Dispose() } }
    }
    throw 'Too many update download redirects.'
}
function Get-Release {
    $metadata=Join-Path $WorkDirectory 'release.json'
    Get-Https ([Uri]"https://api.github.com/repos/$script:Repository/releases/latest") $metadata 2097152
    $release=Get-Content -LiteralPath $metadata -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($release.draft -or $release.prerelease -or $release.tag_name -notmatch '^v(.+)$') { throw 'No stable update is available.' }
    $version=$Matches[1]; [void](Version-Number $version)
    $expectedName="PokeMulti-$version.zip"
    $assets=@($release.assets | Where-Object { $_.name -ceq $expectedName -and $_.state -eq 'uploaded' })
    if ($assets.Count -ne 1) { throw 'The latest release package is not ready yet.' }
    $asset=$assets[0]
    $expectedUrl="https://github.com/$script:Repository/releases/download/v$version/$expectedName"
    if ($asset.browser_download_url -cne $expectedUrl -or $asset.digest -cnotmatch '^sha256:[0-9a-f]{64}$' -or $asset.size -le 0 -or $asset.size -gt 134217728) { throw 'Release verification information is missing or invalid.' }
    return @{ version=$version; url=$expectedUrl; hash=$asset.digest.Substring(7); size=[long]$asset.size }
}
function Read-Manifest([string]$directory) {
    $file=Child-Path $directory 'package-manifest.json'
    if ((Get-Item -LiteralPath $file).Length -gt 4194304) { throw 'Oversized package manifest.' }
    $entries=Get-Content -LiteralPath $file -Raw -Encoding UTF8 | ConvertFrom-Json
    $entries=@($entries)
    if ($entries.Count -lt 3 -or $entries.Count -gt 20000) { throw 'Invalid package manifest.' }
    $names=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $entries) {
        [void](Child-Path $directory $entry.path); Assert-ProgramPath $entry.path
        if ($entry.path -eq 'package-manifest.json' -or !$names.Add($entry.path) -or $entry.sha256 -cnotmatch '^[0-9a-f]{64}$' -or $entry.bytes -lt 0 -or $entry.bytes -gt 67108864) { throw 'Invalid manifest file entry.' }
    }
    foreach ($required in @('pokemulti.exe','pokemulti_game.exe','tools/update.ps1')) { if (!$names.Contains($required)) { throw 'The update is missing a required program file.' } }
    return $entries
}
function Expand-VerifiedPackage([string]$zip,[string]$target,[string]$version,[string]$digest) {
    [void](Version-Number $version)
    if ((Hash-File $zip) -cne $digest) { throw 'The update checksum did not match. Nothing was installed.' }
    if (Test-Path -LiteralPath $target) { throw 'Update staging directory already exists.' }
    Assert-NoLinks $target; [IO.Directory]::CreateDirectory($target) | Out-Null
    $archive=[IO.Compression.ZipFile]::OpenRead($zip)
    try {
        if ($archive.Entries.Count -gt 20000) { throw 'Too many package entries.' }
        $prefix="PokeMulti-$version/"; $names=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase); [long]$total=0
        foreach ($entry in $archive.Entries) {
            # Older Windows Compress-Archive versions emit backslash separators.
            # Canonicalize before every root, traversal and duplicate check.
            $entryName=$entry.FullName.Replace('\','/')
            if (!$entryName.StartsWith($prefix,[StringComparison]::Ordinal)) { throw 'Unexpected package root.' }
            if ((($entry.ExternalAttributes -shr 16) -band 0xF000) -eq 0xA000 -or ($entry.ExternalAttributes -band 0x400)) { throw 'Linked files are not allowed in updates.' }
            $relative=$entryName.Substring($prefix.Length).TrimEnd('/')
            if (!$relative) { continue }
            $path=Child-Path $target $relative; Assert-ProgramPath $relative
            if (!$names.Add($relative)) { throw 'Duplicate package entry.' }
            if ($entryName.EndsWith('/')) { [IO.Directory]::CreateDirectory($path) | Out-Null; continue }
            $total+=$entry.Length
            if ($entry.Length -gt 67108864 -or $total -gt 536870912) { throw 'Expanded update exceeds its size limit.' }
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path)) | Out-Null
            $source=$entry.Open(); $output=[IO.File]::Open($path,[IO.FileMode]::CreateNew)
            try { $source.CopyTo($output) } finally { $output.Dispose(); $source.Dispose() }
            if ((Get-Item -LiteralPath $path).Length -ne $entry.Length) { throw 'Incomplete extracted update file.' }
        }
    } finally { $archive.Dispose() }
    $manifest=@(Read-Manifest $target)
    $files=@(Get-ChildItem -LiteralPath $target -Recurse -File)
    if ($files.Count -ne $manifest.Count+1) { throw 'Unexpected files outside the update manifest.' }
    foreach ($entry in $manifest) {
        $path=Child-Path $target $entry.path
        if ((Get-Item -LiteralPath $path).Length -ne $entry.bytes -or (Hash-File $path) -cne $entry.sha256) { throw 'Extracted update verification failed.' }
    }
}
function Install-StagedPackage([string]$staged,[string]$destination,[string]$backup) {
    Assert-NoLinks $staged; Assert-NoLinks $destination; Assert-NoLinks $backup
    if (!(Test-Path -LiteralPath (Join-Path $destination 'pokemulti.exe')) -or !(Test-Path -LiteralPath (Join-Path $destination 'package-manifest.json'))) { throw 'Updates require an extracted release package. Development builds must be rebuilt.' }
    $manifest=@(Read-Manifest $staged)
    foreach ($entry in $manifest) { if ((Hash-File (Child-Path $staged $entry.path)) -cne $entry.sha256) { throw 'Staged update changed before installation.' } }
    $paths=@($manifest | ForEach-Object { $_.path })+@('package-manifest.json')
    if (Test-Path -LiteralPath $backup) { throw 'An update backup already exists.' }
    [IO.Directory]::CreateDirectory($backup) | Out-Null
    $changed=[Collections.Generic.List[object]]::new()
    try {
        foreach ($relative in $paths) {
            $from=Child-Path $staged $relative; $to=Child-Path $destination $relative; $old=Child-Path $backup $relative
            $existed=Test-Path -LiteralPath $to
            if ($existed) { [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($old)) | Out-Null; [IO.File]::Copy($to,$old,$false) }
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($to)) | Out-Null
            # A sibling temporary file plus atomic rename prevents partial binaries.
            $temp=$to+'.updating'; if (Test-Path -LiteralPath $temp) { throw 'An earlier update temporary file is present.' }
            try {
                [IO.File]::Copy($from,$temp,$false)
                if ($existed) { [IO.File]::Replace($temp,$to,[NullString]::Value) } else { [IO.File]::Move($temp,$to) }
                $changed.Add(@{ path=$relative; existed=$existed })
            }
            finally { if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Force } }
        }
    } catch {
        $failure=$_.Exception.Message; $rollbackError=$false
        for ($i=$changed.Count-1;$i -ge 0;$i--) {
            $record=$changed[$i]; $to=Child-Path $destination $record.path
            try {
                if ($record.existed) { [IO.File]::Copy((Child-Path $backup $record.path),$to,$true) }
                elseif (Test-Path -LiteralPath $to) { Remove-Item -LiteralPath $to -Force }
            } catch { $rollbackError=$true }
        }
        if ($rollbackError) { throw "Update stopped. Recovery files are in $backup. $failure" }
        throw "Update could not be installed; the previous version was restored. $failure"
    }
}
function Quote-Argument([string]$value) { return '"'+[regex]::Replace($value,'(\\*)"','$1$1\"').TrimEnd('\')+('\\'*($value.Length-$value.TrimEnd('\').Length))+'"' }
function Restart-Launcher {
    $exe=Join-Path $Installation 'pokemulti.exe'
    Start-Process -FilePath $exe -ArgumentList ('--data-dir '+(Quote-Argument $DataDirectory)) -WorkingDirectory $Installation | Out-Null
}
function Run-Update {
    [void](Version-Number $CurrentVersion)
    $script:Installation=[IO.Path]::GetFullPath($Installation); $script:DataDirectory=[IO.Path]::GetFullPath($DataDirectory); $script:WorkDirectory=[IO.Path]::GetFullPath($WorkDirectory)
    $expected=Join-Path $DataDirectory 'updates'
    if ([IO.Path]::GetDirectoryName($WorkDirectory) -ne $expected -or [IO.Path]::GetFileName($WorkDirectory) -notmatch '^job-[0-9a-f]{32}$') { throw 'Unexpected update workspace.' }
    Assert-NoLinks $WorkDirectory; Assert-NoLinks $Installation
    if ($Mode -eq 'Apply') {
        $last=Join-Path $DataDirectory 'update-last.txt'; $lock=$null
        try {
            if ($ParentPid -le 0) { throw 'Missing launcher process.' }
            try { $parent=Get-Process -Id $ParentPid -ErrorAction Stop } catch { $parent=$null }
            if ($parent) {
                if ($parent.Path -ne (Join-Path $Installation 'pokemulti.exe')) { throw 'Launcher process identity changed.' }
                if (!$parent.WaitForExit(30000)) { throw 'Close the launcher before installing updates.' }
            }
            $lock=[IO.File]::Open((Join-Path $Installation '.pokemulti-update.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
            foreach ($process in @(Get-Process pokemulti,pokemulti_game -ErrorAction SilentlyContinue)) {
                if ($process.Path -and [IO.Path]::GetDirectoryName($process.Path) -eq $Installation) { throw 'Close all game sessions using this installation before updating.' }
            }
            Install-StagedPackage (Join-Path $WorkDirectory 'package') $Installation (Join-Path $WorkDirectory 'previous')
            [IO.File]::WriteAllText($last,'Update installed. Your profile, trainer identity and worlds are unchanged.',$script:Utf8)
        } catch { [IO.File]::WriteAllText($last,$_.Exception.Message,$script:Utf8) }
        finally { if ($lock) { $lock.Dispose() } }
        Restart-Launcher
        return
    }
    try {
        $release=Get-Release
        if ((Version-Number $release.version) -le (Version-Number $CurrentVersion)) { Write-Result 'current' 'You already have the latest version.'; return }
        if ($Mode -eq 'Check') { Write-Result 'available' $release.version; return }
        $zip=Join-Path $WorkDirectory 'package.zip'
        Get-Https ([Uri]$release.url) $zip 134217728
        if ((Get-Item -LiteralPath $zip).Length -ne $release.size) { throw 'Downloaded package size did not match.' }
        Expand-VerifiedPackage $zip (Join-Path $WorkDirectory 'package') $release.version $release.hash
        Write-Result 'ready' $release.version
    } catch { Write-Result 'error' $_.Exception.Message }
}
if ($MyInvocation.InvocationName -ne '.') { Run-Update }
