param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$BuildDirectory = "build/native",
    [string]$DeployComponentDirectory = ""
)

$ErrorActionPreference = "Stop"
$configure = @("-S", ".", "-B", $BuildDirectory, "-A", "Win32")
if ($DeployComponentDirectory) {
    $configure += "-DOMP_DRIP_DEPLOY_COMPONENT_DIR=$DeployComponentDirectory"
}
& cmake @configure
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed" }
& cmake --build $BuildDirectory --config $Configuration
if ($LASTEXITCODE -ne 0) { throw "Native build failed" }
& ctest --test-dir $BuildDirectory -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Native tests failed" }
