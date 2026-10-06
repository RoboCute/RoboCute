param([int]$X1 = 1100, [int]$Y1 = 700, [int]$X2 = 1400, [int]$Y2 = 560)
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
namespace DG { public class U {
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
}}
'@
[DG.U]::SetCursorPos($X1, $Y1) | Out-Null
Start-Sleep -Milliseconds 100
[DG.U]::mouse_event(0x02, 0, 0, 0, [UIntPtr]::Zero) # LEFTDown
for ($i = 1; $i -le 20; $i++) {
  $x = $X1 + [int](($X2 - $X1) * $i / 20)
  $y = $Y1 + [int](($Y2 - $Y1) * $i / 20)
  [DG.U]::SetCursorPos($x, $y) | Out-Null
  Start-Sleep -Milliseconds 15
  [DG.U]::mouse_event(0x01, 0, 0, 0, [UIntPtr]::Zero) # MOVE
}
[DG.U]::mouse_event(0x04, 0, 0, 0, [UIntPtr]::Zero) # LEFTUP
Write-Output "dragged $X1,$Y1 -> $X2,$Y2"
