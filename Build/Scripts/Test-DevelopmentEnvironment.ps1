[CmdletBinding()]
param(
    [string]$EngineRoot = $env:UE58_ROOT,
    [string]$LyraRoot = $env:LYRA58_ROOT,
    [string]$Root
)

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}

$failures = New-Object 'System.Collections.Generic.List[string]'

function Add-Failure {
    param([string]$Message)
    $script:failures.Add($Message)
}

function Convert-ToVersion {
    param(
        [string]$Value,
        [string]$Description
    )

    try {
        return [version]$Value
    }
    catch {
        Add-Failure "$Description has invalid version '$Value'."
        return $null
    }
}

function Invoke-NativeCommandSafely {
    param(
        [string]$Executable,
        [string[]]$Arguments
    )

    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = @(& $Executable @Arguments 2>&1)
        return [PSCustomObject]@{
            Started = $true
            ExitCode = $LASTEXITCODE
            Output = $output
        }
    }
    catch {
        return [PSCustomObject]@{
            Started = $false
            ExitCode = $null
            Output = @($_.Exception.Message)
        }
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }
}

function Convert-OutputToText {
    param([object[]]$Output)
    return (($Output | ForEach-Object { $_.ToString() }) -join "`n").Trim()
}

if ($PSVersionTable.PSVersion -lt [version]'5.1') {
    Add-Failure "PowerShell 5.1 or newer is required; found $($PSVersionTable.PSVersion)."
}

try {
    $resolvedRoot = (Resolve-Path -LiteralPath $Root -ErrorAction Stop).Path
}
catch {
    Add-Failure "Repository root does not exist or cannot be resolved: $Root"
    $resolvedRoot = $Root
}

$manifestPath = Join-Path $resolvedRoot 'Build\Environment\Toolchain.json'
$manifest = $null
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    Add-Failure "Toolchain manifest is missing: $manifestPath"
}
else {
    try {
        $manifest = Get-Content -LiteralPath $manifestPath -Raw -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop
    }
    catch {
        Add-Failure "Toolchain manifest could not be parsed: $($_.Exception.Message)"
    }
}

