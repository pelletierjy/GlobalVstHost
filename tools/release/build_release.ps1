#Requires -Version 7.2
<#
.SYNOPSIS
    Release-engineering script for JyGlobalVST.

.DESCRIPTION
    Builds Release binaries, optionally signs them, stages MSI artifacts,
    embeds version into manifest, and outputs SHA-256 hashes.

.PARAMETER Version
    Semantic version to embed (e.g. "0.1.0"). Defaults to the CMake project version.

.PARAMETER Configuration
    CMake build configuration: Release or Debug. Default: Release.

.PARAMETER Sign
    If set, invokes the signing scripts (requires JYGLOBALVST_CERT_THUMBPRINT env var).

.PARAMETER StageDir
    Directory to stage final artifacts. Default: .\staging\release

.PARAMETER BuildDriver
    If set, builds the WaveRT driver (requires WDK).

.PARAMETER BuildService
    If set, builds the Windows Service binary.

.EXAMPLE
    .\tools\release\build_release.ps1 -Version "0.2.0" -Sign -BuildDriver
#>
[CmdletBinding()]
param(
    [string] $Version = "",
    [ValidateSet("Release", "Debug")]
    [string] $Configuration = "Release",
    [switch] $Sign,
    [string] $StageDir = ".\staging\release",
    [switch] $BuildDriver,
    [switch] $BuildService
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot | Split-Path -Parent
Set-Location $Root

# -------------------------------------------------------------------------
# Resolve version
# -------------------------------------------------------------------------
if (-not $Version) {
    # Try to read from CMakeLists.txt project(VERSION ...)
    $cmakeContent = Get-Content "CMakeLists.txt" -Raw
    if ($cmakeContent -match 'VERSION\s+"([^"]+)"') {
        $Version = $Matches[1]
    }
    else {
        $Version = "1.1.0"
    }
}
Write-Host "Building JyGlobalVST v$Version ($Configuration)" -ForegroundColor Cyan

# -------------------------------------------------------------------------
# Clean + configure
# -------------------------------------------------------------------------
$buildDir = "build-release"
if (Test-Path $buildDir) {
    Remove-Item -Recurse -Force $buildDir
}

$cmakeArgs = @("-B", $buildDir, "-A", "x64", "-DCMAKE_BUILD_TYPE=$Configuration")
if (-not $BuildDriver) {
    $cmakeArgs += "-DJYGLOBALVST_BUILD_DRIVER=OFF"
}
if ($BuildService) {
    $cmakeArgs += "-DJYGLOBALVST_BUILD_SERVICE=ON"
}

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed" }

# -------------------------------------------------------------------------
# Build
# -------------------------------------------------------------------------
& cmake --build $buildDir --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

# -------------------------------------------------------------------------
# Tests
# -------------------------------------------------------------------------
& ctest --test-dir $buildDir -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Tests failed" }

# -------------------------------------------------------------------------
# Sign (optional)
# -------------------------------------------------------------------------
if ($Sign) {
    $thumbprint = $env:JYGLOBALVST_CERT_THUMBPRINT
    if (-not $thumbprint) {
        throw "JYGLOBALVST_CERT_THUMBPRINT environment variable is not set"
    }

    $signScript = "src/installer/signing/sign-binary.ps1"
    if (Test-Path $signScript) {
        $binDir = "$buildDir\bin\$Configuration"
        $binaries = Get-ChildItem -Path $binDir -Include "*.exe", "*.dll" -Recurse
        foreach ($bin in $binaries) {
            & $signScript -Path $bin.FullName -Thumbprint $thumbprint
            if ($LASTEXITCODE -ne 0) { throw "Signing failed for $($bin.Name)" }
        }
    }
    else {
        Write-Warning "Signing script not found at $signScript — skipping binary signing"
    }
}

# -------------------------------------------------------------------------
# Stage
# -------------------------------------------------------------------------
if (Test-Path $StageDir) {
    Remove-Item -Recurse -Force $StageDir
}
New-Item -ItemType Directory -Force -Path $StageDir | Out-Null

$binDir = "$buildDir\bin\$Configuration"
$artifacts = @(
    "$binDir\jyglobalvst_tray.exe"
    "$binDir\jyglobalvst_apo.dll"
)

if ($BuildService) {
    $artifacts += "$binDir\jyglobalvst_service.exe"
}
if ($BuildDriver) {
    $artifacts += "$binDir\jyglobalvst_driver.sys"
    $artifacts += "src/driver/waveRT/jyglobalvst-driver.inf"
}

$manifest = @{
    version = $Version
    configuration = $Configuration
    timestamp = (Get-Date -Format "o")
    git_commit = (git rev-parse HEAD)
    git_branch = (git rev-parse --abbrev-ref HEAD)
    artifacts = @()
}

foreach ($artifact in $artifacts) {
    if (Test-Path $artifact) {
        $name = Split-Path -Leaf $artifact
        $dest = Join-Path $StageDir $name
        Copy-Item -Path $artifact -Destination $dest -Force

        $hash = (Get-FileHash -Path $dest -Algorithm SHA256).Hash
        $manifest.artifacts += @{
            name = $name
            sha256 = $hash.ToLower()
            size_bytes = (Get-Item $dest).Length
        }
        Write-Host "  Staged: $name ($hash)" -ForegroundColor Green
    }
    else {
        Write-Warning "Artifact not found: $artifact"
    }
}

# Write manifest
$manifestPath = Join-Path $StageDir "manifest.json"
$manifest | ConvertTo-Json -Depth 4 | Set-Content -Path $manifestPath -Encoding UTF8
Write-Host "  Manifest: $manifestPath" -ForegroundColor Green

# Summary
Write-Host "`nRelease staging complete." -ForegroundColor Cyan
Write-Host "  Directory: $(Resolve-Path $StageDir)"
Write-Host "  Version:   $Version"
Write-Host "  Config:    $Configuration"
Write-Host "  Signed:    $Sign"
