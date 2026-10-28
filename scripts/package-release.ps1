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
$catalogPath = Join-Path $projectRoot "catalog/default/catalog.bin"
$catalogSeedPath = Join-Path $projectRoot "catalog/default/catalog.seed.json"
$minHookLicense = Join-Path $projectRoot ".deps/minhook/LICENSE.txt"

Assert-WindowsX86Pe $componentPath
Assert-WindowsX86Pe $clientPath
foreach ($requiredPath in @($serverInclude, $catalogInclude, $catalogPath, $catalogSeedPath, $minHookLicense,
        (Join-Path $projectRoot "README.md"))) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required release source is missing: $requiredPath"
    }
}

$seed = Get-Content -LiteralPath $catalogSeedPath -Raw | ConvertFrom-Json
$catalogHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $catalogPath).Hash.ToLowerInvariant()
if ($seed.schema -ne 1 -or $seed.protocol -ne 1 -or $catalogHash -ne $seed.files.'omp-drip/catalog.bin') {
    throw "Default catalog does not match its seed. Regenerate it before packaging."
}
foreach ($asset in @('models/player.img', 'data/clothes.dat', 'data/shopping.dat')) {
    if ($seed.files.$asset -notmatch '^[0-9a-f]{64}$') { throw "Missing default asset hash: $asset" }
}
$catalogBytes = [System.IO.File]::ReadAllBytes($catalogPath)
if ($catalogBytes.Length -lt 10 -or [System.Text.Encoding]::ASCII.GetString($catalogBytes, 0, 4) -ne 'HCAT' `
        -or [System.BitConverter]::ToUInt16($catalogBytes, 4) -notin @(1, 2)) {
    throw "Invalid default catalog header."
}
$catalogCount = [System.BitConverter]::ToUInt32($catalogBytes, 6)
$includeText = Get-Content -LiteralPath $catalogInclude -Raw
if ($catalogCount -eq 0 -or $includeText -notmatch ('#define DRIP_CATALOG_COUNT \(' + $catalogCount + '\)') `
        -or -not $includeText.Contains('#define DRIP_CATALOG_VERSION "' + $seed.catalogVersion + '"')) {
    throw "Default Pawn include does not match the catalog. Regenerate it before packaging."
}

if (Test-Path -LiteralPath $outputRoot) {
    throw "Output directory already exists. Remove only this generated directory or choose -OutputDirectory: $outputRoot"
}

$stageRoot = Join-Path $projectRoot ("build/package-stage/" + $Version + "-" + [guid]::NewGuid().ToString("N"))
$serverStage = Join-Path $stageRoot "server"
$clientStage = Join-Path $stageRoot "client"
$catalogStage = Join-Path $stageRoot "catalog"
$releaseFiles = @()

