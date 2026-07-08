# tests/audit/rt_audit.ps1
# T106 — Real-time constraint audit per Constitution §V.
#
# Gate: zero unannotated hits for forbidden patterns inside audio-thread
# entry-point functions (audioDeviceIOCallbackWithContext / processBlock).
# We approximate "reachable from processBlock" by only checking lines that
# fall inside the body of these functions.
#
# Forbidden in audio-thread code: malloc, new (heap alloc), std::vector::push_back,
# std::mutex acquisition.

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

$forbiddenPatterns = @(
    'malloc\s*\(',
    '(?<!placement\s+)new\s+\w',
    '\.push_back\s*\(',
    'std::mutex',
    '\.lock\s*\(',
    'std::lock_guard',
    'std::unique_lock',
    'std::recursive_mutex'
)

function Get-AudioFunctionRanges
{
    param([string[]]$Lines)

    $ranges = @()
    $inFunction = $false
    $braceDepth = 0
    $startLine = -1

    for ($i = 0; $i -lt $Lines.Count; $i++)
    {
        $line = $Lines[$i]

        if (-not $inFunction)
        {
            # Detect start of audio-thread entry point.
            if ($line -match '\b(audioDeviceIOCallbackWithContext|processBlock)\s*\(')
            {
                $inFunction = $true
                $startLine = $i
                # Count braces on this line.
                $braceDepth = [regex]::Matches($line, '\{').Count - [regex]::Matches($line, '\}').Count
                if ($braceDepth -gt 0)
                {
                    # function body starts on same line.
                }
            }
        }
        else
        {
            $braceDepth += [regex]::Matches($line, '\{').Count - [regex]::Matches($line, '\}').Count
            if ($braceDepth -le 0)
            {
                $ranges += [PSCustomObject]@{ Start = $startLine; End = $i }
                $inFunction = $false
                $braceDepth = 0
            }
        }
    }

    return $ranges
}

$unannotatedHits = 0

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
        $lines = Get-Content $file.FullName
        $ranges = Get-AudioFunctionRanges -Lines $lines
        if ($ranges.Count -eq 0)
        {
            continue
        }

        foreach ($range in $ranges)
        {
            for ($i = $range.Start; $i -le $range.End; $i++)
            {
                $line = $lines[$i]
                foreach ($pat in $forbiddenPatterns)
                {
                    if ($line -match $pat)
                    {
                        # Allow if annotated with RT-SAFE on same or previous line.
                        $annotated = $false
                        if ($line -match '// RT-SAFE:')
                        {
                            $annotated = $true
                        }
                        elseif ($i -gt 0 -and $lines[$i - 1] -match '// RT-SAFE:')
                        {
                            $annotated = $true
                        }

                        if (-not $annotated)
                        {
                            $unannotatedHits++
                            $rel = [System.IO.Path]::GetRelativePath($Root, $file.FullName)
                            Write-Host "UNANNOTATED: $rel :$($i+1)  $line" -ForegroundColor Red
                        }
                    }
                }
            }
        }
    }
}

Write-Host ""
Write-Host "T106 RT-Safety Audit Results" -ForegroundColor Cyan
Write-Host "  Unannotated forbidden hits in audio-thread functions: $unannotatedHits"

if ($unannotatedHits -gt 0)
{
    Write-Host "  STATUS: FAIL" -ForegroundColor Red
    if ($FailOnFind) { exit 1 }
}
else
{
    Write-Host "  STATUS: PASS" -ForegroundColor Green
}
