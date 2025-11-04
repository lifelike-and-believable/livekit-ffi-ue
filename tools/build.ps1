<#  tools/build.ps1
    One-click builder for livekit_ffi on Windows (MSVC + clang-cl + libclang).

    USAGE:
      # Stub (no LiveKit crates) — fastest sanity check
      .\tools\build.ps1

      # Full LiveKit build (audio+data)
      .\tools\build.ps1 -WithLiveKit

      # Full build and copy the lib into your UE plugin third-party folder
      .\tools\build.ps1 -WithLiveKit -UePluginDir "ue\Plugins\LiveKitBridge"

      # Clean first
      .\tools\build.ps1 -WithLiveKit -Clean
#>

[CmdletBinding()]
param(
  [switch]$WithLiveKit,
  [switch]$Clean,
  [string]$Toolchain = "1.81.0",
  [string]$SdkVersion,                  # e.g. "10.0.22621.0" (autodetect if omitted)
  [string]$MsvcVersion,                 # e.g. "14.38.33130"  (autodetect if omitted)
  [string]$LlvmDir = "C:\Program Files\LLVM",
  [string]$CrateRelPath = "livekit_ffi",
  [string]$UePluginDir,                 # e.g. "ue\Plugins\LiveKitBridge"
  [string]$Config = "release"           # or "debug"
)

function Fail($msg) { Write-Host "❌ $msg" -ForegroundColor Red; exit 1 }
function Info($msg) { Write-Host "• $msg" -ForegroundColor Cyan }
function Ok($msg)   { Write-Host "✔ $msg" -ForegroundColor Green }

# --- repo layout sanity ---
$RepoRoot = (Resolve-Path ".").Path
$CrateDir = Join-Path $RepoRoot $CrateRelPath
if (-not (Test-Path "$CrateDir\Cargo.toml")) {
  Fail "Could not find Cargo.toml under '$CrateDir'. Run this script from the repo root."
}

# --- ensure VS env (MSVC, SDK) ---
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { Fail "vswhere not found. Install Visual Studio 2022 (C++ workload)." }
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { Fail "Could not locate a VS installation with C++ tools." }
$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"

# Pull versions if not supplied
if (-not $MsvcVersion) {
  $msvcLib = Get-ChildItem -Directory (Join-Path $vsPath "VC\Tools\MSVC") | Sort-Object Name -Descending | Select-Object -First 1
  $MsvcVersion = $msvcLib.Name
}
if (-not $SdkVersion) {
  $sdkBase = "C:\Program Files (x86)\Windows Kits\10\Include"
  $sdk = Get-ChildItem -Directory $sdkBase | Sort-Object Name -Descending | Select-Object -First 1
  if (-not $sdk) { Fail "Windows 10/11 SDK not found under '$sdkBase'." }
  $SdkVersion = $sdk.Name
}
Info "VS: $vsPath"
Info "MSVC: $MsvcVersion"
Info "WinSDK: $SdkVersion"

# Import MSVC environment into this session
cmd /c "`"$vcvars`" && set" | ForEach-Object {
  $parts = $_ -split '='; if ($parts.Length -ge 2) { Set-Item -Path "Env:\$($parts[0])" -Value ($parts[1..($parts.Length-1)] -join '=' ) }
} | Out-Null
Ok "MSVC environment loaded"

# --- ensure LLVM / libclang ---
$clangExe = Join-Path $LlvmDir "bin\clang.exe"
$libclang = Join-Path $LlvmDir "bin\libclang.dll"
if (-not (Test-Path $clangExe) -or -not (Test-Path $libclang)) {
  Write-Warning "LLVM not found at '$LlvmDir'. Install LLVM to the default path or set -LlvmDir."
  Write-Host "Download: https://github.com/llvm/llvm-project/releases"
  Fail "Missing LLVM/Clang (needed for bindgen)."
}
$env:LIBCLANG_PATH = (Join-Path $LlvmDir "bin")
$env:CLANG_PATH    = $clangExe
$env:CC  = "clang-cl"
$env:CXX = "clang-cl"
$env:CXXFLAGS = "/EHsc /Zc:__cplusplus"
Ok "LLVM / libclang configured"

# --- optional: help bindgen find includes explicitly (usually not required) ---
$extra = @(
  "--target=x86_64-pc-windows-msvc",
  "-I`"C:\Program Files (x86)\Windows Kits\10\Include\$SdkVersion\ucrt`"",
  "-I`"C:\Program Files (x86)\Windows Kits\10\Include\$SdkVersion\um`"",
  "-I`"C:\Program Files (x86)\Windows Kits\10\Include\$SdkVersion\shared`"",
  "-I`"$vsPath\VC\Tools\MSVC\$MsvcVersion\include`""
) -join ' '
$env:BINDGEN_EXTRA_CLANG_ARGS = $extra

# --- ensure Rust toolchain ---
if (-not (Get-Command rustup -ErrorAction SilentlyContinue)) {
  Write-Warning "rustup not found. Install from https://rustup.rs and re-run."
  Fail "Missing rustup."
}
& rustup toolchain install $Toolchain | Out-Null
& rustup default $Toolchain       | Out-Null
Ok "Rust toolchain pinned to $Toolchain"

# --- build ---
Push-Location $CrateDir
try {
  if ($Clean) {
    Info "Cleaning previous artifacts…"
    cargo clean
  }

  $features = @()
  if ($WithLiveKit) { $features += "with_livekit" }

  $cmd = @("cargo","build","--$Config")
  if ($features.Count -gt 0) { $cmd += @("--features", ($features -join ",")) }

  Info ("Building: " + ($cmd -join " "))
  & $cmd[0] $cmd[1..($cmd.Length-1)]
  if ($LASTEXITCODE -ne 0) { Fail "Cargo build failed." }

  # artifact path
  $libPath = Join-Path $CrateDir "target\$Config\livekit_ffi.lib"
  if (-not (Test-Path $libPath)) { Fail "Built library not found at $libPath" }
  Ok "Built: $libPath"

  if ($UePluginDir) {
    $dst = Join-Path $RepoRoot "$UePluginDir\Source\LiveKitBridge\ThirdParty\livekit_ffi\lib\Win64\Release"
    New-Item -ItemType Directory -Force -Path $dst | Out-Null
    Copy-Item $libPath $dst -Force
    Ok "Copied to UE plugin: $dst"
  }
}
finally {
  Pop-Location
}
Ok "Done."
