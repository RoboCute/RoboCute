param([string]$Pattern = "engine-host", [int]$Seconds = 10)
$procs = Get-CimInstance Win32_Process -Filter "name='electron.exe' or name='node.exe'" | Where-Object { $_.CommandLine -like "*$Pattern*" }
if (-not $procs) { Write-Error "no process matching $Pattern"; exit 1 }
$pid_ = $procs[0].ProcessId
$p1 = Get-Process -Id $pid_
$c1 = $p1.CPU; $t1 = Get-Date
Start-Sleep -Seconds $Seconds
$p2 = Get-Process -Id $pid_ -ErrorAction SilentlyContinue
if (-not $p2) { Write-Output "pid $pid_ died during sampling"; exit 0 }
$c2 = $p2.CPU; $t2 = Get-Date
$dt = ($t2 - $t1).TotalSeconds
$cores = [Environment]::ProcessorCount
Write-Output ("pid {0}: CPU {1:N1}% of one core over {2}s ({3:N1}% of {4} cores), WS {5} MB" -f $pid_, (($c2-$c1)/$dt*100), $dt, (($c2-$c1)/$dt*100/$cores), $cores, [int]($p2.WorkingSet64/1MB))
