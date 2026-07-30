[CmdletBinding()]
param(
    [ValidateSet('UrbanSpearEditor', 'UrbanSpear')]
    [string]$Target = 'UrbanSpearEditor',

    [ValidateSet('Development', 'DebugGame', 'Shipping')]
    [string]$Configuration = 'Development',

    [string]$EngineRoot = $env:UE58_ROOT,
    [string]$Root
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}
if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    throw 'UE58_ROOT is not set. Pass -EngineRoot or set UE58_ROOT.'
}

$resolvedRoot = (Resolve-Path -LiteralPath $Root -ErrorAction Stop).Path
$resolvedEngineRoot = (Resolve-Path -LiteralPath $EngineRoot -ErrorAction Stop).Path
$buildBat = Join-Path $resolvedEngineRoot 'Engine\Build\BatchFiles\Build.bat'
$project = Join-Path $resolvedRoot 'UrbanSpear.uproject'

if (-not (Test-Path -LiteralPath $buildBat -PathType Leaf)) {
    throw "Build.bat missing: $buildBat"
}
if (-not (Test-Path -LiteralPath $project -PathType Leaf)) {
    throw "Unreal project missing: $project"
}

& $buildBat $Target Win64 $Configuration "-Project=$project" -WaitMutex -NoHotReloadFromIDE
$buildExitCode = $LASTEXITCODE
if ($buildExitCode -ne 0) {
    throw "Unreal build failed with exit code $buildExitCode"
}

Write-Output "PASS: $Target Win64 $Configuration compiled."
