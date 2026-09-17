# Opens the same video in an isolated Chrome profile, forces 720p, measures it, and records the
# codec / resolution / decoder Chrome really used (via the DevTools protocol), then closes Chrome.
# Usage: bench_browser.ps1 -Label chrome_hw            (hardware video decoding, Chrome default)
#        bench_browser.ps1 -Label chrome_sw -Software  (--disable-accelerated-video-decode)
param(
    [Parameter(Mandatory = $true)][string]$Label,
    [switch]$Software,
    [string]$VideoId = "V5Asw04DsD8",
    [int]$Warmup = 30,
    [int]$Seconds = 30,
    [int]$Port = 9223,
    [string]$OutDir = ""
)

$results = if ($OutDir) { $OutDir } else { Join-Path $PSScriptRoot "results" }
New-Item -ItemType Directory -Force -Path $results | Out-Null
$profileDir = Join-Path $env:TEMP "yt-bench-profile"

function Get-BenchChromeProcs {
    Get-CimInstance Win32_Process -Filter "Name='chrome.exe' OR Name='msedge.exe'" |
        Where-Object { $_.CommandLine -like '*yt-bench-profile*' }
}
function Stop-BenchChrome {
    Get-BenchChromeProcs | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    Start-Sleep -Seconds 2
}

# ---------------- minimal DevTools protocol client ----------------
$script:ws = $null
$script:pending = $null
$script:buf = New-Object byte[] 1048576
$script:nextId = 1

function Connect-Cdp {
    for ($i = 0; $i -lt 20; $i++) {
        try {
            $targets = Invoke-RestMethod "http://127.0.0.1:$Port/json" -TimeoutSec 3
            $page = $targets | Where-Object { $_.type -eq 'page' -and $_.url -like '*youtube.com/watch*' } | Select-Object -First 1
            if ($page) { break }
        } catch {}
        Start-Sleep -Seconds 1
    }
    if (-not $page) { throw "YouTube tab not found on DevTools port $Port" }
    $script:ws = New-Object System.Net.WebSockets.ClientWebSocket
    $script:ws.ConnectAsync([Uri]$page.webSocketDebuggerUrl, [Threading.CancellationToken]::None).Wait()
}

function Receive-CdpMessage([int]$TimeoutMs) {
    $ms = New-Object IO.MemoryStream
    while ($true) {
        if (-not $script:pending) {
            $seg = New-Object ArraySegment[byte] -ArgumentList @(, $script:buf)
            $script:pending = $script:ws.ReceiveAsync($seg, [Threading.CancellationToken]::None)
        }
        if (-not $script:pending.Wait($TimeoutMs)) { return $null }
        $r = $script:pending.Result
        $script:pending = $null
        $ms.Write($script:buf, 0, $r.Count)
        if ($r.EndOfMessage) { break }
    }
    return [Text.Encoding]::UTF8.GetString($ms.ToArray()) | ConvertFrom-Json
}

function Invoke-Cdp([string]$Method, $Params = @{}, [int]$TimeoutMs = 10000, [ref]$Events) {
    $id = $script:nextId++
    $json = @{ id = $id; method = $Method; params = $Params } | ConvertTo-Json -Depth 10 -Compress
    $bytes = [Text.Encoding]::UTF8.GetBytes($json)
    $seg = New-Object ArraySegment[byte] -ArgumentList @(, $bytes)
    $script:ws.SendAsync($seg, [Net.WebSockets.WebSocketMessageType]::Text, $true, [Threading.CancellationToken]::None).Wait()
    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    while ((Get-Date) -lt $deadline) {
        $msg = Receive-CdpMessage 500
        if (-not $msg) { continue }
        if ($msg.id -eq $id) { return $msg }
        if ($Events -and $msg.method) { $Events.Value += $msg }
    }
    return $null
}

function Invoke-Js([string]$Expression) {
    $r = Invoke-Cdp "Runtime.evaluate" @{ expression = $Expression; returnByValue = $true; awaitPromise = $true }
    if ($r -and $r.result -and $r.result.result) { return $r.result.result.value }
    return $null
}

