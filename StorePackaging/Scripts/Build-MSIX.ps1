#Requires -Version 5.1
<#
.SYNOPSIS
Build the Microsoft Store submission package for JyGlobalVST.

.DESCRIPTION
Produces an UNSIGNED .msix and .msixbundle suitable for upload to Partner Center.
Store submissions must not carry a developer signature - Partner Center re-signs the
package with the Store certificate. For a locally installable, self-signed package
use Build-MSIX-Local.ps1 instead.

Packaging is done with the Windows SDK MakeAppx tool directly. It does NOT use
JyGlobalVST.msixproj, which was never a valid packaging project (it declared
ConfigurationType=AppPackage and imported Microsoft.Cpp.targets, rather than being a
.wapproj importing Microsoft.DesktopBridge.targets).

IDENTITY: a Store package must carry the Identity that Partner Center assigns to your
reserved app name. The values checked into AppxManifest.xml are placeholders that match
the local self-signed test certificate, and Partner Center will reject them. Supply the
real values via -PackageName / -Publisher / -PublisherDisplayName, or accept a
non-submittable package with -AllowPlaceholderIdentity.

Find these in Partner Center under your app > Product management > Product identity.

.PARAMETER Version
Package version, Major.Minor.Build.Revision. Defaults to version-info.txt.

.PARAMETER PackageName
Identity/@Name from Partner Center, e.g. "12345TheCompany.JyGlobalVST".

.PARAMETER Publisher
Identity/@Publisher from Partner Center. Full subject, e.g.
"CN=1B2C3D4E-5F60-7182-93A4-B5C6D7E8F901".

.PARAMETER PublisherDisplayName
Properties/PublisherDisplayName from Partner Center - your seller display name.

.PARAMETER DisplayName
The app name shown in the Store and Start menu. For a Store submission this must match a
name you reserved in Partner Center. Defaults to the manifest value.

.PARAMETER AllowPlaceholderIdentity
Build even though the identity is still the checked-in placeholder. The output is
useful for validation and inspection but CANNOT be submitted to the Store.

.PARAMETER Configuration
Release (default) or Debug. Store submissions should always be Release.

.PARAMETER SkipBuild
Reuse the already-built executable instead of invoking CMake.

.PARAMETER BuildDir
CMake build directory. Defaults to <repo>\build.

.EXAMPLE
.\Build-MSIX.ps1 -PackageName "12345Contoso.JyGlobalVST" `
                 -Publisher "CN=1B2C3D4E-5F60-7182-93A4-B5C6D7E8F901" `
                 -PublisherDisplayName "Contoso"

.EXAMPLE
# Inspect the packaging pipeline without real Store identity
.\Build-MSIX.ps1 -AllowPlaceholderIdentity -SkipBuild
#>