if ($null -ne $manifest) {
    if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
        $EngineRoot = [string]$manifest.UnrealEngine.DefaultRoot
    }
    if ([string]::IsNullOrWhiteSpace($LyraRoot)) {
        $LyraRoot = [string]$manifest.Lyra.DefaultRoot
    }

    $minimumVsVersion = Convert-ToVersion -Value ([string]$manifest.VisualStudio.MinimumVersion) -Description 'Visual Studio minimum version'
    $minimumSdkVersion = Convert-ToVersion -Value ([string]$manifest.WindowsSdk.MinimumVersion) -Description 'Windows SDK minimum version'

    $vswhereCandidates = @(
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe')
    )
    $vswhereCommand = Get-Command -Name 'vswhere.exe' -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -ne $vswhereCommand) {
        $vswhereCandidates += $vswhereCommand.Source
    }
    $vswherePath = $vswhereCandidates | Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Select-Object -First 1

    if ([string]::IsNullOrWhiteSpace($vswherePath)) {
        Add-Failure 'vswhere.exe is missing; install Visual Studio Installer.'
    }
    else {
        $requiredWorkload = [string]$manifest.VisualStudio.RequiredWorkload
        $qualifiedVs = Invoke-NativeCommandSafely -Executable $vswherePath -Arguments @('-latest', '-products', '*', '-requires', $requiredWorkload, '-format', 'json', '-utf8')
        if (-not $qualifiedVs.Started -or $qualifiedVs.ExitCode -ne 0) {
            Add-Failure "vswhere failed while locating Visual Studio with workload $requiredWorkload (exit code $($qualifiedVs.ExitCode)): $(Convert-OutputToText $qualifiedVs.Output)"
        }
        else {
            $qualifiedVsJson = Convert-OutputToText $qualifiedVs.Output
            try {
                $qualifiedInstances = @($qualifiedVsJson | ConvertFrom-Json -ErrorAction Stop)
            }
            catch {
                $qualifiedInstances = @()
                Add-Failure "vswhere returned invalid JSON: $($_.Exception.Message)"
            }

            if ($qualifiedInstances.Count -eq 0) {
                Add-Failure "No Visual Studio instance with workload $requiredWorkload is installed."
            }
            else {
                $qualifiedInstance = $qualifiedInstances[0]
                $qualifiedVersion = Convert-ToVersion -Value ([string]$qualifiedInstance.installationVersion) -Description 'Installed Visual Studio version'
                if ($null -ne $qualifiedVersion -and $null -ne $minimumVsVersion -and $qualifiedVersion -lt $minimumVsVersion) {
                    Add-Failure "Visual Studio $qualifiedVersion is below required version $minimumVsVersion."
                }
            }
        }
    }

    $engineVersionPath = Join-Path $EngineRoot 'Engine\Build\Build.version'
    if (-not (Test-Path -LiteralPath $engineVersionPath -PathType Leaf)) {
        Add-Failure "Unreal Engine Build.version is missing: $engineVersionPath"
    }
    else {
        try {
            $engineVersion = Get-Content -LiteralPath $engineVersionPath -Raw -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop
            $actualEngineMajorMinor = "$($engineVersion.MajorVersion).$($engineVersion.MinorVersion)"
            $requiredEngineMajorMinor = [string]$manifest.UnrealEngine.MajorMinor
            if ($actualEngineMajorMinor -ne $requiredEngineMajorMinor) {
                Add-Failure "Unreal Engine version is $actualEngineMajorMinor; required version is $requiredEngineMajorMinor."
            }
        }
        catch {
            Add-Failure "Unreal Engine Build.version could not be parsed: $($_.Exception.Message)"
        }
    }

    $sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
    $sdkIncludeRoot = Join-Path $sdkRoot 'Include'
    if (-not (Test-Path -LiteralPath $sdkIncludeRoot -PathType Container)) {
        Add-Failure "Windows SDK include directory is missing: $sdkIncludeRoot"
    }
    else {
        $sdkVersions = @(
            Get-ChildItem -LiteralPath $sdkIncludeRoot -Directory -ErrorAction SilentlyContinue |
                ForEach-Object {
                    try { [version]$_.Name } catch { $null }
                } |
                Where-Object { $null -ne $_ } |
                Sort-Object -Descending
        )
        if ($sdkVersions.Count -eq 0) {
            Add-Failure "No versioned Windows SDK directories were found under $sdkIncludeRoot."
        }
        else {
            $selectedSdkVersion = $sdkVersions[0]
            $selectedSdkVersionText = $selectedSdkVersion.ToString()
            if ($null -ne $minimumSdkVersion -and $selectedSdkVersion -lt $minimumSdkVersion) {
                Add-Failure "Highest Windows SDK is $selectedSdkVersion; required version is $minimumSdkVersion."
            }

            $sdkArtifacts = @(
                [PSCustomObject]@{ Path = (Join-Path $sdkIncludeRoot $selectedSdkVersionText); Type = 'Container'; Description = 'Include directory' },
                [PSCustomObject]@{ Path = (Join-Path $sdkRoot "Lib\$selectedSdkVersionText\um\x64\kernel32.lib"); Type = 'Leaf'; Description = 'kernel32.lib' },
                [PSCustomObject]@{ Path = (Join-Path $sdkRoot "Lib\$selectedSdkVersionText\ucrt\x64\ucrt.lib"); Type = 'Leaf'; Description = 'ucrt.lib' },
                [PSCustomObject]@{ Path = (Join-Path $sdkRoot "bin\$selectedSdkVersionText\x64\rc.exe"); Type = 'Leaf'; Description = 'rc.exe' }
            )
            foreach ($sdkArtifact in $sdkArtifacts) {
                if (-not (Test-Path -LiteralPath $sdkArtifact.Path -PathType $sdkArtifact.Type)) {
                    Add-Failure "Windows SDK $selectedSdkVersion is missing $($sdkArtifact.Description): $($sdkArtifact.Path)"
                }
            }
        }
    }

    $lyraProjectPath = Join-Path $LyraRoot ([string]$manifest.Lyra.ProjectFile)
    if (-not (Test-Path -LiteralPath $lyraProjectPath -PathType Leaf)) {
        Add-Failure "Lyra project file is missing: $lyraProjectPath"
    }
    else {
        try {
            $lyraProject = Get-Content -LiteralPath $lyraProjectPath -Raw -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop
            $requiredEngineAssociation = [string]$manifest.UnrealEngine.MajorMinor
            if ([string]$lyraProject.EngineAssociation -ne $requiredEngineAssociation) {
                Add-Failure "Lyra EngineAssociation is '$($lyraProject.EngineAssociation)'; required value is '$requiredEngineAssociation'."
            }

            $lyraModuleNames = @($lyraProject.Modules | ForEach-Object { [string]$_.Name })
            foreach ($requiredModule in @('LyraGame', 'LyraEditor')) {
                if ($lyraModuleNames -notcontains $requiredModule) {
                    Add-Failure "Lyra project is missing required module '$requiredModule'."
                }
            }
        }
        catch {
            Add-Failure "Lyra project file could not be parsed: $($_.Exception.Message)"
        }
    }

    $gitCommand = Get-Command -Name 'git.exe' -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -eq $gitCommand) {
        Add-Failure 'git.exe is missing; Git LFS 3.x cannot be checked.'
    }
    else {
        $gitLfs = Invoke-NativeCommandSafely -Executable $gitCommand.Source -Arguments @('lfs', 'version')
        $gitLfsText = Convert-OutputToText $gitLfs.Output
        if (-not $gitLfs.Started -or $gitLfs.ExitCode -ne 0) {
            Add-Failure "git lfs version failed (exit code $($gitLfs.ExitCode)): $gitLfsText"
        }
        elseif ($gitLfsText -notmatch '(?i)git-lfs/3(?:\.|\s)') {
            Add-Failure "Git LFS 3.x is required; found: $gitLfsText"
        }
    }

    try {
        $rootPath = [System.IO.Path]::GetPathRoot($resolvedRoot)
        $driveName = $rootPath.TrimEnd('\').TrimEnd(':')
        $drive = Get-PSDrive -Name $driveName -PSProvider FileSystem -ErrorAction Stop
        $freeBytes = [decimal]$drive.Free
        $minimumFreeGb = [decimal]$manifest.Disk.MinimumFreeGb
        $minimumFreeBytes = $minimumFreeGb * [decimal](1GB)
        $freeGb = [math]::Round(([double]$freeBytes / 1GB), 2)
        Write-Output "INFO: Repository disk free space: $freeGb GB (required: $minimumFreeGb GB)."
        if ($freeBytes -lt $minimumFreeBytes) {
            Add-Failure "Repository disk has $freeGb GB free; at least $minimumFreeGb GB is required."
        }
    }
    catch {
        Add-Failure "Repository disk free space could not be determined: $($_.Exception.Message)"
    }
}

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        Write-Error "FAIL: $failure" -ErrorAction Continue
    }
    Write-Error "Development environment check failed with $($failures.Count) issue(s)." -ErrorAction Continue
    exit 1
}

Write-Output 'PASS: VS, Windows SDK, UE 5.8, Lyra 5.8, Git LFS, and disk space are ready.'
Write-Output "UE58_ROOT=$EngineRoot"
Write-Output "LYRA58_ROOT=$LyraRoot"
exit 0

