Add-Type -TypeDefinition @'
using System; using System.Text; using System.Runtime.InteropServices; using System.Collections.Generic;
namespace LW {
  public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
  public class U {
    [DllImport("user32.dll")] public static extern bool EnumWindows(CB cb, IntPtr l); public delegate bool CB(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern int GetWindowTextW(IntPtr h, StringBuilder sb, int max);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  }
}
'@
$rows = @()
[LW.U]::EnumWindows({ param($h, $l)
  $sb = New-Object System.Text.StringBuilder 256
  [void][LW.U]::GetWindowTextW($h, $sb, 256)
  [void][LW.U]::GetWindowThreadProcessId($h, [ref]$wpid)
  $p = Get-Process -Id $wpid -ErrorAction SilentlyContinue
  if ($p -and $p.Name -eq 'electron') {
    $r = New-Object LW.RECT
    [void][LW.U]::GetWindowRect($h, [ref]$r)
    $script:rows += "hwnd=$h pid=$pid '$($sb.ToString())' rect=$($r.Left),$($r.Top) $($r.Right-$r.Left)x$($r.Bottom-$r.Top)"
  }
  return $true
}, [IntPtr]::Zero)
$rows | ForEach-Object { Write-Output $_ }
