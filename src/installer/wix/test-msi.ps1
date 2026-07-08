##
## Test MSI Installation & Uninstallation
## Usage: .\test-msi.ps1 -MsiPath "path\to\GlobalVSTHost-1.0.0-x64.msi" -Test Silent
##

param(
    [string]$MsiPath = "$PSScriptRoot\..\..\..\..\build\GlobalVSTHost-1.0.0-x64.msi",
    [ValidateSet("Silent", "Interactive", "Uninstall")]
    [string]$Test = "Silent"
)

# Verify MSI exists
if (-not (Test-Path $MsiPath)) {
    Write-Error "MSI not found: $MsiPath"
    Write-Host "Build MSI first: cmake --build build --config Release --target msi_installer"
    exit 1
}

Write-Host "Testing MSI: $MsiPath" -ForegroundColor Green
Write-Host "Test type: $Test" -ForegroundColor Green

switch ($Test) {
    "Silent" {
        Write-Host ""
        Write-Host "=== Test 1: Silent Installation ===" -ForegroundColor Cyan
        Write-Host "Running: msiexec /i `"$MsiPath`" /quiet"
        msiexec /i "$MsiPath" /quiet

        if ($LASTEXITCODE -eq 0) {
            Write-Host "✓ Silent install succeeded" -ForegroundColor Green
        } else {
            Write-Error "✗ Silent install failed with exit code $LASTEXITCODE"
            exit 1
        }

        Write-Host ""
        Write-Host "=== Verifying Installation ===" -ForegroundColor Cyan

        # Check executable
        $exePath = "$env:LOCALAPPDATA\JyGlobalVST\JyGlobalVST.exe"
        if (Test-Path $exePath) {
            Write-Host "✓ Executable found: $exePath" -ForegroundColor Green
        } else {
            Write-Error "✗ Executable NOT found: $exePath"
            exit 1
        }

        # Check DLL
        $dllPath = "$env:LOCALAPPDATA\JyGlobalVST\jyglobalvst_audio_engine.dll"
        if (Test-Path $dllPath) {
            Write-Host "✓ Audio engine DLL found: $dllPath" -ForegroundColor Green
        } else {
            Write-Error "✗ Audio engine DLL NOT found: $dllPath"
            exit 1
        }

        # Check Add/Remove Programs
        $uninstallKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\JyGlobalVST"
        if (Test-Path $uninstallKey) {
            $displayName = (Get-ItemProperty $uninstallKey).DisplayName
            $displayVersion = (Get-ItemProperty $uninstallKey).DisplayVersion
            Write-Host "✓ Registry entry found: $displayName v$displayVersion" -ForegroundColor Green
        } else {
            Write-Error "✗ Registry entry NOT found"
            exit 1
        }

        Write-Host ""
        Write-Host "=== Test 2: Silent Uninstall ===" -ForegroundColor Cyan

        # Get ProductCode from registry
        $uninstallEntry = Get-ChildItem "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall" |
            Where-Object { (Get-ItemProperty $_).DisplayName -like "*Global VST*" }

        if ($null -eq $uninstallEntry) {
            Write-Error "✗ Cannot find uninstall registry entry"
            exit 1
        }

        $guid = $uninstallEntry.PSChildName
        Write-Host "Found ProductCode: $guid" -ForegroundColor Gray
        Write-Host "Running: msiexec /x $guid /quiet"

        msiexec /x $guid /quiet

        if ($LASTEXITCODE -eq 0) {
            Write-Host "✓ Silent uninstall succeeded" -ForegroundColor Green
        } else {
            Write-Error "✗ Silent uninstall failed with exit code $LASTEXITCODE"
            exit 1
        }

        # Verify removal
        if (-not (Test-Path $exePath)) {
            Write-Host "✓ Executable removed: $exePath" -ForegroundColor Green
        } else {
            Write-Error "✗ Executable still exists after uninstall"
            exit 1
        }

        if (-not (Test-Path "$env:LOCALAPPDATA\JyGlobalVST")) {
            Write-Host "✓ Installation folder removed" -ForegroundColor Green
        } else {
            Write-Warning "⚠ Installation folder still exists (may contain user data)"
        }

        Write-Host ""
        Write-Host "=== All Tests Passed ===" -ForegroundColor Green
        Write-Host "The MSI supports silent install/uninstall and is ready for WinGet submission."
    }

    "Interactive" {
        Write-Host ""
        Write-Host "=== Interactive Installation ===" -ForegroundColor Cyan
        Write-Host "Running installer UI. Follow the wizard to complete installation."
        Write-Host ""

        msiexec /i "$MsiPath"

        Write-Host ""
        Write-Host "=== Verifying Installation ===" -ForegroundColor Cyan

        $exePath = "$env:LOCALAPPDATA\JyGlobalVST\JyGlobalVST.exe"
        if (Test-Path $exePath) {
            Write-Host "✓ Installation verified" -ForegroundColor Green
        } else {
            Write-Warning "⚠ Installation may have been cancelled"
        }
    }

    "Uninstall" {
        Write-Host ""
        Write-Host "=== Uninstall ===" -ForegroundColor Cyan

        $uninstallEntry = Get-ChildItem "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall" |
            Where-Object { (Get-ItemProperty $_).DisplayName -like "*Global VST*" }

        if ($null -eq $uninstallEntry) {
            Write-Error "✗ Application not found in registry"
            exit 1
        }

        $guid = $uninstallEntry.PSChildName
        $displayName = (Get-ItemProperty $uninstallEntry).DisplayName

        Write-Host "Found: $displayName ($guid)"
        Write-Host "Running: msiexec /x $guid /quiet"

        msiexec /x $guid /quiet

        if ($LASTEXITCODE -eq 0) {
            Write-Host "✓ Uninstall succeeded" -ForegroundColor Green
        } else {
            Write-Error "✗ Uninstall failed with exit code $LASTEXITCODE"
            exit 1
        }
    }
}
