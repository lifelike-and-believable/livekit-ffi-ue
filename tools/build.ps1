Param(
  [switch]$WithLiveKit,
  [string]$UePluginDir = "",
  [switch]$Clean,
  # Sandbox build options
  [switch]$BuildSandbox,
  [string]$SandboxProject = "ue\\LiveKitSandbox\\LiveKitSandbox.uproject",
  [string]$SandboxTarget = "LiveKitSandboxEditor",
  [string]$SandboxPlatform = "Win64",
  [string]$SandboxConfig = "Development",
  [string]$UEBase = ""
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

function Test-CargoToml($p) {
  Test-Path (Join-Path $p "Cargo.toml")
}

if (Test-CargoToml $Candidate1) {
  $CrateDir = $Candidate1
} elseif (Test-CargoToml $Candidate2) {
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
$prevRUSTFLAGS = $env:RUSTFLAGS
# Force static MSVC runtime to match third-party static libs (webrtc-sys)
$env:RUSTFLAGS = "-C target-feature=+crt-static"
Info "[env] RUSTFLAGS=$($env:RUSTFLAGS)"
$cmd = @("+" + $RequiredToolchain) + $cargoArgs
Write-Host ("cargo " + ($cmd -join " ")) -ForegroundColor DarkGray
$proc = Start-Process cargo -ArgumentList $cmd -NoNewWindow -PassThru -Wait
$exit = $proc.ExitCode
Pop-Location
# Restore RUSTFLAGS after build
$env:RUSTFLAGS = $prevRUSTFLAGS
if ($exit -ne 0) { Die "[cargo] Build failed with exit code $exit" }

Info "[cargo] Build completed"

# --- Copy artifacts to UE plugin ---
if ($UePluginDir) {
  if (Test-Path $UePluginDir) {
    $targetDir = Join-Path $CrateDir "target\release"
    # DLL-style artifacts (if crate were cdylib)
    $binDir = Join-Path $UePluginDir "Binaries\Win64"
    $dllNames = @("livekit_ffi.dll","livekit_ffi.dll.lib","livekit_ffi.pdb")

    $dllFound = @()
    foreach ($n in $dllNames) {
      $p = Join-Path $targetDir $n
      if (Test-Path $p) { $dllFound += $p }
    }

    if ($dllFound.Count -gt 0) {
      New-Item -ItemType Directory -Force -Path $binDir | Out-Null
      foreach ($f in $dllFound) { Copy-Item $f -Destination $binDir -Force }
      Info "[copy] Copied $($dllFound.Count) artifact(s) to $binDir"
    } else {
      Warn "[copy] No DLL artifacts found in $targetDir; skipping copy to Binaries"
    }

    # Import lib for linking via ThirdParty (Windows, cdylib)
    $importLib = Join-Path $targetDir "livekit_ffi.dll.lib"
    if (Test-Path $importLib) {
      $tpDir = Join-Path $UePluginDir "Source\LiveKitBridge\ThirdParty\livekit_ffi\lib\Win64\Release"
      New-Item -ItemType Directory -Force -Path $tpDir | Out-Null
      Copy-Item $importLib -Destination (Join-Path $tpDir "livekit_ffi.dll.lib") -Force
      Info "[copy] Copied import lib to $tpDir"
    } else {
      Warn "[copy] Import lib not found at $importLib"
    }

    # Optional: still copy static lib (for non-UE consumers); UE will use import lib
    $staticLib = Join-Path $targetDir "livekit_ffi.lib"
    if (Test-Path $staticLib) {
      $tpDir = Join-Path $UePluginDir "Source\LiveKitBridge\ThirdParty\livekit_ffi\lib\Win64\Release"
      New-Item -ItemType Directory -Force -Path $tpDir | Out-Null
      Copy-Item $staticLib -Destination (Join-Path $tpDir "livekit_ffi.lib") -Force
      Info "[copy] Copied static lib to $tpDir (not used by UE)"
    }
  } else {
    Warn "[copy] UE plugin dir not found: $UePluginDir (skipping copy)"
  }
}

Info "[build] Done."

# --- Build UE Sandbox (optional) ---
if ($BuildSandbox) {
  Info "[ue] Building sandbox project ($SandboxTarget $SandboxPlatform $SandboxConfig)"

  $uproject = Join-Path $RepoRoot $SandboxProject
  if (-not (Test-Path $uproject)) {
    Die "[ue] Sandbox .uproject not found: $uproject"
  }

  # Resolve UE root
  $ueRoot = $null
  if ($UEBase -and (Test-Path $UEBase)) { $ueRoot = $UEBase }
  elseif ($env:UE5_ROOT -and (Test-Path $env:UE5_ROOT)) { $ueRoot = $env:UE5_ROOT }
  elseif ($env:UE5_EDITOR_EXE -and (Test-Path $env:UE5_EDITOR_EXE)) {
    # Derive UE root from editor exe path: ...\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe
    $p1 = Split-Path -Parent $env:UE5_EDITOR_EXE   # Win64
    $p2 = Split-Path -Parent $p1                   # Binaries
    $p3 = Split-Path -Parent $p2                   # Engine
    $ueRoot = Split-Path -Parent $p3               # UE_5.6
  } else {
    $default = "C:\\Program Files\\Epic Games\\UE_5.6"
    if (Test-Path $default) { $ueRoot = $default }
  }

  if (-not $ueRoot) { Die "[ue] Could not resolve Unreal Engine root. Provide -UEBase or set UE5_ROOT/UE5_EDITOR_EXE." }

  $buildBat = Join-Path $ueRoot "Engine\Build\BatchFiles\Build.bat"
  if (-not (Test-Path $buildBat)) { Die "[ue] Build.bat not found at $buildBat" }

  Info "[ue] Using UE root: $ueRoot"
  Info "[ue] Invoking: Build.bat $SandboxTarget $SandboxPlatform $SandboxConfig -Project=\"$uproject\" -WaitMutex"

  $ubtArgs = @(
    $SandboxTarget,
    $SandboxPlatform,
    $SandboxConfig,
    "-Project=$uproject",
    "-WaitMutex"
  )

  $proc = Start-Process -FilePath $buildBat -ArgumentList $ubtArgs -NoNewWindow -PassThru -Wait -WorkingDirectory $ueRoot
  $exit = $proc.ExitCode
  if ($exit -ne 0) { Die "[ue] Build.bat failed with exit code $exit" }

  Info "[ue] Sandbox build completed"
}
