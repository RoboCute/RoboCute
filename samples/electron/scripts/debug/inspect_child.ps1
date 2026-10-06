$logContent = Get-Content shared_run.txt
$line = $logContent | Select-String -Pattern 'shared viewport hwnd=([0-9A-Fa-f]+)' | Select-Object -First 1
$hex = $line.Matches[0].Groups[1].Value
$h = [IntPtr]::new([Convert]::ToInt64($hex, 16))
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
namespace IC {
  public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
  public struct POINT { public int X; public int Y; }
  public class U {
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
    [DllImport("user32.dll")] public static extern uint GetWindowLongW(IntPtr h, int idx);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern int MapWindowPoints(IntPtr from, IntPtr to, ref POINT p, int n);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern int GetWindowTextW(IntPtr h, System.Text.StringBuilder sb, int max);
    [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr h, uint cmd);
  }
}
'@
$vis = [IC.U]::IsWindowVisible($h)
$par = [IC.U]::GetParent($h)
$style = [IC.U]::GetWindowLongW($h, -16)
$exstyle = [IC.U]::GetWindowLongW($h, -20)
$sb = New-Object System.Text.StringBuilder 256
[void][IC.U]::GetWindowTextW($par, $sb, 256)
$sbTitle = New-Object System.Text.StringBuilder 256
[void][IC.U]::GetWindowTextW($h, $sbTitle, 256)
$cr = New-Object IC.RECT
[void][IC.U]::GetClientRect($par, [ref]$cr)
Write-Output ("child hwnd={0:X} vis={1} style=0x{2:X} ex=0x{3:X}" -f $h.ToInt64(), $vis, $style, $exstyle)
Write-Output ("parent hwnd={0:X} title='{1}' client={2}x{3}" -f $par.ToInt64(), $sb.ToString(), ($cr.Right-$cr.Left), ($cr.Bottom-$cr.Top))
Write-Output ("child title='{0}'" -f $sbTitle.ToString())
# owner (root) window: walk up
$root = $par
while ($true) {
  $up = [IC.U]::GetParent($root)
  if ($up -eq [IntPtr]::Zero) { break }
  $root = $up
}
$rr = New-Object IC.RECT
[void][IC.U]::GetWindowRect($root, [ref]$rr)
Write-Output ("root hwnd={0:X} rect={1},{2} {3}x{4}" -f $root.ToInt64(), $rr.Left, $rr.Top, ($rr.Right-$rr.Left), ($rr.Bottom-$rr.Top))
