param([string]$Key = "S", [string]$Log = "app_run.txt")
# Focus the app window and send one keystroke (for UI shortcut tests).
$logContent = Get-Content $Log -ErrorAction SilentlyContinue
$line = $logContent | Select-String -Pattern 'shared viewport hwnd=([0-9A-Fa-f]+)' | Select-Object -First 1
if (-not $line) { Write-Error "viewport hwnd not found in $Log"; exit 1 }
$hex = $line.Matches[0].Groups[1].Value
$h = [IntPtr]::new([Convert]::ToInt64($hex, 16))
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
namespace SK { public class U {
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
}}
'@
$parent = [SK.U]::GetParent($h)
[SK.U]::SetWindowPos($parent, [IntPtr]::new(-1), 0, 0, 0, 0, 0x0003 -bor 0x0040) | Out-Null
[SK.U]::SetForegroundWindow($parent) | Out-Null
Start-Sleep -Milliseconds 400
$vk = [byte]([char]$Key.ToUpper())
[SK.U]::keybd_event($vk, 0, 0, [UIntPtr]::Zero)          # key down
Start-Sleep -Milliseconds 60
[SK.U]::keybd_event($vk, 0, 0x0002, [UIntPtr]::Zero)      # key up
Write-Output "sent key $Key to app window"
