param([string]$Out = "viewport_capture.png", [string]$Log = "shared_run.txt")
# viewport hwnd comes from the engine log line "shared viewport hwnd=000000000044107A"
$logContent = Get-Content $Log -ErrorAction SilentlyContinue
$line = $logContent | Select-String -Pattern 'shared viewport hwnd=([0-9A-Fa-f]+)' | Select-Object -First 1
if (-not $line) { Write-Error "viewport hwnd not found in $Log"; exit 1 }
$hex = $line.Matches[0].Groups[1].Value
$h = [IntPtr]::new([Convert]::ToInt64($hex, 16))
Write-Output "viewport hwnd=$h"
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
namespace CR { public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
  public class U { [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r); } }
'@
# raise the parent browser window above the IDE so the capture sees it
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
namespace CR2 { public class U2 {
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
}}
'@
$parent = [CR2.U2]::GetParent($h)
[CR2.U2]::SetWindowPos($parent, [IntPtr]::new(-1), 0, 0, 0, 0, 0x0003 -bor 0x0040) | Out-Null  # HWND_TOPMOST, NOMOVE|NOSIZE|SHOWWINDOW
Start-Sleep -Milliseconds 600
$r = New-Object CR.RECT
[CR.U]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.Right - $r.Left; $ht = $r.Bottom - $r.Top
Write-Output "rect $($r.Left),$($r.Top) ${w}x${ht}"
if ($w -le 0 -or $ht -le 0) { Write-Error "empty rect"; exit 1 }
$bmp = New-Object System.Drawing.Bitmap $w, $ht
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Output "saved $Out"
