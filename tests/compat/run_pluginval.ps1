# tests/compat/run_pluginval.ps1
#
# T029 — Pluginval CLI runner for CI integration.
#
# Scans for VST3 plugins in standard locations, runs them through Pluginval,
# and reports pass/fail status for ARC X, Sonarworks Reference, ReaEQ, FabFilter Pro-Q 3.
#
# Usage:
#   .\run_pluginval.ps1 [-PluginPaths @("path1", "path2")] [-Verbose]

param(
    [string[]]$PluginPaths = @(
        "${env:ProgramFiles}\Common Files\VST3",
        "${env:LocalAppData}\Programs\Common\VST3"
    ),
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"
$PluginvalExe = "pluginval.exe"

# Plugins to test in CI (reference set per tasks.md T029).
$TestPlugins = @(
    "ARC X",
    "Sonarworks Reference",
    "ReaEQ",
    "FabFilter Pro-Q 3"
)

function Get-PluginvalPath {
    # Try to find pluginval.exe in PATH.
    $found = Get-Command $PluginvalExe -ErrorAction SilentlyContinue
    if ($found) {
        return $found.Source
    }

    # Check common locations.
    $commonPaths = @(
        "${env:ProgramFiles}\JUCE\pluginval\pluginval.exe",
        "${env:LocalAppData}\Programs\JUCE\pluginval\pluginval.exe"
    )
    foreach ($path in $commonPaths) {
        if (Test-Path $path) {
            return $path
        }
    }

    throw "pluginval.exe not found. Ensure it is installed and in PATH."
}

function Find-VST3Plugins {
    param([string[]]$SearchPaths)

    $plugins = @()

    foreach ($path in $SearchPaths) {
        if (-not (Test-Path $path)) {
            if ($Verbose) {
                Write-Host "Skipping non-existent path: $path"
            }
            continue
        }

        Get-ChildItem -Path $path -Filter "*.vst3" -Recurse -ErrorAction SilentlyContinue |
            ForEach-Object {
                $plugins += @{
                    Name = $_.BaseName
                    Path = $_.FullName
                }
            }
    }

    return $plugins
}

function Invoke-Pluginval {
    param(
        [string]$PluginvalPath,
        [string]$PluginPath
    )

    try {
        & $PluginvalPath --validate "$PluginPath" 2>&1
        return $LASTEXITCODE
    }
    catch {
        if ($Verbose) {
            Write-Host "Error running pluginval on $PluginPath : $_"
        }
        return -1
    }
}

function Test-PluginvalResults {
    param(
        [string]$PluginPath,
        [int]$ExitCode
    )

    if ($ExitCode -eq 0) {
        Write-Host "PASS: $PluginPath"
        return $true
    }
    else {
        Write-Host "FAIL: $PluginPath (exit code: $ExitCode)"
        return $false
    }
}

# Main execution
Write-Host "JyGlobalVST Pluginval CI Runner"
Write-Host "================================"

$pluginvalPath = Get-PluginvalPath
Write-Host "Using pluginval: $pluginvalPath"

$allPlugins = Find-VST3Plugins -SearchPaths $PluginPaths
Write-Host "Found $($allPlugins.Count) VST3 plugin(s)"

if ($allPlugins.Count -eq 0) {
    Write-Warning "No VST3 plugins found in search paths"
    exit 1
}

# Filter to test plugins we care about.
$pluginsToTest = $allPlugins | Where-Object { $TestPlugins -contains $_.Name }

if ($pluginsToTest.Count -eq 0) {
    Write-Warning "No reference test plugins found (looking for: $($TestPlugins -join ', '))"
    Write-Host "Available plugins:"
    $allPlugins | ForEach-Object { Write-Host "  - $($_.Name)" }
    exit 1
}

Write-Host "Testing $($pluginsToTest.Count) reference plugin(s):"
$passCount = 0
$failCount = 0

foreach ($plugin in $pluginsToTest) {
    if ($Verbose) {
        Write-Host "Testing: $($plugin.Name)"
    }

    $exitCode = Invoke-Pluginval -PluginvalPath $pluginvalPath -PluginPath $plugin.Path
    if (Test-PluginvalResults -PluginPath $plugin.Name -ExitCode $exitCode) {
        $passCount++
    }
    else {
        $failCount++
    }
}

Write-Host ""
Write-Host "Pluginval results: $passCount PASS, $failCount FAIL"

if ($failCount -gt 0) {
    exit 1
}
exit 0
