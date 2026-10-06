$logContent = Get-Content shared_run.txt
$line = $logContent | Select-String -Pattern 'shared viewport hwnd=([0-9A-Fa-f]+)' | Select-Object -First 1
$hex = $line.Matches[0].Groups[1].Value
$child = [IntPtr]::new([Convert]::ToInt64($hex, 16))
Add-Type -TypeDefinition @'
using System; using System.Text; using System.Runtime.InteropServices;
namespace TR {
  public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
  public class U {
    [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr h, uint cmd);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern int GetClassNameW(IntPtr h, StringBuilder sb, int max);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  }
}
'@
function DumpTree($h, $depth) {
  $r = New-Object TR.RECT
  [void][TR.U]::GetWindowRect($h, [ref]$r)
  $cls = New-Object System.Text.StringBuilder 128
  [void][TR.U]::GetClassNameW($h, $cls, 128)
  $vis = [TR.U]::IsWindowVisible($h)
  $mark = if ($h -eq $child) { "  <== OUR VIEWPORT" } else { "" }
  Write-Output ("{0}hwnd={1:X} '{2}' vis={3} {4},{5} {6}x{7}{8}" -f ('  ' * $depth), $h.ToInt64(), $cls.ToString(), $vis, $r.Left, $r.Top, ($r.Right-$r.Left), ($r.Bottom-$r.Top), $mark)
  $c = [TR.U]::GetWindow($h, 5) # GW_CHILD
  $n = 0
  while ($c -ne [IntPtr]::Zero -and $n -lt 20) {
    DumpTree $c ($depth + 1)
    $c = [TR.U]::GetWindow($c, 2) # GW_HWNDNEXT
    $n++
  }
}
$par = [TR.U]::GetParent($child)
DumpTree $par 0
