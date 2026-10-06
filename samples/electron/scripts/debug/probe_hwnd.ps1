param([string]$Hex = "690D60")
$h = [IntPtr]::new([Convert]::ToInt64($Hex, 16))
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
namespace PB { public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
 public class U {
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint wpid);
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [DllImport("kernel32.dll")] public static extern uint GetLastError();
}}
'@
$is = [PB.U]::IsWindow($h)
$vis = [PB.U]::IsWindowVisible($h)
$r = New-Object PB.RECT
$ok = [PB.U]::GetWindowRect($h, [ref]$r)
$err = [PB.U]::GetLastError()
[void][PB.U]::GetWindowThreadProcessId($h, [ref]$wpid)
$par = [PB.U]::GetParent($h)
Write-Output "hwnd=$h isWindow=$is visible=$vis getRect=$ok err=$err rect=$($r.Left),$($r.Top) $($r.Right-$r.Left)x$($r.Bottom-$r.Top) pid=$wpid parent=$par"
