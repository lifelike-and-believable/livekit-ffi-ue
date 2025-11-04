Param(
  [switch]$WithLiveKit,
  [string]$UePluginDir = "",
  [switch]$Clean
)

$ErrorActionPreference = "Stop"

function Info($msg) { Write-Host $msg -ForegroundColor Cyan }
function Warn($msg) { Write-Warning $msg }
function Die($msg)  { Write-Error $msg; exit 1 }

Info "[build] Starting build.ps1"

# --- Toolchain ---
$RequiredToolchain = "1.87.0-x86_64-pc-windows-msvc"
Info "[rustup] Ensuring Rust toolchain $RequiredToolchain is installed"
& rustup toolchain install $RequiredToolchain | Out-Null
Info "[rustup] Setting local override to $RequiredToolchain"
& rustup override set $RequiredToolchain | Out-Null
& rustc --version
& cargo --version

# --- Resolve repo and crate dirs ---
$ScriptDir = Split-Path -Parent $PSCommandPath
# tools folder lives at <RepoRoot>\tools, so RepoRoot is parent of ScriptDir
$RepoRoot = Split-Path -Parent $ScriptDir

# Try typical layout first
$Candidate1 = Join-Path $RepoRoot "livekit_ffi"
$Candidate2 = $RepoRoot

function Has-CargoToml($p) {
  Test-Path (Join-Path $p "Cargo.toml")
}

if (Has-CargoToml $Candidate1) {
  $CrateDir = $Candidate1
} elseif (Has-CargoToml $Candidate2) {
  $CrateDir = $Candidate2
} else {
  Die "[paths] Could not find Cargo.toml in '$Candidate1' or '$Candidate2'"
}

Info "[paths] Using crate dir: $CrateDir"

# --- Clean ---
if ($Clean) {
  Info "[cargo] Cleaning previous artifacts"
  Push-Location $CrateDir
  & cargo +$RequiredToolchain clean
  Pop-Location
}

# --- Build args ---
$features = @()
if ($WithLiveKit) { $features += "with_livekit" }

$cargoArgs = @("build","--release")
if ($features.Count -gt 0) {
  $cargoArgs += @("--features", ($features -join ","))
}

# --- Build ---
Info "[cargo] Building with toolchain $RequiredToolchain"
Push-Location $CrateDir
$cmd = @("+" + $RequiredToolchain) + $cargoArgs
Write-Host ("cargo " + ($cmd -join " ")) -ForegroundColor DarkGray
$proc = Start-Process cargo -ArgumentList $cmd -NoNewWindow -PassThru -Wait
$exit = $proc.ExitCode
Pop-Location
if ($exit -ne 0) { Die "[cargo] Build failed with exit code $exit" }

Info "[cargo] Build completed"

# --- Copy artifacts to UE plugin ---
if ($UePluginDir) {
  if (Test-Path $UePluginDir) {
    $targetDir = Join-Path $CrateDir "target\release"
    $binDir = Join-Path $UePluginDir "Binaries\Win64"
    $names = @("livekit_ffi.dll","livekit_ffi.dll.lib","livekit_ffi.pdb")

    $found = @()
    foreach ($n in $names) {
      $p = Join-Path $targetDir $n
      if (Test-Path $p) { $found += $p }
    }

    if ($found.Count -gt 0) {
      New-Item -ItemType Directory -Force -Path $binDir | Out-Null
      foreach ($f in $found) { Copy-Item $f -Destination $binDir -Force }
      Info "[copy] Copied $($found.Count) artifact(s) to $binDir"
    } else {
      Warn "[copy] No expected artifacts found in $targetDir; skipping copy"
    }
  } else {
    Warn "[copy] UE plugin dir not found: $UePluginDir (skipping copy)"
  }
}

Info "[build] Done."
