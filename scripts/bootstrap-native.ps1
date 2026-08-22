param([string]$DependencyRoot = ".deps")

$ErrorActionPreference = "Stop"

function Clone-IfMissing([string]$Path, [string[]]$Arguments) {
    if (-not (Test-Path -LiteralPath $Path)) {
        & git clone @Arguments $Path
        if ($LASTEXITCODE -ne 0) { throw "git clone failed for $Path" }
    }
}

Clone-IfMissing "$DependencyRoot/open.mp" @(
    "--depth", "1", "--branch", "v1.5.8.3079", "https://github.com/openmultiplayer/open.mp.git"
)
& git -C "$DependencyRoot/open.mp" submodule update --init --recursive SDK Shared/Network lib/pawn lib/pawn-natives lib/RakNet
if ($LASTEXITCODE -ne 0) { throw "open.mp submodule initialization failed" }

Clone-IfMissing "$DependencyRoot/minhook" @(
    "--depth", "1", "https://github.com/TsudaKageyu/minhook.git"
)

Write-Host "Native dependencies are ready in $DependencyRoot"
