[CmdletBinding()]
param([string]$Root)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}

$failures = [System.Collections.Generic.List[string]]::new()
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