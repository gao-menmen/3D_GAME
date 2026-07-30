[CmdletBinding()]
param(
    [string]$Root,
    [switch]$RequireStaged
)

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}

$failures = New-Object 'System.Collections.Generic.List[string]'

function Add-Failure {
    param([string]$Message)
    $script:failures.Add($Message)
}

function ConvertTo-GitHubRepositorySlug {
    param([string]$RemoteUrl)

    $patterns = @(
        '^(?:https?://)(?:[^/@]+@)?github\.com/(?<owner>[^/]+)/(?<repo>[^/]+?)(?:\.git)?/?$',
        '^ssh://git@github\.com/(?<owner>[^/]+)/(?<repo>[^/]+?)(?:\.git)?/?$',
        '^git@github\.com:(?<owner>[^/]+)/(?<repo>[^/]+?)(?:\.git)?/?$'
    )
    foreach ($pattern in $patterns) {
        $match = [regex]::Match($RemoteUrl.Trim(), $pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
        if ($match.Success) {
            return "$($match.Groups['owner'].Value)/$($match.Groups['repo'].Value)"
        }
    }

    throw "Origin remote is not a supported GitHub URL: $RemoteUrl"
}

function Get-OriginRepositorySlug {
    param(
        [string]$GitExecutable,
        [string]$WorkingTreeRoot
    )

    $fetchOutput = @(& $GitExecutable -C $WorkingTreeRoot remote get-url --all origin 2>&1)
    if ($LASTEXITCODE -ne 0 -or $fetchOutput.Count -eq 0) {
        throw "Origin fetch URL cannot be read: $(($fetchOutput | ForEach-Object { $_.ToString() }) -join ' ')"
    }
    $pushOutput = @(& $GitExecutable -C $WorkingTreeRoot remote get-url --push --all origin 2>&1)
    if ($LASTEXITCODE -ne 0 -or $pushOutput.Count -eq 0) {
        throw "Origin push URL cannot be read: $(($pushOutput | ForEach-Object { $_.ToString() }) -join ' ')"
    }

    $repositorySlugs = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($remoteUrl in @($fetchOutput) + @($pushOutput)) {
        [void]$repositorySlugs.Add((ConvertTo-GitHubRepositorySlug -RemoteUrl ([string]$remoteUrl)))
    }
    if ($repositorySlugs.Count -ne 1) {
        throw "Origin fetch and push URLs must all target the same GitHub repository. Targets: $($repositorySlugs -join ', ')"
    }

    return @($repositorySlugs)[0]
}

try {
    $resolvedRoot = (Resolve-Path -LiteralPath $Root -ErrorAction Stop).Path.TrimEnd('\')
}
catch {
    Write-Error "FAIL: Repository root does not exist: $Root" -ErrorAction Continue
    exit 1
}
$generatedDirectoryPattern = '(^|/)(Binaries|DerivedDataCache|Intermediate|Saved|\.vs|Artifacts)(/|$)'


$git = Get-Command -Name 'git.exe' -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
$repositorySlug = $null
if ($null -eq $git) {
    Add-Failure 'git.exe is missing; repository and LFS checks cannot run.'
}
else {
    $topLevelOutput = @(& $git.Source -C $resolvedRoot rev-parse --show-toplevel 2>&1)
    if ($LASTEXITCODE -ne 0) {
        Add-Failure "Root is not a Git working tree: $(($topLevelOutput | ForEach-Object { $_.ToString() }) -join ' ')"
    }
    else {
        $topLevel = [System.IO.Path]::GetFullPath((($topLevelOutput -join "`n").Trim())).TrimEnd('\')
        if (-not [string]::Equals($resolvedRoot, $topLevel, [System.StringComparison]::OrdinalIgnoreCase)) {
            Add-Failure "Root must be the Git working tree root. Actual root: $topLevel"
        }
        else {
            try {
                $repositorySlug = Get-OriginRepositorySlug -GitExecutable $git.Source -WorkingTreeRoot $resolvedRoot
            }
            catch {
                Add-Failure $_.Exception.Message
            }
        }
    }
}

$required = @(
    'UrbanSpear.uproject',
    'Source\LyraGame.Target.cs',
    'Source\LyraEditor.Target.cs',
    'Source\LyraGame\LyraGame.Build.cs',
    'Source\LyraEditor\LyraEditor.Build.cs',
    'Plugins\GameFeatures\ShooterCore\ShooterCore.uplugin',
    'Content'
)
foreach ($relative in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $resolvedRoot $relative))) {
        Add-Failure "Missing Lyra baseline path: $relative"
    }
}

$descriptorPath = Join-Path $resolvedRoot 'UrbanSpear.uproject'
if (Test-Path -LiteralPath $descriptorPath -PathType Leaf) {
    try {
        $descriptor = Get-Content -LiteralPath $descriptorPath -Raw -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop
        if ([string]$descriptor.EngineAssociation -ne '5.8') {
            Add-Failure "UrbanSpear.uproject must use EngineAssociation 5.8; actual: $($descriptor.EngineAssociation)"
        }
        $moduleNames = @($descriptor.Modules | ForEach-Object { [string]$_.Name })
        foreach ($module in @('LyraGame', 'LyraEditor')) {
            if ($moduleNames -notcontains $module) {
                Add-Failure "UrbanSpear.uproject is missing Lyra module: $module"
            }
        }
    }
    catch {
        Add-Failure "UrbanSpear.uproject could not be parsed: $($_.Exception.Message)"
    }
}

