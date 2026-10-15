<#
.SYNOPSIS
    Build, test and run this VLC worktree on Windows and Linux (WSL).
    See docs/agents/build.md.

.EXAMPLE
    extras\agents\jvlc.ps1 setup                       # once per machine
    extras\agents\jvlc.ps1 build                       # win64 + linux, everything
    extras\agents\jvlc.ps1 build -Platform win64 -Qt   # only the Qt module
    extras\agents\jvlc.ps1 test                        # Qt model + QML tests, both platforms
    extras\agents\jvlc.ps1 run -Platform win64 -Shot C:\...\shots\31\win.png
    extras\agents\jvlc.ps1 run -Platform x11 -Keep     # then: shot -Platform x11 -Shot ...
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('setup', 'build', 'test', 'run', 'shot', 'shell', 'info', 'clean', 'help')]
    [string]$Command = 'help',

    # build/test/shell/clean: win64, linux or all. run/shot: win64, x11 or wayland.
    [string]$Platform = 'all',

    # build: only rebuild the Qt module (qt_plugin), plus its install on win64.
    [switch]$Qt,
    # build: meson targets to build instead of everything.
    [string[]]$Target = @(),
    # build: re-run meson setup.
    [switch]$Reconfigure,
    # build: parallel jobs (default $env:JVLC_JOBS or 4).
    [int]$Jobs = 0,

    # test: only tests whose name matches this wildcard (e.g. '*navigation*').
    [string]$Filter = '*',

    # run/shot: PNG path of the screenshot (run: default no screenshot).
    [string]$Shot = '',
    # run: seconds to wait after the window appears, before the screenshot.
    [int]$Wait = 5,
    # run: leave VLC running after the screenshot.
    [switch]$Keep,
    # run: extra VLC arguments (media, options).
    [string[]]$VlcArgs = @()
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3

$Shared = if ($env:JVLC_SHARED) { $env:JVLC_SHARED } else { Join-Path $HOME 'jvlc-shared' }
$Distro = @{ win64 = 'jvlc-win64'; linux = 'jvlc-linux' }
$ImageTag = '20260611225331'   # extras/ci/gitlab-ci.yml VLC_WIN64_IMAGE / VLC_DEBIAN_IMAGE
$Images = @{
    win64 = "registry.videolan.org/vlc-debian-win64-posix:$ImageTag"
    linux = "registry.videolan.org/vlc-debian-unstable:$ImageTag"
}
$Root = (git -C $PSScriptRoot rev-parse --show-toplevel).Trim()
if (-not $Root) { throw 'jvlc: not inside a git worktree' }
$Root = [IO.Path]::GetFullPath($Root)

# One key per worktree: <leaf>-<hash of the full path>.
function Get-Key {
    $sha = [Security.Cryptography.SHA1]::Create()
    $h = $sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Root.ToLowerInvariant()))
    $hex = -join ($h[0..3] | ForEach-Object { $_.ToString('x2') })
    $leaf = (Split-Path $Root -Leaf) -replace '[^A-Za-z0-9_.-]', '_'
    "$leaf-$hex"
}
$Key = Get-Key
$Out = Join-Path $Shared "builds\$Key"            # Windows-side outputs of this worktree
$InstallWin = Join-Path $Out 'vlc-win64'           # meson install of the win64 build
$Work = "/opt/jvlc/w/$Key"                         # inside both distros

function ConvertTo-WslPath([string]$p) {
    $full = [IO.Path]::GetFullPath($p)
    '/mnt/' + $full.Substring(0, 1).ToLowerInvariant() + ($full.Substring(2) -replace '\\', '/')
}
$ScriptsWsl = ConvertTo-WslPath (Join-Path $Root 'extras\agents\wsl')

# Passed to the WSL scripts as environment variables (wsl.exe would expand a
# '*' argument as a shell glob).
if ($Jobs -gt 0) { $env:JVLC_JOBS = "$Jobs" }
$env:JVLC_FILTER = $Filter
$env:WSLENV = ((@("$env:WSLENV" -split ':') + 'JVLC_JOBS', 'JVLC_FILTER') | Where-Object { $_ } | Select-Object -Unique) -join ':'

