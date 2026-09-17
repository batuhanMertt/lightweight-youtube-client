# Saves a PNG of the youtube-client window (for the README).
# Usage: powershell -ExecutionPolicy Bypass -File tools\screenshot.ps1 -Name home -Delay 5
param(
    [string]$Name = "screenshot",
    [int]$Delay = 5
)

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr hwnd, int attr, out RECT rect, int size);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);
}
"@
[void][Win]::SetProcessDPIAware()

Write-Host "Bring the YouTube client window to the front. Capturing in $Delay s..."
Start-Sleep -Seconds $Delay

$proc = Get-Process -Name "youtube-client" -ErrorAction SilentlyContinue |
    Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $proc) { Write-Host "youtube-client window not found"; exit 1 }

# Capture the window's own content with PrintWindow, so other windows on top of it
# (chat apps, notifications, ...) don't end up in the image.
$hwnd = $proc.MainWindowHandle
$full = New-Object Win+RECT
[void][Win]::GetWindowRect($hwnd, [ref]$full)
$fw = $full.Right - $full.Left
$fh = $full.Bottom - $full.Top

$bmp = New-Object System.Drawing.Bitmap $fw, $fh
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
$ok = [Win]::PrintWindow($hwnd, $hdc, 2)   # 2 = PW_RENDERFULLCONTENT
$g.ReleaseHdc($hdc)
$g.Dispose()
if (-not $ok) { Write-Host "PrintWindow failed"; exit 1 }

# The app isn't DPI-aware, so on scaled displays PrintWindow draws it at its logical size
# inside a larger (physical-size) bitmap. Trim the unused (transparent/white) area on the right/bottom.
function Test-WhiteColumn($bmp, $x) {
    for ($y = 0; $y -lt $bmp.Height; $y += 8) {
        $c = $bmp.GetPixel($x, $y)
        if ($c.A -gt 10 -and ($c.R -lt 250 -or $c.G -lt 250 -or $c.B -lt 250)) { return $false }
    }
    return $true
}
function Test-WhiteRow($bmp, $y, $maxX) {
    for ($x = 0; $x -lt $maxX; $x += 8) {
        $c = $bmp.GetPixel($x, $y)
        if ($c.A -gt 10 -and ($c.R -lt 250 -or $c.G -lt 250 -or $c.B -lt 250)) { return $false }
    }
    return $true
}
$cw = $bmp.Width
while ($cw -gt 1 -and (Test-WhiteColumn $bmp ($cw - 1))) { $cw-- }
$ch = $bmp.Height
while ($ch -gt 1 -and (Test-WhiteRow $bmp ($ch - 1) $cw)) { $ch-- }
if ($cw -lt $bmp.Width -or $ch -lt $bmp.Height) {
    $crop = $bmp.Clone((New-Object System.Drawing.Rectangle 0, 0, $cw, $ch), $bmp.PixelFormat)
    $bmp.Dispose()
    $bmp = $crop
}
$w = $bmp.Width
$h = $bmp.Height

$outDir = Join-Path (Split-Path $PSScriptRoot -Parent) "docs\images"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$out = Join-Path $outDir "$Name.png"
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Host "Saved $out ($w x $h)"