$gh = Get-Command -Name 'gh.exe' -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
if ($null -eq $gh) {
    Add-Failure 'GitHub CLI is missing; repository privacy cannot be checked.'
}
elseif (-not [string]::IsNullOrWhiteSpace($repositorySlug)) {
    $visibilityOutput = @(& $gh.Source repo view $repositorySlug --json visibility --jq '.visibility' 2>&1)
    if ($LASTEXITCODE -ne 0) {
        Add-Failure "Repository visibility check failed for ${repositorySlug}: $(($visibilityOutput | ForEach-Object { $_.ToString() }) -join ' ')"
    }
    elseif ((($visibilityOutput -join "`n").Trim()) -ne 'PRIVATE') {
        Add-Failure "Repository $repositorySlug must be PRIVATE before importing Lyra."
    }
}

if ($null -ne $git) {
    $indexedPathsOutput = @(& $git.Source -C $resolvedRoot ls-files 2>&1)
    if ($LASTEXITCODE -ne 0) {
        Add-Failure "git ls-files failed while checking generated directories: $(($indexedPathsOutput | ForEach-Object { $_.ToString() }) -join ' ')"
    }
    else {
        $generatedPaths = @($indexedPathsOutput | Where-Object { ([string]$_).Replace('\','/') -match $generatedDirectoryPattern })
        if ($generatedPaths.Count -gt 0) {
            $examples = @($generatedPaths | Select-Object -First 10) -join ', '
            Add-Failure "Generated directories contain $($generatedPaths.Count) Git-tracked or staged file(s): $examples"
        }
    }
}

$assets = @(Get-ChildItem -LiteralPath $resolvedRoot -Recurse -File -ErrorAction SilentlyContinue | Where-Object {
    if ($_.Extension -notin @('.uasset','.umap')) {
        return $false
    }
    $relativeAssetPath = $_.FullName.Substring($resolvedRoot.Length).TrimStart('\').Replace('\','/')
    return $relativeAssetPath -notmatch $generatedDirectoryPattern
})
if ($assets.Count -eq 0) {
    Add-Failure 'No Unreal binary asset was imported.'
}
elseif ($null -ne $git) {
    $relativeAssets = @($assets | ForEach-Object { $_.FullName.Substring($resolvedRoot.Length).TrimStart('\').Replace('\','/') })
    for ($offset = 0; $offset -lt $relativeAssets.Count; $offset += 200) {
        $last = [Math]::Min($offset + 199, $relativeAssets.Count - 1)
        $batch = @($relativeAssets[$offset..$last])
        $attrOutput = @(& $git.Source -C $resolvedRoot check-attr filter -- @batch 2>&1)
        if ($LASTEXITCODE -ne 0) {
            Add-Failure "git check-attr failed for Unreal binary assets: $(($attrOutput | ForEach-Object { $_.ToString() }) -join ' ')"
            break
        }
        foreach ($attrLine in $attrOutput) {
            if ([string]$attrLine -notmatch ': filter: lfs$') {
                Add-Failure "Asset is not covered by Git LFS: $attrLine"
            }
        }
    }

    if ($RequireStaged) {
        $indexedAssetsOutput = @(& $git.Source -C $resolvedRoot ls-files -- '*.uasset' '*.umap' 2>&1)
        if ($LASTEXITCODE -ne 0) {
            Add-Failure "git ls-files failed for Unreal binary assets: $(($indexedAssetsOutput | ForEach-Object { $_.ToString() }) -join ' ')"
        }
        else {
            $indexedAssets = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
            foreach ($path in $indexedAssetsOutput) {
                [void]$indexedAssets.Add(([string]$path).Replace('\','/'))
            }
            foreach ($asset in $relativeAssets) {
                if (-not $indexedAssets.Contains($asset)) {
                    Add-Failure "Asset is not present in the Git index: $asset"
                }
            }
        }

        $lfsJsonOutput = (& $git.Source -C $resolvedRoot lfs ls-files --json 2>&1 | Out-String)
        if ($LASTEXITCODE -ne 0) {
            Add-Failure "git lfs ls-files --json failed: $($lfsJsonOutput.Trim())"
        }
        else {
            try {
                $lfsData = $lfsJsonOutput | ConvertFrom-Json -ErrorAction Stop
                $lfsAssets = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
                foreach ($file in @($lfsData.files)) {
                    [void]$lfsAssets.Add(([string]$file.name).Replace('\','/'))
                }
                foreach ($asset in $relativeAssets) {
                    if (-not $lfsAssets.Contains($asset)) {
                        Add-Failure "Asset is not registered as an indexed Git LFS object: $asset"
                    }
                }
            }
            catch {
                Add-Failure "git lfs ls-files returned invalid JSON: $($_.Exception.Message)"
            }
        }

        $lfsFsckOutput = @(& $git.Source -C $resolvedRoot lfs fsck --pointers 2>&1)
        if ($LASTEXITCODE -ne 0) {
            Add-Failure "git lfs fsck --pointers failed: $(($lfsFsckOutput | ForEach-Object { $_.ToString() }) -join ' ')"
        }
    }
}

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        Write-Error "FAIL: $failure" -ErrorAction Continue
    }
    Write-Error "Lyra import check failed with $($failures.Count) issue(s)." -ErrorAction Continue
    exit 1
}

$stagedLabel = if ($RequireStaged) { ', staged LFS pointers' } else { '' }
Write-Output "PASS: Lyra baseline, private origin repository, generated-directory exclusions${stagedLabel}, and LFS coverage are valid."
exit 0
