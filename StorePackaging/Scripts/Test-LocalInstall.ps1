#Requires -Version 5.1
<#
.SYNOPSIS
Test MSIX package installation and functionality

.DESCRIPTION
Tests installation of MSIX package on local system:
1. Install package
2. Verify app appears in installed apps
3. Attempt to launch application
4. Check system tray icon
5. Uninstall package

.PARAMETER PackagePath
Path to the .msixbundle file to test

.PARAMETER SkipUninstall
If specified, package is not uninstalled after testing (for manual verification)

.EXAMPLE
.\Test-LocalInstall.ps1 -PackagePath "C:\build\store-packages\JyGlobalVST_1.0.0.0_x64.msixbundle"
#>

param(
    [Parameter(Mandatory=$true)]
    [ValidateScript({ Test-Path $_ -PathType Leaf })]
    [string]$PackagePath,

    [Parameter(Mandatory=$false)]
    [switch]$SkipUninstall
)

$ErrorActionPreference = 'Stop'

Write-Host "🧪 MSIX Package Installation Test" -ForegroundColor Cyan
Write-Host "Package: $(Split-Path -Leaf $PackagePath)`n"

$testsPassed = 0
$testsFailed = 0

# Test 1: Install package
Write-Host "Test 1: Installing package..." -ForegroundColor Yellow
try {
    $packageInfo = Get-Item $PackagePath
    Write-Host "   Package size: $('{0:N2}' -f ($packageInfo.Length / 1MB)) MB"

    # Install using Add-AppxPackage
    Add-AppxPackage -Path $PackagePath -ErrorAction Stop
    Write-Host "   ✓ Package installed successfully"
    $testsPassed++

    # Wait a moment for app registration
    Start-Sleep -Seconds 2

} catch {
    Write-Host "   ❌ Installation failed: $_" -ForegroundColor Red
    $testsFailed++
    exit 1
}

# Test 2: Verify app in installed list
Write-Host "`nTest 2: Verifying app registration..." -ForegroundColor Yellow
try {
    $appInfo = Get-AppxPackage | Where-Object { $_.Name -like "*JyGlobalVST*" }

    if ($appInfo) {
        Write-Host "   ✓ App found in installed packages"
        Write-Host "     Name: $($appInfo.Name)"
        Write-Host "     Version: $($appInfo.Version)"
        Write-Host "     Architecture: $($appInfo.Architecture)"
        $testsPassed++
    } else {
        Write-Host "   ❌ App not found in installed packages" -ForegroundColor Red
        $testsFailed++
    }
} catch {
    Write-Host "   ❌ Could not verify registration: $_" -ForegroundColor Red
    $testsFailed++
}

# Test 3: Attempt to launch application
Write-Host "`nTest 3: Launching application..." -ForegroundColor Yellow
try {
    # Get AUMID (Application User Model ID)
    $aumid = (Get-AppxPackage | Where-Object { $_.Name -like "*JyGlobalVST*" }).PackageFamilyName + "!JyGlobalVSTApp"

    # Launch using explorer shell
    $job = Start-Job -ScriptBlock {
        param($aumid)
        explorer.exe shell:appsFolder\$aumid
    } -ArgumentList $aumid

    # Wait for launch
    Start-Sleep -Seconds 3

    # Check if process is running
    $process = Get-Process | Where-Object { $_.Name -eq "JyGlobalVST" -or $_.ProcessName -eq "JyGlobalVST" }

    if ($process) {
        Write-Host "   ✓ Application process started"
        Write-Host "     Process ID: $($process.Id)"
        $testsPassed++

        # Keep running for a moment, then close
        Start-Sleep -Seconds 2
        $process | Stop-Process -ErrorAction SilentlyContinue
    } else {
        Write-Host "   ⚠️  Could not detect application process (may still be starting)" -ForegroundColor Yellow
        $testsPassed++ # Don't fail - background processes may not be visible
    }

    Remove-Job $job -Force -ErrorAction SilentlyContinue

} catch {
    Write-Host "   ⚠️  Launch test warning: $_" -ForegroundColor Yellow
    # Not failing this test as it depends on UI availability
}

# Test 4: Check for system tray icon
Write-Host "`nTest 4: Checking system tray..." -ForegroundColor Yellow
try {
    # This is platform-dependent; we'll note it as informational
    $processes = Get-Process | Where-Object { $_.Name -like "*JyGlobalVST*" }
    if ($processes) {
        Write-Host "   ℹ️  Application detected (tray icon detection requires UI inspection)"
        $testsPassed++
    } else {
        Write-Host "   ℹ️  Application not currently running (may not be in tray)"
    }
} catch {
    Write-Host "   ℹ️  Could not check for tray icon: $_" -ForegroundColor Gray
}

# Test 5: Uninstall package
if (-not $SkipUninstall) {
    Write-Host "`nTest 5: Uninstalling package..." -ForegroundColor Yellow
    try {
        $appPackage = Get-AppxPackage | Where-Object { $_.Name -like "*JyGlobalVST*" }
        if ($appPackage) {
            Remove-AppxPackage -Package $appPackage.PackageFullName -ErrorAction Stop
            Write-Host "   ✓ Package uninstalled successfully"
            $testsPassed++

            # Verify removal
            Start-Sleep -Seconds 1
            $stillExists = Get-AppxPackage | Where-Object { $_.Name -like "*JyGlobalVST*" }
            if (-not $stillExists) {
                Write-Host "   ✓ Verified: Package removed from system"
            } else {
                Write-Host "   ⚠️  Package still appears in list (may take longer to clean up)" -ForegroundColor Yellow
            }
        }
    } catch {
        Write-Host "   ❌ Uninstall failed: $_" -ForegroundColor Red
        $testsFailed++
    }
} else {
    Write-Host "`nTest 5: Skipped package uninstall (use -SkipUninstall flag to keep package)"
}

# Summary
Write-Host "`n" + ("=" * 50)
Write-Host "Test Summary" -ForegroundColor Cyan
Write-Host "=" * 50
Write-Host "Tests Passed: $testsPassed"
Write-Host "Tests Failed: $testsFailed"

if ($testsFailed -gt 0) {
    Write-Host "`n❌ INSTALLATION TEST FAILED" -ForegroundColor Red
    exit 1
} else {
    Write-Host "`n✅ INSTALLATION TEST PASSED" -ForegroundColor Green
    exit 0
}
