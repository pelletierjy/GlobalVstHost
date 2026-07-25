#Requires -Version 5.1
<#
.SYNOPSIS
Build a self-signed MSIX package of JyGlobalVST for local sideload testing.

.DESCRIPTION
Produces an installable .msix without needing Partner Center or a real code-signing
certificate. Uses the Windows SDK tools (MakeAppx, SignTool) directly rather than the
.msixproj, so no Windows Application Packaging Project / VS workload is required.

Steps:
  1. Locate Windows SDK tools (MakeAppx.exe, SignTool.exe)
  2. Build the tray app in Release (skippable with -SkipBuild)
  3. Stage the package payload: exe + VC++ runtime DLLs + Assets + AppxManifest.xml
  4. Pack with MakeAppx
  5. Create (or reuse) a self-signed code-signing cert whose subject matches the
     manifest Publisher, and sign the package with it
  6. Export the .cer so it can be trusted, and print the install commands

NOTE: This package is for LOCAL TESTING ONLY. Store submissions are uploaded unsigned
(Partner Center re-signs them) and must use the Publisher identity assigned to your
Partner Center account.

.PARAMETER Version
Package version, format Major.Minor.Build.Revision. Defaults to the value in
StorePackaging\version-info.txt.

.PARAMETER Configuration
Build configuration. Release (default) or Debug.

.PARAMETER SkipBuild
Reuse the already-built JyGlobalVST.exe instead of invoking CMake.

.PARAMETER BuildDir
CMake build directory. Defaults to <repo>\build.

.PARAMETER InstallCert
Import the signing certificate into LocalMachine\TrustedPeople so the package can be
installed. REQUIRES AN ELEVATED SHELL.

.PARAMETER Install
Install the package with Add-AppxPackage after building. Requires the cert to already
be trusted (see -InstallCert).

.EXAMPLE
# Build and sign; prints the two commands to run elevated afterwards
.\Build-MSIX-Local.ps1

.EXAMPLE
# From an elevated shell: build, trust the cert, and install in one go
.\Build-MSIX-Local.ps1 -InstallCert -Install
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

function Write-Step($msg) { Write-Host "`n==> $msg" -ForegroundColor Cyan }
function Write-Ok($msg) { Write-Host "    [ok] $msg" -ForegroundColor Green }
function Write-Warn($msg) { Write-Host "    [!]  $msg" -ForegroundColor Yellow }
function Fail($msg) { Write-Host "`n[FAIL] $msg" -ForegroundColor Red; exit 1 }

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$storeDir = Join-Path $repoRoot 'StorePackaging'
$manifestSrc = Join-Path $storeDir 'AppxManifest.xml'
if (-not $BuildDir) { $BuildDir = Join-Path $repoRoot 'build' }

Write-Host "JyGlobalVST - local MSIX build" -ForegroundColor White
Write-Host "Repo: $repoRoot"

# --- Version -----------------------------------------------------------------
if (-not $Version) {
    $versionFile = Join-Path $storeDir 'version-info.txt'
    if (Test-Path $versionFile) {
        $Version = (Get-Content $versionFile |
            Where-Object { $_ -match '^\s*\d+\.\d+\.\d+\.\d+\s*$' } |
            Select-Object -First 1).Trim()
    }
    if (-not $Version) { $Version = '1.0.0.0' }
}
Write-Host "Version: $Version | Config: $Configuration"

# --- Locate SDK tools --------------------------------------------------------
Write-Step 'Locating Windows SDK tools'

function Find-SdkTool([string]$name) {
    $roots = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin",
        "$env:ProgramFiles\Windows Kits\10\bin"
    ) | Where-Object { Test-Path $_ }

    $candidates = foreach ($root in $roots) {
        Get-ChildItem -Path $root -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^10\.\d+\.\d+\.\d+$' } |
            ForEach-Object {
                $p = Join-Path $_.FullName "x64\$name"
                if (Test-Path $p) { [pscustomobject]@{ Ver = [version]$_.Name; Path = $p } }
            }
    }
    ($candidates | Sort-Object Ver -Descending | Select-Object -First 1).Path
}

