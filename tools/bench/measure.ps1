# Measures RAM (working set + private bytes) and CPU of the YouTube client (+ its mpv
# player) and of a browser playing the same video, so the README numbers are reproducible.
#
# The browser is measured only through processes started with the dedicated profile
# folder "yt-bench-profile" (see start_browser.bat). That keeps your normal browser tabs
# and other apps that embed Chromium/WebView2 (WhatsApp, Teams, ...) out of the numbers.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\bench\measure.ps1 -Label app_playing -Seconds 30 -Delay 10
param(
    [string]$Label = "sample",
    [string]$OutDir = "",
    [int]$Seconds = 30,
    # Grace period before sampling starts, so you can bring the player window back to the
    # front (an occluded window stops drawing video frames and would look unfairly cheap).
    [int]$Delay = 10
)
if ($Delay -gt 0) {
    Write-Host "Bring the player window to the front. Measuring starts in $Delay s..."
    Start-Sleep -Seconds $Delay
}

$cores = [Environment]::ProcessorCount

function Get-ClientProcs {
    Get-Process -Name "youtube-client", "mpv" -ErrorAction SilentlyContinue
}

function Get-BrowserProcs {
    $ids = Get-CimInstance Win32_Process -Filter "Name='chrome.exe' OR Name='msedge.exe'" |
        Where-Object { $_.CommandLine -like '*yt-bench-profile*' } |
        ForEach-Object { $_.ProcessId }
    if ($ids) { Get-Process -Id $ids -ErrorAction SilentlyContinue }
}

$groups = [ordered]@{
    "youtube-client + mpv" = ${function:Get-ClientProcs}
    "Browser (bench prof.)" = ${function:Get-BrowserProcs}
}

function Get-CpuTotal($getter) {
    $t = 0.0
    foreach ($p in (& $getter)) {
        try { $t += $p.TotalProcessorTime.TotalSeconds } catch {}
    }
    return $t
}

$startCpu = @{}
$ramSamples = @{}
foreach ($g in $groups.Keys) {
    $startCpu[$g] = Get-CpuTotal $groups[$g]
    $ramSamples[$g] = New-Object System.Collections.ArrayList
}

$sw = [Diagnostics.Stopwatch]::StartNew()
while ($sw.Elapsed.TotalSeconds -lt $Seconds) {
    foreach ($g in $groups.Keys) {
        $procs = @(& $groups[$g])
        if ($procs.Count -gt 0) {
            $ws = ($procs | Measure-Object -Property WorkingSet64 -Sum).Sum / 1MB
            $pb = ($procs | Measure-Object -Property PrivateMemorySize64 -Sum).Sum / 1MB
            [void]$ramSamples[$g].Add([pscustomobject]@{ WS = $ws; PB = $pb; N = $procs.Count })
        }
    }
    Start-Sleep -Seconds 2
}
$elapsed = $sw.Elapsed.TotalSeconds

$os = Get-CimInstance Win32_OperatingSystem
$cpuName = (Get-CimInstance Win32_Processor | Select-Object -First 1).Name
$ci = [Globalization.CultureInfo]::InvariantCulture
$lines = @()
$lines += "Label: $Label   Date: $(Get-Date -Format s)   Window: $([math]::Round($elapsed))s"
$lines += "CPU: $cpuName ($cores logical)   RAM: $([math]::Round($os.TotalVisibleMemorySize/1MB,1)) GB"
$lines += ""
$lines += "{0,-22} {1,6} {2,14} {3,16} {4,10}" -f "Group", "Procs", "WorkingSet MB", "PrivateBytes MB", "CPU %"
$summary = [ordered]@{ label = $Label; date = (Get-Date -Format s); seconds = [math]::Round($elapsed); cpu = $cpuName; logicalCores = $cores; groups = [ordered]@{} }
foreach ($g in $groups.Keys) {
    $s = $ramSamples[$g]
    if ($s.Count -eq 0) {
        $lines += "{0,-22} {1,6}" -f $g, "-"
        continue
    }
    $ws = ($s | Measure-Object -Property WS -Average).Average
    $pb = ($s | Measure-Object -Property PB -Average).Average
    $n  = ($s | Measure-Object -Property N -Maximum).Maximum
    $cpu = ((Get-CpuTotal $groups[$g]) - $startCpu[$g]) / ($elapsed * $cores) * 100
    if ($cpu -lt 0) { $cpu = 0 }
    $lines += [string]::Format($ci, "{0,-22} {1,6} {2,14:F1} {3,16:F1} {4,10:F1}", $g, $n, $ws, $pb, $cpu)
    $summary.groups[$g] = [ordered]@{
        processes = $n
        workingSetMb = [math]::Round($ws, 1)
        privateMb = [math]::Round($pb, 1)
        cpuPercentOfAllCores = [math]::Round($cpu, 2)
        coresUsed = [math]::Round($cpu * $cores / 100, 2)
    }
}

$outDir = if ($OutDir) { $OutDir } else { Join-Path $PSScriptRoot "results" }
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$outFile = Join-Path $outDir "$Label.txt"
$lines | Set-Content -Path $outFile -Encoding UTF8
$summary | ConvertTo-Json -Depth 5 | Set-Content -Path (Join-Path $outDir "$Label.json") -Encoding UTF8
$lines | ForEach-Object { Write-Host $_ }
Write-Host ""
Write-Host "Saved to $outFile"
