[CmdletBinding()]
param(
    [string]$Root,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}

$Root = [System.IO.Path]::GetFullPath($Root)
$manifestPath = [System.IO.Path]::GetFullPath((Join-Path $Root 'Build\Manifests\FoundationModules.json'))
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Foundation module manifest not found: $manifestPath"
}

try {
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
}
catch {
    throw "Foundation module manifest is invalid JSON: $($_.Exception.Message)"
}

function Assert-ValidIdentifier {
    param(
        [AllowNull()][object]$Value,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $name = [string]$Value
    if ([string]::IsNullOrWhiteSpace($name) -or $name -notmatch '^[A-Za-z_][A-Za-z0-9_]*$') {
        throw ('{0} must match ^[A-Za-z_][A-Za-z0-9_]*$: ''{1}''' -f $Label, $name)
    }
    return $name
}

function Get-NormalizedDirectoryPrefix {
    param([Parameter(Mandatory = $true)][string]$Path)

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $trimmed = $fullPath.TrimEnd([char[]]@(
        [System.IO.Path]::DirectorySeparatorChar,
        [System.IO.Path]::AltDirectorySeparatorChar
    ))
    return $trimmed + [System.IO.Path]::DirectorySeparatorChar
}

function Assert-PathInsideDirectory {
    param(
        [Parameter(Mandatory = $true)][string]$Candidate,
        [Parameter(Mandatory = $true)][string]$Directory,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $candidateFullPath = [System.IO.Path]::GetFullPath($Candidate)
    $directoryPrefix = Get-NormalizedDirectoryPrefix -Path $Directory
    if (-not $candidateFullPath.StartsWith($directoryPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label escapes its allowed directory: $candidateFullPath"
    }
    return $candidateFullPath
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
    if (-not [string]::Equals($pathFullPath, $boundaryFullPath, [System.StringComparison]::OrdinalIgnoreCase) -and
        -not $pathFullPath.StartsWith($boundaryPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label is outside its reparse-point validation boundary: $pathFullPath"
    }

    $pathsToCheck = [System.Collections.Generic.List[string]]::new()
    $pathsToCheck.Add($boundaryFullPath)
    if (-not [string]::Equals($pathFullPath, $boundaryFullPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        $relativePath = $pathFullPath.Substring($boundaryPrefix.Length)
        $currentPath = $boundaryFullPath
        foreach ($segment in $relativePath.Split([char[]]@(
            [System.IO.Path]::DirectorySeparatorChar,
            [System.IO.Path]::AltDirectorySeparatorChar
        ), [System.StringSplitOptions]::RemoveEmptyEntries)) {
            $currentPath = Join-Path $currentPath $segment
            $pathsToCheck.Add($currentPath)
        }
    }

    foreach ($candidate in $pathsToCheck) {
        if (-not (Test-Path -LiteralPath $candidate)) {
            break
        }
        $item = Get-Item -LiteralPath $candidate -Force
        if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "$Label contains a reparse point and generation was stopped: $candidate"
        }
        if ($item -isnot [System.IO.DirectoryInfo]) {
            throw "$Label contains a non-directory path ancestor: $candidate"
        }
    }
}

function Normalize-GeneratedContent {
    param([Parameter(Mandatory = $true)][string]$Content)
    return $Content.Replace("`r`n", "`n").Replace("`r", "`n")
}

function Test-ByteArraysEqual {
    param(
        [Parameter(Mandatory = $true)][byte[]]$Left,
        [Parameter(Mandatory = $true)][byte[]]$Right
    )

    if ($Left.Length -ne $Right.Length) { return $false }
    for ($index = 0; $index -lt $Left.Length; $index++) {
        if ($Left[$index] -ne $Right[$index]) { return $false }
    }
    return $true
}
$pluginName = Assert-ValidIdentifier -Value $manifest.Plugin -Label 'Plugin name'
$runtimeModules = @($manifest.RuntimeModules | ForEach-Object {
    Assert-ValidIdentifier -Value $_ -Label 'Runtime module name'
})
$testModule = Assert-ValidIdentifier -Value $manifest.TestModule -Label 'Test module name'

if ($runtimeModules.Count -ne 7) {
    throw "Foundation manifest must contain exactly seven runtime modules; found $($runtimeModules.Count)."
}

$uniqueNames = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($name in $runtimeModules) {
    if (-not $uniqueNames.Add($name)) {
        throw "Duplicate runtime module name: $name"
    }
}
if (-not $uniqueNames.Add($testModule)) {
    throw "Test module must not duplicate a runtime module: $testModule"
}

$pluginsRoot = [System.IO.Path]::GetFullPath((Join-Path $Root 'Plugins'))
$pluginRoot = Assert-PathInsideDirectory -Candidate (Join-Path $pluginsRoot $pluginName) -Directory $pluginsRoot -Label 'Plugin root'
$sourceRoot = Assert-PathInsideDirectory -Candidate (Join-Path $pluginRoot 'Source') -Directory $pluginRoot -Label 'Plugin source root'
Assert-NoReparsePointInExistingPath -Path $pluginRoot -Boundary $Root -Label 'Plugin root'

$expectedModuleNames = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($name in @($runtimeModules) + $testModule) {
    [void]$expectedModuleNames.Add($name)
}

if (Test-Path -LiteralPath $sourceRoot -PathType Container) {
    $unexpectedDirectories = @(Get-ChildItem -LiteralPath $sourceRoot -Directory -Force | Where-Object {
        -not $expectedModuleNames.Contains($_.Name)
    } | ForEach-Object { $_.FullName })
    if ($unexpectedDirectories.Count) {
        throw "Unexpected module directories must be resolved before generation:`n- $($unexpectedDirectories -join "`n- ")"
    }
}

$generatedFiles = [System.Collections.Generic.List[object]]::new()
$generatedPaths = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)

function Add-GeneratedFile {
    param(
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string]$Content
    )

    $candidate = Assert-PathInsideDirectory -Candidate (Join-Path $script:pluginRoot $RelativePath) -Directory $script:pluginRoot -Label "Generated file '$RelativePath'"
    if (-not $script:generatedPaths.Add($candidate)) {
        throw "Generated output path is duplicated: $RelativePath"
    }
    $script:generatedFiles.Add([pscustomobject]@{
        RelativePath = $RelativePath
        FullPath = $candidate
        Content = Normalize-GeneratedContent -Content $Content
    })
}

$modules = @()
foreach ($name in $runtimeModules) {
    $modules += [ordered]@{
        Name = $name
        Type = 'Runtime'
        LoadingPhase = 'Default'
    }
}
$modules += [ordered]@{
    Name = $testModule
    Type = 'DeveloperTool'
    LoadingPhase = 'PostEngineInit'
}

$descriptor = [ordered]@{
    FileVersion = 3
    Version = 1
    VersionName = '0.1.0'
    FriendlyName = 'Urban Spear Foundation'
    Description = 'Shared runtime boundaries and tests.'
    Category = 'Urban Spear'
    EnabledByDefault = $true
    CanContainContent = $false
    Modules = $modules
} | ConvertTo-Json -Depth 20
Add-GeneratedFile -RelativePath "$pluginName.uplugin" -Content $descriptor

foreach ($name in $runtimeModules) {
    $buildRules = @"
using UnrealBuildTool;

public class $name : ModuleRules
{
    public $name(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core" });
    }
}
"@
    Add-GeneratedFile -RelativePath "Source\$name\$name.Build.cs" -Content $buildRules

    $moduleSource = @"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, $name)
"@
    Add-GeneratedFile -RelativePath "Source\$name\Private\${name}Module.cpp" -Content $moduleSource
}

$runtimeDependencyLines = @($runtimeModules | ForEach-Object { '            "' + $_ + '",' }) -join "`r`n"
$testBuildRules = @"
using UnrealBuildTool;

public class $testModule : ModuleRules
{
    public $testModule(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
$runtimeDependencyLines
        });
    }
}
"@
Add-GeneratedFile -RelativePath "Source\$testModule\$testModule.Build.cs" -Content $testBuildRules

$testModuleSource = @"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, $testModule)
"@
Add-GeneratedFile -RelativePath "Source\$testModule\Private\${testModule}Module.cpp" -Content $testModuleSource

$runtimeTestLines = @($runtimeModules | ForEach-Object { '        TEXT("' + $_ + '"),' }) -join "`r`n"
$automationTests = @"
#include "CoreMinimal.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanModuleLoadTest,
    "UrbanSpear.Foundation.ModuleLoad",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanModuleLoadTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const TCHAR* Names[] =
    {
$runtimeTestLines
    };

    for (const TCHAR* Name : Names)
    {
        TestTrue(FString::Printf(TEXT("%s registered"), Name), FModuleManager::Get().ModuleExists(Name));
        TestTrue(
            FString::Printf(TEXT("%s loads"), Name),
            FModuleManager::Get().LoadModulePtr<IModuleInterface>(Name) != nullptr);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanProjectIdentityTest,
    "UrbanSpear.Foundation.ProjectIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanProjectIdentityTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    TestEqual(TEXT("Project name"), FString(FApp::GetProjectName()), FString(TEXT("UrbanSpear")));
    TestTrue(
        TEXT("Lyra front-end map exists"),
        FPackageName::DoesPackageExist(TEXT("/Game/System/FrontEnd/Maps/L_LyraFrontEnd")));

    return true;
}

#endif
"@
Add-GeneratedFile -RelativePath "Source\$testModule\Private\UrbanFoundationAutomationTests.cpp" -Content $automationTests

$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$conflicts = [System.Collections.Generic.List[string]]::new()
foreach ($file in $generatedFiles) {
    $parent = Split-Path -Parent $file.FullPath
    Assert-NoReparsePointInExistingPath -Path $parent -Boundary $Root -Label "Generated file '$($file.RelativePath)'"

    if (-not (Test-Path -LiteralPath $file.FullPath)) {
        continue
    }
    if (-not (Test-Path -LiteralPath $file.FullPath -PathType Leaf)) {
        $conflicts.Add("$($file.RelativePath) exists but is not a file")
        continue
    }

    $item = Get-Item -LiteralPath $file.FullPath -Force
    if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        $conflicts.Add("$($file.RelativePath) is a reparse point")
        continue
    }
    $linkTypeProperty = $item.PSObject.Properties['LinkType']
    if ($null -ne $linkTypeProperty -and [string]$item.LinkType -eq 'HardLink') {
        $conflicts.Add("$($file.RelativePath) is a hard link")
        continue
    }

    $existingBytes = [System.IO.File]::ReadAllBytes($file.FullPath)
    $expectedBytes = $utf8NoBom.GetBytes($file.Content)
    if (-not (Test-ByteArraysEqual -Left $existingBytes -Right $expectedBytes) -and -not $Force) {
        $conflicts.Add("$($file.RelativePath) differs from the generated UTF-8 template")
    }
}

if ($conflicts.Count) {
    throw "Generation stopped without changing files. Resolve conflicts or rerun with -Force:`n- $($conflicts -join "`n- ")"
}

$created = 0
$updated = 0
$unchanged = 0
foreach ($file in $generatedFiles) {
    $expectedBytes = $utf8NoBom.GetBytes($file.Content)
    $exists = Test-Path -LiteralPath $file.FullPath -PathType Leaf
    if ($exists) {
        $existingBytes = [System.IO.File]::ReadAllBytes($file.FullPath)
        if (Test-ByteArraysEqual -Left $existingBytes -Right $expectedBytes) {
            $unchanged++
            continue
        }
    }

    $parent = Split-Path -Parent $file.FullPath
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    [System.IO.File]::WriteAllBytes($file.FullPath, $expectedBytes)
    if ($exists) { $updated++ } else { $created++ }
}

Write-Output "UrbanFoundation scaffold synchronized: created=$created updated=$updated unchanged=$unchanged."
