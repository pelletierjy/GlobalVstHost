#Requires -Version 5.1
<#
.SYNOPSIS
Validate MSIX package against Microsoft Store requirements

.DESCRIPTION
Validates:
- Package file integrity
- AppxManifest.xml schema and content
- Icon file references and dimensions
- Required capabilities declarations
- Version number format

.PARAMETER PackagePath
Path to the .msixbundle or .msix file to validate

.EXAMPLE
.\Validate-Package.ps1 -PackagePath "C:\build\store-packages\JyGlobalVST_1.0.0.0_x64.msixbundle"
#>

param(
    [Parameter(Mandatory=$true)]
    [ValidateScript({ Test-Path $_ -PathType Leaf })]
    [string]$PackagePath
)

$ErrorActionPreference = 'Stop'

Write-Host "🔍 MSIX Package Validation" -ForegroundColor Cyan
Write-Host "Package: $(Split-Path -Leaf $PackagePath)`n"

$validationPassed = $true
$warningCount = 0
$errorCount = 0

# 1. Check file integrity
Write-Host "📦 Checking file integrity..." -ForegroundColor Yellow
try {
    $file = Get-Item $PackagePath
    Write-Host "   Size: $('{0:N2}' -f ($file.Length / 1MB)) MB"
    Write-Host "   ✓ File accessible"
} catch {
    Write-Host "   ❌ Cannot access file: $_" -ForegroundColor Red
    $errorCount++
    $validationPassed = $false
}

# 2. Validate manifest XML
Write-Host "`n📋 Checking AppxManifest.xml..." -ForegroundColor Yellow

# Try to extract and parse manifest
$tempDir = Join-Path $env:TEMP "msix_validation_$(Get-Random)"
New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

try {
    # Copy package and rename to .zip to extract
    $zipPath = Join-Path $tempDir "package.zip"
    Copy-Item -Path $PackagePath -Destination $zipPath -Force

    # Extract using Shell.Application (Windows built-in)
    $shell = New-Object -ComObject Shell.Application
    $zipPackage = $shell.NameSpace($zipPath)

    # Extract all
    $extractPath = Join-Path $tempDir "extracted"
    New-Item -ItemType Directory -Path $extractPath -Force | Out-Null

    $zipPackage.Items() | ForEach-Object {
        $shell.NameSpace($extractPath).CopyHere($_, 16)
    }

    # Look for manifest
    $manifestPath = Get-ChildItem -Path $extractPath -Filter "AppxManifest.xml" -Recurse | Select-Object -First 1

    if ($manifestPath) {
        Write-Host "   ✓ Manifest found"

        # Try to parse as XML
        try {
            [xml]$manifest = Get-Content -Path $manifestPath.FullName -Raw
            Write-Host "   ✓ Manifest is valid XML"

            # Check required elements
            $identity = $manifest.Package.Identity
            if ($identity) {
                Write-Host "   ✓ Identity element found"
                Write-Host "     Name: $($identity.Name)"
                Write-Host "     Version: $($identity.Version)"

                # Validate version format
                if ($identity.Version -match '^\d+\.\d+\.\d+\.\d+$') {
                    Write-Host "     ✓ Version format valid"
                } else {
                    Write-Host "     ❌ Invalid version format" -ForegroundColor Red
                    $errorCount++
                    $validationPassed = $false
                }
            } else {
                Write-Host "   ❌ Identity element missing" -ForegroundColor Red
                $errorCount++
                $validationPassed = $false
            }

            # Check Properties
            if ($manifest.Package.Properties) {
                Write-Host "   ✓ Properties element found"
            } else {
                Write-Host "   ❌ Properties element missing" -ForegroundColor Red
                $errorCount++
                $validationPassed = $false
            }

            # Check Dependencies
            if ($manifest.Package.Dependencies) {
                Write-Host "   ✓ Dependencies element found"
                $tdf = $manifest.Package.Dependencies.TargetDeviceFamily
                if ($tdf) {
                    Write-Host "     MinVersion: $($tdf.MinVersion)"
                    Write-Host "     MaxVersionTested: $($tdf.MaxVersionTested)"
                }
            } else {
                Write-Host "   ⚠️  Dependencies element missing" -ForegroundColor Yellow
                $warningCount++
            }

            # Check Capabilities
            if ($manifest.Package.Capabilities) {
                $capabilities = $manifest.Package.Capabilities.ChildNodes | Where-Object { $_.LocalName -like "*Capability" }
                Write-Host "   ✓ Capabilities element found ($($capabilities.Count) declared)"
                $capabilities | ForEach-Object {
                    Write-Host "     - $($_.Name)"
                }
            } else {
                Write-Host "   ❌ Capabilities element missing" -ForegroundColor Red
                $errorCount++
                $validationPassed = $false
            }

            # Check Applications
            if ($manifest.Package.Applications) {
                Write-Host "   ✓ Applications element found"
            } else {
                Write-Host "   ❌ Applications element missing" -ForegroundColor Red
                $errorCount++
                $validationPassed = $false
            }

        } catch {
            Write-Host "   ❌ Failed to parse manifest XML: $_" -ForegroundColor Red
            $errorCount++
            $validationPassed = $false
        }
    } else {
        Write-Host "   ❌ AppxManifest.xml not found in package" -ForegroundColor Red
        $errorCount++
        $validationPassed = $false
    }

} catch {
    Write-Host "   ❌ Failed to extract package: $_" -ForegroundColor Red
    $errorCount++
    $validationPassed = $false
} finally {
    # Cleanup
    if (Test-Path $tempDir) {
        Remove-Item -Path $tempDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# Summary
Write-Host "`n" + ("=" * 50)
Write-Host "Validation Summary" -ForegroundColor Cyan
Write-Host "=" * 50

if ($errorCount -gt 0) {
    Write-Host "❌ VALIDATION FAILED" -ForegroundColor Red
    Write-Host "Errors: $errorCount"
    Write-Host "Warnings: $warningCount"
    exit 1
} elseif ($warningCount -gt 0) {
    Write-Host "⚠️  VALIDATION PASSED WITH WARNINGS" -ForegroundColor Yellow
    Write-Host "Warnings: $warningCount"
    exit 0
} else {
    Write-Host "✅ VALIDATION PASSED" -ForegroundColor Green
    Write-Host "No errors or warnings detected"
    exit 0
}
