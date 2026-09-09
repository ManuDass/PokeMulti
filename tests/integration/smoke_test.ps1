param([string]$Rom = '', [string]$Configuration = 'Debug', [switch]$Packaged, [switch]$PlayGame, [string]$LaunchBat = '', [switch]$HostWorld, [string]$PackageDirectory = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sourceVersion = [regex]::Match([IO.File]::ReadAllText((Join-Path $projectRoot "CMakeLists.txt")), 'project\(PokeMulti VERSION ([0-9.]+)').Groups[1].Value
$exe = Join-Path $projectRoot "build\$Configuration\pokemulti.exe"
if ($Packaged) { $exe = Join-Path $projectRoot "dist\PokeMulti-$sourceVersion\pokemulti.exe" }
if ($PackageDirectory) { $exe = Join-Path ([IO.Path]::GetFullPath($PackageDirectory)) 'pokemulti.exe' }
if (!(Test-Path -LiteralPath $exe)) { throw "Build $Configuration first." }
$testRoot = Join-Path $projectRoot ("cache\ui-check-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class FrUiTest {
 public delegate bool EnumProc(IntPtr h, IntPtr p);
 [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc callback,IntPtr p);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h,System.Text.StringBuilder b,int n);
 public static IntPtr FindOwned(string klass,uint pid) { IntPtr result=IntPtr.Zero; EnumWindows((h,p)=>{uint owner;GetWindowThreadProcessId(h,out owner);var b=new System.Text.StringBuilder(128);GetClassName(h,b,128);if(owner==pid&&b.ToString()==klass){result=h;return false;}return true;},IntPtr.Zero);return result; }

 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string c, string n);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint p);
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
 [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr h, int i);
 [DllImport("user32.dll")] public static extern int SetWindowLong(IntPtr h, int i,int value);
 [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int w,int height,uint flags);
 [DllImport("user32.dll", EntryPoint="SendMessageW", CharSet=CharSet.Unicode)] public static extern IntPtr SendText(IntPtr h, uint m, IntPtr w, string t);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out Rect r);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out Rect r);
 [DllImport("user32.dll")] public static extern bool ScreenToClient(IntPtr h, ref Point p);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
 [DllImport("gdi32.dll")] public static extern bool SetViewportOrgEx(IntPtr dc, int x, int y, IntPtr old);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int command);
 public static IntPtr FindGameWindow() { return FindWindow("SDL_app", null); }
 public struct Rect { public int Left, Top, Right, Bottom; }
 public struct Point { public int X,Y; }
}
'@
function Wait-Until([scriptblock]$Condition, [string]$Failure) {
    $deadline = [DateTime]::UtcNow.AddSeconds(50)
    while ([DateTime]::UtcNow -lt $deadline) {
        if (& $Condition) { return }
        Start-Sleep -Milliseconds 50
    }
    throw $Failure
}
function Capture-Window([string]$Name) {
    $capture = Join-Path $testRoot 'ui-capture.bmp'
    if (Test-Path -LiteralPath $capture) { Remove-Item -LiteralPath $capture }
    [FrUiTest]::PostMessage($script:windowHandle,0x8028,[IntPtr]::Zero,[IntPtr]::Zero) | Out-Null
    Wait-Until { Test-Path -LiteralPath $capture } 'Launcher did not render a capture.'
    $bitmap = [System.Drawing.Bitmap]::FromFile($capture)
    try {
        if ($bitmap.GetPixel(0,0).ToArgb() -eq $bitmap.GetPixel([int]($bitmap.Width/2),[int]($bitmap.Height/2)).ToArgb()) {
            throw 'Launcher capture is blank.'
        }
    } finally { $bitmap.Dispose() }
    Copy-Item -LiteralPath $capture -Destination (Join-Path $testRoot $Name)
}
function Start-Launcher([bool]$WithRom) {
    $arguments = '--data-dir "' + $testRoot + '"'
    if ($WithRom) { $arguments += ' --rom "' + [IO.Path]::GetFullPath($Rom) + '"' }
    if ($LaunchBat) {
        $batchPath = (Resolve-Path -LiteralPath (Join-Path $projectRoot $LaunchBat)).Path
        $batch = Start-Process -FilePath $env:ComSpec -ArgumentList ('/d /c ""' + $batchPath + '" ' + $arguments + '"') -WorkingDirectory $testRoot -WindowStyle Hidden -PassThru
        Wait-Until {
            $script:batchLauncher = Get-CimInstance Win32_Process -Filter "Name='pokemulti.exe'" | Where-Object { $_.CommandLine.Contains($testRoot) } | Select-Object -First 1
            return $null -ne $script:batchLauncher
        } 'Batch did not launch a process using the isolated test profile.'
        $process = Get-Process -Id $script:batchLauncher.ProcessId
        $script:launcherProcess = $process
        if ($script:batchLauncher.ExecutablePath -ne $exe) { throw "Batch launched the wrong version: $($script:batchLauncher.ExecutablePath)" }
        Write-Host "PASS: $LaunchBat started $($process.Path)"
    } else {
        $process = Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
    }
    $script:launcherHandle = $process.Handle
    $script:windowHandle = [IntPtr]::Zero
    Wait-Until {
        $candidate = [FrUiTest]::FindOwned('FireRedRecompLauncher',[uint32]$process.Id)
        [uint32]$owner = 0
        if ($candidate -ne [IntPtr]::Zero) { [FrUiTest]::GetWindowThreadProcessId($candidate,[ref]$owner) | Out-Null }
        if ($owner -eq $process.Id) { $script:windowHandle = $candidate; return $true }
        if ($process.HasExited) { throw "Launcher exited early: $($process.ExitCode)" }
        return $false
    } 'Launcher window did not appear.'
    # Keep test windows offscreen and out of the taskbar. Native EDIT controls
    # require a visible ancestor to paint their real contents into WM_PRINT.
    [FrUiTest]::SetWindowLong($script:windowHandle,-20,([FrUiTest]::GetWindowLong($script:windowHandle,-20) -bor 0x08000080)) | Out-Null
    [FrUiTest]::SetWindowPos($script:windowHandle,[IntPtr]::Zero,-20000,-20000,0,0,0x15) | Out-Null
    [FrUiTest]::ShowWindow($script:windowHandle,8) | Out-Null
    return $process
}
function Close-Launcher($Process) {
    [FrUiTest]::PostMessage($script:windowHandle,0x10,[IntPtr]::Zero,[IntPtr]::Zero) | Out-Null
    if (!$Process.WaitForExit(5000)) { throw 'Launcher did not close.' }
    if ($Process.ExitCode -ne 0) { throw "Launcher exit: $($Process.ExitCode)" }
    if (![IO.File]::ReadAllText((Join-Path $testRoot 'logs\launcher.log')).Contains("Launcher $sourceVersion.")) { throw 'Launcher reported a stale version.' }
}
$launcherProcess = $null
$gameProcess = $null
$oldVideo = $env:SDL_VIDEODRIVER
$oldAudio = $env:SDL_AUDIODRIVER
try {
    if ($PlayGame) { $env:SDL_VIDEODRIVER = "windows"; $env:SDL_AUDIODRIVER = "dummy" }
    $launcherProcess = Start-Launcher $false
    Wait-Until { [FrUiTest]::GetDlgItem($script:windowHandle,101) -ne [IntPtr]::Zero } 'Welcome controls missing.'
    Capture-Window 'welcome.bmp'
    Close-Launcher $launcherProcess; $launcherProcess = $null
    if ($Rom) {
        $launcherProcess = Start-Launcher $true
        Wait-Until {
            $edit = [FrUiTest]::GetDlgItem($script:windowHandle,201)
            return $edit -ne [IntPtr]::Zero -and (([FrUiTest]::GetWindowLong($edit,-16) -band 0x10000000) -ne 0)
        } 'Validated ROM did not reach profile creation.'
        [FrUiTest]::SendText([FrUiTest]::GetDlgItem($script:windowHandle,201),0xC,[IntPtr]::Zero,'Test Trainer') | Out-Null
        Wait-Until { [FrUiTest]::IsWindowEnabled([FrUiTest]::GetDlgItem($script:windowHandle,112)) } 'Cartridge artwork generation did not finish.'
        Capture-Window 'profile.bmp'
        # Actual pointer drag must change the perspective-rendered cartridge.
        [FrUiTest]::SendMessage($script:windowHandle,0x201,[IntPtr]1,[IntPtr](220 + (340 -shl 16))) | Out-Null
        [FrUiTest]::SendMessage($script:windowHandle,0x200,[IntPtr]1,[IntPtr](285 + (355 -shl 16))) | Out-Null
        [FrUiTest]::SendMessage($script:windowHandle,0x202,[IntPtr]0,[IntPtr](285 + (355 -shl 16))) | Out-Null
        Capture-Window 'cartridge-rotated.bmp'
        [FrUiTest]::SendMessage($script:windowHandle,0x201,[IntPtr]1,[IntPtr](285 + (355 -shl 16))) | Out-Null
        [FrUiTest]::SendMessage($script:windowHandle,0x200,[IntPtr]1,[IntPtr](220 + (340 -shl 16))) | Out-Null
        [FrUiTest]::SendMessage($script:windowHandle,0x202,[IntPtr]0,[IntPtr](220 + (340 -shl 16))) | Out-Null
        [FrUiTest]::PostMessage($script:windowHandle,0x111,[IntPtr]102,[IntPtr]::Zero) | Out-Null
        Wait-Until { Test-Path -LiteralPath (Join-Path $testRoot 'profile.cfg') } 'Profile was not saved.'
        Wait-Until { [FrUiTest]::GetDlgItem($script:windowHandle,106) -ne [IntPtr]::Zero } 'Main menu missing.'
        $profileText = Get-Content -LiteralPath (Join-Path $testRoot 'profile.cfg') -Raw
        if (!$profileText.Contains('player_name "Test Trainer"')) { throw 'Entered player name was not saved.' }
        foreach ($id in 103..105) {
            $control = [FrUiTest]::GetDlgItem($script:windowHandle,$id)
            if ($control -eq [IntPtr]::Zero -or ![FrUiTest]::IsWindowEnabled($control)) { throw "Implemented game action $id is not enabled." }
        }
        Capture-Window 'menu.bmp'
        [FrUiTest]::PostMessage($script:windowHandle,0x111,[IntPtr]205,[IntPtr]::Zero) | Out-Null
        Wait-Until { (([FrUiTest]::GetWindowLong([FrUiTest]::GetDlgItem($script:windowHandle,206),-16) -band 0x10000000) -ne 0) } 'New world screen missing.'
        [FrUiTest]::SendText([FrUiTest]::GetDlgItem($script:windowHandle,201),0xC,[IntPtr]::Zero,'Another adventure') | Out-Null
        Capture-Window 'new-world.bmp'
        [FrUiTest]::PostMessage($script:windowHandle,0x111,[IntPtr]206,[IntPtr]::Zero) | Out-Null
        Wait-Until { @(Get-ChildItem -LiteralPath (Join-Path $testRoot 'worlds') -Filter world.cfg -Recurse).Count -eq 2 } 'Second world was not created.'
        Wait-Until { (([FrUiTest]::GetWindowLong([FrUiTest]::GetDlgItem($script:windowHandle,241),-16) -band 0x10000000) -ne 0) } 'World selector did not show both worlds.'
        Capture-Window 'two-worlds.bmp'
        $worldHashes=@{};Get-ChildItem -LiteralPath (Join-Path $testRoot 'worlds') -Recurse -File | ForEach-Object { $worldHashes[$_.FullName]=(Get-FileHash -LiteralPath $_.FullName).Hash }
        [FrUiTest]::PostMessage($script:windowHandle,0x111,[IntPtr]114,[IntPtr]::Zero) | Out-Null
        Wait-Until { [FrUiTest]::IsWindowVisible([FrUiTest]::GetDlgItem($script:windowHandle,115)) } 'Edit username screen missing.'
        [FrUiTest]::SendText([FrUiTest]::GetDlgItem($script:windowHandle,201),0xC,[IntPtr]::Zero,'Renamed Trainer') | Out-Null
        Capture-Window 'edit-username.bmp'
        [FrUiTest]::PostMessage($script:windowHandle,0x111,[IntPtr]115,[IntPtr]::Zero) | Out-Null
        Wait-Until { (Get-Content -LiteralPath (Join-Path $testRoot 'profile.cfg') -Raw).Contains('player_name "Renamed Trainer"') } 'Edited username did not persist.'
        foreach ($file in $worldHashes.Keys) { if ((Get-FileHash -LiteralPath $file).Hash -ne $worldHashes[$file]) { throw 'Username edit changed a world file.' } }
        [FrUiTest]::PostMessage($script:windowHandle,0x111,[IntPtr]116,[IntPtr]::Zero) | Out-Null
        Wait-Until { [FrUiTest]::IsWindowVisible([FrUiTest]::GetDlgItem($script:windowHandle,117)) } 'Updates screen missing.'
        Capture-Window 'updates.bmp'
        [FrUiTest]::PostMessage($script:windowHandle,0x111,[IntPtr]108,[IntPtr]::Zero) | Out-Null
        Wait-Until { [FrUiTest]::IsWindowVisible([FrUiTest]::GetDlgItem($script:windowHandle,104)) } 'Return from updates failed.'
        [FrUiTest]::PostMessage($script:windowHandle,0x111,[IntPtr]104,[IntPtr]::Zero) | Out-Null
        Wait-Until { (([FrUiTest]::GetWindowLong([FrUiTest]::GetDlgItem($script:windowHandle,233),-16) -band 0x10000000) -ne 0) } 'Host player limit missing.'
        [FrUiTest]::SendText([FrUiTest]::GetDlgItem($script:windowHandle,233),0xC,[IntPtr]::Zero,'8') | Out-Null
        Capture-Window 'host-eight.bmp'
        [FrUiTest]::PostMessage($script:windowHandle,0x111,[IntPtr]108,[IntPtr]::Zero) | Out-Null
        Wait-Until { (([FrUiTest]::GetWindowLong([FrUiTest]::GetDlgItem($script:windowHandle,105),-16) -band 0x10000000) -ne 0) } 'Return to worlds failed.'
        [FrUiTest]::PostMessage($script:windowHandle,0x111,[IntPtr]105,[IntPtr]::Zero) | Out-Null
        Wait-Until { (([FrUiTest]::GetWindowLong([FrUiTest]::GetDlgItem($script:windowHandle,230),-16) -band 0x10000000) -ne 0) } 'Join screen missing.'
        Capture-Window 'join-world.bmp'
        [FrUiTest]::PostMessage($script:windowHandle,0x111,[IntPtr]108,[IntPtr]::Zero) | Out-Null
        Wait-Until { (([FrUiTest]::GetWindowLong([FrUiTest]::GetDlgItem($script:windowHandle,106),-16) -band 0x10000000) -ne 0) } 'Return from join failed.'
        [FrUiTest]::PostMessage($script:windowHandle,0x111,[IntPtr]106,[IntPtr]::Zero) | Out-Null
        Wait-Until { [FrUiTest]::GetDlgItem($script:windowHandle,110) -ne [IntPtr]::Zero } 'Settings missing.'
        Capture-Window 'settings.bmp'
        Close-Launcher $launcherProcess; $launcherProcess = $null
        $launcherProcess = Start-Launcher $false
        Wait-Until { [FrUiTest]::GetDlgItem($script:windowHandle,106) -ne [IntPtr]::Zero } 'Stored profile did not revalidate on relaunch.'
        if ($PlayGame) {
            if ($HostWorld) {
                [FrUiTest]::PostMessage($script:windowHandle,0x111,[IntPtr]104,[IntPtr]::Zero) | Out-Null
                Wait-Until { ([FrUiTest]::GetWindowLong([FrUiTest]::GetDlgItem($script:windowHandle,233),-16) -band 0x10000000) -ne 0 } 'Host setup missing after reload.'
                [FrUiTest]::SendText([FrUiTest]::GetDlgItem($script:windowHandle,233),0xC,[IntPtr]::Zero,'8') | Out-Null
                # Bind an unused test port; the production default is unchanged.
                $listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0);$listener.Start();$hostTestPort=$listener.LocalEndpoint.Port;$listener.Stop()
                [FrUiTest]::SendText([FrUiTest]::GetDlgItem($script:windowHandle,232),0xC,[IntPtr]::Zero,([string]$hostTestPort)) | Out-Null
                [FrUiTest]::PostMessage($script:windowHandle,0x111,[IntPtr]207,[IntPtr]::Zero) | Out-Null
            } else { [FrUiTest]::PostMessage($script:windowHandle,0x111,[IntPtr]103,[IntPtr]::Zero) | Out-Null }
            $script:gameWindow = [IntPtr]::Zero
            Wait-Until {
                $child = Get-CimInstance Win32_Process -Filter ("Name='pokemulti_game.exe' AND ParentProcessId=" + $launcherProcess.Id) | Select-Object -First 1
                [uint32]$owner = if ($child) { $child.ProcessId } else { 0 }
                $candidate = if ($owner) { [FrUiTest]::FindOwned('SDL_app',$owner) } else { [IntPtr]::Zero }
                if ($owner -ne 0) {
                    $script:gameProcess = Get-Process -Id $owner
                    if ($candidate -ne [IntPtr]::Zero) {
                        $expectedRuntime = Join-Path (Split-Path -Parent $exe) 'pokemulti_game.exe'
                        if ($child.ExecutablePath -ne $expectedRuntime) { throw "Wrong runtime: $($child.ExecutablePath)" }
                        if ($HostWorld -and (!$child.CommandLine.Contains('--capacity 8') -or !$child.CommandLine.Contains('--online host') -or !$child.CommandLine.Contains('--world-dir'))) { throw 'Host world settings did not reach the runtime.' }
                        Write-Host "PASS: game executable $($child.ExecutablePath)"
                        $script:gameWindow = $candidate
                        $script:gameProcess = Get-Process -Id $owner
                        $script:gameHandle = $script:gameProcess.Handle
                        [FrUiTest]::ShowWindow($candidate,0) | Out-Null
                        return $true
                    }
                }
                return $false
            } 'Play did not open the game window as a launcher child.'
            if ([FrUiTest]::IsWindowVisible($script:windowHandle)) { throw 'Launcher stayed visible during play.' }
            $launchControl = if ($HostWorld) { 207 } else { 103 }
            if ([FrUiTest]::IsWindowEnabled([FrUiTest]::GetDlgItem($script:windowHandle,$launchControl))) { throw 'World launch stayed enabled while the save was open.' }
            Wait-Until { (Get-ChildItem -LiteralPath (Join-Path $testRoot 'native-cache') -Filter '*.dll' -File -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1) -ne $null } 'Launched game did not compile a native block.'
            [FrUiTest]::PostMessage($script:gameWindow,0x100,[IntPtr]0x7B,[IntPtr]0x00580001) | Out-Null
            [FrUiTest]::PostMessage($script:gameWindow,0x101,[IntPtr]0x7B,[IntPtr]0x00580001) | Out-Null
            Wait-Until { Test-Path -LiteralPath (Join-Path $testRoot 'game-ui.bmp') } 'Integrated game screenshot was not produced by F12.'
            [FrUiTest]::PostMessage($script:gameWindow,0x10,[IntPtr]::Zero,[IntPtr]::Zero) | Out-Null
            if (!$script:gameProcess.WaitForExit(8000)) { throw 'Game did not close cleanly.' }
            if ($script:gameProcess.ExitCode -ne 0) { throw "Game exit: $($script:gameProcess.ExitCode)" }
            if (![IO.File]::ReadAllText((Join-Path $testRoot 'runtime.log')).Contains("[PokeMulti] Runtime $sourceVersion")) { throw 'Runtime reported a stale version.' }
            $script:gameProcess = $null
            Wait-Until { [FrUiTest]::IsWindowVisible($script:windowHandle) -and [FrUiTest]::IsWindowVisible([FrUiTest]::GetDlgItem($script:windowHandle,103)) -and [FrUiTest]::IsWindowEnabled([FrUiTest]::GetDlgItem($script:windowHandle,103)) } 'World selection did not return after closing the game.'
            if (![FrUiTest]::IsWindowVisible($script:windowHandle)) { throw 'Launcher did not return after the game closed.' }
            Write-Output 'PASS: Play launched the game, native compilation ran, the game closed and Play re-enabled.'
        }
        Close-Launcher $launcherProcess; $launcherProcess = $null
        Write-Output 'PASS: ROM validation, profile creation, enabled Play/Host/Join controls, settings and profile reload.'
    }
    Write-Output "PASS: native window rendering and clean exit. Captures: $testRoot"
} finally {
    $env:SDL_VIDEODRIVER = $oldVideo
    $env:SDL_AUDIODRIVER = $oldAudio
    if ($script:gameProcess -and !$script:gameProcess.HasExited) {
        [FrUiTest]::PostMessage($script:gameWindow,0x10,[IntPtr]::Zero,[IntPtr]::Zero) | Out-Null
        if (!$script:gameProcess.WaitForExit(3000)) { $script:gameProcess.Kill() }
    }
    if ($launcherProcess -and !$launcherProcess.HasExited) {
        [FrUiTest]::PostMessage($script:windowHandle,0x10,[IntPtr]::Zero,[IntPtr]::Zero) | Out-Null
        if (!$launcherProcess.WaitForExit(3000)) { $launcherProcess.Kill() }
    }
}
