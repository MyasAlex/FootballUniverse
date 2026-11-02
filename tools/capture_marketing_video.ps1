param(
 [string]$GameDirectory = 'dist/FootballUniverse',
 [string]$OutputDirectory = 'marketing/video-frames'
)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class VideoWin {
  [StructLayout(LayoutKind.Sequential)] public struct Rect {public int Left,Top,Right,Bottom;}
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out Rect r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint flags);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
  [DllImport("user32.dll",EntryPoint="SendMessageW",CharSet=CharSet.Unicode)]
  public static extern IntPtr SendMessageText(IntPtr h,uint m,IntPtr w,[MarshalAs(UnmanagedType.LPWStr)]string text);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int w,int ht,uint flags);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h,int id);
}
'@
$folder=(Resolve-Path $GameDirectory).Path
$out=[System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $out -Force | Out-Null
$exe=Join-Path $folder 'FootballUniverse.exe'
if(!(Test-Path $exe)){throw "EXE missing"}
$script:frame=0
$script:scenes=New-Object System.Collections.Generic.List[string]
$codec=[System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object {$_.MimeType -eq 'image/jpeg'} | Select-Object -First 1
$enc=New-Object System.Drawing.Imaging.EncoderParameters(1)
$enc.Param[0]=New-Object System.Drawing.Imaging.EncoderParameter([System.Drawing.Imaging.Encoder]::Quality,[long]84)
function Command([IntPtr]$h,[int]$id){
  [VideoWin]::SendMessage($h,0x111,[IntPtr]$id,[IntPtr]::Zero)|Out-Null
  Start-Sleep -Milliseconds 75
}
function Page([IntPtr]$h,[int]$idx){ Command $h (100+$idx) }
function Capture([IntPtr]$h,[string]$scene,[int]$frames){
  $r=New-Object VideoWin+Rect
  if(![VideoWin]::GetWindowRect($h,[ref]$r)){throw "Missing window"}
  $w=$r.Right-$r.Left; $ht=$r.Bottom-$r.Top
  if($w -lt 900 -or $ht -lt 680){throw "Window too small: $w x $ht"}
  Write-Host "Recording $scene / $frames frames @ $w x $ht"
  for($i=0;$i -lt $frames;$i++){
    $bitmap=New-Object System.Drawing.Bitmap($w,$ht,[System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $gr=[System.Drawing.Graphics]::FromImage($bitmap)
    try{
      $dc=$gr.GetHdc()
      try{$ok=[VideoWin]::PrintWindow($h,$dc,0)}finally{$gr.ReleaseHdc($dc)}
      if(!$ok){throw "PrintWindow failed"}
      $filename='frame_{0:D5}.jpg' -f $script:frame
      $bitmap.Save((Join-Path $out $filename),$codec,$enc)
      $script:scenes.Add("$($script:frame),$scene")
      $script:frame++
    }finally{$gr.Dispose();$bitmap.Dispose()}
    Start-Sleep -Milliseconds 110
  }
}
$proc=Start-Process -FilePath $exe -WorkingDirectory $folder -PassThru
try{
 $h=[IntPtr]::Zero
 for($i=0;$i -lt 50;$i++){
   Start-Sleep -Milliseconds 200
   $proc.Refresh()
   if($proc.HasExited){throw "Game exited: $($proc.ExitCode)"}
   $h=$proc.MainWindowHandle
   if($h -ne [IntPtr]::Zero){break}
 }
 if($h -eq [IntPtr]::Zero){throw "No Windows GUI"}
 [VideoWin]::SetWindowPos($h,[IntPtr]::Zero,10,10,1280,850,0x0004)|Out-Null
 [VideoWin]::SetForegroundWindow($h)|Out-Null
 Start-Sleep -Milliseconds 600
 Capture $h 'setup' 16
 $role=[VideoWin]::GetDlgItem($h,611)
 if($role -eq [IntPtr]::Zero){throw "Role selector missing"}
 [VideoWin]::SendMessage($role,0x14E,[IntPtr]4,[IntPtr]::Zero)|Out-Null
 Command $h 600
 Capture $h 'dashboard' 25
 Page $h 2
 Capture $h 'tactics' 26
 Page $h 6
 Capture $h 'transfers' 26
 Page $h 21
 $query=[VideoWin]::GetDlgItem($h,700)
 [VideoWin]::SendMessageText($query,0x000C,[IntPtr]::Zero,'Ronaldo')|Out-Null
 Command $h 705
 Capture $h 'world-database' 30
 Page $h 0
 for($i=0;$i -lt 6;$i++){Command $h 300}
 Page $h 4
 Command $h 300
 Command $h 301
 Capture $h 'live-match' 95
 Command $h 301
 Page $h 19
 Capture $h 'analytics' 25
 Page $h 20
 Capture $h 'board' 22
 $script:scenes | Set-Content -Path (Join-Path $out 'frame-scenes.csv') -Encoding Ascii
 Write-Host "VIDEO CAPTURE COMPLETE: $script:frame frames"
 if($script:frame -lt 250){throw "Insufficient actual UI frames"}
}finally{
 $proc.Refresh()
 if(!$proc.HasExited){Stop-Process -Id $proc.Id -Force}
}