$makeAppx = Find-SdkTool 'makeappx.exe'
$signTool = Find-SdkTool 'signtool.exe'
if (-not $makeAppx) { Fail 'MakeAppx.exe not found. Install the Windows 10/11 SDK.' }
if (-not $signTool) { Fail 'SignTool.exe not found. Install the Windows 10/11 SDK.' }
Write-Ok "MakeAppx: $makeAppx"
Write-Ok "SignTool: $signTool"

# --- Build -------------------------------------------------------------------
$exePath = Join-Path $BuildDir "src\tray-app\jyglobalvst_tray_artefacts\$Configuration\JyGlobalVST.exe"

if ($SkipBuild) {
    Write-Step 'Skipping build (-SkipBuild)'
    if (-not (Test-Path $exePath)) { Fail "No existing executable at $exePath" }
} else {
    Write-Step "Building JyGlobalVST ($Configuration)"
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { Fail 'cmake not found on PATH.' }

    Push-Location $repoRoot
    try {
        cmake -B $BuildDir -A x64 -DJYGLOBALVST_BUILD_TESTS=OFF
        if ($LASTEXITCODE -ne 0) { Fail 'CMake configuration failed.' }
        cmake --build $BuildDir --config $Configuration --target jyglobalvst_tray
        if ($LASTEXITCODE -ne 0) { Fail 'Build failed.' }
    } finally {
        Pop-Location
    }
}
if (-not (Test-Path $exePath)) { Fail "Executable not found: $exePath" }
Write-Ok "Executable: $exePath ($('{0:N2}' -f ((Get-Item $exePath).Length / 1MB)) MB)"

# --- Stage payload -----------------------------------------------------------
Write-Step 'Staging package payload'

$outDir = Join-Path $BuildDir 'store-packages'
$payloadDir = Join-Path $outDir 'payload'
if (Test-Path $payloadDir) { Remove-Item -Recurse -Force $payloadDir }
New-Item -ItemType Directory -Force -Path $payloadDir | Out-Null

Copy-Item -Path $exePath -Destination (Join-Path $payloadDir 'JyGlobalVST.exe')

# Assets (*.png only - stray files like .gitkeep must not end up in the package)
$assetsDst = Join-Path $payloadDir 'Assets'
New-Item -ItemType Directory -Force -Path $assetsDst | Out-Null
Copy-Item -Path (Join-Path $storeDir 'Assets\*.png') -Destination $assetsDst
Write-Ok "Assets: $((Get-ChildItem $assetsDst).Count) file(s)"

# Manifest, with the requested version stamped in
$manifestXml = Get-Content $manifestSrc -Raw
$manifestXml = [regex]::Replace($manifestXml, '(<Identity\b[^>]*?Version=")[\d.]+(")', "`${1}$Version`${2}")
$manifestDst = Join-Path $payloadDir 'AppxManifest.xml'
Set-Content -Path $manifestDst -Value $manifestXml -Encoding UTF8 -NoNewline

# Read back the publisher we must match with the signing cert
$publisher = ([xml]$manifestXml).Package.Identity.Publisher
Write-Ok "Manifest publisher: $publisher"

# VC++ runtime DLLs. The app links the dynamic CRT, and Program Files\WindowsApps is
# not a redist search path, so these must ship inside the package.
$crtNames = @('msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll', 'msvcp140_1.dll', 'msvcp140_2.dll', 'msvcp140_atomic_wait.dll', 'concrt140.dll')
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$crtDir = $null
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -property installationPath | Select-Object -First 1
    if ($vsPath) {
        $crtDir = Get-ChildItem -Path (Join-Path $vsPath 'VC\Redist\MSVC') -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' } |
            Sort-Object { [version]$_.Name } -Descending |
            ForEach-Object { Get-ChildItem -Path (Join-Path $_.FullName 'x64') -Directory -Filter 'Microsoft.VC*.CRT' -ErrorAction SilentlyContinue } |
            Select-Object -First 1 -ExpandProperty FullName
    }
}
if ($crtDir) {
    $copied = 0
    foreach ($dll in $crtNames) {
        $src = Join-Path $crtDir $dll
        if (Test-Path $src) { Copy-Item $src $payloadDir; $copied++ }
    }
    Write-Ok "VC++ runtime: $copied DLL(s) from $crtDir"
} else {
    Write-Warn 'VC++ redist folder not found - the packaged app may fail to start with a missing-DLL error.'
}

# --- Pack --------------------------------------------------------------------
Write-Step 'Packing MSIX'

