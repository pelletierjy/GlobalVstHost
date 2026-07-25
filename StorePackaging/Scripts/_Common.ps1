# Shared helpers for the MSIX packaging scripts.
# Dot-sourced by Build-MSIX.ps1, Build-MSIX-Local.ps1 and Validate-Package.ps1.
# Not intended to be run directly.

Set-StrictMode -Version Latest

# The placeholder identity checked into AppxManifest.xml. A Store submission MUST
# replace these with the values Partner Center assigns to the reserved app name.
$script:PlaceholderPackageName = 'JyGlobalVST'
$script:PlaceholderPublisher = 'CN=JyGlobalVST, O=JyGlobalVST'

function Write-Step { param([string]$Message) Write-Host "`n==> $Message" -ForegroundColor Cyan }
function Write-Ok { param([string]$Message) Write-Host "    [ok] $Message" -ForegroundColor Green }
function Write-Warn { param([string]$Message) Write-Host "    [!]  $Message" -ForegroundColor Yellow }
function Write-Fail { param([string]$Message) Write-Host "`n[FAIL] $Message" -ForegroundColor Red; exit 1 }

<#
.SYNOPSIS
Locate a Windows SDK tool (makeappx.exe, signtool.exe) picking the newest SDK present.
#>
function Find-SdkTool
{
    param([Parameter(Mandatory = $true)][string]$Name)

    $roots = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin",
        "$env:ProgramFiles\Windows Kits\10\bin"
    ) | Where-Object { Test-Path $_ }

    $candidates = foreach ($root in $roots)
    {
        Get-ChildItem -Path $root -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^10\.\d+\.\d+\.\d+$' } |
            ForEach-Object {
                $candidate = Join-Path $_.FullName "x64\$Name"
                if (Test-Path $candidate) { [pscustomobject]@{ Ver = [version]$_.Name; Path = $candidate } }
            }
    }

    ($candidates | Sort-Object Ver -Descending | Select-Object -First 1).Path
}

<#
.SYNOPSIS
Read the package version from StorePackaging\version-info.txt.
#>
function Get-VersionFromFile
{
    param([Parameter(Mandatory = $true)][string]$StoreDir)

    $versionFile = Join-Path $StoreDir 'version-info.txt'
    if (-not (Test-Path $versionFile)) { return $null }

    $line = Get-Content $versionFile |
        Where-Object { $_ -match '^\s*\d+\.\d+\.\d+\.\d+\s*$' } |
        Select-Object -First 1

    if ($line) { $line.Trim() } else { $null }
}

<#
.SYNOPSIS
Locate the newest x64 MSVC redistributable CRT directory.

.DESCRIPTION
The app links the dynamic CRT. C:\Program Files\WindowsApps is not a redist search
path, so these DLLs must be staged into the package payload or the packaged app fails
to start with a missing-DLL error.
#>
function Get-CrtRedistDir
{
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { return $null }

    $vsPath = & $vswhere -latest -property installationPath | Select-Object -First 1
    if (-not $vsPath) { return $null }

    $redistRoot = Join-Path $vsPath 'VC\Redist\MSVC'
    if (-not (Test-Path $redistRoot)) { return $null }

    Get-ChildItem -Path $redistRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' } |
        Sort-Object { [version]$_.Name } -Descending |
        ForEach-Object { Get-ChildItem -Path (Join-Path $_.FullName 'x64') -Directory -Filter 'Microsoft.VC*.CRT' -ErrorAction SilentlyContinue } |
        Select-Object -First 1 -ExpandProperty FullName
}

<#
.SYNOPSIS
Stage a complete MSIX payload directory: manifest, executable, assets, CRT DLLs.

.DESCRIPTION
Writes AppxManifest.xml with the requested version and identity stamped in, copies the
built executable, the PNG assets and the dynamic CRT DLLs. Returns an object describing
what was staged, including the effective Publisher (needed to pick a signing cert).

.PARAMETER PackageName
Overrides Identity/@Name. Leave empty to keep the manifest value.

.PARAMETER Publisher
Overrides Identity/@Publisher. Must be the full subject, e.g. "CN=1234ABCD-...".
Leave empty to keep the manifest value.

