[CmdletBinding()]
param([string]$Root)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}

$Root = [System.IO.Path]::GetFullPath($Root)
$failures = [System.Collections.Generic.List[string]]::new()
$expectedPlugin = 'UrbanFoundation'
$expectedRuntimeModules = @(
    'UrbanCore',
    'UrbanCombat',
    'UrbanAI',
    'UrbanMission',
    'UrbanModes',
    'UrbanUI',
    'UrbanOnline'
)
$expectedTestModule = 'UrbanFoundationTests'
$expectedModules = @($expectedRuntimeModules) + $expectedTestModule

function Add-Failure {
    param([Parameter(Mandatory = $true)][string]$Message)
    $script:failures.Add($Message)
}

function Test-ItemIsHardLink {
    param([Parameter(Mandatory = $true)][System.IO.FileSystemInfo]$Item)
    $linkTypeProperty = $Item.PSObject.Properties['LinkType']
    return ($null -ne $linkTypeProperty -and [string]$Item.LinkType -eq 'HardLink')
}

function Test-NoReparsePointInExistingPath {
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
        Add-Failure "$Label is outside its validation boundary: $pathFullPath"
        return $false
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
            Add-Failure "$Label contains a reparse point: $candidate"
            return $false
        }
        if ($item -isnot [System.IO.DirectoryInfo]) {
            Add-Failure "$Label contains a non-directory path ancestor: $candidate"
            return $false
        }
    }

    return $true
}
function Get-RequiredJson {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-NoReparsePointInExistingPath -Path (Split-Path -Parent $Path) -Boundary $script:Root -Label "JSON parent path '$Path'")) {
        return $null
    }
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        Add-Failure "Missing JSON file: $Path"
        return $null
    }

    $item = Get-Item -LiteralPath $Path -Force
    if ($item -isnot [System.IO.FileInfo] -or (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) -or (Test-ItemIsHardLink -Item $item)) {
        Add-Failure "JSON path is not a plain independent file: $Path"
        return $null
    }
    if ($item.Length -le 0) {
        Add-Failure "JSON file is empty: $Path"
        return $null
    }

    try {
        return [System.IO.File]::ReadAllText($item.FullName) | ConvertFrom-Json
    }
    catch {
        Add-Failure "Invalid JSON file: $Path ($($_.Exception.Message))"
        return $null
    }
}
function Get-RequiredPlainFileContent {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$RelativePath
    )

    if (-not (Test-NoReparsePointInExistingPath -Path (Split-Path -Parent $Path) -Boundary $script:Root -Label "Parent path for '$RelativePath'")) {
        return $null
    }
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        Add-Failure "Missing: $RelativePath"
        return $null
    }

    $item = Get-Item -LiteralPath $Path -Force
    if ($item -isnot [System.IO.FileInfo] -or (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) -or (Test-ItemIsHardLink -Item $item)) {
        Add-Failure "Not a plain independent file: $RelativePath"
        return $null
    }

    if ($item.Length -le 0) {
        Add-Failure "Empty file: $RelativePath"
        return $null
    }

    return [System.IO.File]::ReadAllText($item.FullName)
}

function Normalize-ExpectedContent {
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

function Test-ExpectedUtf8File {
    param(
        [Parameter(Mandatory = $true)][string]$PluginRoot,
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string]$ExpectedContent
    )

    $path = Join-Path $PluginRoot $RelativePath
    if (-not (Test-NoReparsePointInExistingPath -Path (Split-Path -Parent $path) -Boundary $script:Root -Label "Template parent path for '$RelativePath'")) {
        return
    }
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        return
    }

    $item = Get-Item -LiteralPath $path -Force
    if ($item -isnot [System.IO.FileInfo] -or (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) -or (Test-ItemIsHardLink -Item $item)) {
        return
    }

    $utf8NoBom = [System.Text.UTF8Encoding]::new($false)
    $expectedBytes = $utf8NoBom.GetBytes((Normalize-ExpectedContent -Content $ExpectedContent))
    $actualBytes = [System.IO.File]::ReadAllBytes($item.FullName)
    if (-not (Test-ByteArraysEqual -Left $actualBytes -Right $expectedBytes)) {
        Add-Failure "Generated file does not exactly match its UTF-8 template: $RelativePath"
    }
}
function Test-RegexRequirement {
    param(
        [AllowNull()][string]$Content,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$FailureMessage
    )

    if (-not [string]::IsNullOrEmpty($Content) -and $Content -notmatch $Pattern) {
        Add-Failure $FailureMessage
    }
}

