param(
    [string]$GtaDirectory = "C:/Program Files (x86)/GTA RIP",
    [string]$PawnCompiler,
    [string[]]$PawnInclude = @(),
    [ValidateSet("pt", "en")][string]$Language = "pt"
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$outputRoot = Join-Path $projectRoot "build/cj-default"
$exampleName = if ($Language -eq "en") { "cj-default-en" } else { "cj-default" }
$gtaRoot = (Resolve-Path -LiteralPath $GtaDirectory).Path
$compilerPath = $null
$includePaths = @()
if ($PawnCompiler) {
    $compilerPath = (Resolve-Path -LiteralPath $PawnCompiler).Path
    foreach ($directory in $PawnInclude) {
        $includePaths += (Resolve-Path -LiteralPath $directory).Path
    }
}

Push-Location $projectRoot
try {
    & node tools/generate-catalog.mjs `
        --player-img (Join-Path $gtaRoot "models/player.img") `
        --clothes-dat (Join-Path $gtaRoot "data/clothes.dat") `
        --shopping-dat (Join-Path $gtaRoot "data/shopping.dat") `
        --overrides examples/cj-default.overrides.json `
        --out $outputRoot
    if ($LASTEXITCODE -ne 0) { throw "CJ catalog generation failed" }

    if ($compilerPath) {
        # O catalogo gerado deve preceder o placeholder de include/.
        $compilerArgs = @(
            "examples/$exampleName.pwn",
            "-i$outputRoot/generated",
            "-i$projectRoot/include"
        )
        foreach ($directory in $includePaths) {
            $compilerArgs += "-i$directory"
        }
        $compilerArgs += @("-o$outputRoot/$exampleName.amx", "-;+", "-(+")
        & $compilerPath @compilerArgs
        if ($LASTEXITCODE -ne 0) { throw "CJ example compilation failed" }
        Write-Host "Gamemode: $outputRoot/$exampleName.amx"
    }

    Write-Host "Client catalog: $outputRoot/omp-drip/catalog.bin"
    Write-Host "Pawn catalog: $outputRoot/generated/omp-drip-catalog.inc"
    if ($Language -eq "en") { Write-Host "Next steps: examples/README.en.md" }
    else { Write-Host "Next steps: examples/README.md" }
}
finally {
    Pop-Location
}
