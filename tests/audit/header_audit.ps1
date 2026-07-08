# tests/audit/header_audit.ps1
# T107 — REALTIME CONSTRAINTS header presence audit per Constitution §V.
#
# Every .cpp/.h file under src/audio-engine/routing/, chain/, vst-host/,
# monitoring/ MUST carry the string "REALTIME CONSTRAINTS" in a comment
# within the first 50 lines.

param(
    [string]$Root = "$PSScriptRoot/../..",
    [switch]$FailOnFind
)

$ErrorActionPreference = 'Stop'

$audioDirs = @(
    'src/audio-engine/routing',
    'src/audio-engine/chain',
    'src/audio-engine/vst-host',
    'src/audio-engine/monitoring'
)

$missing = 0

foreach ($dir in $audioDirs)
{
    $fullDir = Join-Path $Root $dir
    if (-not (Test-Path $fullDir))
    {
        continue
    }

    $files = Get-ChildItem -Path $fullDir -Include '*.cpp','*.h' -Recurse -File
    foreach ($file in $files)
    {
        $rel = $file.FullName.Substring($Root.Length).TrimStart('\', '/')
        $lines = Get-Content $file.FullName -TotalCount 50
        $found = $false
        foreach ($line in $lines)
        {
            if ($line -match 'REALTIME CONSTRAINTS')
            {
                $found = $true
                break
            }
        }
        if (-not $found)
        {
            $missing++
            Write-Host "MISSING HEADER: $rel" -ForegroundColor Red
        }
    }
}

Write-Host ""
Write-Host "T107 Header Audit Results" -ForegroundColor Cyan
Write-Host "  Files missing REALTIME CONSTRAINTS header: $missing"

if ($missing -gt 0)
{
    Write-Host "  STATUS: FAIL" -ForegroundColor Red
    if ($FailOnFind) { exit 1 }
}
else
{
    Write-Host "  STATUS: PASS" -ForegroundColor Green
}
