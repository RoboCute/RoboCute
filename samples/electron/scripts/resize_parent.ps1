param([int]$W = 1600, [int]$H = 1000)
# Resize the app window (found by title) to verify the viewport -> engine
# resize -> new surface epoch path.
$win = (Get-Process electron -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -like 'RoboCute*' } | Select-Object -First 1).MainWindowHandle
if (-not $win) { Write-Error 'RoboCute window not found'; exit 1 }
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
namespace RS { public class U {
  [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int ht, bool repaint);
}}
'@
[RS.U]::MoveWindow($win, 300, 200, $W, $H, $true) | Out-Null
Write-Output "app window resized to ${W}x${H}"
