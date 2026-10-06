param([string]$Out = "app_capture.png", [string]$Log = "app_run.txt")
# Capture the WHOLE app window: find the native viewport hwnd in the engine
# log, take its parent (the browser window), raise it, capture its rect.
$logContent = Get-Content $Log -ErrorAction SilentlyContinue
$line = $logContent | Select-String -Pattern 'shared viewport hwnd=([0-9A-Fa-f]+)' | Select-Object -First 1
if (-not $line) { Write-Error "viewport hwnd not found in $Log"; exit 1 }
$hex = $line.Matches[0].Groups[1].Value
$h = [IntPtr]::new([Convert]::ToInt64($hex, 16))
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
namespace CA { public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
  public class U {
    [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  }
}
'@
$parent = [CA.U]::GetParent($h)
[CA.U]::SetWindowPos($parent, [IntPtr]::new(-1), 0, 0, 0, 0, 0x0003 -bor 0x0040) | Out-Null
Start-Sleep -Milliseconds 500
$r = New-Object CA.RECT
[CA.U]::GetWindowRect($parent, [ref]$r) | Out-Null
$w = $r.Right - $r.Left; $ht = $r.Bottom - $r.Top
Write-Output "app window rect $($r.Left),$($r.Top) ${w}x${ht}"
$bmp = New-Object System.Drawing.Bitmap $w, $ht
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Output "saved $Out"
