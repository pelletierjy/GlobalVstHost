#Requires -Version 5.1
<#
.SYNOPSIS
Build MSIX package for Microsoft Store publication

.DESCRIPTION
Automates the complete MSIX build process:
1. Validates build environment (CMake, Visual Studio)
2. Compiles application in Release configuration
3. Generates/updates AppxManifest.xml
4. Creates MSIX package and bundle
5. Validates package integrity

.PARAMETER Version
Application version (format: Major.Minor.Build.Revision, e.g., 1.0.0.0)

.PARAMETER Configuration
Build configuration: Release (default) or Debug

.PARAMETER Architecture
Target architecture: x64 (default, only option for v1)

.EXAMPLE
.\Build-MSIX.ps1 -Version 1.0.0.0 -Configuration Release
#>

param(
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^\d+\.\d+\.\d+\.\d+$')]
    [string]$Version,

    [Parameter(Mandatory=$false)]
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',

    [Parameter(Mandatory=$false)]
    [ValidateSet('x64')]
    [string]$Architecture = 'x64'
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

Write-Host "🔨 JyGlobalVST MSIX Build Script" -ForegroundColor Cyan
Write-Host "Version: $Version | Config: $Configuration | Arch: $Architecture`n"

# Check prerequisites
Write-Host "📋 Checking prerequisites..." -ForegroundColor Yellow

# Check CMake
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    Write-Host "❌ CMake not found. Please install CMake 3.22 or later." -ForegroundColor Red
    exit 1
}
Write-Host "✓ CMake found: $($cmake.Source)"


# Check Visual Studio (via vswhere - version-agnostic)
$vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
$msbuildPath = $null
if (Test-Path $vswhere) {
    $msbuildPath = & $vswhere -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
}
if (-not $msbuildPath -or -not (Test-Path $msbuildPath)) {
    Write-Host "❌ Visual Studio with MSBuild not found." -ForegroundColor Red
    exit 1
}
Write-Host "✓ Visual Studio found: $msbuildPath"

# Check MSIX Packaging Tool (optional)
$msixTool = Get-Command MsixPackagingTool -ErrorAction SilentlyContinue
if ($msixTool) {
    Write-Host "✓ MSIX Packaging Tool available"
} else {
    Write-Host "⚠️  MSIX Packaging Tool not found (optional)" -ForegroundColor Yellow
}

# Get repository root
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Write-Host "`n📁 Repository root: $repoRoot"

# Create build directories
$buildDir = Join-Path $repoRoot "build"
$storePackageDir = Join-Path $buildDir "store-packages"

Write-Host "`n🏗️  Configuring CMake..."
Push-Location $repoRoot
cmake -B $buildDir -A x64
if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ CMake configuration failed" -ForegroundColor Red
    exit 1
}
Pop-Location

Write-Host "🔨 Building application ($Configuration)..."
cmake --build $buildDir --config $Configuration
if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Build failed" -ForegroundColor Red
    exit 1
}

# Verify executable exists
$exePath = Join-Path $buildDir "src\tray-app\jyglobalvst_tray_artefacts\$Configuration\JyGlobalVST.exe"
if (-not (Test-Path $exePath)) {
    Write-Host "❌ Executable not found: $exePath" -ForegroundColor Red
    exit 1
}
Write-Host "✓ Executable built: $exePath"

# Update manifest with version
Write-Host "`n📝 Updating AppxManifest.xml with version $Version..."
$manifestPath = Join-Path $repoRoot "StorePackaging\AppxManifest.xml"
$manifest = Get-Content $manifestPath -Raw
$manifest = $manifest -replace 'Version="[\d.]+"', "Version=`"$Version`""
Set-Content -Path $manifestPath -Value $manifest -NoNewline

# Create output directory
if (-not (Test-Path $storePackageDir)) {
    New-Item -ItemType Directory -Force -Path $storePackageDir | Out-Null
}

Write-Host "✓ Manifest updated"

# Package with Visual Studio (via MsBuild)
Write-Host "`n📦 Creating MSIX package..."

$msixprojPath = Join-Path $repoRoot "StorePackaging\JyGlobalVST.msixproj"
& $msbuildPath $msixprojPath /p:Configuration=$Configuration /p:Platform=$Architecture /p:AppxPackageDir=$storePackageDir

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ MSIX build failed" -ForegroundColor Red
    exit 1
}

# Check for generated package
$packagePattern = Join-Path $storePackageDir "JyGlobalVST*$Architecture.msixbundle"
$packages = @(Get-ChildItem -Path $storePackageDir -Filter "*.msixbundle" -ErrorAction SilentlyContinue)

if ($packages.Count -gt 0) {
    Write-Host "`n✅ MSIX package created successfully!" -ForegroundColor Green
    foreach ($pkg in $packages) {
        Write-Host "   📦 $($pkg.Name) ($('{0:N2}' -f ($pkg.Length / 1MB)) MB)"
    }

    # Run validation if available
    Write-Host "`n🔍 Validating package..."
    $validateScript = Join-Path $PSScriptRoot "Validate-Package.ps1"
    if (Test-Path $validateScript) {
        & $validateScript -PackagePath $packages[0].FullName
    }
} else {
    Write-Host "`n⚠️  MSIX package file not found in $storePackageDir" -ForegroundColor Yellow
    Write-Host "Note: Manual packaging may be required. See: StorePackaging/Documentation/MSIX-Build-Guide.md"
}

# Copy manifest to output for reference
$manifestOutput = Join-Path $storePackageDir "AppxManifest.xml"
Copy-Item -Path $manifestPath -Destination $manifestOutput -Force

Write-Host "`n✅ Build process complete!" -ForegroundColor Green
Write-Host "Output: $storePackageDir"

exit 0
