#Requires -Version 5.1
<#
.SYNOPSIS
Validate a built MSIX package before Store submission.

.DESCRIPTION
Unpacks the package with the Windows SDK MakeAppx tool and checks:
- Required manifest elements (Identity, Properties, Dependencies, Resources, Applications)
- Version number format
- Full-trust desktop app wiring (Executable + EntryPoint + runFullTrust)
- Whether the identity is still the checked-in placeholder (blocks Store submission)
- Every logo referenced by the manifest exists, is a PNG, has the exact expected pixel
  dimensions, and carries an alpha channel
- The dynamic CRT DLLs the executable imports are present in the payload

This is a local sanity check, not a substitute for Partner Center's own validation.
It cannot predict certification outcomes.

.PARAMETER PackagePath
Path to the .msix or .msixbundle to validate.

.EXAMPLE
.\Validate-Package.ps1 -PackagePath build\store-packages\JyGlobalVST_1.0.0.0_x64.msix
#>

param(
    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path $_ -PathType Leaf })]
    [string]$PackagePath
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot '_Common.ps1')

$errorCount = 0
$warningCount = 0
function Add-Error { param([string]$m) Write-Host "    [ERROR] $m" -ForegroundColor Red; $script:errorCount++ }
function Add-Warning { param([string]$m) Write-Host "    [warn]  $m" -ForegroundColor Yellow; $script:warningCount++ }

Write-Host 'MSIX package validation' -ForegroundColor White
Write-Host "Package: $(Split-Path -Leaf $PackagePath)"

$makeAppx = Find-SdkTool 'makeappx.exe'
if (-not $makeAppx) { Write-Fail 'MakeAppx.exe not found. Install the Windows 10/11 SDK.' }

$isBundle = [System.IO.Path]::GetExtension($PackagePath) -eq '.msixbundle'
$workDir = Join-Path ([System.IO.Path]::GetTempPath()) "msix_validate_$PID"
if (Test-Path $workDir) { Remove-Item -Recurse -Force $workDir }
New-Item -ItemType Directory -Force -Path $workDir | Out-Null

