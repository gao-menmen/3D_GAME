[CmdletBinding()]
param(
    [string]$SourceRoot = $env:LYRA58_ROOT,
    [string]$DestinationRoot
)

if ([string]::IsNullOrWhiteSpace($DestinationRoot)) {
    $DestinationRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}

$ErrorActionPreference = 'Stop'

function Invoke-RobocopyDirectory {
    param(
        [string]$Source,
        [string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source -PathType Container)) {
        throw "Required Lyra source directory is missing: $Source"
    }

    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    & robocopy.exe $Source $Destination /E /COPY:DAT /DCOPY:DAT /R:2 /W:2 /XD Binaries DerivedDataCache Intermediate Saved .vs Artifacts /NFL /NDL /NJH /NJS /NP
    $robocopyExit = $LASTEXITCODE
    if ($robocopyExit -ge 8) {
        throw "Robocopy failed with exit code $robocopyExit while copying $Source"
    }
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

$destination = (Resolve-Path -LiteralPath $DestinationRoot -ErrorAction Stop).Path.TrimEnd('\')
$git = Get-Command -Name 'git.exe' -CommandType Application -ErrorAction Stop | Select-Object -First 1
$topLevelOutput = @(& $git.Source -C $destination rev-parse --show-toplevel 2>&1)
if ($LASTEXITCODE -ne 0) {
    throw "Destination is not a Git working tree: $(($topLevelOutput | ForEach-Object { $_.ToString() }) -join ' ')"
}
$topLevel = [System.IO.Path]::GetFullPath((($topLevelOutput -join "`n").Trim())).TrimEnd('\')
if (-not [string]::Equals($destination, $topLevel, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "DestinationRoot must be the Git working tree root. Actual root: $topLevel"
}
$repositorySlug = Get-OriginRepositorySlug -GitExecutable $git.Source -WorkingTreeRoot $destination

$manifestPath = Join-Path $destination 'Build\Environment\Toolchain.json'
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Toolchain manifest is missing: $manifestPath"
}
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json

if ([string]::IsNullOrWhiteSpace($SourceRoot)) {
    $SourceRoot = [string]$manifest.Lyra.DefaultRoot
}
$source = (Resolve-Path -LiteralPath $SourceRoot -ErrorAction Stop).Path.TrimEnd('\')
if ([string]::Equals($source, $destination, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Source and destination must differ.'
}

$sourceProjectName = [string]$manifest.Lyra.ProjectFile
if ([string]::IsNullOrWhiteSpace($sourceProjectName)) {
    throw 'Toolchain manifest Lyra.ProjectFile is empty.'
}
$sourceProject = Join-Path $source $sourceProjectName
if (-not (Test-Path -LiteralPath $sourceProject -PathType Leaf)) {
    throw "Lyra project is missing: $sourceProject"
}
$sourceDescriptor = Get-Content -LiteralPath $sourceProject -Raw | ConvertFrom-Json
if ([string]$sourceDescriptor.EngineAssociation -ne [string]$manifest.UnrealEngine.MajorMinor) {
    throw "Lyra EngineAssociation is '$($sourceDescriptor.EngineAssociation)'; required value is '$($manifest.UnrealEngine.MajorMinor)'."
}
$sourceModules = @($sourceDescriptor.Modules | ForEach-Object { [string]$_.Name })
foreach ($requiredModule in @('LyraGame','LyraEditor')) {
    if ($sourceModules -notcontains $requiredModule) {
        throw "Lyra project is missing required module '$requiredModule'."
    }
}

$gh = Get-Command -Name 'gh.exe' -CommandType Application -ErrorAction Stop | Select-Object -First 1
$visibilityOutput = @(& $gh.Source repo view $repositorySlug --json visibility --jq '.visibility' 2>&1)
if ($LASTEXITCODE -ne 0) {
    throw "Repository visibility check failed for ${repositorySlug}: $(($visibilityOutput | ForEach-Object { $_.ToString() }) -join ' ')"
}
if ((($visibilityOutput -join "`n").Trim()) -ne 'PRIVATE') {
    throw "Refusing to import Lyra into public repository $repositorySlug."
}

$directoryMappings = @(
    @('Config','Config'),
    @('Content','Content'),
    @('Plugins','Plugins'),
    @('Source\LyraGame','Source\LyraGame'),
    @('Source\LyraEditor','Source\LyraEditor')
)
foreach ($mapping in $directoryMappings) {
    Invoke-RobocopyDirectory -Source (Join-Path $source $mapping[0]) -Destination (Join-Path $destination $mapping[1])
}

$sourceTargetFiles = @('LyraGame.Target.cs','LyraEditor.Target.cs')
$destinationSource = Join-Path $destination 'Source'
New-Item -ItemType Directory -Path $destinationSource -Force | Out-Null
foreach ($targetFile in $sourceTargetFiles) {
    $sourceTarget = Join-Path (Join-Path $source 'Source') $targetFile
    if (-not (Test-Path -LiteralPath $sourceTarget -PathType Leaf)) {
        throw "Required Lyra target is missing: $sourceTarget"
    }
    Copy-Item -LiteralPath $sourceTarget -Destination (Join-Path $destinationSource $targetFile) -Force
}

$urbanProject = Join-Path $destination 'UrbanSpear.uproject'
$sourceDescriptor.EngineAssociation = [string]$manifest.UnrealEngine.MajorMinor
$sourceDescriptor | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $urbanProject -Encoding UTF8

Write-Output "Verified private destination repository $repositorySlug"
Write-Output "Imported Lyra baseline from $source"
Write-Output "Created $urbanProject"
exit 0
