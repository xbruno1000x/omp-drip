param(
    [Parameter(Mandatory)][string]$GtaDirectory
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$gtaRoot = (Resolve-Path -LiteralPath $GtaDirectory).Path
$outputRoot = Join-Path $projectRoot "build/default-catalog"
$catalogRoot = Join-Path $projectRoot "catalog/default"

Push-Location $projectRoot
try {
    & node tools/generate-catalog.mjs `
        --player-img (Join-Path $gtaRoot "models/player.img") `
        --clothes-dat (Join-Path $gtaRoot "data/clothes.dat") `
        --shopping-dat (Join-Path $gtaRoot "data/shopping.dat") `
        --overrides (Join-Path $catalogRoot "overrides.json") `
        --out $outputRoot
    if ($LASTEXITCODE -ne 0) { throw "Default catalog generation failed" }

    Copy-Item -LiteralPath (Join-Path $outputRoot "omp-drip/catalog.bin") -Destination $catalogRoot
    Copy-Item -LiteralPath (Join-Path $outputRoot "omp-drip/catalog.seed.json") -Destination $catalogRoot
    Copy-Item -LiteralPath (Join-Path $outputRoot "generated/omp-drip-catalog.inc") `
        -Destination (Join-Path $projectRoot "include/omp-drip-catalog.inc")
    Write-Host "Default catalog and Pawn include updated. Run npm test before releasing."
}
finally {
    Pop-Location
}
