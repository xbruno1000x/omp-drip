param(
    [Parameter(Mandatory)]
    [ValidatePattern('^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$|^dev-[A-Za-z0-9._-]+$')]
    [string]$Version,
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$BuildDirectory = "build/native",
    [string]$OutputDirectory = "dist/release"
)

$ErrorActionPreference = "Stop"

function Get-AbsolutePath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $projectRoot $Path))
}

function Assert-WindowsX86Pe([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required binary is missing: $Path"
    }
    if ((Get-Item -LiteralPath $Path).Length -lt 1024) {
        throw "Binary is unexpectedly small: $Path"
    }

    $stream = [System.IO.File]::OpenRead($Path)
    $reader = [System.IO.BinaryReader]::new($stream)
    try {
        $stream.Position = 0x3c
        $peOffset = $reader.ReadUInt32()
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) { throw "Not a PE file: $Path" }
        $machine = $reader.ReadUInt16()
        if ($machine -ne 0x014c) {
            throw ("Expected Windows x86 (0x014c), got 0x{0:x4}: {1}" -f $machine, $Path)
        }
        $stream.Position = $peOffset + 24
        if ($reader.ReadUInt16() -ne 0x010b) { throw "Expected PE32 optional header: $Path" }
    }
    finally {
        $reader.Dispose()
        $stream.Dispose()
    }
}

function Copy-Required([string]$Source, [string]$Destination) {
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        throw "Required release file is missing: $Source"
    }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Destination) | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination
}

function Write-Utf8File([string]$Path, [string]$Content) {
    [System.IO.File]::WriteAllText($Path, $Content, [System.Text.UTF8Encoding]::new($false))
}

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$buildRoot = Get-AbsolutePath $BuildDirectory
$outputRoot = Get-AbsolutePath $OutputDirectory
$componentPath = Join-Path $buildRoot "$Configuration/omp-drip.dll"
$clientPath = Join-Path $buildRoot "$Configuration/omp-drip.asi"
$serverInclude = Join-Path $projectRoot "include/omp-drip.inc"
$catalogInclude = Join-Path $projectRoot "include/omp-drip-catalog.inc"
$minHookLicense = Join-Path $projectRoot ".deps/minhook/LICENSE.txt"

Assert-WindowsX86Pe $componentPath
Assert-WindowsX86Pe $clientPath
foreach ($requiredPath in @($serverInclude, $catalogInclude, $minHookLicense,
        (Join-Path $projectRoot "README.md"),
        (Join-Path $projectRoot "docs/api.md"),
        (Join-Path $projectRoot "docs/releases.md"))) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required release source is missing: $requiredPath"
    }
}

if (Test-Path -LiteralPath $outputRoot) {
    throw "Output directory already exists. Remove only this generated directory or choose -OutputDirectory: $outputRoot"
}

$stageRoot = Join-Path $projectRoot ("build/package-stage/" + $Version + "-" + [guid]::NewGuid().ToString("N"))
$serverStage = Join-Path $stageRoot "server"
$clientStage = Join-Path $stageRoot "client"
$releaseFiles = @()

try {
    New-Item -ItemType Directory -Force -Path $serverStage, $clientStage, $outputRoot | Out-Null

    Copy-Required $componentPath (Join-Path $serverStage "components/omp-drip.dll")
    Copy-Required $serverInclude (Join-Path $serverStage "include/omp-drip.inc")
    Copy-Required $catalogInclude (Join-Path $serverStage "include/omp-drip-catalog.inc")
    Copy-Required (Join-Path $projectRoot "README.md") (Join-Path $serverStage "README.md")
    Copy-Required (Join-Path $projectRoot "docs/api.md") (Join-Path $serverStage "docs/api.md")
    Copy-Required (Join-Path $projectRoot "docs/releases.md") (Join-Path $serverStage "docs/releases.md")

    Write-Utf8File (Join-Path $serverStage "INSTALL.txt") @"
OMP-DRIP SERVER $Version

1. Copy components/omp-drip.dll into the components directory of your open.mp server.
2. Copy both files from include/ into the include directory used by your Pawn compiler.
3. Include <omp-drip> and replace the catalog placeholder with a catalog generated for your own GTA assets.

This public binary uses the unconfigured development manifest. GTA assets, generated
catalogs and package-specific manifests are intentionally not included. For enforced
client hashes, generate the client package and rebuild the server component with its
generated manifest before deployment.
"@

Copy-Required $clientPath (Join-Path $clientStage "omp-drip.asi")
Copy-Required $minHookLicense (Join-Path $clientStage "LICENSES/MinHook.txt")
Write-Utf8File (Join-Path $clientStage "INSTALL.txt") @"
OMP-DRIP CLIENT $Version

1. Copy omp-drip.asi into the root of a supported GTA San Andreas installation.
2. Use an ASI loader and Mod Loader, then install a package generated from your own player.img,
   clothes.dat, shopping.dat and catalog.bin files.
3. Keep the package files together. The client and server must use matching generated hashes
   when the server is built with a configured manifest.

This release contains the generic client plugin only. It does not include GTA assets,
catalog.bin or manifest.json. Generate and install a complete package before using the
client, even when the server uses the unconfigured development manifest.
"@

$serverZip = Join-Path $outputRoot "omp-drip-server-v$Version-windows-x86.zip"
$clientZip = Join-Path $outputRoot "omp-drip-client-v$Version-windows-x86.zip"
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($serverStage, $serverZip,
    [System.IO.Compression.CompressionLevel]::Optimal, $false)
[System.IO.Compression.ZipFile]::CreateFromDirectory($clientStage, $clientZip,
    [System.IO.Compression.CompressionLevel]::Optimal, $false)

$componentAsset = Join-Path $outputRoot "omp-drip-v$Version-windows-x86.dll"
$clientAsset = Join-Path $outputRoot "omp-drip-v$Version-windows-x86.asi"
$includeAsset = Join-Path $outputRoot "omp-drip-v$Version.inc"
Copy-Required $componentPath $componentAsset
Copy-Required $clientPath $clientAsset
Copy-Required $serverInclude $includeAsset
$licenseAsset = Join-Path $outputRoot "MinHook-LICENSE.txt"
Copy-Required $minHookLicense $licenseAsset

$releaseFiles = @(
    $serverZip,
    $clientZip,
    $componentAsset,
    $clientAsset,
    $includeAsset,
    $licenseAsset
)
$checksumLines = foreach ($file in $releaseFiles) {
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $file).Hash.ToLowerInvariant()
    "$hash  $([System.IO.Path]::GetFileName($file))"
}
Write-Utf8File (Join-Path $outputRoot "SHA256SUMS.txt") (($checksumLines -join "`n") + "`n")

Write-Host "Release assets written to $outputRoot"
Get-ChildItem -LiteralPath $outputRoot -File | Sort-Object Name | ForEach-Object {
    Write-Host ("  {0} ({1} bytes)" -f $_.Name, $_.Length)
}
}
finally {
    if (Test-Path -LiteralPath $stageRoot) {
        $resolvedStage = (Resolve-Path -LiteralPath $stageRoot).Path
        $allowedStageParent = [System.IO.Path]::GetFullPath((Join-Path $projectRoot "build/package-stage"))
        if (-not $resolvedStage.StartsWith($allowedStageParent + [System.IO.Path]::DirectorySeparatorChar,
                [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to clean a staging directory outside $allowedStageParent"
        }
        Remove-Item -LiteralPath $resolvedStage -Recurse -Force
    }
}
