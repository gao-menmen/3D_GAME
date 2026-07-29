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

function Get-RemoteRepositoryIdentity {
    param([string]$RemoteUrl)

    if ([string]::IsNullOrWhiteSpace($RemoteUrl)) {
        return $null
    }

    $candidate = $RemoteUrl.Trim()
    $repositoryPath = $null
    $uri = $null

    if ([System.Uri]::TryCreate($candidate, [System.UriKind]::Absolute, [ref]$uri) -and
        ($uri.Scheme -eq 'https' -or $uri.Scheme -eq 'ssh')) {
        $repositoryPath = $uri.AbsolutePath
    }
    elseif ($candidate -match '^[^@\s]+@[^:\s]+:(?<RepositoryPath>.+)$') {
        $repositoryPath = $Matches.RepositoryPath
    }
    else {
        return $null
    }

    $repositoryPath = $repositoryPath.Trim().Trim('/').Replace('\', '/')
    $repositoryPath = [System.Text.RegularExpressions.Regex]::Replace(
        $repositoryPath,
        '\.git$',
        '',
        [System.Text.RegularExpressions.RegexOptions]::IgnoreCase
    )
    $parts = @($repositoryPath -split '/')
    if ($parts.Count -ne 2 -or [string]::IsNullOrWhiteSpace($parts[0]) -or [string]::IsNullOrWhiteSpace($parts[1])) {
        return $null
    }

    return "$($parts[0])/$($parts[1])"
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
            'repo', 'view', $expectedRepositoryIdentity, '--json', 'visibility', '--jq', '.visibility'
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
        $originResult = Invoke-NativeCommandSafely -Executable $gitPath -Arguments @(
            '-C', $resolvedRoot, 'remote', 'get-url', 'origin'
        )
        if (-not $originResult.Started -or $originResult.ExitCode -ne 0) {
            Add-Failure 'Git command failed while reading the origin repository identity.'
        }
        else {
            $originUrl = Convert-CommandOutputToText -Output $originResult.Output
            $originIdentity = Get-RemoteRepositoryIdentity -RemoteUrl $originUrl
            if ($null -eq $originIdentity) {
                Add-Failure 'Git origin URL is not a supported HTTPS or SSH repository URL.'
            }
            elseif (-not [string]::Equals($originIdentity, $expectedRepositoryIdentity, [System.StringComparison]::OrdinalIgnoreCase)) {
                Add-Failure 'Git origin repository identity does not match Repository.'
            }
        }
    }

    if ($rootIsUsable) {
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
        $gitIgnorePath = Join-Path $resolvedRoot '.gitignore'
        if (-not (Test-Path -LiteralPath $gitIgnorePath -PathType Leaf)) {
            Add-Failure '.gitignore is missing.'
        }
        else {
            try {
                $ignoreLines = [System.IO.File]::ReadAllLines($gitIgnorePath)
                foreach ($rule in $requiredIgnoreRules) {
                    if ($ignoreLines -notcontains $rule) {
                        Add-Failure ".gitignore is missing required rule: $rule"
                    }
                }
            }
            catch {
                Add-Failure '.gitignore could not be read.'
            }
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
        $gitAttributesPath = Join-Path $resolvedRoot '.gitattributes'
        if (-not (Test-Path -LiteralPath $gitAttributesPath -PathType Leaf)) {
            Add-Failure '.gitattributes is missing.'
        }
        else {
            try {
                $attributeLines = [System.IO.File]::ReadAllLines($gitAttributesPath)
                foreach ($pattern in $requiredLfsPatterns) {
                    $expectedRule = "$pattern filter=lfs diff=lfs merge=lfs -text"
                    if ($attributeLines -notcontains $expectedRule) {
                        Add-Failure ".gitattributes is missing required LFS rule: $expectedRule"
                    }
                }
            }
            catch {
                Add-Failure '.gitattributes could not be read.'
            }
        }

        if ($gitRepositoryReady) {
            $ignoreProbes = @(
                '.superpowers/.repository-baseline-probe',
                'Binaries/.repository-baseline-probe',
                'DerivedDataCache/.repository-baseline-probe',
                'Intermediate/.repository-baseline-probe',
                'Saved/.repository-baseline-probe',
                '.vs/.repository-baseline-probe',
                'Artifacts/.repository-baseline-probe',
                'RepositoryBaseline.sln',
                'RepositoryBaseline.VC.db'
            )
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

            $attributeProbes = @()
            foreach ($pattern in $requiredLfsPatterns) {
                $attributeProbes += "RepositoryBaseline$($pattern.Substring(1))"
            }
            $checkAttributeArguments = @('-C', $resolvedRoot, 'check-attr', 'filter', 'diff', 'merge', 'text', '--') + $attributeProbes
            $checkAttributeResult = Invoke-NativeCommandSafely -Executable $gitPath -Arguments $checkAttributeArguments
            if (-not $checkAttributeResult.Started -or $checkAttributeResult.ExitCode -ne 0) {
                Add-Failure 'Git check-attr command failed while validating effective LFS attributes.'
            }
            else {
                $effectiveAttributeLines = @($checkAttributeResult.Output | ForEach-Object { $_.ToString().Trim() })
                for ($index = 0; $index -lt $requiredLfsPatterns.Count; $index++) {
                    $pattern = $requiredLfsPatterns[$index]
                    $probe = $attributeProbes[$index]
                    $expectedAttributeLines = @(
                        "${probe}: filter: lfs",
                        "${probe}: diff: lfs",
                        "${probe}: merge: lfs",
                        "${probe}: text: unset"
                    )
                    $attributesAreValid = $true
                    foreach ($expectedLine in $expectedAttributeLines) {
                        if ($effectiveAttributeLines -notcontains $expectedLine) {
                            $attributesAreValid = $false
                        }
                    }
                    if (-not $attributesAreValid) {
                        Add-Failure "Effective LFS attributes are invalid for pattern: $pattern"
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
