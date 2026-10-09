param([string]$Key = "S")
# Focus the app window (found by title) and send one keystroke (UI shortcut tests).
$win = (Get-Process electron -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -like 'RoboCute*' } | Select-Object -First 1).MainWindowHandle
if (-not $win) { Write-Error 'RoboCute window not found'; exit 1 }
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
namespace SK { public class U {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
}}
'@
[SK.U]::SetWindowPos($win, [IntPtr]::new(-1), 0, 0, 0, 0, 0x0003 -bor 0x0040) | Out-Null
[SK.U]::SetForegroundWindow($win) | Out-Null
Start-Sleep -Milliseconds 400
$vk = [byte]([char]$Key.ToUpper())
[SK.U]::keybd_event($vk, 0, 0, [UIntPtr]::Zero)          # key down
Start-Sleep -Milliseconds 60
[SK.U]::keybd_event($vk, 0, 0x0002, [UIntPtr]::Zero)      # key up
Write-Output "sent key $Key to app window"
