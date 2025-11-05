param(
  [Parameter(Mandatory=$true)][string]$Identity,
  [Parameter(Mandatory=$true)][string]$Room,
  [string]$Ttl = "24h",
  [string]$Name,
  [switch]$NoPublish,
  [switch]$NoSubscribe,
  [switch]$NoPublishData,
  [switch]$Json
)

$ErrorActionPreference = 'Stop'

if (-not $env:LIVEKIT_API_KEY -or -not $env:LIVEKIT_API_SECRET) {
  Write-Error "Please set LIVEKIT_API_KEY and LIVEKIT_API_SECRET environment variables."
}

# Build argument list for Node CLI
$cli = Join-Path $PSScriptRoot 'token-mint\index.js'
if (-not (Test-Path $cli)) {
  Write-Error "Token CLI not found at $cli. Run from repo root after installing dependencies."
}

$ArgsList = @('--identity', $Identity, '--room', $Room, '--ttl', $Ttl)
if ($Name) { $ArgsList += @('--name', $Name) }
if ($NoPublish) { $ArgsList += '--no-publish' }
if ($NoSubscribe) { $ArgsList += '--no-subscribe' }
if ($NoPublishData) { $ArgsList += '--no-publishData' }
if ($Json) { $ArgsList += '--json' }

# Ensure dependencies installed
$pkgJson = Join-Path $PSScriptRoot 'token-mint\package.json'
$nodeModules = Join-Path $PSScriptRoot 'token-mint\node_modules'
if (-not (Test-Path $nodeModules)) {
  Write-Host "[token] Installing dependencies in tools/token-mint..."
  Push-Location (Split-Path $pkgJson)
  try { npm install | Out-Host } finally { Pop-Location }
}

# Invoke
node $cli @ArgsList
