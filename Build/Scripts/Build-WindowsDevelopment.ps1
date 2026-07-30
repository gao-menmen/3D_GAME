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

        $item = Get-Item -LiteralPath $candidate -Force -ErrorAction Stop
        if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "$Label contains a reparse point: $candidate"
        }
        if ($item -isnot [System.IO.DirectoryInfo]) {
            throw "$Label contains a non-directory ancestor: $candidate"
        }
    }
}

function Get-FilesFromSafeDirectoryTree {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $rootItem = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    if ($rootItem -isnot [System.IO.DirectoryInfo]) {
        throw "$Label is not a directory: $Path"
    }
    if (($rootItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "$Label contains a reparse point: $($rootItem.FullName)"
    }

    $directories = [System.Collections.Generic.Queue[System.IO.DirectoryInfo]]::new()
    $files = [System.Collections.Generic.List[System.IO.FileInfo]]::new()
    $directories.Enqueue($rootItem)
    while ($directories.Count -gt 0) {
        $directory = $directories.Dequeue()
        foreach ($item in Get-ChildItem -LiteralPath $directory.FullName -Force -ErrorAction Stop) {
            if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "$Label contains a reparse point: $($item.FullName)"
            }
            if ($item -is [System.IO.DirectoryInfo]) {
                $directories.Enqueue($item)
            }
            elseif ($item -is [System.IO.FileInfo]) {
                $files.Add($item)
            }
        }
    }

    return $files.ToArray()
}

$resolvedRoot = (Resolve-Path -LiteralPath $Root -ErrorAction Stop).Path
$resolvedEngineRoot = (Resolve-Path -LiteralPath $EngineRoot -ErrorAction Stop).Path
if (-not (Test-Path -LiteralPath $resolvedRoot -PathType Container)) {
    throw "Repository root is not a directory: $resolvedRoot"
}
if (-not (Test-Path -LiteralPath $resolvedEngineRoot -PathType Container)) {
    throw "Unreal Engine root is not a directory: $resolvedEngineRoot"
}

$runUat = Join-Path $resolvedEngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
$project = Join-Path $resolvedRoot 'UrbanSpear.uproject'
$archive = [System.IO.Path]::GetFullPath((Join-Path $resolvedRoot 'Artifacts\Windows-Development'))
$manifestPath = Join-Path $archive '.urban-spear-build.json'

if (-not (Test-Path -LiteralPath $runUat -PathType Leaf)) {
    throw "RunUAT.bat missing: $runUat"
}
if (-not (Test-Path -LiteralPath $project -PathType Leaf)) {
    throw "Unreal project missing: $project"
}

Assert-NoReparsePointInExistingPath -Path $archive -Boundary $resolvedRoot -Label 'Windows Development archive path'
if (Test-Path -LiteralPath $archive) {
    [void](Get-FilesFromSafeDirectoryTree -Path $archive -Label 'Windows Development archive path')
    Remove-Item -LiteralPath $archive -Recurse -Force -ErrorAction Stop
}
New-Item -ItemType Directory -Path $archive -Force -ErrorAction Stop | Out-Null
Assert-NoReparsePointInExistingPath -Path $archive -Boundary $resolvedRoot -Label 'Windows Development archive path'

$buildStartedUtc = [DateTime]::UtcNow
$uatArguments = @(
    'BuildCookRun',
    "-project=$project",
    '-target=UrbanSpear',
    '-noP4',
    '-platform=Win64',
    '-clientconfig=Development',
    '-build',
    '-cook',
    '-stage',
    '-pak',
    '-archive',
    "-archivedirectory=$archive",
    '-utf8output'
)
& $runUat @uatArguments
$buildExitCode = $LASTEXITCODE
if ($buildExitCode -ne 0) {
    throw "BuildCookRun failed with exit code $buildExitCode"
}
$buildCompletedUtc = [DateTime]::UtcNow

Assert-NoReparsePointInExistingPath -Path $archive -Boundary $resolvedRoot -Label 'Windows Development archive path'
$archiveFiles = @(Get-FilesFromSafeDirectoryTree -Path $archive -Label 'Windows Development archive path')
$executables = @($archiveFiles | Where-Object {
    [string]::Equals($_.Name, 'UrbanSpear.exe', [System.StringComparison]::OrdinalIgnoreCase)
})
if ($executables.Count -ne 1) {
    throw "Expected exactly one UrbanSpear.exe after BuildCookRun; found $($executables.Count) under $archive"
}

$archivePrefix = $archive.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
$executableFullPath = [System.IO.Path]::GetFullPath($executables[0].FullName)
if (-not $executableFullPath.StartsWith($archivePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Packaged executable resolved outside the archive: $executableFullPath"
}
$executableRelativePath = $executableFullPath.Substring($archivePrefix.Length)

$manifest = [ordered]@{
    schemaVersion = 1
    project = 'UrbanSpear'
    configuration = 'Development'
    platform = 'Win64'
    repositoryRoot = $resolvedRoot
    archiveRoot = $archive
    executableRelativePath = $executableRelativePath
    buildStartedUtc = $buildStartedUtc.ToString('o')
    buildCompletedUtc = $buildCompletedUtc.ToString('o')
}
$temporaryManifestPath = Join-Path $archive ('.urban-spear-build.' + [Guid]::NewGuid().ToString('N') + '.tmp')
try {
    $manifestJson = $manifest | ConvertTo-Json -Depth 4
    $utf8WithoutBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($temporaryManifestPath, $manifestJson + [Environment]::NewLine, $utf8WithoutBom)
    Move-Item -LiteralPath $temporaryManifestPath -Destination $manifestPath -Force -ErrorAction Stop
}
finally {
    if (Test-Path -LiteralPath $temporaryManifestPath -PathType Leaf) {
        Remove-Item -LiteralPath $temporaryManifestPath -Force -ErrorAction SilentlyContinue
    }
}

Write-Output "PASS: Windows Development build archived at $archive"
Write-Output "PASS: packaged executable recorded at $executableFullPath"
