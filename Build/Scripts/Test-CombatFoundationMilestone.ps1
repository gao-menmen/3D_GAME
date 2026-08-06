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
$manifestPath = Join-Path $resolvedRoot 'Build\Manifests\CombatFoundationMilestone.json'
$buildRulesPath = Join-Path $resolvedRoot 'Plugins\UrbanFoundation\Source\UrbanCombat\UrbanCombat.Build.cs'

foreach ($requiredFile in @($manifestPath, $buildRulesPath)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required combat milestone file is missing: $requiredFile"
    }
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.schemaVersion -ne 1) {
    throw "Unsupported combat milestone schema: $($manifest.schemaVersion)"
}

$missingPaths = @()
foreach ($relativePath in @($manifest.requiredPaths)) {
    $candidate = [System.IO.Path]::GetFullPath((Join-Path $resolvedRoot ([string]$relativePath)))
    $boundary = $resolvedRoot.TrimEnd('\') + '\'
    if (-not $candidate.StartsWith($boundary, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Combat milestone path escapes repository root: $relativePath"
    }
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        $missingPaths += [string]$relativePath
    }
}
if ($missingPaths.Count -gt 0) {
    throw "Combat milestone paths are missing: $($missingPaths -join ', ')"
}

$buildRules = Get-Content -LiteralPath $buildRulesPath -Raw
$missingDependencies = @($manifest.requiredUrbanCombatDependencies | Where-Object {
    $buildRules -notmatch ('"' + [regex]::Escape([string]$_) + '"')
})
if ($missingDependencies.Count -gt 0) {
    throw "UrbanCombat dependencies are missing: $($missingDependencies -join ', ')"
}

$trackedGenerated = @(& git -c "safe.directory=$($resolvedRoot.Replace('\','/'))" -C $resolvedRoot ls-files | Where-Object {
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
        Where-Object { $_.Directory.Name -like 'Combat*' } |
        Sort-Object LastWriteTimeUtc -Descending)
    if ($reports.Count -eq 0) {
        throw 'No Combat automation report was found. Pass -AutomationReport or run the suite first.'
    }
    $AutomationReport = $reports[0].FullName
}
elseif (Test-Path -LiteralPath $AutomationReport -PathType Container) {
    $AutomationReport = Join-Path $AutomationReport 'index.json'
}

$reportPath = (Resolve-Path -LiteralPath $AutomationReport -ErrorAction Stop).Path
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
if ($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0) {
    throw "Combat automation report is not clean: failed=$($report.failed), notRun=$($report.notRun), inProcess=$($report.inProcess)"
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
        throw "Required Combat test is not clean: $requiredTest state=$($testResult.state), errors=$($testResult.errors), warnings=$($testResult.warnings)"
    }
}
if ($missingTests.Count -gt 0) {
    throw "Combat automation report lacks required tests: $($missingTests -join ', ')"
}

Write-Output "PASS: combat foundation paths, dependencies, repository state, and $(@($manifest.requiredTests).Count) automation tests are valid. Report: $reportPath"
