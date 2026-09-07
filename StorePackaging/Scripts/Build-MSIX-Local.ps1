#Requires -Version 5.1
<#
.SYNOPSIS
Build a self-signed MSIX package of JyGlobalVST for local sideload testing.

.DESCRIPTION
Produces an installable .msix without needing Partner Center or a real code-signing
certificate. Uses the Windows SDK tools (MakeAppx, SignTool) directly, so no Windows
Application Packaging Project / Visual Studio workload is required.

Steps:
  1. Locate Windows SDK tools (MakeAppx.exe, SignTool.exe)
  2. Build the tray app in Release (skippable with -SkipBuild)
  3. Stage the package payload: exe + VC++ runtime DLLs + Assets + AppxManifest.xml
  4. Pack with MakeAppx
  5. Create (or reuse) a self-signed code-signing cert whose subject matches the
     manifest Publisher, and sign the package with it
  6. Export the .cer so it can be trusted, and print the install commands

This package is for LOCAL TESTING ONLY. For a Store submission package use
Build-MSIX.ps1, which emits an unsigned package carrying your Partner Center identity.

.PARAMETER Version
Package version, Major.Minor.Build.Revision. Defaults to version-info.txt.

.PARAMETER Configuration
Release (default) or Debug.

.PARAMETER SkipBuild
Reuse the already-built executable instead of invoking CMake.

.PARAMETER BuildDir
CMake build directory. Defaults to <repo>\build.

.PARAMETER InstallCert
Import the signing certificate into LocalMachine\TrustedPeople so the package can be
installed. REQUIRES AN ELEVATED SHELL.

.PARAMETER Install
Install the package with Add-AppxPackage after building. Requires the cert to already
be trusted (see -InstallCert).

.EXAMPLE
# Build and sign; prints the install commands to run afterwards
.\Build-MSIX-Local.ps1

.EXAMPLE
# Rebuild and reinstall in one go (cert already trusted)
.\Build-MSIX-Local.ps1 -Install
#>