try {
    New-Item -ItemType Directory -Force -Path $serverStage, $clientStage, $catalogStage, $outputRoot | Out-Null

    Copy-Required $componentPath (Join-Path $serverStage "components/omp-drip.dll")
    Copy-Required $serverInclude (Join-Path $serverStage "include/omp-drip.inc")
    Copy-Required $catalogInclude (Join-Path $serverStage "include/omp-drip-catalog.inc")
    Copy-Required (Join-Path $projectRoot "README.md") (Join-Path $serverStage "README.md")

    Write-Utf8File (Join-Path $serverStage "INSTALL.txt") @"
OMP-DRIP SERVER $Version

1. Copy components/omp-drip.dll into the components directory of your open.mp server.
2. Copy both files from include/ into the include directory used by your Pawn compiler.
3. Include <omp-drip> and <omp-drip-catalog> to use the bundled vanilla CJ catalog ($catalogCount items).
4. Distribute the matching client ZIP. For custom assets, generate an alternative catalog and
   replace the Pawn catalog include and the client's omp-drip/catalog.bin together.

This public binary uses the unconfigured development manifest. GTA assets are not included.
For enforced client hashes, generate the client package and rebuild the server component
with its generated manifest before deployment.

Documentation: https://github.com/xbruno1000x/omp-drip/wiki
Pawn API: https://github.com/xbruno1000x/omp-drip/wiki/Pawn-API
sampctl: https://github.com/xbruno1000x/omp-drip/wiki/Installing-with-sampctl
"@

Copy-Required $clientPath (Join-Path $clientStage "omp-drip.asi")
Copy-Required $catalogPath (Join-Path $clientStage "omp-drip/catalog.bin")
Copy-Required $catalogSeedPath (Join-Path $clientStage "omp-drip/catalog.seed.json")
Copy-Required $minHookLicense (Join-Path $clientStage "LICENSES/MinHook.txt")
$clientManifest = [ordered]@{
    schema = 1
    protocol = 1
    version = $Version
    files = [ordered]@{
        'omp-drip.asi' = (Get-FileHash -Algorithm SHA256 -LiteralPath $clientPath).Hash.ToLowerInvariant()
        'models/player.img' = $seed.files.'models/player.img'
        'data/clothes.dat' = $seed.files.'data/clothes.dat'
        'data/shopping.dat' = $seed.files.'data/shopping.dat'
        'omp-drip/catalog.bin' = $catalogHash
    }
}
Write-Utf8File (Join-Path $clientStage "omp-drip/manifest.json") (($clientManifest | ConvertTo-Json -Depth 4) + "`n")
Write-Utf8File (Join-Path $clientStage "INSTALL.txt") @"
OMP-DRIP CLIENT $Version

1. Extract this ZIP into the root of a supported vanilla GTA San Andreas installation,
   preserving the omp-drip/ directory. Use an ASI loader.
2. The bundled catalog uses the original CJ assets in models/player.img, data/clothes.dat
   and data/shopping.dat. Their reference hashes are in omp-drip/catalog.seed.json.
3. Remove any previous custom modloader/omp-drip package before using the vanilla catalog.
   Other installed clothing replacements must also match the chosen catalog.
4. For custom assets, generate an alternative catalog and use scripts/package-client.ps1
   with -Catalog. Custom packages use Mod Loader and take precedence over vanilla assets.

This release includes the ASI, the default catalog and a client manifest. GTA assets are
read from the user's installation and are not included. The public server component uses
the unconfigured development manifest; rebuild it with a generated package manifest to
enforce exact client hashes.

Documentation: https://github.com/xbruno1000x/omp-drip/wiki
"@

$serverZip = Join-Path $outputRoot "omp-drip-server-v$Version-windows-x86.zip"
$clientZip = Join-Path $outputRoot "omp-drip-client-v$Version-windows-x86.zip"
$catalogZip = Join-Path $outputRoot "omp-drip-catalog-v$Version.zip"
Copy-Required $catalogInclude (Join-Path $catalogStage "include/omp-drip-catalog.inc")
Copy-Required $catalogPath (Join-Path $catalogStage "omp-drip/catalog.bin")
Copy-Required $catalogSeedPath (Join-Path $catalogStage "omp-drip/catalog.seed.json")
Copy-Required (Join-Path $projectRoot "catalog/default/README.md") (Join-Path $catalogStage "README.md")
Copy-Required (Join-Path $projectRoot "catalog/default/overrides.json") (Join-Path $catalogStage "overrides.json")
if (-not $Version.StartsWith('dev-')) {
    $package = Get-Content -LiteralPath (Join-Path $projectRoot 'pawn.json') -Raw | ConvertFrom-Json
    $matchingResources = @($package.resources | Where-Object {
        $_.platform -eq 'windows' -and [System.IO.Path]::GetFileName($serverZip) -match $_.name
    })
    if ($matchingResources.Count -ne 1 -or -not $matchingResources[0].archive) {
        throw 'The server ZIP must match exactly one sampctl archive resource.'
    }
    foreach ($plugin in $matchingResources[0].plugins) {
        if (-not (Test-Path -LiteralPath (Join-Path $serverStage $plugin) -PathType Leaf)) {
            throw "sampctl resource is missing from the server archive: $plugin"
        }
    }
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($serverStage, $serverZip,
    [System.IO.Compression.CompressionLevel]::Optimal, $false)
[System.IO.Compression.ZipFile]::CreateFromDirectory($clientStage, $clientZip,
    [System.IO.Compression.CompressionLevel]::Optimal, $false)
[System.IO.Compression.ZipFile]::CreateFromDirectory($catalogStage, $catalogZip,
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
    $catalogZip,
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
