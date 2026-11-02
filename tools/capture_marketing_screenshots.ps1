param(
  [string]$GameDirectory = 'dist/FootballUniverse',
  [string]$OutputDirectory = 'marketing/screenshots'
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class MarketingCapture {
  [StructLayout(LayoutKind.Sequential)]
  public struct Rect { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr hwnd, int id);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
  [DllImport("user32.dll", EntryPoint="SendMessageW", CharSet=CharSet.Unicode)]
  public static extern IntPtr SendMessageText(IntPtr hwnd, uint msg, IntPtr wp, [MarshalAs(UnmanagedType.LPWStr)] string text);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hwnd, IntPtr after, int x, int y, int w, int h, uint flags);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out Rect rect);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool RedrawWindow(IntPtr hwnd, IntPtr rc, IntPtr rgn, uint flags);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
  [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index);
}
'@

$folder = (Resolve-Path $GameDirectory).Path
$out = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $out -Force | Out-Null
$exe = Join-Path $folder 'FootballUniverse.exe'
if (-not (Test-Path $exe)) { throw "EXE missing: $exe" }

function Refresh-Window([IntPtr]$hwnd) {
  [MarketingCapture]::RedrawWindow($hwnd, [IntPtr]::Zero, [IntPtr]::Zero, 0x0185) | Out-Null
  Start-Sleep -Milliseconds 550
}
function Switch-Page([IntPtr]$hwnd, [int]$page) {
  [MarketingCapture]::SendMessage($hwnd, 0x111, [IntPtr](100+$page), [IntPtr]::Zero) | Out-Null
  Refresh-Window $hwnd
}
function Take-Snapshot([IntPtr]$hwnd, [string]$name) {
  Refresh-Window $hwnd
  $r = New-Object MarketingCapture+Rect
  if (-not [MarketingCapture]::GetWindowRect($hwnd, [ref]$r)) {
    throw "GetWindowRect failed for $name"
  }
  $w = $r.Right-$r.Left
  $h = $r.Bottom-$r.Top
  if ($w -lt 950 -or $h -lt 720) { throw "Window dimensions invalid: $w x $h" }

  $bitmap = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $gfx = [System.Drawing.Graphics]::FromImage($bitmap)
  try {
    $hdc = $gfx.GetHdc()
    try {
      $ok = [MarketingCapture]::PrintWindow($hwnd, $hdc, 0)
    } finally {
      $gfx.ReleaseHdc($hdc)
    }
    if (-not $ok) { throw "PrintWindow returned failure for $name" }
    $path = Join-Path $out "$name.png"
    $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Host "Captured $path ($w x $h, $((Get-Item $path).Length) bytes)"
  } finally {
    $gfx.Dispose()
    $bitmap.Dispose()
  }

  # A second, full desktop capture is useful when PrintWindow omits custom child controls.
  $screenW=[MarketingCapture]::GetSystemMetrics(0)
  $screenH=[MarketingCapture]::GetSystemMetrics(1)
  if ($r.Left -ge 0 -and $r.Top -ge 0 -and $r.Right -le $screenW -and $r.Bottom -le $screenH) {
    $screen = New-Object System.Drawing.Bitmap($w, $h)
    $gr = [System.Drawing.Graphics]::FromImage($screen)
    try {
      $gr.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size($w, $h)))
      $screen.Save((Join-Path $out "$name-desktop.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
      $gr.Dispose()
      $screen.Dispose()
    }
  }
}

Write-Host "Desktop resolution: $([MarketingCapture]::GetSystemMetrics(0)) x $([MarketingCapture]::GetSystemMetrics(1))"
$proc = Start-Process -FilePath $exe -WorkingDirectory $folder -PassThru
try {
  $hwnd = [IntPtr]::Zero
  for ($i=0; $i -lt 50; $i++) {
    Start-Sleep -Milliseconds 240
    $proc.Refresh()
    if ($proc.HasExited) { throw "Football Universe exited at startup: $($proc.ExitCode)" }
    $hwnd=$proc.MainWindowHandle
    if ($hwnd -ne [IntPtr]::Zero) { break }
  }
  if ($hwnd -eq [IntPtr]::Zero) { throw 'Main window never appeared' }
  [MarketingCapture]::SetForegroundWindow($hwnd) | Out-Null
  [MarketingCapture]::SetWindowPos($hwnd, [IntPtr]::Zero, 10, 10, 1280, 850, 0x0004) | Out-Null
  Refresh-Window $hwnd

  Take-Snapshot $hwnd '01-career-setup'
  $role=[MarketingCapture]::GetDlgItem($hwnd,611)
  if ($role -eq [IntPtr]::Zero) { throw 'Role selector missing' }
  [MarketingCapture]::SendMessage($role, 0x14E, [IntPtr]4, [IntPtr]::Zero) | Out-Null
  [MarketingCapture]::SendMessage($hwnd, 0x111, [IntPtr]600, [IntPtr]::Zero) | Out-Null
  Start-Sleep -Milliseconds 900

  Switch-Page $hwnd 0
  Take-Snapshot $hwnd '02-club-dashboard'
  Switch-Page $hwnd 2
  Take-Snapshot $hwnd '03-tactics'
  Switch-Page $hwnd 6
  Take-Snapshot $hwnd '04-transfer-market'
  Switch-Page $hwnd 19
  Take-Snapshot $hwnd '05-analytics'
  Switch-Page $hwnd 21
  # Show a recognizable authentic catalogue identity rather than an obscure first entry.
  $query=[MarketingCapture]::GetDlgItem($hwnd,700)
  [MarketingCapture]::SendMessageText($query, 0x000C, [IntPtr]::Zero, 'Ronaldo') | Out-Null
  [MarketingCapture]::SendMessage($hwnd, 0x111, [IntPtr]705, [IntPtr]::Zero) | Out-Null
  $list=[MarketingCapture]::GetDlgItem($hwnd,501)
  [MarketingCapture]::SendMessage($list, 0x0186, [IntPtr]0, [IntPtr]::Zero) | Out-Null
  [MarketingCapture]::SendMessage($hwnd, 0x111, [IntPtr](501 + 65536), [IntPtr]::Zero) | Out-Null
  Take-Snapshot $hwnd '06-global-players'

  # Advance to the first fixture. Game.advance() stops at match day,
  # so extra calls do not skip over the match.
  Switch-Page $hwnd 0
  for($i=0;$i -lt 6;$i++){
    [MarketingCapture]::SendMessage($hwnd, 0x111, [IntPtr]300, [IntPtr]::Zero) | Out-Null
  }
  Switch-Page $hwnd 4
  [MarketingCapture]::SendMessage($hwnd, 0x111, [IntPtr]300, [IntPtr]::Zero) | Out-Null
  Start-Sleep -Milliseconds 350
  [MarketingCapture]::SendMessage($hwnd, 0x111, [IntPtr]300, [IntPtr]::Zero) | Out-Null
  Start-Sleep -Milliseconds 350
  [MarketingCapture]::SendMessage($hwnd, 0x111, [IntPtr]300, [IntPtr]::Zero) | Out-Null
  Start-Sleep -Milliseconds 350
  Take-Snapshot $hwnd '07-live-match'
  $count = @(Get-ChildItem $out -Filter '*.png' | Where-Object { $_.Name -notlike '*-desktop*' }).Count
  if ($count -ne 7) { throw "Expected 7 screenshots, got $count" }
  Write-Host "PASS: $count real GUI screenshots captured from Windows game."
} finally {
  $proc.Refresh()
  if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
}