param(
    [Parameter(Mandatory = $false)]
    [ValidatePattern('^\d+\.\d+\.\d+\.\d+$')]
    [string]$Version,

    [Parameter(Mandatory = $false)]
    [string]$PackageName,

    [Parameter(Mandatory = $false)]
    [string]$Publisher,

    [Parameter(Mandatory = $false)]
    [string]$PublisherDisplayName,

    [Parameter(Mandatory = $false)]
    [string]$DisplayName,

    [Parameter(Mandatory = $false)]
    [switch]$AllowPlaceholderIdentity,

    [Parameter(Mandatory = $false)]
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',

    [Parameter(Mandatory = $false)]
    [switch]$SkipBuild,

    [Parameter(Mandatory = $false)]
    [string]$BuildDir
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

. (Join-Path $PSScriptRoot '_Common.ps1')

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$storeDir = Join-Path $repoRoot 'StorePackaging'
if (-not $BuildDir) { $BuildDir = Join-Path $repoRoot 'build' }

Write-Host 'JyGlobalVST - Microsoft Store package build' -ForegroundColor White
Write-Host "Repo: $repoRoot"

if (-not $Version) { $Version = Get-VersionFromFile -StoreDir $storeDir }
if (-not $Version) { Write-Fail 'No version supplied and none found in version-info.txt.' }
Write-Host "Version: $Version | Config: $Configuration"

if ($Configuration -ne 'Release')
{
    Write-Warn 'Store submissions should be built as Release. Continuing anyway.'
}

# --- SDK tools ---------------------------------------------------------------
Write-Step 'Locating Windows SDK tools'
$makeAppx = Find-SdkTool 'makeappx.exe'
if (-not $makeAppx) { Write-Fail 'MakeAppx.exe not found. Install the Windows 10/11 SDK.' }
Write-Ok "MakeAppx: $makeAppx"

# --- Build ------------------------------------------------------------------
$exePath = Get-TrayExePath -BuildDir $BuildDir -Configuration $Configuration

if ($SkipBuild)
{
    Write-Step 'Skipping build (-SkipBuild)'
}
else
{
    Write-Step "Building JyGlobalVST ($Configuration)"
    Invoke-AppBuild -RepoRoot $repoRoot -BuildDir $BuildDir -Configuration $Configuration
}
if (-not (Test-Path $exePath)) { Write-Fail "Executable not found: $exePath" }
Write-Ok "Executable: $exePath ($('{0:N2}' -f ((Get-Item $exePath).Length / 1MB)) MB)"

# --- Stage payload ----------------------------------------------------------
Write-Step 'Staging package payload'

# Store and local outputs live in separate directories: the two scripts produce
# same-named packages with different identity and signing, so a shared directory would
# silently clobber one with the other.
$outDir = Join-Path $BuildDir 'store-packages\store'
$payloadDir = Join-Path $outDir 'payload'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$payload = New-PackagePayload -StoreDir $storeDir -PayloadDir $payloadDir -ExePath $exePath `
    -Version $Version -PackageName $PackageName -Publisher $Publisher `
    -PublisherDisplayName $PublisherDisplayName -DisplayName $DisplayName

Write-Ok "Assets: $($payload.AssetCount) file(s)"
if ($payload.CrtCopied -gt 0)
{
    Write-Ok "VC++ runtime: $($payload.CrtCopied) DLL(s)"
}
else
{
    Write-Warn 'No VC++ runtime DLLs staged - the packaged app will likely fail to start.'
}
Write-Ok "Identity Name: $($payload.PackageName)"
Write-Ok "Identity Publisher: $($payload.Publisher)"
Write-Ok "PublisherDisplayName: $($payload.PublisherDisplayName)"
Write-Ok "DisplayName: $($payload.DisplayName)"

# --- Identity gate ----------------------------------------------------------
$isPlaceholder = Test-PlaceholderIdentity -PackageName $payload.PackageName -Publisher $payload.Publisher

if ($isPlaceholder)
{
    if (-not $AllowPlaceholderIdentity)
    {
        Write-Host ''
        Write-Host 'The package identity is still the checked-in placeholder:' -ForegroundColor Red
        Write-Host "  Name      = $($payload.PackageName)"
        Write-Host "  Publisher = $($payload.Publisher)"
        Write-Host ''
        Write-Host 'Partner Center will reject this. Supply the real values from' -ForegroundColor Red
        Write-Host 'Partner Center > your app > Product management > Product identity:' -ForegroundColor Red
        Write-Host '  -PackageName "<Identity Name>" -Publisher "<Identity Publisher>" -PublisherDisplayName "<seller name>"'
        Write-Host ''
        Write-Host 'Or pass -AllowPlaceholderIdentity to build a non-submittable package anyway.'
        Write-Fail 'Refusing to build a Store package with placeholder identity.'
    }
    Write-Warn 'PLACEHOLDER IDENTITY - this package CANNOT be submitted to the Store.'
}

# --- Pack -------------------------------------------------------------------
Write-Step 'Packing MSIX'

$msixName = "JyGlobalVST_${Version}_x64.msix"
$msixPath = Join-Path $outDir $msixName
if (Test-Path $msixPath) { Remove-Item -Force $msixPath }

& $makeAppx pack /d $payloadDir /p $msixPath /o
if ($LASTEXITCODE -ne 0) { Write-Fail 'MakeAppx pack failed.' }
Write-Ok "Package: $msixName ($('{0:N2}' -f ((Get-Item $msixPath).Length / 1MB)) MB)"

# --- Bundle -----------------------------------------------------------------
# Only x64 is in scope, so the bundle wraps a single package. It is produced because
# Partner Center and the release pipeline expect a .msixbundle; a bare .msix is also
# accepted for a single-architecture app.
Write-Step 'Creating MSIX bundle'

$bundleInput = Join-Path $outDir 'bundle-input'
if (Test-Path $bundleInput) { Remove-Item -Recurse -Force $bundleInput }
New-Item -ItemType Directory -Force -Path $bundleInput | Out-Null
Copy-Item $msixPath $bundleInput

$bundlePath = Join-Path $outDir "JyGlobalVST_${Version}_x64.msixbundle"
if (Test-Path $bundlePath) { Remove-Item -Force $bundlePath }

& $makeAppx bundle /d $bundleInput /p $bundlePath /bv $Version /o
if ($LASTEXITCODE -ne 0) { Write-Fail 'MakeAppx bundle failed.' }
Remove-Item -Recurse -Force $bundleInput
Write-Ok "Bundle: $(Split-Path -Leaf $bundlePath) ($('{0:N2}' -f ((Get-Item $bundlePath).Length / 1MB)) MB)"

# Manifest copy for reference / diffing between releases
Copy-Item (Join-Path $payloadDir 'AppxManifest.xml') (Join-Path $outDir 'AppxManifest.xml') -Force

# --- Validate ---------------------------------------------------------------
$validateScript = Join-Path $PSScriptRoot 'Validate-Package.ps1'
if (Test-Path $validateScript)
{
    Write-Step 'Validating package'
    & $validateScript -PackagePath $msixPath
    if ($LASTEXITCODE -ne 0) { Write-Fail 'Package validation failed.' }
}

# --- Summary ----------------------------------------------------------------
Write-Host "`n$('-' * 72)"
Write-Host 'STORE PACKAGE BUILD COMPLETE' -ForegroundColor Green
Write-Host $('-' * 72)
Write-Host "Package : $msixPath"
Write-Host "Bundle  : $bundlePath"
Write-Host 'Signing : none (Partner Center signs Store submissions)'

if ($isPlaceholder)
{
    Write-Host ''
    Write-Host 'NOT SUBMITTABLE: placeholder identity. See -PackageName / -Publisher.' -ForegroundColor Yellow
}
else
{
    Write-Host ''
    Write-Host 'Upload the .msixbundle at Partner Center > your app > new submission > Packages.'
    Write-Host 'Remaining submission requirements are tracked in'
    Write-Host '  StorePackaging\Documentation\Partner-Center-Checklist.md'
}

exit 0
