param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$BuildDirectory = "build/native",
    [string]$DeployComponentDirectory = "",
    [ValidatePattern('^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$')]
    [string]$Version
)

$ErrorActionPreference = "Stop"
$configure = @("-S", ".", "-B", $BuildDirectory, "-A", "Win32", "-DCMAKE_POLICY_VERSION_MINIMUM=3.5")
if ($Version) {
    $configure += "-DOMP_DRIP_VERSION=$Version"
}
if ($DeployComponentDirectory) {
    $configure += "-DOMP_DRIP_DEPLOY_COMPONENT_DIR=$DeployComponentDirectory"
}
& cmake @configure
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed" }
& cmake --build $BuildDirectory --config $Configuration
if ($LASTEXITCODE -ne 0) { throw "Native build failed" }
& ctest --test-dir $BuildDirectory -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Native tests failed" }
