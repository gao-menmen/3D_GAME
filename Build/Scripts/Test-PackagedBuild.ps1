[CmdletBinding()]
param(
    [string]$Root,

    [ValidateRange(1, 86400)]
    [int]$TimeoutSeconds = 90,

    [ValidateRange(1, 10080)]
    [int]$MaxBuildAgeMinutes = 240,

    [string[]]$LaunchArguments = @(
        '-nullrhi',
        '-unattended',
        '-nosplash',
        '-NoSound',
        '-ExecCmds=quit'
    )
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
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

function Get-RequiredStringProperty {
    param(
        [Parameter(Mandatory = $true)][object]$Object,
        [Parameter(Mandatory = $true)][string]$PropertyName,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $property = $Object.PSObject.Properties[$PropertyName]
    if ($null -eq $property -or $property.Value -isnot [string] -or [string]::IsNullOrWhiteSpace([string]$property.Value)) {
        throw "$Label is missing or invalid: $PropertyName"
    }
    return [string]$property.Value
}

function ConvertTo-RequiredUtcTimestamp {
    param(
        [Parameter(Mandatory = $true)][string]$Value,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $timestamp = [DateTimeOffset]::MinValue
    $parsed = [DateTimeOffset]::TryParseExact(
        $Value,
        'o',
        [System.Globalization.CultureInfo]::InvariantCulture,
        [System.Globalization.DateTimeStyles]::None,
        [ref]$timestamp
    )
    if (-not $parsed) {
        throw "$Label is not an ISO 8601 round-trip timestamp: $Value"
    }
    return $timestamp.ToUniversalTime()
}

function ConvertTo-WindowsCommandLineArgument {
    param([AllowEmptyString()][string]$Argument)

    if ($null -eq $Argument) {
        throw 'LaunchArguments cannot contain null values.'
    }
    if ($Argument.Length -gt 0 -and $Argument -notmatch '[\s"]') {
        return $Argument
    }

    $builder = New-Object System.Text.StringBuilder
    [void]$builder.Append('"')
    $backslashCount = 0
    foreach ($character in $Argument.ToCharArray()) {
        if ($character -eq [char]'\') {
            $backslashCount++
            continue
        }
        if ($character -eq [char]'"') {
            if ($backslashCount -gt 0) {
                [void]$builder.Append(('\' * ($backslashCount * 2)))
            }
            [void]$builder.Append('\"')
            $backslashCount = 0
            continue
        }
        if ($backslashCount -gt 0) {
            [void]$builder.Append(('\' * $backslashCount))
            $backslashCount = 0
        }
        [void]$builder.Append($character)
    }
    if ($backslashCount -gt 0) {
        [void]$builder.Append(('\' * ($backslashCount * 2)))
    }
    [void]$builder.Append('"')
    return $builder.ToString()
}

function Stop-ProcessTreeSafely {
    param([Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process)

    if ($Process.HasExited) {
        return
    }

    $taskKill = Join-Path $env:SystemRoot 'System32\taskkill.exe'
    if (Test-Path -LiteralPath $taskKill -PathType Leaf) {
        & $taskKill /PID ([string]$Process.Id) /T /F *> $null
        $taskKillExitCode = $LASTEXITCODE
        if ($taskKillExitCode -ne 0 -and -not $Process.HasExited) {
            Write-Warning "taskkill exited with code $taskKillExitCode while cleaning process $($Process.Id); trying direct termination."
        }
    }

    if (-not $Process.HasExited) {
        $Process.Kill()
    }
    [void]$Process.WaitForExit(10000)
    if (-not $Process.HasExited) {
        throw "Timed-out packaged process could not be terminated: PID $($Process.Id)"
    }
}

$resolvedRoot = (Resolve-Path -LiteralPath $Root -ErrorAction Stop).Path
if (-not (Test-Path -LiteralPath $resolvedRoot -PathType Container)) {
    throw "Repository root is not a directory: $resolvedRoot"
}
$project = Join-Path $resolvedRoot 'UrbanSpear.uproject'
if (-not (Test-Path -LiteralPath $project -PathType Leaf)) {
    throw "Unreal project missing: $project"
}

$archive = [System.IO.Path]::GetFullPath((Join-Path $resolvedRoot 'Artifacts\Windows-Development'))
$manifestPath = Join-Path $archive '.urban-spear-build.json'
Assert-NoReparsePointInExistingPath -Path $archive -Boundary $resolvedRoot -Label 'Windows Development archive path'
if (-not (Test-Path -LiteralPath $archive -PathType Container)) {
    throw "Windows Development archive is missing: $archive"
}
$archiveFiles = @(Get-FilesFromSafeDirectoryTree -Path $archive -Label 'Windows Development archive path')

$expectedLauncherPath = [System.IO.Path]::GetFullPath((Join-Path $archive 'UrbanSpear.exe'))
$launchers = @($archiveFiles | Where-Object {
    [string]::Equals([System.IO.Path]::GetFullPath($_.FullName), $expectedLauncherPath, [System.StringComparison]::OrdinalIgnoreCase)
})
if ($launchers.Count -ne 1) {
    throw "Expected the packaged launcher at $expectedLauncherPath; found $($launchers.Count)."
}
$executable = $launchers[0]

if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Build provenance manifest is missing: $manifestPath"
}
try {
    $manifest = Get-Content -LiteralPath $manifestPath -Raw -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop
}
catch {
    throw "Build provenance manifest is invalid JSON: $($_.Exception.Message)"
}

$schemaProperty = $manifest.PSObject.Properties['schemaVersion']
$schemaTypeCode = if ($null -eq $schemaProperty -or $null -eq $schemaProperty.Value) {
    [System.TypeCode]::Empty
}
else {
    [System.Type]::GetTypeCode($schemaProperty.Value.GetType())
}
if ($schemaTypeCode -notin @(
    [System.TypeCode]::SByte,
    [System.TypeCode]::Byte,
    [System.TypeCode]::Int16,
    [System.TypeCode]::UInt16,
    [System.TypeCode]::Int32,
    [System.TypeCode]::UInt32,
    [System.TypeCode]::Int64,
    [System.TypeCode]::UInt64
) -or [decimal]$schemaProperty.Value -ne 1) {
    throw 'Build provenance manifest has an unsupported schemaVersion.'
}
$manifestProject = Get-RequiredStringProperty -Object $manifest -PropertyName 'project' -Label 'Build provenance manifest'
$manifestConfiguration = Get-RequiredStringProperty -Object $manifest -PropertyName 'configuration' -Label 'Build provenance manifest'
$manifestPlatform = Get-RequiredStringProperty -Object $manifest -PropertyName 'platform' -Label 'Build provenance manifest'
if ($manifestProject -ne 'UrbanSpear' -or $manifestConfiguration -ne 'Development' -or $manifestPlatform -ne 'Win64') {
    throw "Build provenance identity is invalid: project=$manifestProject, configuration=$manifestConfiguration, platform=$manifestPlatform"
}

$manifestRepositoryRoot = Get-RequiredStringProperty -Object $manifest -PropertyName 'repositoryRoot' -Label 'Build provenance manifest'
try {
    $manifestRepositoryRoot = [System.IO.Path]::GetFullPath($manifestRepositoryRoot).TrimEnd('\', '/')
}
catch {
    throw "Build provenance repository root is invalid: $manifestRepositoryRoot"
}
if (-not [string]::Equals($manifestRepositoryRoot, $resolvedRoot.TrimEnd('\', '/'), [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Build provenance repository root does not match the requested repository: $manifestRepositoryRoot"
}

$manifestArchiveRoot = Get-RequiredStringProperty -Object $manifest -PropertyName 'archiveRoot' -Label 'Build provenance manifest'
try {
    $manifestArchiveRoot = [System.IO.Path]::GetFullPath($manifestArchiveRoot).TrimEnd('\', '/')
}
catch {
    throw "Build provenance archive root is invalid: $manifestArchiveRoot"
}
if (-not [string]::Equals($manifestArchiveRoot, $archive.TrimEnd('\', '/'), [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Build provenance archive root does not match the fixed repository archive: $manifestArchiveRoot"
}

$relativeExecutable = Get-RequiredStringProperty -Object $manifest -PropertyName 'executableRelativePath' -Label 'Build provenance manifest'
if ([System.IO.Path]::IsPathRooted($relativeExecutable)) {
    throw "Build provenance executable path must be relative: $relativeExecutable"
}
$archivePrefix = $archive.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
$recordedExecutable = [System.IO.Path]::GetFullPath((Join-Path $archive $relativeExecutable))
if (-not $recordedExecutable.StartsWith($archivePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Build provenance executable resolves outside the archive: $recordedExecutable"
}
if (-not [string]::Equals($recordedExecutable, [System.IO.Path]::GetFullPath($executable.FullName), [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Build provenance executable does not match the packaged launcher: $recordedExecutable"
}

$startedText = Get-RequiredStringProperty -Object $manifest -PropertyName 'buildStartedUtc' -Label 'Build provenance manifest'
$completedText = Get-RequiredStringProperty -Object $manifest -PropertyName 'buildCompletedUtc' -Label 'Build provenance manifest'
$buildStartedUtc = ConvertTo-RequiredUtcTimestamp -Value $startedText -Label 'buildStartedUtc'
$buildCompletedUtc = ConvertTo-RequiredUtcTimestamp -Value $completedText -Label 'buildCompletedUtc'
if ($buildCompletedUtc -lt $buildStartedUtc) {
    throw 'Build provenance completion time precedes its start time.'
}
$nowUtc = [DateTimeOffset]::UtcNow
if ($buildCompletedUtc -gt $nowUtc.AddMinutes(5)) {
    throw "Build provenance completion time is unexpectedly in the future: $completedText"
}
$buildAge = $nowUtc - $buildCompletedUtc
if ($buildAge.TotalMinutes -gt $MaxBuildAgeMinutes) {
    throw "Packaged build is older than $MaxBuildAgeMinutes minute(s): completed $completedText"
}

$quotedArguments = @($LaunchArguments | ForEach-Object { ConvertTo-WindowsCommandLineArgument -Argument $_ })
$startInfo = New-Object System.Diagnostics.ProcessStartInfo
$startInfo.FileName = $executable.FullName
$startInfo.Arguments = [string]::Join(' ', $quotedArguments)
$startInfo.WorkingDirectory = $executable.DirectoryName
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$process = New-Object System.Diagnostics.Process
$process.StartInfo = $startInfo
$processStarted = $false
try {
    if (-not $process.Start()) {
        throw "Packaged executable could not be started: $($executable.FullName)"
    }
    $processStarted = $true
    $exited = $process.WaitForExit($TimeoutSeconds * 1000)
    if (-not $exited) {
        Stop-ProcessTreeSafely -Process $process
        throw "Packaged build did not exit within $TimeoutSeconds second(s)."
    }
    $process.WaitForExit()
    $packagedExitCode = $process.ExitCode
    if ($packagedExitCode -ne 0) {
        throw "Packaged build exited with code $packagedExitCode"
    }
}
finally {
    if ($null -ne $process) {
        if ($processStarted -and -not $process.HasExited) {
            Stop-ProcessTreeSafely -Process $process
        }
        $process.Dispose()
    }
}

Write-Output "PASS: current packaged executable started and exited cleanly: $($executable.FullName)"
