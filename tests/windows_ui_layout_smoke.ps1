param(
    [string]$GameDirectory = "dist/FootballUniverse"
)
$ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class FootballWindow {
  [StructLayout(LayoutKind.Sequential)] public struct Rect {
    public int Left, Top, Right, Bottom;
  }
  [StructLayout(LayoutKind.Sequential)] public struct Point {
    public int X, Y;
  }
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr parent, int id);
  [DllImport("user32.dll")] public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr previous, string className, string windowName);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out Rect result);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr window, out Rect result);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr window, ref Point result);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wp, IntPtr lp);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(
    IntPtr window, IntPtr insertAfter, int x, int y, int width, int height, uint flags);
}
'@
function Rect-Of([IntPtr]$control) {
  $rect = New-Object FootballWindow+Rect
  if(-not [FootballWindow]::GetWindowRect($control, [ref]$rect)){throw "GetWindowRect failed"}
  return $rect
}
function Intersect-Area($a, $b) {
  $w=[Math]::Max(0,[Math]::Min($a.Right,$b.Right)-[Math]::Max($a.Left,$b.Left))
  $h=[Math]::Max(0,[Math]::Min($a.Bottom,$b.Bottom)-[Math]::Max($a.Top,$b.Top))
  return ($w*$h)
}
function Check-Layout([IntPtr]$hwnd, [int]$page, [string]$size) {
  $content=[FootballWindow]::GetDlgItem($hwnd,502)
  $list=[FootballWindow]::GetDlgItem($hwnd,501)
  $input=[FootballWindow]::GetDlgItem($hwnd,503)
  $pitch=[FootballWindow]::FindWindowEx($hwnd,[IntPtr]::Zero,"FU_Pitch",$null)
  if($content -eq [IntPtr]::Zero -or $list -eq [IntPtr]::Zero -or $pitch -eq [IntPtr]::Zero){
    throw "Missing UI panels"
  }
  if(-not [FootballWindow]::IsWindowVisible($content)){
    throw "Main text panel invisible on page $page"
  }
  $client = New-Object FootballWindow+Rect
  [FootballWindow]::GetClientRect($hwnd,[ref]$client) | Out-Null
  $topLeft = New-Object FootballWindow+Point
  [FootballWindow]::ClientToScreen($hwnd,[ref]$topLeft) | Out-Null
  $actual = [FootballWindow+Rect]::new()
  $actual.Left=$topLeft.X; $actual.Top=$topLeft.Y
  $actual.Right=$topLeft.X+$client.Right; $actual.Bottom=$topLeft.Y+$client.Bottom
  $panels=@(@{name="main";handle=$content},@{name="list";handle=$list},
            @{name="input";handle=$input},@{name="pitch";handle=$pitch})
  $visible=@()
  foreach($p in $panels){
    if(-not [FootballWindow]::IsWindowVisible($p.handle)){continue}
    $rect=Rect-Of $p.handle
    if($rect.Left -lt $actual.Left -or $rect.Top -lt $actual.Top -or
       $rect.Right -gt $actual.Right -or $rect.Bottom -gt $actual.Bottom){
        throw "Panel $($p.name) outside client on page $page at $size"
    }
    $visible+=@{name=$p.name;rect=$rect}
  }
  for($i=0;$i -lt $visible.Count;$i++){
    for($j=$i+1;$j -lt $visible.Count;$j++){
      $area=Intersect-Area $visible[$i].rect $visible[$j].rect
      if($area -gt 2){
        throw "Overlap page $page at $size: $($visible[$i].name) vs $($visible[$j].name), area $area"
      }
    }
  }
}
$folder=(Resolve-Path $GameDirectory).Path
$exe=Join-Path $folder "FootballUniverse.exe"
$proc=Start-Process -FilePath $exe -WorkingDirectory $folder -PassThru
try {
  $hwnd=[IntPtr]::Zero
  for($i=0;$i -lt 40;$i++){
    Start-Sleep -Milliseconds 250
    $proc.Refresh()
    if($proc.HasExited){throw "Game exited before layout test"}
    $hwnd=$proc.MainWindowHandle
    if($hwnd -ne [IntPtr]::Zero){break}
  }
  if($hwnd -eq [IntPtr]::Zero){throw "Game main window not created"}
  # Start a new sandbox career so every tab can be visited without a save.
  [FootballWindow]::SendMessage($hwnd,0x111,[IntPtr]600,[IntPtr]::Zero) | Out-Null
  Start-Sleep -Milliseconds 250
  foreach($dimensions in @(@(920,750),@(1240,840),@(1600,960))){
    $width=$dimensions[0];$height=$dimensions[1]
    if(-not [FootballWindow]::SetWindowPos($hwnd,[IntPtr]::Zero,5,5,$width,$height,0x0004)){
       throw "Could not resize game"
    }
    Start-Sleep -Milliseconds 80
    foreach($page in @(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15)){
      [FootballWindow]::SendMessage($hwnd,0x111,[IntPtr](100+$page),[IntPtr]::Zero) | Out-Null
      Check-Layout $hwnd $page "$width x $height"
    }
  }
  Write-Host "PASS UI LAYOUT: 16 pages x 3 window sizes; no content/pitch/list overlap."
}
finally {
  $proc.Refresh()
  if(-not $proc.HasExited){Stop-Process -Id $proc.Id -Force}
}