$statsJs = @'
(() => {
  const p = document.getElementById('movie_player');
  const v = document.querySelector('video');
  let s = {};
  try { s = p.getStatsForNerds() || {}; } catch (e) {}
  const q = v && v.getVideoPlaybackQuality ? v.getVideoPlaybackQuality() : {};
  return JSON.stringify({
    adShowing: !!document.querySelector('#movie_player.ad-showing'),
    codecs: s.codecs, resolution: s.resolution, quality: p && p.getPlaybackQuality ? p.getPlaybackQuality() : null,
    videoWidth: v ? v.videoWidth : 0, videoHeight: v ? v.videoHeight : 0,
    paused: v ? v.paused : null, currentTime: v ? v.currentTime : null,
    droppedFrames: q.droppedVideoFrames, totalFrames: q.totalVideoFrames
  });
})()
'@
$forceJs = @'
(() => {
  const p = document.getElementById('movie_player');
  const v = document.querySelector('video');
  try { p.setPlaybackQualityRange('hd720', 'hd720'); } catch (e) {}
  try { if (v && v.paused) v.play(); } catch (e) {}
  return 'ok';
})()
'@

# ---------------- run ----------------
Stop-BenchChrome
$chrome = Join-Path $env:ProgramFiles "Google\Chrome\Application\chrome.exe"
if (-not (Test-Path $chrome)) { throw "Chrome not found at $chrome" }

$chromeArgs = @(
    "--user-data-dir=`"$profileDir`"", "--no-first-run", "--no-default-browser-check",
    "--autoplay-policy=no-user-gesture-required", "--remote-debugging-port=$Port",
    "--new-window", "--window-size=1040,760"
)
if ($Software) { $chromeArgs += "--disable-accelerated-video-decode" }
$chromeArgs += "https://www.youtube.com/watch?v=$VideoId"

Write-Host "[$Label] starting Chrome (software decode: $Software)"
Start-Process -FilePath $chrome -ArgumentList $chromeArgs | Out-Null
Start-Sleep -Seconds 8

Connect-Cdp
$events = @()
[void](Invoke-Cdp "Media.enable" @{} 5000 ([ref]$events))
# Reload so the media player is created after Media.enable and its decoder events reach us.
[void](Invoke-Cdp "Page.reload" @{} 5000 ([ref]$events))
Start-Sleep -Seconds 5
[void](Invoke-Js $forceJs)

# Wait for warm-up; keep forcing 720p (YouTube may reset it when an ad finishes).
$end = (Get-Date).AddSeconds($Warmup)
while ((Get-Date) -lt $end) {
    $m = Receive-CdpMessage 1000
    if ($m -and $m.method) { $events += $m }
    if (((Get-Date) - $end).TotalSeconds -gt -3) { [void](Invoke-Js $forceJs) }
}
# If an ad is still showing, wait up to 60 s more.
for ($i = 0; $i -lt 30; $i++) {
    $st = $null
    try { $st = Invoke-Js $statsJs | ConvertFrom-Json } catch {}
    if ($st -and -not $st.adShowing) { break }
    Start-Sleep -Seconds 2
}
[void](Invoke-Js $forceJs)
Start-Sleep -Seconds 5

$before = $null
try { $before = Invoke-Js $statsJs | ConvertFrom-Json } catch {}
& (Join-Path $PSScriptRoot "measure.ps1") -Label $Label -Seconds $Seconds -Delay 0 -OutDir $results

# Collect what Chrome decoded
$stats = Invoke-Js $statsJs
$after = $null
try { $after = $stats | ConvertFrom-Json } catch {}
$deadline = (Get-Date).AddSeconds(3)
while ((Get-Date) -lt $deadline) { $m = Receive-CdpMessage 500; if ($m -and $m.method) { $events += $m } }
$decoder = @{}
foreach ($e in $events) {
    if ($e.method -eq 'Media.playerPropertiesChanged') {
        foreach ($prop in $e.params.properties) {
            if ($prop.name -in @('kVideoDecoderName', 'kIsPlatformVideoDecoder', 'kVideoTracks', 'kFrameUrl')) {
                $decoder[$prop.name] = $prop.value
            }
        }
    }
}

$out = @()
$out += "stats=$stats"
foreach ($k in $decoder.Keys) { $out += "$k=$($decoder[$k])" }
$out += "software_flag=$Software"
if ($before -and $after) { $out += "played_seconds_during_measurement=$([math]::Round($after.currentTime - $before.currentTime, 1))" }
$out | Set-Content (Join-Path $results "$Label.playback.txt") -Encoding UTF8
$out | ForEach-Object { Write-Host "  $_" }

try { $script:ws.Dispose() } catch {}
Stop-BenchChrome