# LF-separated, no BOM: read by tar.exe and by bash in WSL.
function Write-Lines([string]$path, [string[]]$lines) {
    [IO.File]::WriteAllText($path, (($lines -join "`n") + "`n"), [Text.UTF8Encoding]::new($false))
}

function Get-Platforms([string[]]$allowed) {
    if ($Platform -eq 'all') { return $allowed }
    if ($allowed -notcontains $Platform) { throw "jvlc: -Platform must be one of: all, $($allowed -join ', ')" }
    @($Platform)
}

function Invoke-Wsl([string]$platform, [string[]]$argv, [switch]$Root) {
    $user = if ($Root) { 'root' } else { 'videolan' }
    & wsl.exe -d $Distro[$platform] -u $user --cd / -- @argv
    if ($LASTEXITCODE -ne 0) { throw "jvlc: '$($argv -join ' ')' failed in $($Distro[$platform]) (exit $LASTEXITCODE)" }
}

function Test-Distro([string]$platform) {
    $names = (wsl.exe -l -q) -replace "`0", '' | ForEach-Object { $_.Trim() }
    $names -contains $Distro[$platform]
}

# --- source sync -------------------------------------------------------------
# The build reads a mirror of the worktree inside the distro's ext4 disk:
# reading /mnt/c through 9p costs ~2 ms per file, which makes every ninja run
# slow. Only files whose size or mtime changed since the last sync are copied
# (tar keeps mtimes, so ninja rebuilds exactly what changed).
function Sync-Source([string]$platform) {
    New-Item -ItemType Directory -Force $Out | Out-Null
    $manifest = Join-Path $Out "sync-$platform.tsv"
    $files = (git -C $Root ls-files -z --cached --others --exclude-standard) -split "`0" |
        Where-Object { $_ } | Sort-Object -Unique
    $now = @{}
    foreach ($f in $files) {
        $fi = [IO.FileInfo]::new((Join-Path $Root $f))
        if ($fi.Exists) { $now[$f] = "$($fi.Length):$($fi.LastWriteTimeUtc.Ticks)" }
    }
    $old = @{}
    $mirrorOk = Test-Path $manifest
    if ($mirrorOk) {
        & wsl.exe -d $Distro[$platform] -u videolan -- test -f "$Work/src/meson.build"
        $mirrorOk = $LASTEXITCODE -eq 0
    }
    if ($mirrorOk) {
        foreach ($line in [IO.File]::ReadAllLines($manifest)) {
            $i = $line.IndexOf("`t")
            if ($i -gt 0) { $old[$line.Substring(0, $i)] = $line.Substring($i + 1) }
        }
    }
    $tmp = Join-Path $Shared 'tmp'
    New-Item -ItemType Directory -Force $tmp | Out-Null
    $stem = Join-Path $tmp "sync-$Key-$platform-$PID"
    if (-not $mirrorOk) {
        # First sync: one `git archive` of HEAD (copying thousands of small
        # files through /mnt/c takes minutes), then the local changes below.
        & git -C $Root archive --format=tar -o "$stem.base.tar" HEAD
        if ($LASTEXITCODE -ne 0) { throw 'jvlc: git archive failed' }
        try {
            Invoke-Wsl $platform @('bash', "$ScriptsWsl/sync.sh", $Key, '-', (ConvertTo-WslPath "$stem.base"))
        } finally { Remove-Item -Force -ErrorAction SilentlyContinue "$stem.base.tar" }
        $dirty = @(git -C $Root ls-files -z --modified --others --exclude-standard) + @(git -C $Root diff -z --cached --name-only)
        $dirty = @(($dirty -join "`0") -split "`0" | Where-Object { $_ })
        foreach ($f in $now.Keys) { $old[$f] = $now[$f] }
        foreach ($f in $dirty) { $old.Remove($f) }
        # in HEAD but deleted locally (the loop below then removes them)
        foreach ($f in ((git -C $Root ls-files -z --deleted) -split "`0" | Where-Object { $_ })) { $old[$f] = 'deleted' }
    }
    $changed = @($now.Keys | Where-Object { $old[$_] -ne $now[$_] })
    $deleted = @($old.Keys | Where-Object { -not $now.ContainsKey($_) })
    try {
        if ($changed.Count -gt 0) { Write-Lines "$stem.lst" $changed }
        if ($deleted.Count -gt 0) { Write-Lines "$stem.del" $deleted }
        if ($changed.Count -gt 0 -or $deleted.Count -gt 0) {
            Invoke-Wsl $platform @('bash', "$ScriptsWsl/sync.sh", $Key, (ConvertTo-WslPath $Root), (ConvertTo-WslPath $stem))
        }
    } finally {
        Remove-Item -Force -ErrorAction SilentlyContinue "$stem.lst", "$stem.del"
    }
    [IO.File]::WriteAllLines($manifest, [string[]]($now.GetEnumerator() | ForEach-Object { "$($_.Key)`t$($_.Value)" }))
    Write-Host "jvlc: synced $($changed.Count) changed, $($deleted.Count) deleted file(s) to $($Distro[$platform]):$Work/src"
}

