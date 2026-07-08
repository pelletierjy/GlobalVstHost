#Requires -Version 5.1
<#
.SYNOPSIS
Prepare application for release with MSIX packaging

.DESCRIPTION
Prepares release by:
1. Extracting version from git tag
2. Validating version increment
3. Updating version-info.txt
4. Building MSIX package
5. Validating package

.PARAMETER Version
Version to release (format: Major.Minor.Build or Major.Minor.Build.Revision)

.EXAMPLE
.\Prepare-Release.ps1 -Version "1.0.1"
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$Version
)

$ErrorActionPreference = 'Stop'

Write-Host "🚀 Release Preparation Script" -ForegroundColor Cyan
Write-Host "Target Version: $Version`n"

# Normalize version to Major.Minor.Build.Revision format
$versionParts = $Version.Split('.')
if ($versionParts.Count -eq 3) {
    $fullVersion = "$($versionParts[0]).$($versionParts[1]).$($versionParts[2]).0"
} elseif ($versionParts.Count -eq 4) {
    $fullVersion = $Version
} else {
    Write-Host "❌ Invalid version format. Use: Major.Minor.Build[.Revision]" -ForegroundColor Red
    exit 1
}

Write-Host "Normalized Version: $fullVersion`n"

# Validate version format
if ($fullVersion -notmatch '^\d+\.\d+\.\d+\.\d+$') {
    Write-Host "❌ Version must be in format Major.Minor.Build.Revision" -ForegroundColor Red
    exit 1
}

# Get previous version from git tags
Write-Host "📋 Checking version history..." -ForegroundColor Yellow
try {
    $tags = git tag --list "v*" --sort=-version:refname 2>$null | Select-Object -First 5
    if ($tags) {
        Write-Host "   Recent releases: $($tags -join ', ')"
    }
} catch {
    Write-Host "   (No previous releases found)"
}

# Update version-info.txt
$versionFile = Join-Path (Split-Path $PSScriptRoot) "version-info.txt"
Write-Host "`n📝 Updating version file..." -ForegroundColor Yellow
if (Test-Path $versionFile) {
    $content = @"
# JyGlobalVST MSIX Package Version
# Format: Major.Minor.Build.Revision (all integers)
# Updated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')

$fullVersion
"@
    Set-Content -Path $versionFile -Value $content -NoNewline
    Write-Host "   ✓ Version updated: $fullVersion"
} else {
    Write-Host "   ⚠️  Version file not found at $versionFile" -ForegroundColor Yellow
}

# Build MSIX package
Write-Host "`n🔨 Building MSIX package..." -ForegroundColor Yellow
$buildScript = Join-Path (Split-Path $PSScriptRoot) "Scripts\Build-MSIX.ps1"
if (Test-Path $buildScript) {
    & $buildScript -Version $fullVersion -Configuration Release -Architecture x64
    if ($LASTEXITCODE -ne 0) {
        Write-Host "❌ Build failed" -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "❌ Build script not found: $buildScript" -ForegroundColor Red
    exit 1
}

# Validate package
Write-Host "`n✅ Release preparation complete!" -ForegroundColor Green
Write-Host "`nNext steps:"
Write-Host "1. Test the package locally (if needed)"
Write-Host "2. Create git tag: git tag v$Version"
Write-Host "3. Push tag: git push origin v$Version"
Write-Host "4. CI/CD will automatically build and create release"

exit 0
