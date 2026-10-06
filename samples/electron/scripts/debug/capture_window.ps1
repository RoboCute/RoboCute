param([string]$Title = "RoboCute", [string]$Out = "shared_capture.png")
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Collections.Generic;
namespace CapW {
  public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
  public class U32 {
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern int GetWindowTextW(IntPtr h, StringBuilder sb, int max);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    public static List<IntPtr> FindByTitle(string title, uint[] pids) {
      var hits = new List<IntPtr>();
      EnumWindows((h, l) => {
        var sb = new StringBuilder(256);
        GetWindowTextW(h, sb, 256);
        GetWindowThreadProcessId(h, out uint pid);
        foreach (var p in pids)
          if (p == pid && IsWindowVisible(h) && sb.Length > 0 && sb.ToString().Contains(title)) { hits.Add(h); }
        return true;
      }, IntPtr.Zero);
      return hits;
    }
  }
}
'@
$pids = @((Get-Process electron).Id)
$hits = [CapW.U32]::FindByTitle($Title, $pids)
if ($hits.Count -eq 0) { Write-Error "window not found"; exit 1 }
$h = $hits[0]
$r = New-Object CapW.RECT
[CapW.U32]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.Right - $r.Left; $ht = $r.Bottom - $r.Top
Write-Output "window #$h ${w}x${ht} at $($r.Left),$($r.Top)"
$bmp = New-Object System.Drawing.Bitmap $w, $ht
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Output "saved $Out"
