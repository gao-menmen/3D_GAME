[CmdletBinding()]
param(
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

function Assert-NoReparsePointInExistingPath {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Boundary,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $boundaryFullPath = [System.IO.Path]::GetFullPath($Boundary).TrimEnd([char[]]@(
        [System.IO.Path]::DirectorySeparatorChar,
        [System.IO.Path]::AltDirectorySeparatorChar
    ))
    $pathFullPath = [System.IO.Path]::GetFullPath($Path)
    $boundaryPrefix = $boundaryFullPath + [System.IO.Path]::DirectorySeparatorChar
    if (-not $pathFullPath.StartsWith($boundaryPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label is outside its allowed boundary: $pathFullPath"
    }

    $pathRoot = [System.IO.Path]::GetPathRoot($pathFullPath)
    if ([string]::IsNullOrWhiteSpace($pathRoot)) {
        throw "$Label has no filesystem root: $pathFullPath"
    }

    $pathsToCheck = [System.Collections.Generic.List[string]]::new()
    $pathsToCheck.Add($pathRoot)
    $currentPath = $pathRoot
    $relativePath = $pathFullPath.Substring($pathRoot.Length)
    foreach ($segment in $relativePath.Split([char[]]@(
        [System.IO.Path]::DirectorySeparatorChar,
        [System.IO.Path]::AltDirectorySeparatorChar
    ), [System.StringSplitOptions]::RemoveEmptyEntries)) {
        $currentPath = Join-Path $currentPath $segment
        $pathsToCheck.Add($currentPath)
    }

    foreach ($candidate in $pathsToCheck) {
        if (-not (Test-Path -LiteralPath $candidate)) {
            break
        }

        $item = Get-Item -LiteralPath $candidate -Force
        if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "$Label contains a reparse point: $candidate"
        }
        if ($item -isnot [System.IO.DirectoryInfo]) {
            throw "$Label contains a non-directory ancestor: $candidate"
        }
    }
}

function Get-RequiredNonNegativeIntegerProperty {
    param(
        [Parameter(Mandatory = $true)][object]$Object,
        [Parameter(Mandatory = $true)][string]$PropertyName,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $property = $Object.PSObject.Properties[$PropertyName]
    if ($null -eq $property -or $null -eq $property.Value) {
        throw "$Label is missing or null: $PropertyName"
    }

    $value = $property.Value
    $typeCode = [System.Type]::GetTypeCode($value.GetType())
    $integerTypeCodes = @(
        [System.TypeCode]::SByte,
        [System.TypeCode]::Byte,
        [System.TypeCode]::Int16,
        [System.TypeCode]::UInt16,
        [System.TypeCode]::Int32,
        [System.TypeCode]::UInt32,
        [System.TypeCode]::Int64,
        [System.TypeCode]::UInt64
    )
    if ($integerTypeCodes -notcontains $typeCode) {
        throw "$Label must be a non-negative integer: $PropertyName=$value"
    }

    $integerValue = [decimal]$value
    if ($integerValue -lt 0) {
        throw "$Label must be non-negative: $PropertyName=$value"
    }
    return $integerValue
}

$resolvedRoot = (Resolve-Path -LiteralPath $Root -ErrorAction Stop).Path
$resolvedEngineRoot = (Resolve-Path -LiteralPath $EngineRoot -ErrorAction Stop).Path
$editorCmd = Join-Path $resolvedEngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $resolvedRoot 'UrbanSpear.uproject'

if (-not (Test-Path -LiteralPath $editorCmd -PathType Leaf)) {
    throw "UnrealEditor-Cmd.exe missing: $editorCmd"
}
if (-not (Test-Path -LiteralPath $project -PathType Leaf)) {
    throw "Unreal project missing: $project"
}

$automationRoot = [System.IO.Path]::GetFullPath((Join-Path $resolvedRoot 'Saved\Automation'))
$report = [System.IO.Path]::GetFullPath((Join-Path $automationRoot 'Foundation'))
Assert-NoReparsePointInExistingPath -Path $report -Boundary $resolvedRoot -Label 'Automation report path'

if (Test-Path -LiteralPath $report) {
    Remove-Item -LiteralPath $report -Recurse -Force
}
New-Item -ItemType Directory -Path $report -Force | Out-Null

& $editorCmd $project -unattended -nop4 -nosplash -NullRHI '-ExecCmds=Automation RunTests UrbanSpear.Foundation; Quit' '-TestExit=Automation Test Queue Empty' "-ReportOutputPath=$report" -log
$automationExitCode = $LASTEXITCODE
if ($automationExitCode -ne 0) {
    throw "Automation tests failed with exit code $automationExitCode"
}

$index = Join-Path $report 'index.json'
if (-not (Test-Path -LiteralPath $index -PathType Leaf)) {
    throw "Automation report missing: $index"
}

try {
    $result = Get-Content -LiteralPath $index -Raw | ConvertFrom-Json
}
catch {
    throw "Automation report is invalid JSON: $($_.Exception.Message)"
}

$failed = Get-RequiredNonNegativeIntegerProperty -Object $result -PropertyName 'failed' -Label 'Automation report counter'
$succeeded = Get-RequiredNonNegativeIntegerProperty -Object $result -PropertyName 'succeeded' -Label 'Automation report counter'
$notRun = Get-RequiredNonNegativeIntegerProperty -Object $result -PropertyName 'notRun' -Label 'Automation report counter'
$inProcess = Get-RequiredNonNegativeIntegerProperty -Object $result -PropertyName 'inProcess' -Label 'Automation report counter'
if ($null -eq $result.PSObject.Properties['tests']) {
    throw 'Automation report is missing required property: tests'
}
if ($failed -ne 0 -or $notRun -ne 0 -or $inProcess -ne 0 -or $succeeded -lt 2) {
    throw "Expected 2+ passing, 0 failing, 0 not-run, and 0 in-process tests; succeeded=$succeeded, failed=$failed, notRun=$notRun, inProcess=$inProcess"
}

$requiredTests = @(
    'UrbanSpear.Foundation.ModuleLoad',
    'UrbanSpear.Foundation.ProjectIdentity'
)
foreach ($requiredTest in $requiredTests) {
    $matches = @($result.tests | Where-Object {
        [string]::Equals([string]$_.fullTestPath, $requiredTest, [System.StringComparison]::Ordinal)
    })
    if ($matches.Count -ne 1) {
        throw "Expected exactly one result for $requiredTest; found $($matches.Count)."
    }

    $testResult = $matches[0]
    $stateProperty = $testResult.PSObject.Properties['state']
    if ($null -eq $stateProperty -or $stateProperty.Value -isnot [string]) {
        throw "Test result for $requiredTest has a missing or invalid state."
    }

    $state = [string]$stateProperty.Value
    $errors = Get-RequiredNonNegativeIntegerProperty -Object $testResult -PropertyName 'errors' -Label "Test result for $requiredTest"
    $warnings = Get-RequiredNonNegativeIntegerProperty -Object $testResult -PropertyName 'warnings' -Label "Test result for $requiredTest"
    if (-not [string]::Equals($state, 'Success', [System.StringComparison]::Ordinal) -or $errors -ne 0 -or $warnings -ne 0) {
        throw "Required test did not pass cleanly: $requiredTest state=$state, errors=$errors, warnings=$warnings"
    }
}

Write-Output "PASS: $succeeded UrbanSpear.Foundation tests succeeded, including both required smoke tests. Report: $report"
