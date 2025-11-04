Param(
  [string]$ProjectDir = "ue\LiveKitSandbox",
  [string]$PluginSrc = "ue\Plugins\LiveKitBridge"
)

$ErrorActionPreference = "Stop"

function Info($msg) { Write-Host $msg -ForegroundColor Cyan }
function Warn($msg) { Write-Warning $msg }
function Die($msg)  { Write-Error $msg; exit 1 }

$RepoRoot = Split-Path -Parent $PSCommandPath
$Root = Split-Path -Parent $RepoRoot

$projPath = Join-Path $Root $ProjectDir
if (-not (Test-Path $projPath)) { Die "Project directory not found: $projPath" }

$pluginSrcPath = Join-Path $Root $PluginSrc
if (-not (Test-Path $pluginSrcPath)) { Die "Plugin source not found: $pluginSrcPath" }

$destPluginsDir = Join-Path $projPath "Plugins"
New-Item -ItemType Directory -Force -Path $destPluginsDir | Out-Null

$dest = Join-Path $destPluginsDir "LiveKitBridge"
if (Test-Path $dest) {
  Info "Plugin path already exists: $dest"
  exit 0
}

Info "Linking plugin: $dest -> $pluginSrcPath"
try {
  New-Item -ItemType Junction -Path $dest -Target $pluginSrcPath | Out-Null
} catch {
  Warn "New-Item Junction failed, attempting mklink /J"
  & cmd /c mklink /J "$dest" "$pluginSrcPath"
}

Info "Done."
