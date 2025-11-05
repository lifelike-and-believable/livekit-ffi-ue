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

# Require API credentials; allow caller to set defaults upstream for local testing
if (-not $env:LIVEKIT_API_KEY -or -not $env:LIVEKIT_API_SECRET) {
  throw "Please set LIVEKIT_API_KEY and LIVEKIT_API_SECRET environment variables."
}

function ConvertTo-Base64Url([byte[]] $bytes) {
  $b64 = [System.Convert]::ToBase64String($bytes)
  $b64 = $b64.TrimEnd('=')
  $b64 = $b64.Replace('+','-').Replace('/','_')
  return $b64
}

function Get-Bytes([string]$text) {
  return [System.Text.Encoding]::UTF8.GetBytes($text)
}

function Parse-Ttl([string]$ttl) {
  if ([string]::IsNullOrWhiteSpace($ttl)) { return [TimeSpan]::FromHours(24) }
  $m = [regex]::Match($ttl.Trim(), '^(\d+)([smhd])$')
  if (-not $m.Success) {
    # Fallback: try parsing as .NET TimeSpan (e.g., "1:00:00")
    try { return [TimeSpan]::Parse($ttl) } catch { return [TimeSpan]::FromHours(24) }
  }
  $num = [int]$m.Groups[1].Value
  switch ($m.Groups[2].Value) {
    's' { return [TimeSpan]::FromSeconds($num) }
    'm' { return [TimeSpan]::FromMinutes($num) }
    'h' { return [TimeSpan]::FromHours($num) }
    'd' { return [TimeSpan]::FromDays($num) }
  }
}

# Build LiveKit grants
$canPublish = -not $NoPublish.IsPresent
$canSubscribe = -not $NoSubscribe.IsPresent
$canPublishData = -not $NoPublishData.IsPresent

$now = [DateTimeOffset]::UtcNow
$exp = $now + (Parse-Ttl $Ttl)
$iat = $now
$nbf = $now.AddSeconds(-10) # small clock skew tolerance

$claims = [ordered]@{
  iss = $env:LIVEKIT_API_KEY
  sub = $Identity
  iat = [int]$iat.ToUnixTimeSeconds()
  nbf = [int]$nbf.ToUnixTimeSeconds()
  exp = [int]$exp.ToUnixTimeSeconds()
  video = @{ 
    room = $Room
    roomJoin = $true
    canPublish = $canPublish
    canSubscribe = $canSubscribe
    canPublishData = $canPublishData
  }
}
if ($Name) { $claims.name = $Name }

$header = @{ alg = 'HS256'; typ = 'JWT' }

$headerJson = ($header | ConvertTo-Json -Compress)
$payloadJson = ($claims | ConvertTo-Json -Compress)

$headerB64 = ConvertTo-Base64Url (Get-Bytes $headerJson)
$payloadB64 = ConvertTo-Base64Url (Get-Bytes $payloadJson)
$unsigned = "$headerB64.$payloadB64"

$hmac = [System.Security.Cryptography.HMACSHA256]::new([System.Text.Encoding]::UTF8.GetBytes($env:LIVEKIT_API_SECRET))
$sigBytes = $hmac.ComputeHash((Get-Bytes $unsigned))
$signatureB64 = ConvertTo-Base64Url $sigBytes
$jwt = "$unsigned.$signatureB64"

if ($Json) {
  # Output strict JSON only to the pipeline
  $out = @{ token = $jwt } | ConvertTo-Json -Compress
  Write-Output $out
} else {
  Write-Output $jwt
}
