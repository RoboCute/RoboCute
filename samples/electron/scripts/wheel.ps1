param([int]$X = 1500, [int]$Y = 700, [int]$Notches = 4, [int]$Delta = -120)
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
namespace WH { public class U {
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
}}
'@
[WH.U]::SetCursorPos($X, $Y) | Out-Null
Start-Sleep -Milliseconds 100
$u = [BitConverter]::ToUInt32([BitConverter]::GetBytes([int32]$Delta), 0)
1..$Notches | ForEach-Object {
  [WH.U]::mouse_event(0x0800, 0, 0, $u, [UIntPtr]::Zero) # WHEEL
  Start-Sleep -Milliseconds 80
}
Write-Output "wheeled $Notches notches (delta=$Delta) at $X,$Y"