# --- commands ----------------------------------------------------------------
function Invoke-Setup {
    # 1. distros, imported from the CI images (needs Docker Desktop once)
    foreach ($p in 'win64', 'linux') {
        if (Test-Distro $p) { continue }
        $tar = Join-Path $Shared "wsl\images\$($Images[$p] -replace '^.*/', '' -replace ':', '-').tar"
        if (-not (Test-Path $tar)) {
            docker version --format '{{.Server.Version}}' | Out-Null
            if ($LASTEXITCODE -ne 0) { throw 'jvlc: start Docker Desktop first (needed once to fetch the CI images)' }
            New-Item -ItemType Directory -Force (Split-Path $tar) | Out-Null
            docker pull $Images[$p]
            $c = "jvlc-export-$p"
            docker rm -f $c 2>$null | Out-Null
            docker create --name $c $Images[$p] | Out-Null
            docker export -o $tar $c
            docker rm $c | Out-Null
        }
        $dir = Join-Path $Shared "wsl\$($Distro[$p])"
        New-Item -ItemType Directory -Force $dir | Out-Null
        wsl.exe --import $Distro[$p] $dir $tar --version 2
        if ($LASTEXITCODE -ne 0) { throw "jvlc: wsl --import $($Distro[$p]) failed" }
    }
    foreach ($p in 'win64', 'linux') {
        Invoke-Wsl $p @('sh', "$ScriptsWsl/setup-root.sh", $p) -Root
    }
    # 2. shared Windows contribs (prebuilt from the CI + QtQuickTest)
    $sha = (git -C $Root rev-list -1 HEAD -- extras/tools contrib extras/ci extras/package/win32).Trim()
    $tmp = Join-Path $Shared 'tmp'
    New-Item -ItemType Directory -Force $tmp | Out-Null
    $tar = Join-Path $tmp "contrib-$sha.tar"
    git -C $Root archive --format=tar -o $tar $sha contrib extras/tools
    try {
        Invoke-Wsl win64 @('bash', "$ScriptsWsl/setup-win64.sh", $sha, (ConvertTo-WslPath $tar))
    } finally { Remove-Item -Force -ErrorAction SilentlyContinue $tar }
    Write-Host 'jvlc: setup done'
}

function Invoke-Build {
    $targets = @($Target)
    if ($Qt) { $targets += 'qt_plugin' }
    foreach ($p in Get-Platforms @('win64', 'linux')) {
        $sw = [Diagnostics.Stopwatch]::StartNew()
        Sync-Source $p
        $install = if ($p -eq 'win64') { ConvertTo-WslPath $InstallWin } else { '-' }
        $argv = @('bash', "$ScriptsWsl/build.sh", $p, $Key, $install)
        if ($Reconfigure) { $argv += '--reconfigure' }
        Invoke-Wsl $p ($argv + $targets)
        if ($p -eq 'win64') {
            # portable/ next to libvlccore.dll: VLC keeps its config there, so
            # every worktree has its own, separate from %APPDATA%\vlc.
            New-Item -ItemType Directory -Force (Join-Path $InstallWin 'portable') | Out-Null
        }
        Write-Host ("jvlc: {0} build done in {1:n0} s" -f $p, $sw.Elapsed.TotalSeconds)
    }
}

