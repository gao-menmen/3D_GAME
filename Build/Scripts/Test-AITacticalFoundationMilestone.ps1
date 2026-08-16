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
$manifestPath = Join-Path $resolvedRoot 'Build\Manifests\AITacticalFoundationMilestone.json'
$buildRulesPath = Join-Path $resolvedRoot 'Plugins\UrbanFoundation\Source\UrbanAI\UrbanAI.Build.cs'
foreach ($requiredFile in @($manifestPath, $buildRulesPath)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required AI milestone file is missing: $requiredFile"
    }
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.schemaVersion -ne 1) {
    throw "Unsupported AI tactical foundation schema: $($manifest.schemaVersion)"
}

$boundary = $resolvedRoot.TrimEnd('\') + '\'
$missingPaths = @()
foreach ($relativePath in @($manifest.requiredPaths)) {
    $candidate = [IO.Path]::GetFullPath((Join-Path $resolvedRoot ([string]$relativePath)))
    if (-not $candidate.StartsWith($boundary, [StringComparison]::OrdinalIgnoreCase)) {
        throw "AI milestone path escapes repository root: $relativePath"
    }
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        $missingPaths += [string]$relativePath
    }
}
if ($missingPaths.Count -gt 0) {
    throw "AI milestone paths are missing: $($missingPaths -join ', ')"
}

$buildRules = Get-Content -LiteralPath $buildRulesPath -Raw
$missingDependencies = @($manifest.requiredUrbanAIDependencies | Where-Object {
    $buildRules -notmatch ('"' + [regex]::Escape([string]$_) + '"')
})
if ($missingDependencies.Count -gt 0) {
    throw "UrbanAI dependencies are missing: $($missingDependencies -join ', ')"
}

$trackedGenerated = @(& git -c "safe.directory=$($resolvedRoot.Replace('\','/'))" -C $resolvedRoot ls-files | Where-Object {
    $_ -match '(^|/)(Binaries|Intermediate|Saved|DerivedDataCache|Artifacts)(/|$)'
})
if ($LASTEXITCODE -ne 0) { throw 'git ls-files failed while checking generated directories.' }
if ($trackedGenerated.Count -gt 0) { throw "Generated directories are tracked: $($trackedGenerated -join ', ')" }

if ([string]::IsNullOrWhiteSpace($AutomationReport)) {
    $reports = @(Get-ChildItem -LiteralPath (Join-Path $resolvedRoot 'Saved\Automation') -Filter index.json -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.Directory.Name -like 'AITacticalFoundation*' } |
        Sort-Object LastWriteTimeUtc -Descending)
    if ($reports.Count -eq 0) { throw 'No AI tactical foundation report was found.' }
    $AutomationReport = $reports[0].FullName
}
elseif (Test-Path -LiteralPath $AutomationReport -PathType Container) {
    $AutomationReport = Join-Path $AutomationReport 'index.json'
}

$reportPath = (Resolve-Path -LiteralPath $AutomationReport -ErrorAction Stop).Path
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
if ($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0) {
    throw "AI automation report is not clean: failed=$($report.failed), notRun=$($report.notRun), inProcess=$($report.inProcess)"
}

$missingTests = @()
foreach ($requiredTest in @($manifest.requiredTests)) {
    $matches = @($report.tests | Where-Object { [string]$_.fullTestPath -ceq [string]$requiredTest })
    if ($matches.Count -ne 1) { $missingTests += [string]$requiredTest; continue }
    $result = $matches[0]
    if ($result.state -ne 'Success' -or $result.errors -ne 0 -or $result.warnings -ne 0) {
        throw "Required AI test is not clean: $requiredTest state=$($result.state), errors=$($result.errors), warnings=$($result.warnings)"
    }
}
if ($missingTests.Count -gt 0) { throw "AI report lacks required tests: $($missingTests -join ', ')" }
Write-Output "PASS: AI tactical foundation paths, dependencies, repository state, and $(@($manifest.requiredTests).Count) tests are valid. Report: $reportPath"
