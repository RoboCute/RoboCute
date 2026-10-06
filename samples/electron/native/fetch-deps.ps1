# Fetch Node.js headers for the rbc_ext_node xmake target.
# The addon only needs the N-API / v8 headers to COMPILE; linking uses the
# tracked import library deps/node.lib (N-API is ABI-stable across
# Node.js / Electron).
#
# The tarball layout is include/node/*.h under a versioned root; we mirror the
# official layout under deps/node-v<ver>/include so the include dir matches
# the committed xmake.lua path.
param(
  [string]$Version = "22.18.0",
  [string]$OutDir = "$PSScriptRoot"
)
$ErrorActionPreference = "Stop"
$tarball = Join-Path $OutDir "node-headers.tar.gz"
$url = "https://nodejs.org/dist/v$Version/node-v$Version-headers.tar.gz"
Write-Output "downloading $url"
Invoke-WebRequest -Uri $url -OutFile $tarball -UseBasicParsing
Write-Output "extracting include/ ..."
$tmp = Join-Path $OutDir "_headers_tmp"
if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
New-Item -ItemType Directory -Path $tmp | Out-Null
tar -xzf $tarball -C $tmp
$root = Get-ChildItem $tmp -Directory | Select-Object -First 1
$dest = Join-Path $OutDir "node-v$Version"
if (Test-Path $dest) { Remove-Item -Recurse -Force $dest }
Move-Item (Join-Path $root.FullName "include") $dest
Remove-Item -Recurse -Force $tmp
Write-Output "done -> $dest/include/node"
Write-Output "note: deps/node.lib is tracked in git; if lost, it can be"
Write-Output "regenerated with 'lib /def:node.def' against any node's exports"
Write-Output "or recovered from a previous checkout."