function Invoke-Test {
    $failed = 0
    foreach ($p in Get-Platforms @('win64', 'linux')) {
        Sync-Source $p
        if ($p -eq 'linux') {
            & wsl.exe -d $Distro.linux -u videolan --cd / -- bash "$ScriptsWsl/test-linux.sh" $Key
            if ($LASTEXITCODE -ne 0) { $failed++ }
        } else {
            $failed += Invoke-WindowsTests
        }
    }
    if ($failed) { throw "jvlc: tests failed on $failed platform(s)" }
    Write-Host 'jvlc: all tests passed'
}

# The win64 tests are cross-built in WSL, copied next to the installed
# libvlc*.dll and run natively, one process per test (like meson test).
function Invoke-WindowsTests {
    $testDir = Join-Path $InstallWin 'tests'
    Invoke-Wsl win64 @('bash', "$ScriptsWsl/test-win64.sh", $Key, (ConvertTo-WslPath $InstallWin)) | Out-Host
    $list = Join-Path $testDir 'tests.tsv'
    $env:VLC_TEST_TIMEOUT = '60'
    # libvlc.dll and libvlccore.dll are in the install root; the stand-in
    # module tests would otherwise look for plugins in the WSL build dir
    $env:PATH = "$InstallWin;$env:PATH"
    $env:VLC_LIB_PATH = Join-Path $InstallWin 'plugins'
    $results = @()
    foreach ($line in [IO.File]::ReadAllLines($list)) {
        $name, $exe, $arg = $line -split "`t"
        $argv = @()
        if ($arg) {
            # -input <mirror path>: point to the same file in the worktree
            $argv = @('-input', (Join-Path $Root ($arg -replace "^$([regex]::Escape("$Work/src/"))", '' -replace '/', '\')))
        }
        $sw = [Diagnostics.Stopwatch]::StartNew()
        $log = Join-Path $testDir "$name.log"
        Remove-Item -Force -ErrorAction SilentlyContinue $log, "$log.err"
        # QTest's own output does not reach a redirected stdout here: -o
        $argv += @('-o', "$log,txt")
        $proc = Start-Process -FilePath (Join-Path $testDir $exe) -ArgumentList $argv -WorkingDirectory $testDir `
            -NoNewWindow -PassThru -RedirectStandardOutput "$log.out" -RedirectStandardError "$log.err"
        if (-not $proc.WaitForExit(120000)) { $proc.Kill(); $code = 'TIMEOUT' } else { $code = $proc.ExitCode }
        $ok = ($code -eq 0)
        $results += [pscustomobject]@{ Test = $name; Result = $(if ($ok) { 'OK' } else { "FAIL ($code)" }); Seconds = [math]::Round($sw.Elapsed.TotalSeconds, 1) }
        if (-not $ok) {
            Get-Content $log, "$log.out", "$log.err" -ErrorAction SilentlyContinue | Select-Object -Last 60 | Write-Host
        }
    }
    $results | Format-Table -AutoSize | Out-String | Write-Host
    $bad = @($results | Where-Object { $_.Result -ne 'OK' }).Count
    Write-Host "jvlc: win64: $($results.Count - $bad)/$($results.Count) tests passed (logs in $testDir)"
    if ($results.Count -eq 0) { Write-Host 'jvlc: win64: no test matched'; return 1 }
    [int]($bad -gt 0)
}

Add-Type -Language CSharp -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class JvlcWin {
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr l);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    public static List<IntPtr> Find(Func<IntPtr, uint, string, bool> pred) {
        var res = new List<IntPtr>();
        EnumWindows((h, l) => {
            if (!IsWindowVisible(h)) return true;
            var sb = new StringBuilder(512);
            GetWindowText(h, sb, 512);
            uint pid; GetWindowThreadProcessId(h, out pid);
            if (pred(h, pid, sb.ToString())) res.Add(h);
            return true;
        }, IntPtr.Zero);
        return res;
    }
    public static RECT Bounds(IntPtr h) { RECT r; GetWindowRect(h, out r); return r; }
}
'@
Add-Type -AssemblyName System.Drawing
# per-monitor DPI aware v2: window sizes in physical pixels on every monitor
[void][JvlcWin]::SetProcessDpiAwarenessContext([IntPtr]-4)

function Wait-Window([scriptblock]$match, [int]$timeoutSec) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    while ((Get-Date) -lt $deadline) {
        $h = [JvlcWin]::Find([Func[IntPtr, uint32, string, bool]]$match) | Select-Object -First 1
        if ($h) { return $h }
        Start-Sleep -Milliseconds 500
    }
    $null
}

# PrintWindow(PW_RENDERFULLCONTENT) asks DWM for the window's own content
# (DirectComposition and WSLg windows included), so other windows covering it
# do not matter and the focus is not stolen. It must not be minimized.
function Save-WindowShot([IntPtr]$h, [string]$path) {
    if ([JvlcWin]::IsIconic($h)) {
        [void][JvlcWin]::ShowWindow($h, 4)   # SW_SHOWNOACTIVATE
        Start-Sleep -Milliseconds 800
    }
    $r = [JvlcWin]::Bounds($h)
    $w = $r.R - $r.L; $hgt = $r.B - $r.T
    if ($w -le 0 -or $hgt -le 0) { throw 'jvlc: window has no size' }
    $bmp = [Drawing.Bitmap]::new($w, $hgt)
    $g = [Drawing.Graphics]::FromImage($bmp)
    $dc = $g.GetHdc()
    $ok = [JvlcWin]::PrintWindow($h, $dc, 2)   # PW_RENDERFULLCONTENT
    $g.ReleaseHdc($dc)
    if (-not $ok) { throw 'jvlc: PrintWindow failed' }
    New-Item -ItemType Directory -Force (Split-Path ([IO.Path]::GetFullPath($path))) | Out-Null
    $bmp.Save([IO.Path]::GetFullPath($path), [Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host "jvlc: screenshot saved to $path ($w x $hgt)"
}

# The main window of this worktree's VLC. win64: a vlc.exe of our install.
# Linux: WSLg shows Linux windows through msrdc.exe (RAIL), titled
# "<title> (jvlc-linux)"; $skip excludes windows that existed before launch.
function Find-VlcWindow([string]$platform, [int]$timeoutSec, [IntPtr[]]$skip = @()) {
    if ($platform -eq 'win64') {
        $exe = Join-Path $InstallWin 'vlc.exe'
        return Wait-Window { param($h, $p, $t)
            $t -like '*VLC*' -and (Get-Process -Id $p -ErrorAction SilentlyContinue).Path -eq $exe } $timeoutSec
    }
    Wait-Window { param($h, $p, $t)
        $t -like '*VLC*(jvlc-linux)' -and $skip -notcontains $h -and
        (Get-Process -Id $p -ErrorAction SilentlyContinue).ProcessName -eq 'msrdc' } $timeoutSec
}

function Get-LinuxVlcWindows {
    @([JvlcWin]::Find([Func[IntPtr, uint32, string, bool]] { param($h, $p, $t) $t -like '*VLC*(jvlc-linux)' }))
}

# WSLg does not show what an X11 window draws until it is resized
function Update-X11Window {
    & wsl.exe -d $Distro.linux -u videolan --cd / -- bash "$ScriptsWsl/x11-nudge.sh" $Key
    $global:LASTEXITCODE = 0
    Start-Sleep -Milliseconds 700
}

function Invoke-Shot {
    if (-not $Shot) { throw 'jvlc: shot needs -Shot <file.png>' }
    $p = if ($Platform -eq 'all') { 'win64' } else { $Platform }
    $h = Find-VlcWindow $p 5
    if (-not $h) { throw "jvlc: no VLC window of this worktree ($p)" }
    if ($p -eq 'x11') { Update-X11Window }
    Save-WindowShot $h $Shot
}

function Invoke-Run {
    if ($Platform -eq 'all') { $Platform = 'win64' }
    $common = @('--no-qt-privacy-ask')
    $log = Join-Path $Out "run-$Platform.log"
    New-Item -ItemType Directory -Force $Out | Out-Null
    switch ($Platform) {
        'win64' {
            $exe = Join-Path $InstallWin 'vlc.exe'
            if (-not (Test-Path $exe)) { throw "jvlc: $exe missing, run: jvlc.ps1 build -Platform win64" }
            # no update check: it would show an error banner (no network API key)
            $argv = $common + @('--no-qt-updates-notif', '-vv', "--logfile=$log", '--file-logging') + $VlcArgs
            $proc = Start-Process -FilePath $exe -ArgumentList $argv -PassThru
            $h = Find-VlcWindow win64 60
        }
        { $_ -in 'x11', 'wayland' } {
            & wsl.exe -d $Distro.linux -u videolan -- pkill -f "$Work/build-linux/bin/vlc-static"
            $global:LASTEXITCODE = 0   # pkill: 1 when nothing was running
            $before = Get-LinuxVlcWindows
            $argv = @('bash', "$ScriptsWsl/run-linux.sh", $Key, $Platform) + $common + $VlcArgs
            $proc = Start-Process -FilePath 'wsl.exe' -ArgumentList (@('-d', $Distro.linux, '-u', 'videolan', '--cd', '/', '--') + $argv) `
                -PassThru -WindowStyle Hidden -RedirectStandardOutput "$log.out" -RedirectStandardError $log
            $h = Find-VlcWindow $Platform 90 $before
        }
        default { throw 'jvlc: run -Platform must be win64, x11 or wayland' }
    }
    if (-not $h) {
        Write-Host "jvlc: no VLC window appeared; log: $log"
        Get-Content $log -ErrorAction SilentlyContinue | Select-Object -Last 40 | Write-Host
        throw 'jvlc: run failed'
    }
    Write-Host "jvlc: VLC window up ($Platform), log: $log"
    Start-Sleep -Seconds $Wait
    if ($Shot) {
        if ($Platform -eq 'x11') { Update-X11Window }
        Save-WindowShot $h $Shot
    }
    if (-not $Keep) {
        if ($Platform -eq 'win64') { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }
        else { & wsl.exe -d $Distro.linux -u videolan -- pkill -f "$Work/build-linux/bin/vlc-static" }
    }
    $global:LASTEXITCODE = 0
}

function Invoke-Shell {
    $p = if ($Platform -eq 'all') { 'linux' } else { $Platform }
    & wsl.exe -d $Distro[$p] -u videolan --cd "$Work" -- bash --rcfile "$ScriptsWsl/env.sh" -i
}

function Invoke-Info {
    [pscustomobject]@{
        Worktree   = $Root
        Key        = $Key
        WinInstall = $InstallWin
        Mirror     = "$Work/src (in jvlc-win64 and jvlc-linux)"
        BuildDirs  = "$Work/build-win64 (jvlc-win64), $Work/build-linux (jvlc-linux)"
        Jobs       = $(if ($env:JVLC_JOBS) { $env:JVLC_JOBS } else { '4' })
    } | Format-List | Out-String | Write-Host
}

function Invoke-Clean {
    foreach ($p in Get-Platforms @('win64', 'linux')) {
        Invoke-Wsl $p @('rm', '-rf', $Work)
        Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $Out "sync-$p.tsv")
    }
    if ($Platform -in 'all', 'win64') { Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $InstallWin }
    Write-Host "jvlc: removed the build state of $Key"
}

switch ($Command) {
    'setup' { Invoke-Setup }
    'build' { Invoke-Build }
    'test' { Invoke-Test }
    'run' { Invoke-Run }
    'shot' { Invoke-Shot }
    'shell' { Invoke-Shell }
    'info' { Invoke-Info }
    'clean' { Invoke-Clean }
    default { Get-Help $PSCommandPath -Detailed }
}