.PARAMETER PublisherDisplayName
Overrides Properties/PublisherDisplayName. Leave empty to keep the manifest value.
#>
function New-PackagePayload
{
    param(
        [Parameter(Mandatory = $true)][string]$StoreDir,
        [Parameter(Mandatory = $true)][string]$PayloadDir,
        [Parameter(Mandatory = $true)][string]$ExePath,
        [Parameter(Mandatory = $true)][string]$Version,
        [string]$PackageName,
        [string]$Publisher,
        [string]$PublisherDisplayName
    )

    if (Test-Path $PayloadDir) { Remove-Item -Recurse -Force $PayloadDir }
    New-Item -ItemType Directory -Force -Path $PayloadDir | Out-Null

    # Executable
    Copy-Item -Path $ExePath -Destination (Join-Path $PayloadDir 'JyGlobalVST.exe')

    # Assets: *.png only, so stray files (.gitkeep) never reach the package
    $assetsDst = Join-Path $PayloadDir 'Assets'
    New-Item -ItemType Directory -Force -Path $assetsDst | Out-Null
    Copy-Item -Path (Join-Path $StoreDir 'Assets\*.png') -Destination $assetsDst
    $assetCount = (Get-ChildItem $assetsDst).Count

    # Manifest, with version and (optionally) identity stamped in
    $xml = Get-Content (Join-Path $StoreDir 'AppxManifest.xml') -Raw
    $xml = [regex]::Replace($xml, '(<Identity\b[^>]*?Version=")[\d.]+(")', "`${1}$Version`${2}")

    if ($PackageName)
    {
        $xml = [regex]::Replace($xml, '(<Identity\b[^>]*?Name=")[^"]*(")', "`${1}$PackageName`${2}")
    }
    if ($Publisher)
    {
        $xml = [regex]::Replace($xml, '(<Identity\b[^>]*?Publisher=")[^"]*(")', "`${1}$Publisher`${2}")
    }
    if ($PublisherDisplayName)
    {
        $xml = [regex]::Replace($xml, '(<PublisherDisplayName>)[^<]*(</PublisherDisplayName>)', "`${1}$PublisherDisplayName`${2}")
    }

    Set-Content -Path (Join-Path $PayloadDir 'AppxManifest.xml') -Value $xml -Encoding UTF8 -NoNewline

    $parsed = [xml]$xml

    # Dynamic CRT
    $crtNames = @(
        'msvcp140.dll', 'msvcp140_1.dll', 'msvcp140_2.dll', 'msvcp140_atomic_wait.dll',
        'vcruntime140.dll', 'vcruntime140_1.dll', 'concrt140.dll'
    )
    $crtDir = Get-CrtRedistDir
    $crtCopied = 0
    if ($crtDir)
    {
        foreach ($dll in $crtNames)
        {
            $src = Join-Path $crtDir $dll
            if (Test-Path $src) { Copy-Item $src $PayloadDir; $crtCopied++ }
        }
    }

    [pscustomobject]@{
        PayloadDir           = $PayloadDir
        AssetCount           = $assetCount
        CrtDir               = $crtDir
        CrtCopied            = $crtCopied
        PackageName          = $parsed.Package.Identity.Name
        Publisher            = $parsed.Package.Identity.Publisher
        PublisherDisplayName = $parsed.Package.Properties.PublisherDisplayName
        Version              = $parsed.Package.Identity.Version
    }
}

<#
.SYNOPSIS
Return $true if the identity is still the checked-in placeholder rather than a real
Partner Center identity.
#>
function Test-PlaceholderIdentity
{
    param(
        [Parameter(Mandatory = $true)][string]$PackageName,
        [Parameter(Mandatory = $true)][string]$Publisher
    )

    ($PackageName -eq $script:PlaceholderPackageName) -or ($Publisher -eq $script:PlaceholderPublisher)
}

<#
.SYNOPSIS
Build the app with CMake and return the path to the built executable.
#>
function Invoke-AppBuild
{
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string]$BuildDir,
        [Parameter(Mandatory = $true)][string]$Configuration
    )

    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { Write-Fail 'cmake not found on PATH.' }

    Push-Location $RepoRoot
    try
    {
        cmake -B $BuildDir -A x64 -DJYGLOBALVST_BUILD_TESTS=OFF
        if ($LASTEXITCODE -ne 0) { Write-Fail 'CMake configuration failed.' }
        cmake --build $BuildDir --config $Configuration --target jyglobalvst_tray
        if ($LASTEXITCODE -ne 0) { Write-Fail 'Build failed.' }
    }
    finally
    {
        Pop-Location
    }
}

function Get-TrayExePath
{
    param(
        [Parameter(Mandatory = $true)][string]$BuildDir,
        [Parameter(Mandatory = $true)][string]$Configuration
    )

    Join-Path $BuildDir "src\tray-app\jyglobalvst_tray_artefacts\$Configuration\JyGlobalVST.exe"
}
