Param(
  [string]$Room = "test",
  [string]$Ttl = "168h"
)

$ErrorActionPreference = 'Stop'

# Local testing defaults if not provided via environment
if (-not $env:LIVEKIT_API_KEY) { $env:LIVEKIT_API_KEY = "devkey" }
if (-not $env:LIVEKIT_API_SECRET) { $env:LIVEKIT_API_SECRET = "secret" }

$ScriptRoot = Split-Path -Parent $PSCommandPath
$MintScript = Join-Path $ScriptRoot 'mint-token.ps1'
if (-not (Test-Path $MintScript)) { Write-Error "mint-token.ps1 not found at $MintScript" }

function Mint-Token([string]$Identity, [bool]$IsPublisher) {
  $params = @{ Identity = $Identity; Room = $Room; Ttl = $Ttl; Json = $true }
  if (-not $IsPublisher) {
    $params.NoPublish = $true
    $params.NoPublishData = $true
  }
  $raw = & $MintScript @params
  $raw = ($raw | Out-String).Trim()
  try {
    $obj = $raw | ConvertFrom-Json
    if (-not $obj.token) { throw "No token in JSON output" }
    return ($obj.token.Trim())
  } catch {
    throw "Unexpected token output for '$Identity': $raw"
  }
}

$pub1 = Mint-Token 'ue_publisher1' $true
$pub2 = Mint-Token 'ue_publisher_2' $true
$sub1 = Mint-Token 'ue_subscriber1' $false
$sub2 = Mint-Token 'ue_subscriber2' $false
$sub3 = Mint-Token 'ue_subscriber3' $false
$sub4 = Mint-Token 'ue_subscriber4' $false

$OutFile = Join-Path $ScriptRoot 'test_tokens.txt'
$Content = @"
ue_publisher1
$pub1

ue_publisher_2
$pub2

ue_subscriber1
$sub1

ue_subscriber2
$sub2

ue_subscriber3
$sub3

ue_subscriber4
$sub4
"@

Set-Content -Path $OutFile -Value $Content -NoNewline -Encoding ASCII
Write-Host "[tokens] Wrote $OutFile"
