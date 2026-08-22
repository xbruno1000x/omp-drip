param(
    [Parameter(Mandatory)][string]$Asi,
    [Parameter(Mandatory)][string]$PlayerImg,
    [Parameter(Mandatory)][string]$ClothesDat,
    [Parameter(Mandatory)][string]$ShoppingDat,
    [string]$Catalog = "build/catalog/omp-drip/catalog.bin",
    [string]$Output = "dist/omp-drip-client",
    [string]$Version = "dev"
)

$ErrorActionPreference = "Stop"
& node tools/package.mjs `
    --asi $Asi `
    --player-img $PlayerImg `
    --clothes-dat $ClothesDat `
    --shopping-dat $ShoppingDat `
    --catalog $Catalog `
    --out $Output `
    --version $Version
if ($LASTEXITCODE -ne 0) { throw "Client packaging failed" }
Write-Host "Manifest generated. Rebuild omp-drip-server before deploying the component."