$manifestPath = Join-Path $Root 'Build\Manifests\FoundationModules.json'
$manifest = Get-RequiredJson -Path $manifestPath

if ($null -eq $manifest) {
    Write-Output 'FAIL: UrbanFoundation scaffold validation failed:'
    foreach ($failure in $failures) { Write-Output "- $failure" }
    exit 1
}

$runtimeModules = @($manifest.RuntimeModules | ForEach-Object { [string]$_ })
$testModule = [string]$manifest.TestModule

if ([string]$manifest.Plugin -ne $expectedPlugin) {
    Add-Failure "Manifest Plugin must be $expectedPlugin."
}
if ($runtimeModules.Count -ne 7) {
    Add-Failure "Manifest must declare exactly seven runtime modules; found $($runtimeModules.Count)."
}
if (($runtimeModules | Select-Object -Unique).Count -ne $runtimeModules.Count) {
    Add-Failure 'Manifest runtime module names must be unique.'
}
if ($testModule -in $runtimeModules) {
    Add-Failure 'Manifest test module must not duplicate a runtime module.'
}
for ($index = 0; $index -lt $expectedRuntimeModules.Count; $index++) {
    if ($index -ge $runtimeModules.Count -or $runtimeModules[$index] -ne $expectedRuntimeModules[$index]) {
        Add-Failure "Manifest runtime module order must be: $($expectedRuntimeModules -join ', ')."
        break
    }
}
if ($testModule -ne $expectedTestModule) {
    Add-Failure "Manifest TestModule must be $expectedTestModule."
}

$pluginRoot = Join-Path $Root "Plugins\$expectedPlugin"
$descriptorRelative = "Plugins\$expectedPlugin\$expectedPlugin.uplugin"
$descriptorPath = Join-Path $pluginRoot "$expectedPlugin.uplugin"
$descriptor = Get-RequiredJson -Path $descriptorPath

if ($null -ne $descriptor) {
    $descriptorModules = @($descriptor.Modules)
    $descriptorNames = @($descriptorModules | ForEach-Object { [string]$_.Name })

    if ($descriptorModules.Count -ne 8) {
        Add-Failure "Descriptor must declare exactly eight modules; found $($descriptorModules.Count)."
    }
    if (($descriptorNames | Select-Object -Unique).Count -ne $descriptorNames.Count) {
        Add-Failure 'Descriptor module names must be unique.'
    }

    foreach ($unexpected in @($descriptorNames | Where-Object { $_ -notin $expectedModules })) {
        Add-Failure "Descriptor has unexpected module: $unexpected"
    }

    foreach ($name in $expectedRuntimeModules) {
        $matches = @($descriptorModules | Where-Object { [string]$_.Name -eq $name })
        if ($matches.Count -ne 1) {
            Add-Failure "Descriptor must contain runtime module exactly once: $name"
            continue
        }
        if ([string]$matches[0].Type -ne 'Runtime') {
            Add-Failure "Descriptor module $name must use Type=Runtime."
        }
        if ([string]$matches[0].LoadingPhase -ne 'Default') {
            Add-Failure "Descriptor module $name must use LoadingPhase=Default."
        }
    }

    $testMatches = @($descriptorModules | Where-Object { [string]$_.Name -eq $expectedTestModule })
    if ($testMatches.Count -ne 1) {
        Add-Failure "Descriptor must contain test module exactly once: $expectedTestModule"
    }
    else {
        if ([string]$testMatches[0].Type -ne 'DeveloperTool') {
            Add-Failure "Descriptor module $expectedTestModule must use Type=DeveloperTool."
        }
        if ([string]$testMatches[0].LoadingPhase -ne 'PostEngineInit') {
            Add-Failure "Descriptor module $expectedTestModule must use LoadingPhase=PostEngineInit."
        }
    }
}

