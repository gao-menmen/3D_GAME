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

function Get-RequiredCommandPath {
    param([string]$Name)

    try {
        $command = Get-Command -Name $Name -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    }
    catch {
        $command = $null
    }

    if ($null -eq $command) {
        Add-Failure "Required command is unavailable: $Name"
        return $null
    }

    return $command.Source
}

function Invoke-NativeCommandSafely {
    param(
        [string]$Executable,
        [string[]]$Arguments
    )

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = @(& $Executable @Arguments 2>&1)
        $exitCode = $LASTEXITCODE
        return [PSCustomObject]@{
            Started = $true
            ExitCode = $exitCode
            Output = $output
        }
    }
    catch {
        return [PSCustomObject]@{
            Started = $false
            ExitCode = $null
            Output = @()
        }
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
}

function Convert-CommandOutputToText {
    param([object[]]$Output)
    return (($Output | ForEach-Object { $_.ToString() }) -join "`n").Trim()
}

function Convert-CommandOutputToLines {
    param([object[]]$Output)

    return @(
        $Output |
            ForEach-Object { $_.ToString().Trim() } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    )
}

function Get-GitAttributeRuleParts {
    param([string]$Line)

    $fallbackTokens = @($Line -split '\s+' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    $fallbackPattern = if ($fallbackTokens.Count -gt 0) { $fallbackTokens[0] } else { '' }
    $fallbackAttributes = if ($fallbackTokens.Count -gt 1) {
        @($fallbackTokens | Select-Object -Skip 1)
    }
    else {
        @()
    }

    if (-not $Line.StartsWith('"', [System.StringComparison]::Ordinal)) {
        return [PSCustomObject]@{
            Parsed = $true
            Pattern = $fallbackPattern
            Attributes = $fallbackAttributes
        }
    }

    $patternBuilder = [System.Text.StringBuilder]::new()
    $closingQuoteIndex = -1
    $parseSucceeded = $true
    for ($characterIndex = 1; $characterIndex -lt $Line.Length; $characterIndex++) {
        $character = $Line[$characterIndex]
        if ($character -eq [char]34) {
            $closingQuoteIndex = $characterIndex
            break
        }

        if ($character -ne [char]92) {
            $null = $patternBuilder.Append($character)
            continue
        }

        $characterIndex++
        if ($characterIndex -ge $Line.Length) {
            $parseSucceeded = $false
            break
        }

        $escapedCharacter = $Line[$characterIndex]
        switch ($escapedCharacter) {
            '"' { $null = $patternBuilder.Append([char]34) }
            '\' { $null = $patternBuilder.Append([char]92) }
            'a' { $null = $patternBuilder.Append([char]7) }
            'b' { $null = $patternBuilder.Append([char]8) }
            'f' { $null = $patternBuilder.Append([char]12) }
            'n' { $null = $patternBuilder.Append([char]10) }
            'r' { $null = $patternBuilder.Append([char]13) }
            't' { $null = $patternBuilder.Append([char]9) }
            'v' { $null = $patternBuilder.Append([char]11) }
            default {
                if ($escapedCharacter -ge '0' -and $escapedCharacter -le '7') {
                    $octalDigits = [string]$escapedCharacter
                    for ($octalOffset = 1; $octalOffset -lt 3; $octalOffset++) {
                        $nextIndex = $characterIndex + 1
                        if ($nextIndex -ge $Line.Length -or
                            $Line[$nextIndex] -lt '0' -or
                            $Line[$nextIndex] -gt '7') {
                            break
                        }

                        $characterIndex = $nextIndex
                        $octalDigits += $Line[$characterIndex]
                    }
                    $null = $patternBuilder.Append([char][Convert]::ToInt32($octalDigits, 8))
                }
                else {
                    $parseSucceeded = $false
                }
            }
        }

        if (-not $parseSucceeded) {
            break
        }
    }

    if ($closingQuoteIndex -lt 0 -or
        -not $parseSucceeded -or
        ($closingQuoteIndex + 1 -lt $Line.Length -and -not [char]::IsWhiteSpace($Line[$closingQuoteIndex + 1]))) {
        return [PSCustomObject]@{
            Parsed = $false
            Pattern = $fallbackPattern
            Attributes = $fallbackAttributes
        }
    }

    $attributeText = $Line.Substring($closingQuoteIndex + 1).Trim()
    $attributes = if ([string]::IsNullOrWhiteSpace($attributeText)) {
        @()
    }
    else {
        @($attributeText -split '\s+' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    }

    return [PSCustomObject]@{
        Parsed = $true
        Pattern = $patternBuilder.ToString()
        Attributes = $attributes
    }
}

function Test-VersionedRepositoryRules {
    param(
        [string]$SourceRoot,
        [string]$GitPath,
        [string[]]$IgnoreProbes,
        [string[]]$RequiredLfsPatterns,
        [string[]]$VersionedRulePaths,
        [string[]]$RepositoryAssetPaths
    )

    $temporaryRoot = $null
    $environmentVariableNames = @(
        'HOME',
        'USERPROFILE',
        'XDG_CONFIG_HOME',
        'GIT_CONFIG_GLOBAL',
        'GIT_CONFIG_NOSYSTEM',
        'GIT_ATTR_NOSYSTEM'
    )
    $originalEnvironment = @{}
    foreach ($variableName in $environmentVariableNames) {
        $originalEnvironment[$variableName] = [System.Environment]::GetEnvironmentVariable(
            $variableName,
            [System.EnvironmentVariableTarget]::Process
        )
    }

    try {
        try {
            $temporaryBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
            $temporaryRoot = [System.IO.Path]::GetFullPath(
                (Join-Path $temporaryBase ("RepositoryBaseline-$([System.Guid]::NewGuid().ToString('N'))"))
            )
            $temporaryRepository = Join-Path $temporaryRoot 'repository'
            $temporaryHome = Join-Path $temporaryRoot 'home'
            $emptyGlobalConfig = Join-Path $temporaryRoot 'empty-gitconfig'
            $emptyGlobalExcludes = Join-Path $temporaryRoot 'empty-global-excludes'
            $emptyGlobalAttributes = Join-Path $temporaryRoot 'empty-global-attributes'

            $null = [System.IO.Directory]::CreateDirectory($temporaryRepository)
            $null = [System.IO.Directory]::CreateDirectory($temporaryHome)
            [System.IO.File]::WriteAllText($emptyGlobalConfig, '')
            [System.IO.File]::WriteAllText($emptyGlobalExcludes, '')
            [System.IO.File]::WriteAllText($emptyGlobalAttributes, '')
        }
        catch {
            Add-Failure 'Temporary repository could not be created for versioned rule validation.'
            return
        }

        try {
            [System.Environment]::SetEnvironmentVariable('HOME', $temporaryHome, [System.EnvironmentVariableTarget]::Process)
            [System.Environment]::SetEnvironmentVariable('USERPROFILE', $temporaryHome, [System.EnvironmentVariableTarget]::Process)
            [System.Environment]::SetEnvironmentVariable('XDG_CONFIG_HOME', $temporaryHome, [System.EnvironmentVariableTarget]::Process)
            [System.Environment]::SetEnvironmentVariable('GIT_CONFIG_GLOBAL', $emptyGlobalConfig, [System.EnvironmentVariableTarget]::Process)
            [System.Environment]::SetEnvironmentVariable('GIT_CONFIG_NOSYSTEM', '1', [System.EnvironmentVariableTarget]::Process)
            [System.Environment]::SetEnvironmentVariable('GIT_ATTR_NOSYSTEM', '1', [System.EnvironmentVariableTarget]::Process)
        }
        catch {
            Add-Failure 'Temporary Git environment could not be isolated for versioned rule validation.'
            return
        }

        $isolatedGitConfiguration = @(
            '-c', "core.excludesFile=$emptyGlobalExcludes",
            '-c', "core.attributesFile=$emptyGlobalAttributes"
        )
        $initResult = Invoke-NativeCommandSafely -Executable $GitPath -Arguments (
            $isolatedGitConfiguration + @('init', '--quiet', $temporaryRepository)
        )
        if (-not $initResult.Started -or $initResult.ExitCode -ne 0) {
            Add-Failure 'Git init failed for versioned rule validation.'
            return
        }

        $copySucceeded = $true
        $sourceRootPrefix = [System.IO.Path]::GetFullPath($SourceRoot).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
        $temporaryRepositoryPrefix = [System.IO.Path]::GetFullPath($temporaryRepository).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
        foreach ($relativeRulePath in $VersionedRulePaths) {
            $normalizedRulePath = $relativeRulePath.Replace('\', '/')
            try {
                if ([System.IO.Path]::IsPathRooted($normalizedRulePath) -or $normalizedRulePath -match '(^|/)\.\.(/|$)') {
                    throw 'Unsafe relative rule path.'
                }

                $sourceRulePath = [System.IO.Path]::GetFullPath((Join-Path $SourceRoot $normalizedRulePath))
                $temporaryRulePath = [System.IO.Path]::GetFullPath((Join-Path $temporaryRepository $normalizedRulePath))
                if (-not $sourceRulePath.StartsWith($sourceRootPrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
                    -not $temporaryRulePath.StartsWith($temporaryRepositoryPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
                    throw 'Rule path escaped its repository root.'
                }

                $null = [System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($temporaryRulePath))
                Copy-Item -LiteralPath $sourceRulePath -Destination $temporaryRulePath -ErrorAction Stop
                $temporaryRuleItem = Get-Item -LiteralPath $temporaryRulePath -Force -ErrorAction Stop
                if ($temporaryRuleItem.IsReadOnly) {
                    $temporaryRuleItem.IsReadOnly = $false
                }
            }
            catch {
                Add-Failure "Versioned rule file could not be copied for isolated validation: $normalizedRulePath"
                $copySucceeded = $false
            }
        }
        if (-not $copySucceeded) {
            return
        }

        try {
            $gitInfoPath = Join-Path $temporaryRepository '.git/info'
            [System.IO.File]::WriteAllText((Join-Path $gitInfoPath 'exclude'), '')
            [System.IO.File]::WriteAllText((Join-Path $gitInfoPath 'attributes'), '')
        }
        catch {
            Add-Failure 'Temporary repository-local Git rules could not be cleared for isolated validation.'
            return
        }

        $placeholderCreationSucceeded = $true
        $attributeDirectoryProbeToDetails = @{}
        foreach ($relativeAssetPath in $RepositoryAssetPaths) {
            $normalizedAssetPath = $relativeAssetPath.Replace('\', '/')
            try {
                if ([System.IO.Path]::IsPathRooted($normalizedAssetPath) -or $normalizedAssetPath -match '(^|/)\.\.(/|$)') {
                    throw 'Unsafe relative asset path.'
                }

                $temporaryAssetPath = [System.IO.Path]::GetFullPath((Join-Path $temporaryRepository $normalizedAssetPath))
                if (-not $temporaryAssetPath.StartsWith($temporaryRepositoryPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
                    throw 'Asset path escaped its repository root.'
                }

                $null = [System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($temporaryAssetPath))
                [System.IO.File]::WriteAllBytes($temporaryAssetPath, [byte[]]@())
            }
            catch {
                Add-Failure "Temporary asset probe could not be created for isolated validation: $normalizedAssetPath"
                $placeholderCreationSucceeded = $false
            }
        }
        foreach ($relativeRulePath in $VersionedRulePaths) {
            $normalizedRulePath = $relativeRulePath.Replace('\', '/')
            if ([System.IO.Path]::GetFileName($normalizedRulePath) -cne '.gitattributes') {
                continue
            }

            $relativeAttributesDirectory = [System.IO.Path]::GetDirectoryName($normalizedRulePath)
            if ($null -eq $relativeAttributesDirectory) {
                $relativeAttributesDirectory = ''
            }
            else {
                $relativeAttributesDirectory = $relativeAttributesDirectory.Replace('\', '/')
            }

            foreach ($pattern in $RequiredLfsPatterns) {
                try {
                    do {
                        $probeFileName = "RepositoryBaselineAttributesDirectory-$([System.Guid]::NewGuid().ToString('N'))$($pattern.Substring(1))"
                        if ([string]::IsNullOrWhiteSpace($relativeAttributesDirectory)) {
                            $probeRelativePath = $probeFileName
                        }
                        else {
                            $probeRelativePath = "$relativeAttributesDirectory/$probeFileName"
                        }
                        $temporaryProbePath = [System.IO.Path]::GetFullPath((Join-Path $temporaryRepository $probeRelativePath))
                    } while ([System.IO.File]::Exists($temporaryProbePath) -or [System.IO.Directory]::Exists($temporaryProbePath))

                    if (-not $temporaryProbePath.StartsWith($temporaryRepositoryPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
                        throw 'Attributes-directory probe escaped its repository root.'
                    }

                    $null = [System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($temporaryProbePath))
                    [System.IO.File]::WriteAllBytes($temporaryProbePath, [byte[]]@())
                    $attributeDirectoryProbeToDetails[$probeRelativePath] = @{
                        Pattern = $pattern
                        RulePath = $normalizedRulePath
                    }
                }
                catch {
                    Add-Failure "Temporary attributes-directory probe could not be created for isolated validation: $normalizedRulePath ($pattern)"
                    $placeholderCreationSucceeded = $false
                }
            }
        }

        if (-not $placeholderCreationSucceeded) {
            return
        }

        foreach ($probe in $IgnoreProbes) {
            $ignoreResult = Invoke-NativeCommandSafely -Executable $GitPath -Arguments (
                $isolatedGitConfiguration + @(
                    '-C', $temporaryRepository, 'check-ignore', '--no-index', '--quiet', '--', $probe
                )
            )
            if (-not $ignoreResult.Started -or ($ignoreResult.ExitCode -ne 0 -and $ignoreResult.ExitCode -ne 1)) {
                Add-Failure "Git check-ignore command failed while validating versioned rules for probe: $probe"
            }
            elseif ($ignoreResult.ExitCode -eq 1) {
                Add-Failure "Versioned .gitignore behavior is missing for probe: $probe"
            }
        }

        $attributeProbeToPattern = @{}
        foreach ($pattern in $RequiredLfsPatterns) {
            foreach ($prefix in @('', 'Content/', 'Plugins/UrbanFoundation/Content/')) {
                $probe = "${prefix}RepositoryBaseline$($pattern.Substring(1))"
                $attributeProbeToPattern[$probe] = $pattern
            }
        }

        $attributeTargets = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
        foreach ($probe in $attributeProbeToPattern.Keys) {
            $null = $attributeTargets.Add($probe)
        }
        foreach ($probe in $attributeDirectoryProbeToDetails.Keys) {
            $null = $attributeTargets.Add($probe)
        }
        foreach ($assetPath in $RepositoryAssetPaths) {
            $null = $attributeTargets.Add($assetPath.Replace('\', '/'))
        }

        $orderedAttributeTargets = @($attributeTargets | Sort-Object)
        $attributeBatchSize = 50
        for ($batchStart = 0; $batchStart -lt $orderedAttributeTargets.Count; $batchStart += $attributeBatchSize) {
            $batchEnd = [Math]::Min($batchStart + $attributeBatchSize - 1, $orderedAttributeTargets.Count - 1)
            $attributeBatch = @($orderedAttributeTargets[$batchStart..$batchEnd])
            $checkAttributeResult = Invoke-NativeCommandSafely -Executable $GitPath -Arguments (
                $isolatedGitConfiguration + @(
                    '-c', 'core.quotePath=false', '-C', $temporaryRepository,
                    'check-attr', 'filter', 'diff', 'merge', 'text', '--'
                ) + $attributeBatch
            )
            if (-not $checkAttributeResult.Started -or $checkAttributeResult.ExitCode -ne 0) {
                Add-Failure 'Git check-attr command failed while validating versioned LFS attributes.'
                continue
            }

            $attributeLines = @($checkAttributeResult.Output | ForEach-Object { $_.ToString() })
            foreach ($targetPath in $attributeBatch) {
                $expectedAttributeLines = @(
                    "${targetPath}: filter: lfs",
                    "${targetPath}: diff: lfs",
                    "${targetPath}: merge: lfs",
                    "${targetPath}: text: unset"
                )
                $attributesAreValid = $true
                foreach ($expectedLine in $expectedAttributeLines) {
                    if ($attributeLines -cnotcontains $expectedLine) {
                        $attributesAreValid = $false
                    }
                }

                if (-not $attributesAreValid) {
                    if ($attributeProbeToPattern.ContainsKey($targetPath)) {
                        Add-Failure "Versioned .gitattributes LFS attributes are invalid for pattern: $($attributeProbeToPattern[$targetPath]) (probe: $targetPath)"
                    }
                    elseif ($attributeDirectoryProbeToDetails.ContainsKey($targetPath)) {
                        $probeDetails = $attributeDirectoryProbeToDetails[$targetPath]
                        Add-Failure "Versioned .gitattributes LFS attributes are invalid for pattern: $($probeDetails.Pattern) (rule: $($probeDetails.RulePath))"
                    }
                    else {
                        Add-Failure "Versioned .gitattributes LFS attributes are invalid for repository asset path: $targetPath"
                    }
                }
            }
        }
    }
    finally {
        $environmentRestored = $true
        foreach ($variableName in $environmentVariableNames) {
            try {
                [System.Environment]::SetEnvironmentVariable(
                    $variableName,
                    $originalEnvironment[$variableName],
                    [System.EnvironmentVariableTarget]::Process
                )
            }
            catch {
                $environmentRestored = $false
            }
        }
        if (-not $environmentRestored) {
            Add-Failure 'Temporary Git environment could not be fully restored.'
        }

        if (-not [string]::IsNullOrWhiteSpace($temporaryRoot) -and [System.IO.Directory]::Exists($temporaryRoot)) {
            $cleanupIsSafe = $false
            $normalizedTemporaryRoot = $null
            try {
                $normalizedTemporaryBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
                $normalizedTemporaryRoot = [System.IO.Path]::GetFullPath($temporaryRoot)
                $cleanupIsSafe = $normalizedTemporaryRoot.StartsWith(
                    $normalizedTemporaryBase,
                    [System.StringComparison]::OrdinalIgnoreCase
                ) -and [System.IO.Path]::GetFileName($normalizedTemporaryRoot).StartsWith(
                    'RepositoryBaseline-',
                    [System.StringComparison]::Ordinal
                )
            }
            catch {
                $cleanupIsSafe = $false
            }

            if ($cleanupIsSafe) {
                for ($cleanupAttempt = 0; $cleanupAttempt -lt 3 -and [System.IO.Directory]::Exists($normalizedTemporaryRoot); $cleanupAttempt++) {
                    try {
                        Get-ChildItem -LiteralPath $normalizedTemporaryRoot -Recurse -Force -File -ErrorAction Stop |
                            ForEach-Object {
                                if ($_.IsReadOnly) {
                                    $_.IsReadOnly = $false
                                }
                            }
                        Get-ChildItem -LiteralPath $normalizedTemporaryRoot -Recurse -Force -Directory -ErrorAction Stop |
                            Sort-Object -Property FullName -Descending |
                            ForEach-Object {
                                $_.Attributes = $_.Attributes -band (-bnot [System.IO.FileAttributes]::ReadOnly)
                            }
                        $temporaryRootItem = Get-Item -LiteralPath $normalizedTemporaryRoot -Force -ErrorAction Stop
                        $temporaryRootItem.Attributes = $temporaryRootItem.Attributes -band (-bnot [System.IO.FileAttributes]::ReadOnly)
                        Remove-Item -LiteralPath $normalizedTemporaryRoot -Recurse -Force -ErrorAction Stop
                    }
                    catch {
                        if ($cleanupAttempt -lt 2) {
                            Start-Sleep -Milliseconds 50
                        }
                    }
                }
            }

            if ([System.IO.Directory]::Exists($temporaryRoot)) {
                Add-Failure 'Temporary repository cleanup failed after versioned rule validation.'
            }
        }
    }
}

function Get-NormalizedRepositoryIdentity {
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return $null
    }

    $normalized = $Value.Trim().Trim('/')
    $normalized = [System.Text.RegularExpressions.Regex]::Replace(
        $normalized,
        '\.git$',
        '',
        [System.Text.RegularExpressions.RegexOptions]::IgnoreCase
    )
    $parts = @($normalized -split '/')
    if ($parts.Count -ne 2 -or [string]::IsNullOrWhiteSpace($parts[0]) -or [string]::IsNullOrWhiteSpace($parts[1])) {
        return $null
    }

    return "$($parts[0])/$($parts[1])"
}

function Get-RemoteRepositoryLocation {
    param([string]$RemoteUrl)

    $unsupportedResult = [PSCustomObject]@{
        Status = 'Unsupported'
        Host = $null
        Repository = $null
    }
    if ([string]::IsNullOrWhiteSpace($RemoteUrl)) {
        return $unsupportedResult
    }

    $candidate = $RemoteUrl.Trim()
    $status = 'Unsupported'
    $remoteHost = $null
    $repositoryPath = $null

    if ($candidate -match '^(?i)https://') {
        $uri = $null
        if ([System.Uri]::TryCreate($candidate, [System.UriKind]::Absolute, [ref]$uri) -and
            $uri.Scheme -eq 'https' -and
            [string]::IsNullOrEmpty($uri.UserInfo) -and
            $uri.IsDefaultPort -and
            [string]::IsNullOrEmpty($uri.Query) -and
            [string]::IsNullOrEmpty($uri.Fragment)) {
            $status = 'Valid'
            $remoteHost = $uri.DnsSafeHost
            $repositoryPath = $uri.AbsolutePath
        }
    }
    elseif ($candidate -match '^(?i)ssh://') {
        $uri = $null
        if ([System.Uri]::TryCreate($candidate, [System.UriKind]::Absolute, [ref]$uri) -and
            $uri.Scheme -eq 'ssh' -and
            $uri.UserInfo -eq 'git' -and
            ($uri.Port -eq -1 -or $uri.Port -eq 22) -and
            [string]::IsNullOrEmpty($uri.Query) -and
            [string]::IsNullOrEmpty($uri.Fragment)) {
            $status = 'Valid'
            $remoteHost = $uri.DnsSafeHost
            $repositoryPath = $uri.AbsolutePath
        }
    }
    elseif ($candidate -match '^(?:(?<User>[^@:\s]+)@)?(?<Host>[^:\s]+):(?<RepositoryPath>.+)$') {
        $scpUser = $Matches.User
        $remoteHost = $Matches.Host
        $repositoryPath = $Matches.RepositoryPath
        if (-not [string]::IsNullOrWhiteSpace($scpUser) -and $scpUser -ne 'git') {
            return $unsupportedResult
        }

        if ([string]::Equals($remoteHost, 'github.com', [System.StringComparison]::OrdinalIgnoreCase)) {
            $status = 'Valid'
        }
        else {
            $status = 'UnverifiedSshHost'
        }
    }

    if ($status -eq 'Unsupported') {
        return $unsupportedResult
    }

    $repositoryIdentity = Get-NormalizedRepositoryIdentity -Value $repositoryPath
    if ($null -eq $repositoryIdentity) {
        return $unsupportedResult
    }

    return [PSCustomObject]@{
        Status = $status
        Host = $remoteHost
        Repository = $repositoryIdentity
    }
}
function Test-CsvExactColumnStructure {
    param(
        [string]$Path,
        [int]$ExpectedColumnCount
    )

    $parser = $null
    $structureIsValid = $true
    try {
        Add-Type -AssemblyName Microsoft.VisualBasic -ErrorAction Stop
        $parser = New-Object Microsoft.VisualBasic.FileIO.TextFieldParser -ArgumentList $Path
        $parser.TextFieldType = [Microsoft.VisualBasic.FileIO.FieldType]::Delimited
        $parser.SetDelimiters(',')
        $parser.HasFieldsEnclosedInQuotes = $true
        $parser.TrimWhiteSpace = $false
        $rowNumber = 0
        while (-not $parser.EndOfData) {
            $rowNumber++
            try {
                $fields = @($parser.ReadFields())
            }
            catch {
                Add-Failure "ThirdParty/AssetRegistry.csv has a CSV parsing error at row $rowNumber."
                $structureIsValid = $false
                break
            }

            if ($fields.Count -ne $ExpectedColumnCount) {
                Add-Failure "ThirdParty/AssetRegistry.csv row $rowNumber does not have the exact 8-column structure."
                $structureIsValid = $false
            }
        }
    }
    catch {
        Add-Failure 'ThirdParty/AssetRegistry.csv could not be structurally parsed as CSV.'
        $structureIsValid = $false
    }
    finally {
        if ($null -ne $parser) {
            $parser.Dispose()
        }
    }

    return $structureIsValid
}

$resolvedRoot = $null
$rootIsUsable = $false
$gitRepositoryReady = $false
$expectedRepositoryIdentity = $null
$ghPath = $null
$gitPath = $null
$gitLfsPath = $null

try {
    $expectedRepositoryIdentity = Get-NormalizedRepositoryIdentity -Value $Repository
    if ($null -eq $expectedRepositoryIdentity) {
        Add-Failure 'Repository must use the owner/repo format.'
    }

    $ghPath = Get-RequiredCommandPath -Name 'gh'
    $gitPath = Get-RequiredCommandPath -Name 'git'
    $gitLfsPath = Get-RequiredCommandPath -Name 'git-lfs'

    if ([string]::IsNullOrWhiteSpace($Root)) {
        Add-Failure 'Root does not exist or is unavailable.'
    }
    else {
        try {
            $rootItem = Get-Item -LiteralPath $Root -ErrorAction Stop
            if (-not $rootItem.PSIsContainer) {
                Add-Failure 'Root does not exist or is unavailable.'
            }
            else {
                $resolvedRoot = $rootItem.FullName
                $rootIsUsable = $true
            }
        }
        catch {
            Add-Failure 'Root does not exist or is unavailable.'
        }
    }

    if ($null -ne $ghPath -and $null -ne $expectedRepositoryIdentity) {
        $visibilityResult = Invoke-NativeCommandSafely -Executable $ghPath -Arguments @(
            'repo', 'view', "github.com/$expectedRepositoryIdentity", '--json', 'visibility', '--jq', '.visibility'
        )
        if (-not $visibilityResult.Started -or $visibilityResult.ExitCode -ne 0) {
            Add-Failure 'GitHub CLI command failed while reading repository visibility.'
        }
        else {
            $visibility = (Convert-CommandOutputToText -Output $visibilityResult.Output).ToUpperInvariant()
            if ($visibility -ne 'PRIVATE') {
                if ($visibility -eq 'PUBLIC') {
                    Add-Failure 'Repository visibility is PUBLIC; expected PRIVATE.'
                }
                else {
                    Add-Failure 'Repository visibility could not be confirmed as PRIVATE.'
                }
            }
        }
    }

    if ($null -ne $gitLfsPath) {
        $lfsVersionResult = Invoke-NativeCommandSafely -Executable $gitLfsPath -Arguments @('version')
        if (-not $lfsVersionResult.Started -or $lfsVersionResult.ExitCode -ne 0) {
            Add-Failure 'git-lfs command failed while reading its version.'
        }
        else {
            $lfsVersion = Convert-CommandOutputToText -Output $lfsVersionResult.Output
            if ($lfsVersion -notmatch '(?m)^git-lfs/3\.') {
                Add-Failure "git-lfs must be version 3.x; found a different version."
            }
        }
    }

    if ($rootIsUsable -and $null -ne $gitPath) {
        $gitRootResult = Invoke-NativeCommandSafely -Executable $gitPath -Arguments @(
            '-C', $resolvedRoot, 'rev-parse', '--show-toplevel'
        )
        if (-not $gitRootResult.Started -or $gitRootResult.ExitCode -ne 0) {
            Add-Failure 'Git command failed while validating Root as a repository.'
        }
        else {
            $topLevelText = Convert-CommandOutputToText -Output $gitRootResult.Output
            try {
                $topLevelItem = Get-Item -LiteralPath $topLevelText -ErrorAction Stop
                $topLevelPath = $topLevelItem.FullName.TrimEnd('\', '/')
                $normalizedRootPath = $resolvedRoot.TrimEnd('\', '/')
                if (-not [string]::Equals($topLevelPath, $normalizedRootPath, [System.StringComparison]::OrdinalIgnoreCase)) {
                    Add-Failure 'Root must be the Git repository top-level directory.'
                }
                else {
                    $gitRepositoryReady = $true
                }
            }
            catch {
                Add-Failure 'Git command returned an unusable repository root.'
            }
        }
    }

    if ($gitRepositoryReady -and $null -ne $expectedRepositoryIdentity) {
        $originDestinations = @(
            [PSCustomObject]@{
                Kind = 'fetch'
                Arguments = @('-C', $resolvedRoot, 'remote', 'get-url', '--all', 'origin')
            },
            [PSCustomObject]@{
                Kind = 'push'
                Arguments = @('-C', $resolvedRoot, 'remote', 'get-url', '--push', '--all', 'origin')
            }
        )

        foreach ($destination in $originDestinations) {
            $originResult = Invoke-NativeCommandSafely -Executable $gitPath -Arguments $destination.Arguments
            if (-not $originResult.Started -or $originResult.ExitCode -ne 0) {
                Add-Failure "Git command failed while reading all origin $($destination.Kind) URLs."
                continue
            }

            $originUrls = @(Convert-CommandOutputToLines -Output $originResult.Output)
            if ($originUrls.Count -eq 0) {
                Add-Failure "Git origin has no $($destination.Kind) URL to validate."
                continue
            }

            foreach ($originUrl in $originUrls) {
                $originLocation = Get-RemoteRepositoryLocation -RemoteUrl $originUrl
                $destinationContext = " Invalid origin $($destination.Kind) URL."
                if ($originLocation.Status -eq 'UnverifiedSshHost') {
                    Add-Failure "Git origin uses a custom SSH host alias that cannot be verified as github.com. Use a canonical github.com origin URL.$destinationContext"
                }
                elseif ($originLocation.Status -ne 'Valid') {
                    Add-Failure "Git origin URL must use a canonical github.com HTTPS, ssh://git@github.com, git@github.com SCP, or github.com SCP form.$destinationContext"
                }
                elseif (-not [string]::Equals($originLocation.Host, 'github.com', [System.StringComparison]::OrdinalIgnoreCase)) {
                    Add-Failure "Git origin host must be github.com.$destinationContext"
                }
                elseif (-not [string]::Equals($originLocation.Repository, $expectedRepositoryIdentity, [System.StringComparison]::OrdinalIgnoreCase)) {
                    Add-Failure "Git origin repository identity does not match Repository.$destinationContext"
                }
            }
        }
    }

    if ($rootIsUsable) {
        $gitIgnorePath = Join-Path $resolvedRoot '.gitignore'
        if (-not (Test-Path -LiteralPath $gitIgnorePath -PathType Leaf)) {
            Add-Failure '.gitignore is missing.'
        }

        $requiredLfsPatterns = @(
            '*.uasset',
            '*.umap',
            '*.fbx',
            '*.blend',
            '*.psd',
            '*.tga',
            '*.exr',
            '*.hdr',
            '*.wav',
            '*.flac',
            '*.mp4',
            '*.mov'
        )
        $requiredLfsExtensions = @($requiredLfsPatterns | ForEach-Object { $_.Substring(1).ToLowerInvariant() })
        $gitAttributesPath = Join-Path $resolvedRoot '.gitattributes'
        if (-not (Test-Path -LiteralPath $gitAttributesPath -PathType Leaf)) {
            Add-Failure '.gitattributes is missing.'
        }

        $ignoreProbes = @(
            '.superpowers/.repository-baseline-probe',
            'Plugins/Test/.superpowers/RepositoryBaseline.tmp',
            'Binaries/.repository-baseline-probe',
            'Plugins/Test/Binaries/RepositoryBaseline.dll',
            'DerivedDataCache/.repository-baseline-probe',
            'Plugins/Test/DerivedDataCache/RepositoryBaseline.tmp',
            'Intermediate/.repository-baseline-probe',
            'Source/Test/Intermediate/RepositoryBaseline.obj',
            'Saved/.repository-baseline-probe',
            'Plugins/Test/Saved/RepositoryBaseline.sav',
            '.vs/.repository-baseline-probe',
            'Source/Test/.vs/RepositoryBaseline.tmp',
            'Artifacts/.repository-baseline-probe',
            'Plugins/Test/Artifacts/RepositoryBaseline.tmp',
            '.idea/RepositoryBaseline.xml',
            'Plugins/Test/.idea/RepositoryBaseline.xml',
            'RepositoryBaseline.sln',
            'Plugins/Test/RepositoryBaseline.sln',
            'RepositoryBaseline.suo',
            'Plugins/Test/RepositoryBaseline.suo',
            'RepositoryBaseline.opensdf',
            'Plugins/Test/RepositoryBaseline.opensdf',
            'RepositoryBaseline.sdf',
            'Plugins/Test/RepositoryBaseline.sdf',
            'RepositoryBaseline.VC.db',
            'Source/Test/RepositoryBaseline.VC.db',
            'RepositoryBaseline.VC.opendb',
            'Source/Test/RepositoryBaseline.VC.opendb'
        )

        $versionedRulePaths = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
        $null = $versionedRulePaths.Add('.gitignore')
        $null = $versionedRulePaths.Add('.gitattributes')
        $repositoryAssetPaths = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)

        try {
            $rootPrefix = $resolvedRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
            $assetFiles = Get-ChildItem -LiteralPath $resolvedRoot -Recurse -Force -File -ErrorAction Stop
            foreach ($assetFile in $assetFiles) {
                $extension = $assetFile.Extension.ToLowerInvariant()
                if ($requiredLfsExtensions -contains $extension) {
                    $relativePath = $assetFile.FullName.Substring($rootPrefix.Length).Replace('\', '/')
                    $null = $repositoryAssetPaths.Add($relativePath)
                }
            }
        }
        catch {
            Add-Failure 'Repository asset files could not be enumerated for LFS validation.'
        }

        if ($gitRepositoryReady) {
            $trackedFilesResult = Invoke-NativeCommandSafely -Executable $gitPath -Arguments @(
                '-c', 'core.quotePath=false', '-C', $resolvedRoot, 'ls-files'
            )
            if (-not $trackedFilesResult.Started -or $trackedFilesResult.ExitCode -ne 0) {
                Add-Failure 'Git command failed while enumerating tracked files for repository rule and LFS validation.'
            }
            else {
                foreach ($trackedFileOutput in $trackedFilesResult.Output) {
                    $trackedPath = $trackedFileOutput.ToString()
                    if ([string]::IsNullOrWhiteSpace($trackedPath)) {
                        continue
                    }

                    $normalizedTrackedPath = $trackedPath.Replace('\', '/')
                    if ($normalizedTrackedPath -cmatch '(^|/)\.(gitignore|gitattributes)$') {
                        $null = $versionedRulePaths.Add($normalizedTrackedPath)
                    }

                    $trackedExtension = [System.IO.Path]::GetExtension($normalizedTrackedPath).ToLowerInvariant()
                    if ($requiredLfsExtensions -contains $trackedExtension) {
                        $null = $repositoryAssetPaths.Add($normalizedTrackedPath)
                    }
                }
            }
        }

        $versionedIgnorePaths = @(
            $versionedRulePaths |
                Where-Object { $_ -ceq '.gitignore' -or $_ -clike '*/.gitignore' } |
                Sort-Object
        )
        foreach ($relativeIgnorePath in $versionedIgnorePaths) {
            $versionedIgnorePath = Join-Path $resolvedRoot $relativeIgnorePath
            if (-not (Test-Path -LiteralPath $versionedIgnorePath -PathType Leaf)) {
                if ($relativeIgnorePath -cne '.gitignore') {
                    Add-Failure "Tracked versioned .gitignore is missing from the worktree: $relativeIgnorePath"
                }
                continue
            }

            try {
                $ignoreLines = [System.IO.File]::ReadAllLines($versionedIgnorePath)
                for ($lineIndex = 0; $lineIndex -lt $ignoreLines.Count; $lineIndex++) {
                    $ignoreLine = $ignoreLines[$lineIndex].TrimEnd()
                    if ($ignoreLine.StartsWith('!')) {
                        $isAllowedRootExample = $relativeIgnorePath -ceq '.gitignore' -and $ignoreLine -ceq '!.env.example'
                        if (-not $isAllowedRootExample) {
                            Add-Failure "Unsafe versioned .gitignore negation at $relativeIgnorePath line $($lineIndex + 1); only root !.env.example is allowed."
                        }
                    }
                }
            }
            catch {
                Add-Failure "Versioned .gitignore could not be read: $relativeIgnorePath"
            }
        }

        $nestedAttributePaths = @(
            $versionedRulePaths |
                Where-Object { $_ -clike '*/.gitattributes' } |
                Sort-Object
        )
        foreach ($relativeAttributePath in $nestedAttributePaths) {
            $versionedAttributePath = Join-Path $resolvedRoot $relativeAttributePath
            if (-not (Test-Path -LiteralPath $versionedAttributePath -PathType Leaf)) {
                Add-Failure "Tracked nested .gitattributes is missing from the worktree: $relativeAttributePath"
                continue
            }

            try {
                $attributeLines = [System.IO.File]::ReadAllLines($versionedAttributePath)
                for ($lineIndex = 0; $lineIndex -lt $attributeLines.Count; $lineIndex++) {
                    $attributeLine = $attributeLines[$lineIndex].Trim()
                    if ([string]::IsNullOrWhiteSpace($attributeLine) -or $attributeLine.StartsWith('#')) {
                        continue
                    }

                    $attributeRule = Get-GitAttributeRuleParts -Line $attributeLine
                    $attributeTokens = @($attributeRule.Attributes)
                    if ($attributeTokens.Count -eq 0) {
                        continue
                    }

                    $attributePattern = $attributeRule.Pattern
                    $finalPatternSegment = $attributePattern
                    $lastPatternSeparator = $finalPatternSegment.LastIndexOf('/')
                    if ($lastPatternSeparator -ge 0) {
                        $finalPatternSegment = $finalPatternSegment.Substring($lastPatternSeparator + 1)
                    }

                    if ($attributeRule.Parsed) {
                        $extensionSeparator = $finalPatternSegment.LastIndexOf('.')
                        if ($extensionSeparator -ge 0 -and $extensionSeparator -lt ($finalPatternSegment.Length - 1)) {
                            $patternExtension = $finalPatternSegment.Substring($extensionSeparator + 1)
                            $extensionHasPatternSyntax = $false
                            foreach ($patternCharacter in @('*', '?', '[', ']')) {
                                if ($patternExtension.Contains($patternCharacter)) {
                                    $extensionHasPatternSyntax = $true
                                    break
                                }
                            }

                            $normalizedPatternExtension = ".$($patternExtension.ToLowerInvariant())"
                            if (-not $extensionHasPatternSyntax -and $requiredLfsExtensions -cnotcontains $normalizedPatternExtension) {
                                continue
                            }
                        }
                        elseif (-not [string]::IsNullOrWhiteSpace($attributePattern) -and
                            -not $attributePattern.EndsWith('/', [System.StringComparison]::Ordinal) -and
                            $attributePattern.IndexOfAny([char[]]@('*', '?', '[', ']')) -lt 0) {
                            continue
                        }
                    }

                    $hasDestructiveToken = $false
                    foreach ($attributeToken in $attributeTokens) {
                        foreach ($attributeName in @('filter', 'diff', 'merge')) {
                            if ($attributeToken -ceq $attributeName -or
                                $attributeToken -ceq "-$attributeName" -or
                                $attributeToken -ceq "!$attributeName") {
                                $hasDestructiveToken = $true
                                break
                            }

                            $valuePrefix = "$attributeName="
                            if ($attributeToken.StartsWith($valuePrefix, [System.StringComparison]::Ordinal) -and
                                $attributeToken.Substring($valuePrefix.Length) -cne 'lfs') {
                                $hasDestructiveToken = $true
                                break
                            }
                        }

                        if ($hasDestructiveToken) {
                            break
                        }

                        if ($attributeToken -ceq 'text' -or
                            $attributeToken -ceq '!text' -or
                            $attributeToken.StartsWith('text=', [System.StringComparison]::Ordinal)) {
                            $hasDestructiveToken = $true
                            break
                        }
                    }

                    if ($hasDestructiveToken) {
                        Add-Failure "Unsafe nested .gitattributes rule at $relativeAttributePath line $($lineIndex + 1); nested rules may not cancel or rewrite filter, diff, merge, or text LFS safety attributes."
                    }
                }
            }
            catch {
                Add-Failure "Nested versioned .gitattributes could not be read: $relativeAttributePath"
            }
        }

        if ($null -ne $gitPath) {
            $versionedRuleArguments = @{
                SourceRoot = $resolvedRoot
                GitPath = $gitPath
                IgnoreProbes = $ignoreProbes
                RequiredLfsPatterns = $requiredLfsPatterns
                VersionedRulePaths = @($versionedRulePaths | Sort-Object)
                RepositoryAssetPaths = @($repositoryAssetPaths | Sort-Object)
            }
            Test-VersionedRepositoryRules @versionedRuleArguments
        }

        if ($gitRepositoryReady) {
            foreach ($probe in $ignoreProbes) {
                $ignoreResult = Invoke-NativeCommandSafely -Executable $gitPath -Arguments @(
                    '-C', $resolvedRoot, 'check-ignore', '--no-index', '--quiet', '--', $probe
                )
                if (-not $ignoreResult.Started -or ($ignoreResult.ExitCode -ne 0 -and $ignoreResult.ExitCode -ne 1)) {
                    Add-Failure "Git check-ignore command failed for probe: $probe"
                }
                elseif ($ignoreResult.ExitCode -eq 1) {
                    Add-Failure "Effective ignore behavior is missing for probe: $probe"
                }
            }

            $attributeProbeToPattern = @{}
            foreach ($pattern in $requiredLfsPatterns) {
                foreach ($prefix in @('', 'Content/', 'Plugins/UrbanFoundation/Content/')) {
                    $probe = "${prefix}RepositoryBaseline$($pattern.Substring(1))"
                    $attributeProbeToPattern[$probe] = $pattern
                }
            }

            $attributeTargets = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
            foreach ($probe in $attributeProbeToPattern.Keys) {
                $null = $attributeTargets.Add($probe)
            }
            foreach ($assetPath in $repositoryAssetPaths) {
                $null = $attributeTargets.Add($assetPath)
            }

            $orderedAttributeTargets = @($attributeTargets | Sort-Object)
            $attributeBatchSize = 50
            for ($batchStart = 0; $batchStart -lt $orderedAttributeTargets.Count; $batchStart += $attributeBatchSize) {
                $batchEnd = [Math]::Min($batchStart + $attributeBatchSize - 1, $orderedAttributeTargets.Count - 1)
                $attributeBatch = @($orderedAttributeTargets[$batchStart..$batchEnd])
                $checkAttributeArguments = @(
                    '-c', 'core.quotePath=false', '-C', $resolvedRoot,
                    'check-attr', 'filter', 'diff', 'merge', 'text', '--'
                ) + $attributeBatch
                $checkAttributeResult = Invoke-NativeCommandSafely -Executable $gitPath -Arguments $checkAttributeArguments
                if (-not $checkAttributeResult.Started -or $checkAttributeResult.ExitCode -ne 0) {
                    Add-Failure 'Git check-attr command failed while validating effective LFS attributes.'
                    continue
                }

                $effectiveAttributeLines = @($checkAttributeResult.Output | ForEach-Object { $_.ToString() })
                foreach ($targetPath in $attributeBatch) {
                    $expectedAttributeLines = @(
                        "${targetPath}: filter: lfs",
                        "${targetPath}: diff: lfs",
                        "${targetPath}: merge: lfs",
                        "${targetPath}: text: unset"
                    )
                    $attributesAreValid = $true
                    foreach ($expectedLine in $expectedAttributeLines) {
                        if ($effectiveAttributeLines -cnotcontains $expectedLine) {
                            $attributesAreValid = $false
                        }
                    }

                    if (-not $attributesAreValid) {
                        if ($attributeProbeToPattern.ContainsKey($targetPath)) {
                            Add-Failure "Effective LFS attributes are invalid for pattern: $($attributeProbeToPattern[$targetPath]) (probe: $targetPath)"
                        }
                        else {
                            Add-Failure "Effective LFS attributes are invalid for repository asset path: $targetPath"
                        }
                    }
                }
            }
        }

        $registryPath = Join-Path $resolvedRoot 'ThirdParty/AssetRegistry.csv'
        $expectedColumns = @(
            'AssetId',
            'AssetName',
            'Provider',
            'SourceUrl',
            'License',
            'RepositoryPolicy',
            'ReleasePolicy',
            'Owner'
        )
        $expectedHeader = $expectedColumns -join ','
        if (-not (Test-Path -LiteralPath $registryPath -PathType Leaf)) {
            Add-Failure 'ThirdParty/AssetRegistry.csv is missing.'
        }
        else {
            $registryCanBeParsed = $true
            try {
                $registryLines = [System.IO.File]::ReadAllLines($registryPath)
                if ($registryLines.Count -eq 0 -or $registryLines[0] -ne $expectedHeader) {
                    Add-Failure "ThirdParty/AssetRegistry.csv must have exactly these 8 columns: $expectedHeader"
                    $registryCanBeParsed = $false
                }
            }
            catch {
                Add-Failure 'ThirdParty/AssetRegistry.csv could not be read.'
                $registryCanBeParsed = $false
            }

            if ($registryCanBeParsed) {
                [void](Test-CsvExactColumnStructure -Path $registryPath -ExpectedColumnCount $expectedColumns.Count)
                try {
                    $records = @(Import-Csv -LiteralPath $registryPath -ErrorAction Stop)
                }
                catch {
                    Add-Failure 'ThirdParty/AssetRegistry.csv could not be parsed as CSV.'
                    $records = @()
                    $registryCanBeParsed = $false
                }

                if ($registryCanBeParsed) {
                    if ($records.Count -eq 0) {
                        Add-Failure 'ThirdParty/AssetRegistry.csv must contain at least one record.'
                    }
                    else {
                        $seenAssetIds = @{}
                        $requiredRecordFields = @(
                            'AssetName',
                            'Provider',
                            'SourceUrl',
                            'License',
                            'RepositoryPolicy',
                            'ReleasePolicy',
                            'Owner'
                        )
                        for ($recordIndex = 0; $recordIndex -lt $records.Count; $recordIndex++) {
                            $record = $records[$recordIndex]
                            $rowNumber = $recordIndex + 2
                            $propertyNames = @($record.PSObject.Properties | ForEach-Object { $_.Name })
                            $hasExactColumns = $propertyNames.Count -eq $expectedColumns.Count
                            if ($hasExactColumns) {
                                for ($columnIndex = 0; $columnIndex -lt $expectedColumns.Count; $columnIndex++) {
                                    if ($propertyNames[$columnIndex] -ne $expectedColumns[$columnIndex]) {
                                        $hasExactColumns = $false
                                    }
                                }
                            }
                            if (-not $hasExactColumns) {
                                Add-Failure "ThirdParty/AssetRegistry.csv row $rowNumber does not have the exact 8-column structure."
                                continue
                            }

                            $assetId = [string]$record.AssetId
                            if ([string]::IsNullOrWhiteSpace($assetId)) {
                                Add-Failure "ThirdParty/AssetRegistry.csv row $rowNumber required field AssetId is empty."
                            }
                            else {
                                $assetIdKey = $assetId.Trim().ToLowerInvariant()
                                if ($seenAssetIds.ContainsKey($assetIdKey)) {
                                    Add-Failure "ThirdParty/AssetRegistry.csv AssetId values must be unique; duplicate at row $rowNumber."
                                }
                                else {
                                    $seenAssetIds[$assetIdKey] = $true
                                }
                            }

                            foreach ($field in $requiredRecordFields) {
                                if ([string]::IsNullOrWhiteSpace([string]$record.$field)) {
                                    Add-Failure "ThirdParty/AssetRegistry.csv row $rowNumber required field $field is empty."
                                }
                            }

                            if (-not [string]::IsNullOrWhiteSpace([string]$record.SourceUrl)) {
                                $sourceUri = $null
                                $isAbsoluteUrl = [System.Uri]::TryCreate(
                                    [string]$record.SourceUrl,
                                    [System.UriKind]::Absolute,
                                    [ref]$sourceUri
                                )
                                if (-not $isAbsoluteUrl -or ($sourceUri.Scheme -ne 'http' -and $sourceUri.Scheme -ne 'https')) {
                                    Add-Failure "ThirdParty/AssetRegistry.csv row $rowNumber SourceUrl must be an absolute http/https URL."
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
catch {
    Add-Failure 'Repository baseline validation encountered an unexpected internal error.'
}

if ($failures.Count -gt 0) {
    Write-Output 'FAIL: repository baseline validation failed:'
    foreach ($failure in $failures) {
        Write-Output "- $failure"
    }
    exit 1
}

Write-Output 'PASS: repository privacy, ignore rules, LFS, and asset registry are valid.'
