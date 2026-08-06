[CmdletBinding()]
param(
    [string]$Root,
    [string]$AutomationReport
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}
$resolvedRoot = (Resolve-Path -LiteralPath $Root -ErrorAction Stop).Path
$manifestPath = Join-Path $resolvedRoot 'Build\Manifests\CharacterCameraMilestone.json'
$pluginPath = Join-Path $resolvedRoot 'Plugins\UrbanFoundation\UrbanFoundation.uplugin'
$buildRulesPath = Join-Path $resolvedRoot 'Plugins\UrbanFoundation\Source\UrbanCore\UrbanCore.Build.cs'
$gameConfigPath = Join-Path $resolvedRoot 'Config\DefaultGame.ini'

foreach ($requiredFile in @($manifestPath, $pluginPath, $buildRulesPath, $gameConfigPath)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required milestone file is missing: $requiredFile"
    }
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.schemaVersion -ne 1) {
    throw "Unsupported character-camera milestone schema: $($manifest.schemaVersion)"
}

$missingPaths = @()
foreach ($relativePath in @($manifest.requiredPaths)) {
    $candidate = [System.IO.Path]::GetFullPath((Join-Path $resolvedRoot ([string]$relativePath)))
    if (-not $candidate.StartsWith($resolvedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Milestone path escapes repository root: $relativePath"
    }
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        $missingPaths += [string]$relativePath
    }
}
if ($missingPaths.Count -gt 0) {
    throw "Character-camera milestone paths are missing: $($missingPaths -join ', ')"
}

$plugin = Get-Content -LiteralPath $pluginPath -Raw | ConvertFrom-Json
if ($plugin.CanContainContent -ne $true) {
    throw 'UrbanFoundation.uplugin must keep CanContainContent enabled.'
}

$buildRules = Get-Content -LiteralPath $buildRulesPath -Raw
$missingDependencies = @($manifest.requiredUrbanCoreDependencies | Where-Object {
    $buildRules -notmatch ('"' + [regex]::Escape([string]$_) + '"')
})
if ($missingDependencies.Count -gt 0) {
    throw "UrbanCore dependencies are missing: $($missingDependencies -join ', ')"
}

$gameConfig = Get-Content -LiteralPath $gameConfigPath -Raw
if ($gameConfig -notmatch '(?m)^\+DirectoriesToAlwaysCook=\(Path="/UrbanFoundation/Input"\)\s*$') {
    throw 'Windows packaging must always cook /UrbanFoundation/Input so runtime soft references resolve.'
}

$trackedGenerated = @(& git -C $resolvedRoot ls-files | Where-Object {
    $_ -match '(^|/)(Binaries|Intermediate|Saved|DerivedDataCache|Artifacts)(/|$)'
})
if ($LASTEXITCODE -ne 0) {
    throw 'git ls-files failed while checking generated directories.'
}
if ($trackedGenerated.Count -gt 0) {
    throw "Generated directories are tracked: $($trackedGenerated -join ', ')"
}

if ([string]::IsNullOrWhiteSpace($AutomationReport)) {
    $reports = @(Get-ChildItem -LiteralPath (Join-Path $resolvedRoot 'Saved\Automation') -Filter index.json -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.Directory.Name -like 'CharacterCamera*' } |
        Sort-Object LastWriteTimeUtc -Descending)
    if ($reports.Count -eq 0) {
        throw 'No CharacterCamera automation report was found. Pass -AutomationReport or run the suite first.'
    }
    $AutomationReport = $reports[0].FullName
}
elseif (Test-Path -LiteralPath $AutomationReport -PathType Container) {
    $AutomationReport = Join-Path $AutomationReport 'index.json'
}

$reportPath = (Resolve-Path -LiteralPath $AutomationReport -ErrorAction Stop).Path
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
if ($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0) {
    throw "CharacterCamera automation report is not clean: failed=$($report.failed), notRun=$($report.notRun), inProcess=$($report.inProcess)"
}

$missingTests = @()
foreach ($requiredTest in @($manifest.requiredTests)) {
    $matches = @($report.tests | Where-Object {
        [string]::Equals([string]$_.fullTestPath, [string]$requiredTest, [System.StringComparison]::Ordinal)
    })
    if ($matches.Count -ne 1) {
        $missingTests += [string]$requiredTest
        continue
    }
    $testResult = $matches[0]
    if ($testResult.state -ne 'Success' -or $testResult.errors -ne 0 -or $testResult.warnings -ne 0) {
        throw "Required CharacterCamera test is not clean: $requiredTest state=$($testResult.state), errors=$($testResult.errors), warnings=$($testResult.warnings)"
    }
}
if ($missingTests.Count -gt 0) {
    throw "CharacterCamera automation report lacks required tests: $($missingTests -join ', ')"
}

Write-Output "PASS: character-camera milestone paths, dependencies, repository state, and $(@($manifest.requiredTests).Count) automation tests are valid. Report: $reportPath"