$sourceRoot = Join-Path $pluginRoot 'Source'
$sourceRootIsSafe = Test-NoReparsePointInExistingPath -Path $sourceRoot -Boundary $Root -Label 'Plugin source root'
if (-not $sourceRootIsSafe) {
    # The path helper already recorded the failure; do not traverse the unsafe directory.
}
elseif (-not (Test-Path -LiteralPath $sourceRoot -PathType Container)) {
    Add-Failure 'Missing: Source'
}
else {
    $actualModuleDirectories = @(Get-ChildItem -LiteralPath $sourceRoot -Directory -Force | ForEach-Object { $_.Name })
    foreach ($unexpected in @($actualModuleDirectories | Where-Object { $_ -notin $expectedModules })) {
        Add-Failure "Unexpected module directory: Source\$unexpected"
    }
    foreach ($missing in @($expectedModules | Where-Object { $_ -notin $actualModuleDirectories })) {
        Add-Failure "Missing module directory: Source\$missing"
    }
}

foreach ($name in $expectedRuntimeModules) {
    $escapedName = [regex]::Escape($name)
    $buildRelative = "Source\$name\$name.Build.cs"
    $buildContent = Get-RequiredPlainFileContent -Path (Join-Path $pluginRoot $buildRelative) -RelativePath $buildRelative
    Test-RegexRequirement -Content $buildContent -Pattern "public\s+class\s+$escapedName\s*:\s*ModuleRules" -FailureMessage "Build rules class does not match module $name."
    Test-RegexRequirement -Content $buildContent -Pattern "public\s+$escapedName\s*\(\s*ReadOnlyTargetRules\s+Target\s*\)" -FailureMessage "Build rules constructor does not match module $name."
    Test-RegexRequirement -Content $buildContent -Pattern 'PublicDependencyModuleNames\s*\.\s*AddRange[\s\S]*"Core"' -FailureMessage "Build rules for $name must declare the Core public dependency."

    $moduleRelative = "Source\$name\Private\${name}Module.cpp"
    $moduleContent = Get-RequiredPlainFileContent -Path (Join-Path $pluginRoot $moduleRelative) -RelativePath $moduleRelative
    Test-RegexRequirement -Content $moduleContent -Pattern "IMPLEMENT_MODULE\s*\(\s*FDefaultModuleImpl\s*,\s*$escapedName\s*\)" -FailureMessage "IMPLEMENT_MODULE name does not match $name."
}

$testBuildRelative = "Source\$expectedTestModule\$expectedTestModule.Build.cs"
$testBuildContent = Get-RequiredPlainFileContent -Path (Join-Path $pluginRoot $testBuildRelative) -RelativePath $testBuildRelative
$escapedTestModule = [regex]::Escape($expectedTestModule)
Test-RegexRequirement -Content $testBuildContent -Pattern "public\s+class\s+$escapedTestModule\s*:\s*ModuleRules" -FailureMessage "Build rules class does not match module $expectedTestModule."
Test-RegexRequirement -Content $testBuildContent -Pattern "public\s+$escapedTestModule\s*\(\s*ReadOnlyTargetRules\s+Target\s*\)" -FailureMessage "Build rules constructor does not match module $expectedTestModule."
foreach ($name in $expectedRuntimeModules) {
    Test-RegexRequirement -Content $testBuildContent -Pattern ('"' + [regex]::Escape($name) + '"') -FailureMessage "Test build rules missing runtime dependency: $name"
}

$testModuleRelative = "Source\$expectedTestModule\Private\${expectedTestModule}Module.cpp"
$testModuleContent = Get-RequiredPlainFileContent -Path (Join-Path $pluginRoot $testModuleRelative) -RelativePath $testModuleRelative
Test-RegexRequirement -Content $testModuleContent -Pattern "IMPLEMENT_MODULE\s*\(\s*FDefaultModuleImpl\s*,\s*$escapedTestModule\s*\)" -FailureMessage "IMPLEMENT_MODULE name does not match $expectedTestModule."

