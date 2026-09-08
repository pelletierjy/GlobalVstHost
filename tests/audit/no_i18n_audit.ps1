# tests/audit/no_i18n_audit.ps1
# T107a — English-only audit per FR-019c.
#
# Gate: zero hits for string-externalization artifacts in project source,
# OR each hit annotated with a justification comment.

param(
    [string]$Root = "$PSScriptRoot/../..",
    [switch]$FailOnFind
)

$ErrorActionPreference = 'Stop'

$patterns = @(
    '\.po\b',
    '\.pot\b',
    '\.mo\b',
    '\.resx\b',
    '\.resw\b',
    'gettext\s*\(',
    'wxLocale',
    'juce::TRANS\s*\(',
    'juce::LocalisedStrings'
)

$hits = 0

$files = Get-ChildItem -Path $Root -Include '*.cpp','*.h','*.hpp','*.py','*.ps1','*.md','*.cmake','*.txt' -Recurse -File |
    Where-Object {
        $fp = $_.FullName
        # Exclude third_party, build, .git, .specify, .tools, specs, and the audit scripts themselves.
        $fp -notmatch '\\(third_party|build|\.git|\.specify|\.tools|specs|tests\\audit)\\' -and
        $fp -notmatch '\\tasks\.md$'
    }

foreach ($file in $files)
{
    $lines = Get-Content $file.FullName
    for ($i = 0; $i -lt $lines.Count; $i++)
    {
        $line = $lines[$i]
        foreach ($pat in $patterns)
        {
            if ($line -match $pat)
            {
                # Allow if annotated with justification.
                if ($line -match '// i18n-OK:')
                {
                    continue
                }
                if ($i -gt 0 -and $lines[$i - 1] -match '// i18n-OK:')
                {
                    continue
                }

                $hits++
                $rel = [System.IO.Path]::GetRelativePath($Root, $file.FullName)
                Write-Host "HIT: $rel :$($i+1)  $line" -ForegroundColor Yellow
            }
        }
    }
}

Write-Host ""
Write-Host "T107a English-Only Audit Results" -ForegroundColor Cyan
Write-Host "  i18n artifact hits (FAIL if > 0): $hits"

if ($hits -gt 0)
{
    Write-Host "  STATUS: FAIL" -ForegroundColor Red
    if ($FailOnFind) { exit 1 }
}
else
{
    Write-Host "  STATUS: PASS" -ForegroundColor Green
}
