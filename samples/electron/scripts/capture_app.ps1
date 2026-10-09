param([string]$Out = "app_capture.png")
# Capture the WHOLE app window (found by title): raise it topmost, then grab its
# screen rect. The 3D viewport is part of the Chromium composition, so this
# also captures the shared-texture output.
$Out = [IO.Path]::GetFullPath((Join-Path (Get-Location) $Out))
$win = (Get-Process electron -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -like 'RoboCute*' } | Select-Object -First 1).MainWindowHandle
if (-not $win) { Write-Error 'RoboCute window not found'; exit 1 }
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
namespace CA { public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
  public class U {
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  }
}
'@
[CA.U]::SetWindowPos($win, [IntPtr]::new(-1), 0, 0, 0, 0, 0x0003 -bor 0x0040) | Out-Null
Start-Sleep -Milliseconds 500
$r = New-Object CA.RECT
[CA.U]::GetWindowRect($win, [ref]$r) | Out-Null
$w = $r.Right - $r.Left; $ht = $r.Bottom - $r.Top
Write-Output "app window rect $($r.Left),$($r.Top) ${w}x${ht}"
$bmp = New-Object System.Drawing.Bitmap $w, $ht
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Output "saved $Out"