$automationRelative = "Source\$expectedTestModule\Private\UrbanFoundationAutomationTests.cpp"
$automationContent = Get-RequiredPlainFileContent -Path (Join-Path $pluginRoot $automationRelative) -RelativePath $automationRelative
foreach ($testId in @('UrbanSpear.Foundation.ModuleLoad', 'UrbanSpear.Foundation.ProjectIdentity')) {
    Test-RegexRequirement -Content $automationContent -Pattern ([regex]::Escape('"' + $testId + '"')) -FailureMessage "Automation source missing test ID: $testId"
}
foreach ($name in $expectedRuntimeModules) {
    Test-RegexRequirement -Content $automationContent -Pattern ('TEXT\s*\(\s*"' + [regex]::Escape($name) + '"\s*\)') -FailureMessage "Automation source missing runtime module: $name"
}

$expectedDescriptorModules = @()
foreach ($name in $expectedRuntimeModules) {
    $expectedDescriptorModules += [ordered]@{
        Name = $name
        Type = 'Runtime'
        LoadingPhase = 'Default'
    }
}
$expectedDescriptorModules += [ordered]@{
    Name = $expectedTestModule
    Type = 'DeveloperTool'
    LoadingPhase = 'PostEngineInit'
}
$expectedDescriptorContent = [ordered]@{
    FileVersion = 3
    Version = 1
    VersionName = '0.1.0'
    FriendlyName = 'Urban Spear Foundation'
    Description = 'Shared runtime boundaries and tests.'
    Category = 'Urban Spear'
    EnabledByDefault = $true
    CanContainContent = $false
    Modules = $expectedDescriptorModules
} | ConvertTo-Json -Depth 20
Test-ExpectedUtf8File -PluginRoot $pluginRoot -RelativePath "$expectedPlugin.uplugin" -ExpectedContent $expectedDescriptorContent

foreach ($name in $expectedRuntimeModules) {
    $expectedBuildRules = @"
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
    Test-ExpectedUtf8File -PluginRoot $pluginRoot -RelativePath "Source\$name\$name.Build.cs" -ExpectedContent $expectedBuildRules

    $expectedModuleSource = @"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, $name)
"@
    Test-ExpectedUtf8File -PluginRoot $pluginRoot -RelativePath "Source\$name\Private\${name}Module.cpp" -ExpectedContent $expectedModuleSource
}

$expectedRuntimeDependencyLines = @($expectedRuntimeModules | ForEach-Object { '            "' + $_ + '",' }) -join "`r`n"
$expectedTestBuildRules = @"
using UnrealBuildTool;

public class $expectedTestModule : ModuleRules
{
    public $expectedTestModule(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
$expectedRuntimeDependencyLines
        });
    }
}
"@
Test-ExpectedUtf8File -PluginRoot $pluginRoot -RelativePath "Source\$expectedTestModule\$expectedTestModule.Build.cs" -ExpectedContent $expectedTestBuildRules

$expectedTestModuleSource = @"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, $expectedTestModule)
"@
Test-ExpectedUtf8File -PluginRoot $pluginRoot -RelativePath "Source\$expectedTestModule\Private\${expectedTestModule}Module.cpp" -ExpectedContent $expectedTestModuleSource

$expectedRuntimeTestLines = @($expectedRuntimeModules | ForEach-Object { '        TEXT("' + $_ + '"),' }) -join "`r`n"
$expectedAutomationTests = @"
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
$expectedRuntimeTestLines
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
Test-ExpectedUtf8File -PluginRoot $pluginRoot -RelativePath "Source\$expectedTestModule\Private\UrbanFoundationAutomationTests.cpp" -ExpectedContent $expectedAutomationTests
if ($failures.Count) {
    Write-Output 'FAIL: UrbanFoundation scaffold validation failed:'
    foreach ($failure in $failures) {
        Write-Output "- $failure"
    }
    exit 1
}

Write-Output 'PASS: UrbanFoundation descriptor, seven runtime modules, and test module are valid.'