param([int]$W = 1600, [int]$H = 1000, [string]$Log = "app_run.txt")
# Resize the app window (via the native viewport hwnd from the engine log)
# to verify the viewport + swapchain follow the parent.
$logContent = Get-Content $Log -ErrorAction SilentlyContinue
$line = $logContent | Select-String -Pattern 'shared viewport hwnd=([0-9A-Fa-f]+)' | Select-Object -First 1
if (-not $line) { Write-Error "viewport hwnd not found in $Log"; exit 1 }
$hex = $line.Matches[0].Groups[1].Value
$vh = [IntPtr]::new([Convert]::ToInt64($hex, 16))
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
namespace RS { public class U {
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int ht, bool repaint);
}}
'@
$par = [RS.U]::GetParent($vh)
if ($par -eq [IntPtr]::Zero) { Write-Error "parent not found"; exit 1 }
[RS.U]::MoveWindow($par, 300, 200, $W, $H, $true) | Out-Null
Write-Output "parent resized to ${W}x${H}"
