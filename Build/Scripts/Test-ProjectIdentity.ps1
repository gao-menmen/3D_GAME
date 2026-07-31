[CmdletBinding()]
param([string]$Root)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}

$failures = [System.Collections.Generic.List[string]]::new()

function Get-IniSectionEntries {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$SectionName
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "INI file is missing: $Path"
    }

    $sectionCount = 0
    $insideSection = $false
    $entries = @{}
    foreach ($line in Get-Content -LiteralPath $Path -ErrorAction Stop) {
        if ($line -match '^\s*\[(?<Section>[^\]]+)\]\s*$') {
            $insideSection = [string]::Equals(
                $Matches.Section.Trim(),
                $SectionName,
                [System.StringComparison]::OrdinalIgnoreCase
            )
            if ($insideSection) {
                $sectionCount++
            }
            continue
        }
        if (-not $insideSection -or $line -match '^\s*(?:;|#|$)') {
            continue
        }
        if ($line -match '^\s*(?<Key>[^=]+?)\s*=\s*(?<Value>.*)\s*$') {
            $key = $Matches.Key.Trim()
            $value = $Matches.Value.Trim()
            if (-not $entries.ContainsKey($key)) {
                $entries[$key] = [System.Collections.Generic.List[string]]::new()
            }
            $entries[$key].Add($value)
        }
    }

    return [PSCustomObject]@{
        SectionCount = $sectionCount
        Entries = $entries
    }
}
$projectPath = Join-Path $Root 'UrbanSpear.uproject'
if (-not (Test-Path $projectPath)) {
    $failures.Add('UrbanSpear.uproject is missing.')
}
elseif ((Get-Content $projectPath -Raw | ConvertFrom-Json).EngineAssociation -ne '5.8') {
    $failures.Add('EngineAssociation must be 5.8.')
}

$gameTarget = Join-Path $Root 'Source\UrbanSpear.Target.cs'
$editorTarget = Join-Path $Root 'Source\UrbanSpearEditor.Target.cs'
if (-not (Test-Path $gameTarget)) {
    $failures.Add('UrbanSpear game target is missing.')
}
else {
    $gameTargetContent = Get-Content $gameTarget -Raw
    if ($gameTargetContent -notmatch 'class UrbanSpearTarget : LyraGameTarget') {
        $failures.Add('Game target must inherit LyraGameTarget.')
    }
    if ($gameTargetContent -notmatch 'GeneratedProjectName\s*=\s*"UrbanSpearTargets"') {
        $failures.Add('Game target must isolate its generated project name.')
    }
}

if (-not (Test-Path $editorTarget)) {
    $failures.Add('UrbanSpear editor target is missing.')
}
else {
    $editorTargetContent = Get-Content $editorTarget -Raw
    if ($editorTargetContent -notmatch 'class UrbanSpearEditorTarget : LyraEditorTarget') {
        $failures.Add('Editor target must inherit LyraEditorTarget.')
    }
    if ($editorTargetContent -notmatch 'GeneratedProjectName\s*=\s*"UrbanSpearTargets"') {
        $failures.Add('Editor target must isolate its generated project name.')
    }
}

$defaultEnginePath = Join-Path $Root 'Config\DefaultEngine.ini'
try {
    $buildSettings = Get-IniSectionEntries -Path $defaultEnginePath -SectionName '/Script/BuildSettings.BuildSettings'
    if ($buildSettings.SectionCount -ne 1) {
        $failures.Add("DefaultEngine.ini must contain exactly one [/Script/BuildSettings.BuildSettings] section; found $($buildSettings.SectionCount).")
    }
    else {
        foreach ($expectedEntry in @(
            [PSCustomObject]@{ Key = 'DefaultGameTarget'; Value = 'UrbanSpear' },
            [PSCustomObject]@{ Key = 'DefaultEditorTarget'; Value = 'UrbanSpearEditor' }
        )) {
            $matchingKeys = @($buildSettings.Entries.Keys | Where-Object {
                [string]::Equals($_, $expectedEntry.Key, [System.StringComparison]::OrdinalIgnoreCase)
            })
            $values = @()
            if ($matchingKeys.Count -eq 1) {
                $values = @($buildSettings.Entries[$matchingKeys[0]])
            }
            if ($matchingKeys.Count -ne 1 -or $values.Count -ne 1 -or -not [string]::Equals(
                [string]$values[0],
                $expectedEntry.Value,
                [System.StringComparison]::Ordinal
            )) {
                $actual = if ($values.Count -eq 0) { '<missing>' } else { $values -join ', ' }
                $failures.Add("DefaultEngine.ini [/Script/BuildSettings.BuildSettings] must set $($expectedEntry.Key)=$($expectedEntry.Value) exactly once; actual: $actual")
            }
        }
    }
}
catch {
    $failures.Add("DefaultEngine.ini build target validation failed: $($_.Exception.Message)")
}

$defaultEngineText = Get-Content -LiteralPath $defaultEnginePath -Raw -ErrorAction Stop
if ($defaultEngineText -match '(?mi)^\s*\+VulkanTargetedShaderFormats\s*=') {
    $failures.Add('Windows packaging must not target Vulkan; the foundation build supports DirectX on Windows.')
}

$ini = Get-Content (Join-Path $Root 'Config\DefaultGame.ini') -Raw
foreach ($entry in @('ProjectName=Project Urban Spear','BuildTarget=UrbanSpear')) {
    if ($ini -notmatch [regex]::Escape($entry)) {
        $failures.Add("DefaultGame.ini missing: $entry")
    }
}

if ($failures.Count) {
    Write-Output 'FAIL: project identity validation failed:'
    foreach ($failure in $failures) {
        Write-Output "- $failure"
    }
    exit 1
}

Write-Output 'PASS: descriptor, metadata, and UrbanSpear targets are valid.'