try
{
    # --- Unpack --------------------------------------------------------------
    Write-Step 'Unpacking package'
    $extractDir = Join-Path $workDir 'extracted'

    if ($isBundle)
    {
        $bundleDir = Join-Path $workDir 'bundle'
        & $makeAppx unbundle /p $PackagePath /d $bundleDir /o | Out-Null
        if ($LASTEXITCODE -ne 0) { Write-Fail 'MakeAppx unbundle failed.' }

        $inner = Get-ChildItem -Path $bundleDir -Filter '*.msix' -Recurse | Select-Object -First 1
        if (-not $inner) { Write-Fail 'No .msix found inside the bundle.' }
        Write-Ok "Inner package: $($inner.Name)"

        & $makeAppx unpack /p $inner.FullName /d $extractDir /o | Out-Null
    }
    else
    {
        & $makeAppx unpack /p $PackagePath /d $extractDir /o | Out-Null
    }
    if ($LASTEXITCODE -ne 0) { Write-Fail 'MakeAppx unpack failed.' }

    Write-Ok "Size: $('{0:N2}' -f ((Get-Item $PackagePath).Length / 1MB)) MB"

    # --- Manifest ------------------------------------------------------------
    Write-Step 'Checking AppxManifest.xml'

    $manifestFile = Join-Path $extractDir 'AppxManifest.xml'
    if (-not (Test-Path $manifestFile)) { Write-Fail 'AppxManifest.xml not present in package.' }

    [xml]$manifest = Get-Content $manifestFile -Raw
    $pkg = $manifest.Package

    foreach ($element in @('Identity', 'Properties', 'Dependencies', 'Resources', 'Applications', 'Capabilities'))
    {
        if ($pkg.$element) { Write-Ok "$element present" } else { Add-Error "$element element missing" }
    }

    $identity = $pkg.Identity
    if ($identity)
    {
        Write-Host "    Name      : $($identity.Name)"
        Write-Host "    Publisher : $($identity.Publisher)"
        Write-Host "    Version   : $($identity.Version)"
        Write-Host "    Arch      : $($identity.ProcessorArchitecture)"

        if ($identity.Version -notmatch '^\d+\.\d+\.\d+\.\d+$')
        {
            Add-Error "Version '$($identity.Version)' is not Major.Minor.Build.Revision"
        }

        if (Test-PlaceholderIdentity -PackageName $identity.Name -Publisher $identity.Publisher)
        {
            Add-Warning 'Identity is the checked-in placeholder - NOT submittable to the Store. Rebuild with the Partner Center identity (see Build-MSIX.ps1 -PackageName/-Publisher).'
        }
    }

    # --- Full-trust desktop app wiring ---------------------------------------
    Write-Step 'Checking desktop application wiring'

    $app = $pkg.Applications.Application
    if (-not $app)
    {
        Add-Error 'No Application element found'
    }
    else
    {
        if ($app.Executable) { Write-Ok "Executable: $($app.Executable)" }
        else { Add-Error 'Application/@Executable missing (a packaged Win32 app must not use StartPage)' }

        if ($app.EntryPoint -eq 'Windows.FullTrustApplication')
        {
            Write-Ok 'EntryPoint: Windows.FullTrustApplication'
        }
        else
        {
            Add-Error "Application/@EntryPoint is '$($app.EntryPoint)', expected Windows.FullTrustApplication"
        }

        if ($app.Executable -and -not (Test-Path (Join-Path $extractDir $app.Executable)))
        {
            Add-Error "Declared executable '$($app.Executable)' is not in the package"
        }
    }

    $capNames = @($pkg.Capabilities.ChildNodes | Where-Object { $_.LocalName -like '*Capability' } | ForEach-Object { $_.Name })
    Write-Host "    Capabilities: $($capNames -join ', ')"
    if ($capNames -notcontains 'runFullTrust')
    {
        Add-Error 'runFullTrust capability missing - a packaged Win32 desktop app requires it'
    }

    # --- Logos ---------------------------------------------------------------
    Write-Step 'Checking logo assets'
    Add-Type -AssemblyName System.Drawing

    # Manifest attribute/element -> required pixel dimensions
    $logoRefs = @(
        @{ Path = $pkg.Properties.Logo; Expect = @(50, 50); Source = 'Properties/Logo' }
        @{ Path = $app.VisualElements.Square150x150Logo; Expect = @(150, 150); Source = 'Square150x150Logo' }
        @{ Path = $app.VisualElements.Square44x44Logo; Expect = @(44, 44); Source = 'Square44x44Logo' }
    )

    foreach ($ref in $logoRefs)
    {
        if (-not $ref.Path) { Add-Error "$($ref.Source) not declared"; continue }

        $logoPath = Join-Path $extractDir ($ref.Path -replace '/', '\')
        if (-not (Test-Path $logoPath)) { Add-Error "$($ref.Source): file missing from package ($($ref.Path))"; continue }

        if ([System.IO.Path]::GetExtension($logoPath) -ne '.png')
        {
            Add-Error "$($ref.Source): must be PNG, got $([System.IO.Path]::GetExtension($logoPath))"
            continue
        }

        $img = [System.Drawing.Image]::FromFile($logoPath)
        try
        {
            $w = $img.Width; $h = $img.Height
            $hasAlpha = [System.Drawing.Image]::IsAlphaPixelFormat($img.PixelFormat)

            if ($w -ne $ref.Expect[0] -or $h -ne $ref.Expect[1])
            {
                Add-Error "$($ref.Source): expected $($ref.Expect[0])x$($ref.Expect[1]), got ${w}x${h}"
            }
            elseif (-not $hasAlpha)
            {
                Add-Warning "$($ref.Source): ${w}x${h} but no alpha channel ($($img.PixelFormat))"
            }
            else
            {
                Write-Ok "$($ref.Source): ${w}x${h}, alpha OK"
            }
        }
        finally
        {
            $img.Dispose()
        }
    }

    # --- Runtime dependencies ------------------------------------------------
    Write-Step 'Checking bundled runtime DLLs'

    # WindowsApps is not a redist search path, so the dynamic CRT must be in the payload.
    $requiredCrt = @('MSVCP140.dll', 'VCRUNTIME140.dll', 'VCRUNTIME140_1.dll')
    $present = Get-ChildItem -Path $extractDir -Filter '*.dll' | ForEach-Object { $_.Name }

    foreach ($dll in $requiredCrt)
    {
        if ($present -contains $dll) { Write-Ok "$dll present" }
        else { Add-Error "$dll missing - the packaged app will fail to start" }
    }
}
finally
{
    if (Test-Path $workDir) { Remove-Item -Recurse -Force $workDir -ErrorAction SilentlyContinue }
}

# --- Summary -----------------------------------------------------------------
Write-Host "`n$('-' * 72)"
if ($errorCount -gt 0)
{
    Write-Host "VALIDATION FAILED - $errorCount error(s), $warningCount warning(s)" -ForegroundColor Red
    exit 1
}
elseif ($warningCount -gt 0)
{
    Write-Host "VALIDATION PASSED WITH WARNINGS - $warningCount warning(s)" -ForegroundColor Yellow
    exit 0
}
else
{
    Write-Host 'VALIDATION PASSED' -ForegroundColor Green
    exit 0
}
