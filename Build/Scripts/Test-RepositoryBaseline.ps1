[CmdletBinding()]
param(
    [string]$Repository = 'gao-menmen/3D_GAME',
    [string]$Root
)

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}

$failures = [System.Collections.Generic.List[string]]::new()

function Add-Failure {
    param([string]$Message)
    $script:failures.Add($Message)
}

$visibilityOutput = & gh repo view $Repository --json visibility --jq '.visibility' 2>&1
if ($LASTEXITCODE -ne 0) {
    Add-Failure "Unable to read repository visibility for ${Repository}: $($visibilityOutput -join ' ')"
}
else {
    $visibility = (($visibilityOutput | Out-String).Trim()).ToUpperInvariant()
    if ($visibility -ne 'PRIVATE') {
        Add-Failure "Repository visibility is $visibility; expected PRIVATE."
    }
}

$lfsVersionOutput = & git -C $Root lfs version 2>&1
if ($LASTEXITCODE -ne 0) {
    Add-Failure "git-lfs is unavailable: $($lfsVersionOutput -join ' ')"
}
elseif (($lfsVersionOutput | Out-String) -notmatch '(?m)^git-lfs/3\.') {
    Add-Failure "git-lfs must be version 3.x; found: $(($lfsVersionOutput | Out-String).Trim())"
}

$requiredIgnoreRules = @(
    '.superpowers/',
    'Binaries/',
    'DerivedDataCache/',
    'Intermediate/',
    'Saved/',
    '.vs/',
    'Artifacts/',
    '*.sln',
    '*.VC.db'
)
$gitIgnorePath = Join-Path $Root '.gitignore'
if (-not (Test-Path -LiteralPath $gitIgnorePath -PathType Leaf)) {
    Add-Failure '.gitignore is missing.'
}
else {
    $ignoreLines = [System.IO.File]::ReadAllLines($gitIgnorePath)
    foreach ($rule in $requiredIgnoreRules) {
        if ($ignoreLines -notcontains $rule) {
            Add-Failure ".gitignore is missing required rule: $rule"
        }
    }
}

$requiredLfsPatterns = @('*.uasset', '*.umap', '*.fbx', '*.tga', '*.exr', '*.wav', '*.mp4')
$gitAttributesPath = Join-Path $Root '.gitattributes'
if (-not (Test-Path -LiteralPath $gitAttributesPath -PathType Leaf)) {
    Add-Failure '.gitattributes is missing.'
}
else {
    $attributeLines = [System.IO.File]::ReadAllLines($gitAttributesPath)
    foreach ($pattern in $requiredLfsPatterns) {
        $expectedRule = "$pattern filter=lfs diff=lfs merge=lfs -text"
        if ($attributeLines -notcontains $expectedRule) {
            Add-Failure ".gitattributes is missing required LFS rule: $expectedRule"
        }
    }
}

$registryPath = Join-Path $Root 'ThirdParty/AssetRegistry.csv'
$expectedHeader = 'AssetId,AssetName,Provider,SourceUrl,License,RepositoryPolicy,ReleasePolicy,Owner'
if (-not (Test-Path -LiteralPath $registryPath -PathType Leaf)) {
    Add-Failure 'ThirdParty/AssetRegistry.csv is missing.'
}
else {
    $firstLine = Get-Content -LiteralPath $registryPath -TotalCount 1
    if ($firstLine -ne $expectedHeader) {
        Add-Failure "ThirdParty/AssetRegistry.csv has an invalid header; expected: $expectedHeader"
    }
}

if ($failures.Count -gt 0) {
    Write-Output 'FAIL: repository baseline validation failed:'
    foreach ($failure in $failures) {
        Write-Output "- $failure"
    }
    exit 1
}

Write-Output 'PASS: repository privacy, ignore rules, LFS, and asset registry are valid.'