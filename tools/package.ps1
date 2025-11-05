Param(
  [string]$CrateDir = "livekit_ffi",
  [string]$OutDir = "artifacts/windows-x64"
)

$ErrorActionPreference = "Stop"

function Info($msg) { Write-Host $msg -ForegroundColor Cyan }
function Warn($msg) { Write-Warning $msg }
function Die($msg)  { Write-Error $msg; exit 1 }

$RepoRoot = Split-Path -Parent $PSCommandPath
$RepoRoot = Split-Path -Parent $RepoRoot  # tools -> repo root

if (-not (Test-Path $CrateDir)) { $CrateDir = Join-Path $RepoRoot $CrateDir }
if (-not (Test-Path $CrateDir)) { Die "[package] Crate directory not found: $CrateDir" }

$TargetDir = Join-Path $CrateDir "target\release"
if (-not (Test-Path $TargetDir)) { Die "[package] Target dir not found: $TargetDir (build first)" }

$IncludeSrc = Join-Path $CrateDir "include"
if (-not (Test-Path $IncludeSrc)) { Die "[package] Include dir not found: $IncludeSrc" }

# Layout:
# artifacts/windows-x64/
#   include/ *.h
#   bin/ livekit_ffi.dll, livekit_ffi.pdb
#   lib/Win64/Release/ livekit_ffi.dll.lib and livekit_ffi.lib

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$OutInclude = Join-Path $OutDir "include"
$OutBin     = Join-Path $OutDir "bin"
$OutLib     = Join-Path $OutDir "lib\Win64\Release"
New-Item -ItemType Directory -Force -Path $OutInclude | Out-Null
New-Item -ItemType Directory -Force -Path $OutBin | Out-Null
New-Item -ItemType Directory -Force -Path $OutLib | Out-Null

Info "[package] Copying headers"
Copy-Item (Join-Path $IncludeSrc "*.h") -Destination $OutInclude -Force

Info "[package] Copying binaries/libs"
$dll = Join-Path $TargetDir "livekit_ffi.dll"
$pdb = Join-Path $TargetDir "livekit_ffi.pdb"
$implib = Join-Path $TargetDir "livekit_ffi.dll.lib"
$static = Join-Path $TargetDir "livekit_ffi.lib"

if (Test-Path $dll) { Copy-Item $dll -Destination $OutBin -Force } else { Warn "[package] Missing $dll" }
if (Test-Path $pdb) { Copy-Item $pdb -Destination $OutBin -Force } else { Warn "[package] Missing $pdb" }
if (Test-Path $implib) { Copy-Item $implib -Destination $OutLib -Force } else { Warn "[package] Missing $implib" }
if (Test-Path $static) { Copy-Item $static -Destination $OutLib -Force } else { Warn "[package] Missing $static" }

Info "[package] Done -> $OutDir"
