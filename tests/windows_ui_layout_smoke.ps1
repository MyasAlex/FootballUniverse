param([string]$GameDirectory = 'dist/FootballUniverse')
$ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class FootballWindow {
  [StructLayout(LayoutKind.Sequential)] public struct Rect {public int Left, Top, Right, Bottom;}
  [StructLayout(LayoutKind.Sequential)] public struct Point {public int X, Y;}
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr parent, int id);
  [DllImport("user32.dll")] public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr previous, string cls, string name);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr wnd);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr wnd, out Rect r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr wnd, out Rect r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr wnd, ref Point p);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr wnd, uint message, IntPtr wp, IntPtr lp);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr wnd, IntPtr insert, int x,int y,int w,int h,uint flags);
}
'@
function Rect-Of([IntPtr]$handle){
  $r=New-Object FootballWindow+Rect
  if(-not [FootballWindow]::GetWindowRect($handle,[ref]$r)){throw 'GetWindowRect failed'}
  return $r
}
function Overlap($a,$b){
  [Math]::Max(0,[Math]::Min($a.Right,$b.Right)-[Math]::Max($a.Left,$b.Left))*
  [Math]::Max(0,[Math]::Min($a.Bottom,$b.Bottom)-[Math]::Max($a.Top,$b.Top))
}
function Check-Layout([IntPtr]$win,[int]$page,[string]$size){
  $content=[FootballWindow]::GetDlgItem($win,502)
  $list=[FootballWindow]::GetDlgItem($win,501)
  $input=[FootballWindow]::GetDlgItem($win,503)
  $pitch=[FootballWindow]::FindWindowEx($win,[IntPtr]::Zero,'FU_Pitch',$null)
  if($content -eq [IntPtr]::Zero -or $list -eq [IntPtr]::Zero -or $pitch -eq [IntPtr]::Zero){throw 'Missing panels'}
  if(-not [FootballWindow]::IsWindowVisible($content)){throw "Text hidden: page $page"}
  $client=New-Object FootballWindow+Rect
  [FootballWindow]::GetClientRect($win,[ref]$client)|Out-Null
  $origin=New-Object FootballWindow+Point
  [FootballWindow]::ClientToScreen($win,[ref]$origin)|Out-Null
  $left=$origin.X;$top=$origin.Y;$right=$left+$client.Right;$bottom=$top+$client.Bottom
  $controls=@(@{name='text';h=$content},@{name='list';h=$list},@{name='input';h=$input},@{name='pitch';h=$pitch})
  if($page -eq 6 -or $page -eq 21){
    foreach($id in @(700,701,702,703,704,705,706,707)){$controls+=@{name="filter$id";h=[FootballWindow]::GetDlgItem($win,$id)}}
  }
  $visible=@()
  foreach($entry in $controls){
    if(-not [FootballWindow]::IsWindowVisible($entry.h)){continue}
    $r=Rect-Of $entry.h
    if($r.Left -lt $left -or $r.Top -lt $top -or $r.Right -gt $right -or $r.Bottom -gt $bottom){
      throw "Panel $($entry.name) outside window on page $page at $size"
    }
    $visible+=@{name=$entry.name;r=$r}
  }
  for($i=0;$i -lt $visible.Count;$i++){
    for($j=$i+1;$j -lt $visible.Count;$j++){
      if((Overlap $visible[$i].r $visible[$j].r) -gt 2){
        throw "Overlap page $page at ${size}: $($visible[$i].name) and $($visible[$j].name)"
      }
    }
  }
}
$folder=(Resolve-Path $GameDirectory).Path
$exe=Join-Path $folder 'FootballUniverse.exe'
$proc=Start-Process -FilePath $exe -WorkingDirectory $folder -PassThru
try{
  $hwnd=[IntPtr]::Zero
  for($i=0;$i -lt 40;$i++){
    Start-Sleep -Milliseconds 250
    $proc.Refresh();if($proc.HasExited){throw 'Game exited before layout test'}
    $hwnd=$proc.MainWindowHandle;if($hwnd -ne [IntPtr]::Zero){break}
  }
  if($hwnd -eq [IntPtr]::Zero){throw 'Main window did not open'}
  # sandbox role exposes every screen and every text panel in 0.15.0
  $combo=[FootballWindow]::GetDlgItem($hwnd,611)
  [FootballWindow]::SendMessage($combo,0x14E,[IntPtr]4,[IntPtr]::Zero)|Out-Null
  [FootballWindow]::SendMessage($hwnd,0x111,[IntPtr]600,[IntPtr]::Zero)|Out-Null
  Start-Sleep -Milliseconds 300
  foreach($dim in @(@(950,760),@(1240,840),@(1600,960))){
    $width=$dim[0];$height=$dim[1]
    if(-not [FootballWindow]::SetWindowPos($hwnd,[IntPtr]::Zero,5,5,$width,$height,0x0004)){
      throw 'Could not resize game'
    }
    Start-Sleep -Milliseconds 100
    foreach($page in 0..21){
      [FootballWindow]::SendMessage($hwnd,0x111,[IntPtr](100+$page),[IntPtr]::Zero)|Out-Null
      Check-Layout $hwnd $page "$width x $height"
    }
  }
  Write-Host 'PASS: 22 screens x 3 sizes, including market filters and pitch; no text-panel overlap'
}finally{
  $proc.Refresh()
  if(-not $proc.HasExited){Stop-Process -Id $proc.Id -Force}
}
