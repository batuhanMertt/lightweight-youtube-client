# Runs the codec / decoding comparison for one video and writes results\<videoId>\codec_summary.md.
# Close other heavy apps first. Takes ~7 minutes. Don't touch the mouse while it runs.
#
# Default video: "Big Buck Bunny 60fps 4K" by the Blender Foundation. YouTube serves it in AV1,
# VP9 and H.264, like most recent videos. (Some old videos only exist in H.264; for those every
# player gets H.264 and there is no codec difference to measure.)
param(
    [string]$VideoId = "aqz-KE-bpKQ"
)
$ErrorActionPreference = "Continue"
$bench = $PSScriptRoot
$results = Join-Path (Join-Path $bench "results") $VideoId
New-Item -ItemType Directory -Force -Path $results | Out-Null

$runs = @(
    @{ Label = "app_h264_hw"; Kind = "app";    Codec = "h264"; HwDec = "auto-safe"; What = "App, H.264, GPU decoding (default)" },
    @{ Label = "app_h264_sw"; Kind = "app";    Codec = "h264"; HwDec = "no";        What = "App, H.264, CPU decoding" },
    @{ Label = "app_vp9_sw";  Kind = "app";    Codec = "vp9";  HwDec = "no";        What = "App, VP9 forced, CPU decoding" },
    @{ Label = "app_av1_sw";  Kind = "app";    Codec = "av1";  HwDec = "no";        What = "App, AV1 forced, CPU decoding" },
    @{ Label = "chrome_hw";   Kind = "chrome"; Software = $false;                   What = "Chrome, GPU decoding (default)" },
    @{ Label = "chrome_sw";   Kind = "chrome"; Software = $true;                    What = "Chrome, CPU decoding (--disable-accelerated-video-decode)" }
)

foreach ($r in $runs) {
    try {
        if ($r.Kind -eq "app") {
            & (Join-Path $bench "bench_app.ps1") -Label $r.Label -Codec $r.Codec -HwDec $r.HwDec -VideoId $VideoId -OutDir $results
        } elseif ($r.Software) {
            & (Join-Path $bench "bench_browser.ps1") -Label $r.Label -Software -VideoId $VideoId -OutDir $results
        } else {
            & (Join-Path $bench "bench_browser.ps1") -Label $r.Label -VideoId $VideoId -OutDir $results
        }
    } catch {
        "ERROR: $_" | Set-Content (Join-Path $results "$($r.Label).error.txt")
        Write-Host "[$($r.Label)] ERROR: $_"
    }
}

# ---------- summary ----------
$ci = [Globalization.CultureInfo]::InvariantCulture
$md = @("Video: https://www.youtube.com/watch?v=$VideoId", "",
        "| Run | What was actually decoded | Private MB | Working set MB | CPU % (all cores) | CPU cores used | Dropped frames | Video advanced during 30 s |",
        "|---|---|---:|---:|---:|---:|---:|---:|")
foreach ($r in $runs) {
    $jsonPath = Join-Path $results "$($r.Label).json"
    $infoPath = Join-Path $results "$($r.Label).playback.txt"
    if (-not (Test-Path $jsonPath)) { $md += "| $($r.What) | (failed) | | | | | | |"; continue }
    $j = Get-Content $jsonPath -Raw | ConvertFrom-Json
    $groupName = if ($r.Kind -eq "app") { "youtube-client + mpv" } else { "Browser (bench prof.)" }
    $g = $j.groups.$groupName
    $desc = ""; $drops = ""; $played = ""
    if (Test-Path $infoPath) {
        $kv = @{}
        Get-Content $infoPath | ForEach-Object { $i = $_.IndexOf('='); if ($i -gt 0) { $kv[$_.Substring(0, $i)] = $_.Substring($i + 1) } }
        if ($r.Kind -eq "app") {
            $dec = if ($kv['hwdec'] -and $kv['hwdec'] -ne 'no') { "GPU ($($kv['hwdec']))" } else { "CPU" }
            $fps = ""; try { $fps = "@" + [math]::Round([double]::Parse($kv['fps'], $ci)) } catch {}
            $desc = "$($kv['codec']) $($kv['width'])x$($kv['height'])$fps, $dec"
            $drops = $kv['frame_drops']
            $played = $kv['played_seconds_during_measurement']
        } else {
            $s = $null; try { $s = $kv['stats'] | ConvertFrom-Json } catch {}
            $dec = if ($kv['kIsPlatformVideoDecoder'] -eq 'True' -or $kv['kIsPlatformVideoDecoder'] -eq 'true') { "GPU" } else { "CPU" }
            $fps = ""; if ($s.resolution -match '@(\d+)') { $fps = "@" + $Matches[1] }
            $desc = "$($s.codecs) $($s.videoWidth)x$($s.videoHeight)$fps, $dec ($($kv['kVideoDecoderName']))"
            $drops = "$($s.droppedFrames) of $($s.totalFrames)"
            $played = $kv['played_seconds_during_measurement']
        }
    }
    if ($g) {
        $md += [string]::Format($ci, "| {0} | {1} | {2:F0} | {3:F0} | {4:F1} | {5:F2} | {6} | {7} s |", $r.What, $desc, $g.privateMb, $g.workingSetMb, $g.cpuPercentOfAllCores, $g.coresUsed, $drops, $played)
    } else {
        $md += "| $($r.What) | $desc | (no process) | | | | | |"
    }
}
$md | Set-Content (Join-Path $results "codec_summary.md") -Encoding UTF8
$md | ForEach-Object { Write-Host $_ }
Write-Host "Done."