$msixPath = Join-Path $outDir "JyGlobalVST_${Version}_x64.msix"
if (Test-Path $msixPath) { Remove-Item -Force $msixPath }

& $makeAppx pack /d $payloadDir /p $msixPath /o
if ($LASTEXITCODE -ne 0) { Fail 'MakeAppx pack failed.' }
Write-Ok "Package: $msixPath ($('{0:N2}' -f ((Get-Item $msixPath).Length / 1MB)) MB)"

# --- Signing certificate -----------------------------------------------------
Write-Step 'Preparing self-signed code-signing certificate'

# SignTool requires the cert subject to match the manifest Publisher exactly.
$cert = Get-ChildItem Cert:\CurrentUser\My |
    Where-Object { $_.Subject -eq $publisher -and $_.NotAfter -gt (Get-Date) -and $_.HasPrivateKey } |
    Sort-Object NotAfter -Descending |
    Select-Object -First 1

if ($cert) {
    Write-Ok "Reusing existing cert $($cert.Thumbprint) (expires $($cert.NotAfter.ToString('yyyy-MM-dd')))"
} else {
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

# --- Sign --------------------------------------------------------------------
Write-Step 'Signing package'

& $signTool sign /fd SHA256 /sha1 $cert.Thumbprint /tr http://timestamp.digicert.com /td SHA256 $msixPath
if ($LASTEXITCODE -ne 0) {
    Write-Warn 'Timestamped signing failed (offline?). Retrying without a timestamp.'
    & $signTool sign /fd SHA256 /sha1 $cert.Thumbprint $msixPath
    if ($LASTEXITCODE -ne 0) { Fail 'SignTool failed.' }
}
Write-Ok 'Package signed'

# --- Optional: trust the cert ------------------------------------------------
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)

if ($InstallCert) {
    Write-Step 'Trusting certificate (LocalMachine\TrustedPeople)'
    if (-not $isAdmin) { Fail '-InstallCert requires an elevated PowerShell session.' }
    Import-Certificate -FilePath $cerPath -CertStoreLocation Cert:\LocalMachine\TrustedPeople | Out-Null
    Write-Ok 'Certificate trusted'
}

# --- Optional: install -------------------------------------------------------
if ($Install) {
    Write-Step 'Installing package'
    $existing = Get-AppxPackage -Name 'JyGlobalVST' -ErrorAction SilentlyContinue
    if ($existing) {
        Write-Host "    Removing existing $($existing.PackageFullName)"
        Remove-AppxPackage -Package $existing.PackageFullName
    }
    Add-AppxPackage -Path $msixPath
    $installed = Get-AppxPackage -Name 'JyGlobalVST'
    Write-Ok "Installed $($installed.PackageFullName)"
    Write-Host "    Launch: explorer.exe shell:appsFolder\$($installed.PackageFamilyName)!JyGlobalVSTApp"
}

# --- Summary -----------------------------------------------------------------
Write-Host "`n" + ('-' * 72)
Write-Host 'BUILD COMPLETE' -ForegroundColor Green
Write-Host ('-' * 72)
Write-Host "Package : $msixPath"
Write-Host "Cert    : $cerPath"

if (-not ($InstallCert -and $Install)) {
    Write-Host "`nInstall in TWO steps - they run as different accounts:" -ForegroundColor Cyan
    Write-Host "`n  1) ELEVATED PowerShell (trusts the signer machine-wide, once per cert):"
    Write-Host "     Import-Certificate -FilePath '$cerPath' -CertStoreLocation Cert:\LocalMachine\TrustedPeople"
    Write-Host "`n  2) NORMAL PowerShell as the logged-in user '$env:USERNAME' (NOT elevated):"
    Write-Host "     Add-AppxPackage -Path '$msixPath'"
    Write-Host "`n  MSIX installs are per-user. If you elevate with a different admin account," -ForegroundColor Yellow
    Write-Host "  Add-AppxPackage registers the app for THAT account and it will not appear" -ForegroundColor Yellow
    Write-Host "  in the Start menu of the account you are logged into." -ForegroundColor Yellow
    Write-Host "`nThen launch JyGlobalVST from the Start menu."
    Write-Host "To remove:  Get-AppxPackage -Name JyGlobalVST | Remove-AppxPackage"
}

exit 0