param(
    [Parameter(Mandatory = $false)]
    [ValidatePattern('^\d+\.\d+\.\d+\.\d+$')]
    [string]$Version,

    [Parameter(Mandatory = $false)]
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',

    [Parameter(Mandatory = $false)]
    [switch]$SkipBuild,

    [Parameter(Mandatory = $false)]
    [string]$BuildDir,

    [Parameter(Mandatory = $false)]
    [switch]$InstallCert,

    [Parameter(Mandatory = $false)]
    [switch]$Install
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

. (Join-Path $PSScriptRoot '_Common.ps1')

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$storeDir = Join-Path $repoRoot 'StorePackaging'
if (-not $BuildDir) { $BuildDir = Join-Path $repoRoot 'build' }

Write-Host 'JyGlobalVST - local MSIX build' -ForegroundColor White
Write-Host "Repo: $repoRoot"

if (-not $Version) { $Version = Get-VersionFromFile -StoreDir $storeDir }
if (-not $Version) { $Version = '1.1.0.0' }
Write-Host "Version: $Version | Config: $Configuration"

# --- SDK tools ---------------------------------------------------------------
Write-Step 'Locating Windows SDK tools'
$makeAppx = Find-SdkTool 'makeappx.exe'
$signTool = Find-SdkTool 'signtool.exe'
if (-not $makeAppx) { Write-Fail 'MakeAppx.exe not found. Install the Windows 10/11 SDK.' }
if (-not $signTool) { Write-Fail 'SignTool.exe not found. Install the Windows 10/11 SDK.' }
Write-Ok "MakeAppx: $makeAppx"
Write-Ok "SignTool: $signTool"

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

# Separate from the Store output directory: both scripts produce same-named packages
# with different identity and signing, so sharing a directory would clobber one.
$outDir = Join-Path $BuildDir 'store-packages\local'
$payloadDir = Join-Path $outDir 'payload'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$payload = New-PackagePayload -StoreDir $storeDir -PayloadDir $payloadDir -ExePath $exePath -Version $Version

Write-Ok "Assets: $($payload.AssetCount) file(s)"
if ($payload.CrtCopied -gt 0)
{
    Write-Ok "VC++ runtime: $($payload.CrtCopied) DLL(s) from $($payload.CrtDir)"
}
else
{
    Write-Warn 'VC++ redist folder not found - the packaged app may fail to start with a missing-DLL error.'
}
Write-Ok "Manifest publisher: $($payload.Publisher)"

# --- Pack -------------------------------------------------------------------
Write-Step 'Packing MSIX'

$msixPath = Join-Path $outDir "JyGlobalVST_${Version}_x64.msix"
if (Test-Path $msixPath) { Remove-Item -Force $msixPath }

& $makeAppx pack /d $payloadDir /p $msixPath /o
if ($LASTEXITCODE -ne 0) { Write-Fail 'MakeAppx pack failed.' }
Write-Ok "Package: $msixPath ($('{0:N2}' -f ((Get-Item $msixPath).Length / 1MB)) MB)"

# --- Signing certificate ----------------------------------------------------
Write-Step 'Preparing self-signed code-signing certificate'

# SignTool requires the cert subject to match the manifest Publisher exactly.
$publisher = $payload.Publisher
$cert = Get-ChildItem Cert:\CurrentUser\My |
    Where-Object { $_.Subject -eq $publisher -and $_.NotAfter -gt (Get-Date) -and $_.HasPrivateKey } |
    Sort-Object NotAfter -Descending |
    Select-Object -First 1

if ($cert)
{
    Write-Ok "Reusing existing cert $($cert.Thumbprint) (expires $($cert.NotAfter.ToString('yyyy-MM-dd')))"
}
else
{
    $notAfter = (Get-Date).AddYears(3)
    $cert = New-SelfSignedCertificate `
        -Type CodeSigningCert `
        -Subject $publisher `
        -KeyAlgorithm RSA `
        -KeyLength 2048 `
        -HashAlgorithm SHA256 `
        -KeyUsage DigitalSignature `
        -CertStoreLocation Cert:\CurrentUser\My `
        -NotAfter $notAfter `
        -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3', '2.5.29.19={text}')
    Write-Ok "Created cert $($cert.Thumbprint) (expires $($notAfter.ToString('yyyy-MM-dd')))"
}

$cerPath = Join-Path $outDir 'JyGlobalVST-LocalTest.cer'
Export-Certificate -Cert $cert -FilePath $cerPath -Force | Out-Null
Write-Ok "Public cert exported: $cerPath"

# --- Sign -------------------------------------------------------------------
Write-Step 'Signing package'

& $signTool sign /fd SHA256 /sha1 $cert.Thumbprint /tr http://timestamp.digicert.com /td SHA256 $msixPath
if ($LASTEXITCODE -ne 0)
{
    Write-Warn 'Timestamped signing failed (offline?). Retrying without a timestamp.'
    & $signTool sign /fd SHA256 /sha1 $cert.Thumbprint $msixPath
    if ($LASTEXITCODE -ne 0) { Write-Fail 'SignTool failed.' }
}
Write-Ok 'Package signed'

# --- Optional: trust the cert -----------------------------------------------
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)

if ($InstallCert)
{
    Write-Step 'Trusting certificate (LocalMachine\TrustedPeople)'
    if (-not $isAdmin) { Write-Fail '-InstallCert requires an elevated PowerShell session.' }
    Import-Certificate -FilePath $cerPath -CertStoreLocation Cert:\LocalMachine\TrustedPeople | Out-Null
    Write-Ok 'Certificate trusted'
}

# --- Optional: install ------------------------------------------------------
if ($Install)
{
    Write-Step 'Installing package'
    if ($isAdmin)
    {
        Write-Warn "Running elevated as '$env:USERNAME'. MSIX installs are per-user: if this is not"
        Write-Warn 'the account you are logged into, the app will not appear in your Start menu.'
    }
    $existing = Get-AppxPackage -Name 'JyGlobalVST' -ErrorAction SilentlyContinue
    if ($existing)
    {
        Write-Host "    Removing existing $($existing.PackageFullName)"
        Remove-AppxPackage -Package $existing.PackageFullName
    }
    Add-AppxPackage -Path $msixPath
    $installed = Get-AppxPackage -Name 'JyGlobalVST'
    Write-Ok "Installed $($installed.PackageFullName) for user '$env:USERNAME'"
    Write-Host "    Launch: explorer.exe shell:appsFolder\$($installed.PackageFamilyName)!JyGlobalVSTApp"
}

# --- Summary ----------------------------------------------------------------
Write-Host "`n$('-' * 72)"
Write-Host 'BUILD COMPLETE' -ForegroundColor Green
Write-Host $('-' * 72)
Write-Host "Package : $msixPath"
Write-Host "Cert    : $cerPath"

if (-not $Install)
{
    Write-Host "`nInstall in TWO steps - they may run as different accounts:" -ForegroundColor Cyan
    Write-Host "`n  1) ELEVATED PowerShell (trusts the signer machine-wide, once per cert):"
    Write-Host "     Import-Certificate -FilePath '$cerPath' -CertStoreLocation Cert:\LocalMachine\TrustedPeople"
    Write-Host "`n  2) NORMAL PowerShell as the logged-in user (NOT elevated):"
    Write-Host "     Add-AppxPackage -Path '$msixPath'"
    Write-Host "`n  MSIX installs are per-user. If you elevate with a different admin account," -ForegroundColor Yellow
    Write-Host '  Add-AppxPackage registers the app for THAT account and it will not appear' -ForegroundColor Yellow
    Write-Host '  in the Start menu of the account you are logged into.' -ForegroundColor Yellow
    Write-Host "`nThen launch JyGlobalVST from the Start menu."
    Write-Host 'To remove:  Get-AppxPackage -Name JyGlobalVST | Remove-AppxPackage'
}

exit 0
