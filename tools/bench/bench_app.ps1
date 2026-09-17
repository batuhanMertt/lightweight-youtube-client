# Starts youtube-client with a given codec / decoding mode, lets the video play, measures it,
# records what mpv was really decoding, then closes the app.
# Usage: bench_app.ps1 -Label app_h264_hw -Codec h264 -HwDec auto-safe
param(
    [Parameter(Mandatory = $true)][string]$Label,
    [ValidateSet("h264", "vp9", "av1", "any")][string]$Codec = "h264",
    [string]$HwDec = "auto-safe",          # "auto-safe" = hardware, "no" = software (CPU) decoding
    [string]$VideoId = "V5Asw04DsD8",
    [int]$Warmup = 30,
    [int]$Seconds = 30,
    [string]$OutDir = ""
)

$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$exeDir = Join-Path $root "build"
$exe = Join-Path $exeDir "youtube-client.exe"
$results = if ($OutDir) { $OutDir } else { Join-Path $PSScriptRoot "results" }
New-Item -ItemType Directory -Force -Path $results | Out-Null

function Stop-App {
    $app = Get-Process -Name "youtube-client" -ErrorAction SilentlyContinue
    foreach ($p in @($app)) {
        if ($p) {
            Get-CimInstance Win32_Process -Filter "Name='mpv.exe' AND ParentProcessId=$($p.Id)" |
                ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
            Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        }
    }
    Start-Sleep -Seconds 2
}

Stop-App
Remove-Item (Join-Path $exeDir "playback_info.txt") -ErrorAction SilentlyContinue

$env:YTC_AUTOPLAY = $VideoId
$env:YTC_VIDEO_CODEC = $Codec
$env:YTC_HWDEC = $HwDec
$env:YTC_BENCH = "1"   # keep the app window on top so it keeps presenting frames
Write-Host "[$Label] starting app (codec=$Codec, hwdec=$HwDec)"
Start-Process -FilePath $exe -WorkingDirectory $exeDir | Out-Null
Remove-Item Env:YTC_AUTOPLAY, Env:YTC_VIDEO_CODEC, Env:YTC_HWDEC, Env:YTC_BENCH -ErrorAction SilentlyContinue

function Get-PlaybackPosition {
    $f = Join-Path $exeDir "playback_info.txt"
    if (-not (Test-Path $f)) { return $null }
    $line = Get-Content $f | Where-Object { $_ -like 'position=*' } | Select-Object -First 1
    if (-not $line) { return $null }
    return [double]::Parse($line.Substring(9), [Globalization.CultureInfo]::InvariantCulture)
}

Start-Sleep -Seconds $Warmup
$posStart = Get-PlaybackPosition
& (Join-Path $PSScriptRoot "measure.ps1") -Label $Label -Seconds $Seconds -Delay 0 -OutDir $results
Start-Sleep -Seconds 1
$posEnd = Get-PlaybackPosition

$info = Join-Path $exeDir "playback_info.txt"
if (Test-Path $info) {
    Copy-Item $info (Join-Path $results "$Label.playback.txt") -Force
    $played = if ($posStart -ne $null -and $posEnd -ne $null) { [math]::Round($posEnd - $posStart, 1) } else { "" }
    Add-Content (Join-Path $results "$Label.playback.txt") "played_seconds_during_measurement=$played"
    Get-Content $info | ForEach-Object { Write-Host "  $_" }
} else {
    "no playback info (video did not start?)" | Set-Content (Join-Path $results "$Label.playback.txt")
}

Stop-App
