# Urban Spear Unreal/Lyra 工程基础与 Windows 可运行构建 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立一个以 Unreal Engine 5.8 和 Lyra Starter Game 为基础、可编译、可自动测试、可启动并可打包为 Windows 64 位 Development 版本的 Project Urban Spear 工程基线。

**Architecture:** 保留 Lyra 的 `LyraGame` 模块作为上游基础，使用轻量的 `UrbanSpear` Game/Editor Target 提供项目身份，并在独立 `UrbanFoundation` 插件中建立七个已批准的 Urban 模块边界。第三方二进制素材通过 Git LFS 管理，导入 Lyra 前先把 GitHub 仓库切换为私有；构建、自动化测试和 Windows 打包通过版本化 PowerShell 脚本执行。

**Tech Stack:** Unreal Engine 5.8.x、Lyra Starter Game 5.8、Visual Studio 2026 Community 18.x（Game development with C++）、C++、PowerShell 5.1+、Git、Git LFS、GitHub CLI、Unreal Automation Framework、BuildCookRun。

---

## 范围与后续计划边界

本计划只交付“工程基础与 Windows 可运行构建”，不实现角色双视角、枪械、伤害、AI、任务、城市关卡、正式 UI 或最终画质。后续分别编写并审批以下计划：

1. `urban-spear-camera-and-character`：第一/第三人称、移动与输入；
2. `urban-spear-combat`：枪械、弹药、伤害、护甲与装备；
3. `urban-spear-ai`：感知、掩体、搜索、包抄与增援；
4. `urban-spear-mission-and-save`：目标、撤离、检查点与存档；
5. `urban-spear-city-blockout`：五区域灰盒章节；
6. `urban-spear-ui-audio-accessibility`：菜单、HUD、设置、字幕与音频；
7. `urban-spear-release-performance`：正式 Windows 构建、画质档位、性能与发布验收。

## 锁定工具基线（2026-07-29 核对）

- Unreal Engine：`5.8.x`；
- Visual Studio：`Visual Studio 2026 Community 18.x`，最低 18.0；
- 兼容后备：仅在 VS 2026 安装器无法工作时使用 `Visual Studio 2022 17.14.3+`；
- Windows SDK：`10.0.26100.0+`；
- Lyra：从 Fab/Epic Games Launcher 获取与 UE 5.8 匹配的版本。

官方参考：

- `https://dev.epicgames.com/documentation/en-us/unreal-engine/hardware-and-software-specifications-for-unreal-engine`
- `https://dev.epicgames.com/documentation/en-us/unreal-engine/install-unreal-engine`
- `https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine`
- `https://learn.microsoft.com/en-us/visualstudio/gamedev/unreal/get-started/vs-tools-unreal-install`
- `https://visualstudio.microsoft.com/downloads/`

## 目标文件结构

```text
.gitattributes
.gitignore
README.md
.github/pull_request_template.md
Build/Environment/Toolchain.json
Build/Manifests/FoundationModules.json
Build/Scripts/Test-RepositoryBaseline.ps1
Build/Scripts/Test-DevelopmentEnvironment.ps1
Build/Scripts/Import-LyraBaseline.ps1
Build/Scripts/Test-LyraImport.ps1
Build/Scripts/Test-ProjectIdentity.ps1
Build/Scripts/New-UrbanFoundationScaffold.ps1
Build/Scripts/Test-FoundationScaffold.ps1
Build/Scripts/Invoke-UrbanBuild.ps1
Build/Scripts/Invoke-UrbanAutomation.ps1
Build/Scripts/Build-WindowsDevelopment.ps1
Build/Scripts/Test-PackagedBuild.ps1
Config/DefaultGame.ini
Source/UrbanSpear.Target.cs
Source/UrbanSpearEditor.Target.cs
Plugins/UrbanFoundation/UrbanFoundation.uplugin
Plugins/UrbanFoundation/Source/<eight modules>/**
ThirdParty/AssetRegistry.csv
docs/architecture/lyra-baseline.md
docs/build/windows-development.md
docs/legal/third-party-assets.md
docs/roadmap/implementation-sequence.md
UrbanSpear.uproject
```

`Content/`、Lyra `Source/LyraGame`、Lyra 插件和配置由官方样例导入并进入私有仓库；`.uasset`、`.umap` 等二进制文件必须成为 Git LFS 指针。`Binaries/`、`DerivedDataCache/`、`Intermediate/`、`Saved/`、`.vs/` 和 `Artifacts/` 永不提交。

---

### Task 1: 建立私有仓库、忽略规则、Git LFS 与素材合规门禁

**Files:**
- Create: `.gitattributes`
- Modify: `.gitignore`
- Create: `Build/Scripts/Test-RepositoryBaseline.ps1`
- Create: `ThirdParty/AssetRegistry.csv`
- Create: `docs/legal/third-party-assets.md`

- [ ] **Step 1: 写入会先失败的仓库基线测试**

创建 `Build/Scripts/Test-RepositoryBaseline.ps1`：

```powershell
[CmdletBinding()]
param(
    [string]$Repository = 'gao-menmen/3D_GAME',
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
)
$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()

$visibility = gh repo view $Repository --json visibility --jq '.visibility'
if ($LASTEXITCODE -ne 0 -or $visibility.Trim() -ne 'PRIVATE') {
    $failures.Add("Repository must be PRIVATE before licensed payload is committed; actual: $visibility")
}
$lfsVersion = git lfs version
if ($LASTEXITCODE -ne 0 -or $lfsVersion -notmatch '^git-lfs/3\.') { $failures.Add('Git LFS 3.x is required.') }

$requiredIgnore = @('.superpowers/','Binaries/','DerivedDataCache/','Intermediate/','Saved/','.vs/','Artifacts/','*.sln','*.VC.db')
$ignore = if (Test-Path (Join-Path $Root '.gitignore')) { Get-Content (Join-Path $Root '.gitignore') -Raw } else { '' }
foreach ($line in $requiredIgnore) {
    if ($ignore -notmatch [regex]::Escape($line)) { $failures.Add(".gitignore missing: $line") }
}

$requiredLfs = @('*.uasset','*.umap','*.fbx','*.tga','*.exr','*.wav','*.mp4')
$attrs = if (Test-Path (Join-Path $Root '.gitattributes')) { Get-Content (Join-Path $Root '.gitattributes') -Raw } else { '' }
foreach ($pattern in $requiredLfs) {
    $rule = "$pattern filter=lfs diff=lfs merge=lfs -text"
    if ($attrs -notmatch [regex]::Escape($rule)) { $failures.Add(".gitattributes missing: $rule") }
}

$registry = Join-Path $Root 'ThirdParty\AssetRegistry.csv'
if (-not (Test-Path $registry)) { $failures.Add('ThirdParty/AssetRegistry.csv is missing.') }
elseif ((Get-Content $registry -First 1) -ne 'AssetId,AssetName,Provider,SourceUrl,License,RepositoryPolicy,ReleasePolicy,Owner') {
    $failures.Add('Asset registry header is invalid.')
}

if ($failures.Count) { $failures | ForEach-Object { Write-Error $_ }; exit 1 }
Write-Host 'PASS: repository privacy, ignore rules, LFS, and asset registry are valid.'
```

- [ ] **Step 2: 运行测试并确认失败**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-RepositoryBaseline.ps1
```

Expected: `FAIL`；报告当前仓库为 `PUBLIC`，并列出缺失的 Unreal 忽略项、LFS 规则和素材登记表。

- [ ] **Step 3: 在导入 Epic/Fab 内容前把仓库改为私有**

```powershell
gh repo edit gao-menmen/3D_GAME --visibility private --accept-visibility-change-consequences
gh repo view gao-menmen/3D_GAME --json visibility --jq '.visibility'
```

Expected: 输出 `PRIVATE`。

- [ ] **Step 4: 写入 Unreal 忽略规则**

将 `.gitignore` 完整替换为：

```gitignore
# Local brainstorming companion artifacts
.superpowers/

# Unreal generated content
Binaries/
DerivedDataCache/
Intermediate/
Saved/
.vs/
Artifacts/

# IDE generated files
*.sln
*.suo
*.opensdf
*.sdf
*.VC.db
*.VC.opendb
.idea/

# Local settings and secrets
*.user
*.userprefs
.env
.env.*
!.env.example
```

- [ ] **Step 5: 写入 Git LFS 规则**

创建 `.gitattributes`：

```gitattributes
*.uasset filter=lfs diff=lfs merge=lfs -text
*.umap filter=lfs diff=lfs merge=lfs -text
*.fbx filter=lfs diff=lfs merge=lfs -text
*.blend filter=lfs diff=lfs merge=lfs -text
*.psd filter=lfs diff=lfs merge=lfs -text
*.tga filter=lfs diff=lfs merge=lfs -text
*.exr filter=lfs diff=lfs merge=lfs -text
*.hdr filter=lfs diff=lfs merge=lfs -text
*.wav filter=lfs diff=lfs merge=lfs -text
*.flac filter=lfs diff=lfs merge=lfs -text
*.mp4 filter=lfs diff=lfs merge=lfs -text
*.mov filter=lfs diff=lfs merge=lfs -text
```

Run:

```powershell
git lfs install
git check-attr filter -- example.uasset example.umap example.fbx
```

Expected: 三行均显示 `filter: lfs`。

- [ ] **Step 6: 建立素材登记与发布规则**

创建 `ThirdParty/AssetRegistry.csv`：

```csv
AssetId,AssetName,Provider,SourceUrl,License,RepositoryPolicy,ReleasePolicy,Owner
EPIC-LYRA-5.8,Lyra Starter Game 5.8,Epic Games,https://www.fab.com/listings/93faede1-4434-47c0-85f1-bf27c0820ad0,Epic Content License,Private repository only,Cooked packaged product only,Project Owner
```

创建 `docs/legal/third-party-assets.md`：

```markdown
# Third-Party Asset Policy

The source repository remains private while it contains Epic Games, Fab, Marketplace, or paid third-party payload. Every imported pack receives a row in `ThirdParty/AssetRegistry.csv` before commit.

A packaged Windows build may include a registered asset only when its license permits distribution in a cooked product. Source assets, credentials, receipts, account identifiers, and local absolute paths are not published.

Lyra Starter Game 5.8 is acquired through Epic Games Launcher/Fab, retained in the private development repository, and externally distributed only as cooked output under the recorded Epic license policy.
```

- [ ] **Step 7: 重新运行测试并提交**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-RepositoryBaseline.ps1
git add .gitignore .gitattributes Build/Scripts/Test-RepositoryBaseline.ps1 ThirdParty/AssetRegistry.csv docs/legal/third-party-assets.md
git commit -m "build: establish Unreal repository and asset safety rules"
```

Expected: 测试输出 `PASS`，提交成功。

---

### Task 2: 安装并验证 UE 5.8、VS 2026、Epic Launcher 与 Lyra

**Files:**
- Create: `Build/Environment/Toolchain.json`
- Create: `Build/Scripts/Test-DevelopmentEnvironment.ps1`

- [ ] **Step 1: 写入工具链清单**

创建 `Build/Environment/Toolchain.json`：

```json
{
  "UnrealEngine": { "MajorMinor": "5.8", "DefaultRoot": "C:\\Program Files\\Epic Games\\UE_5.8" },
  "VisualStudio": { "MinimumVersion": "18.0.0", "RequiredWorkload": "Microsoft.VisualStudio.Workload.NativeGame" },
  "WindowsSdk": { "MinimumVersion": "10.0.26100.0" },
  "Lyra": { "ProjectFile": "LyraStarterGame.uproject", "DefaultRoot": "C:\\Users\\gao-menmen\\Documents\\Unreal Projects\\LyraStarterGame" },
  "Disk": { "MinimumFreeGb": 120 }
}
```

- [ ] **Step 2: 写入会先失败的环境检查**

创建 `Build/Scripts/Test-DevelopmentEnvironment.ps1`：

```powershell
[CmdletBinding()]
param(
    [string]$EngineRoot = $env:UE58_ROOT,
    [string]$LyraRoot = $env:LYRA58_ROOT,
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
)
$ErrorActionPreference = 'Stop'
$config = Get-Content (Join-Path $Root 'Build\Environment\Toolchain.json') -Raw | ConvertFrom-Json
if ([string]::IsNullOrWhiteSpace($EngineRoot)) { $EngineRoot = $config.UnrealEngine.DefaultRoot }
if ([string]::IsNullOrWhiteSpace($LyraRoot)) { $LyraRoot = $config.Lyra.DefaultRoot }
$failures = [System.Collections.Generic.List[string]]::new()

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { $failures.Add('Visual Studio Installer/vswhere is missing.') }
else {
    $json = & $vswhere -latest -products * -requires $config.VisualStudio.RequiredWorkload -format json
    $vs = $json | ConvertFrom-Json | Select-Object -First 1
    if (-not $vs) { $failures.Add('Game development with C++ workload is missing.') }
    elseif ([version]$vs.installationVersion -lt [version]$config.VisualStudio.MinimumVersion) { $failures.Add("Visual Studio version too old: $($vs.installationVersion)") }
}

$buildVersionPath = Join-Path $EngineRoot 'Engine\Build\Build.version'
if (-not (Test-Path $buildVersionPath)) { $failures.Add("Unreal Engine missing: $buildVersionPath") }
else {
    $v = Get-Content $buildVersionPath -Raw | ConvertFrom-Json
    if ("$($v.MajorVersion).$($v.MinorVersion)" -ne $config.UnrealEngine.MajorMinor) { $failures.Add('Unreal Engine must be 5.8.') }
}

$sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Include'
$sdk = if (Test-Path $sdkRoot) { Get-ChildItem $sdkRoot -Directory | ForEach-Object { try { [version]$_.Name } catch {} } | Sort-Object -Descending | Select-Object -First 1 }
if (-not $sdk -or $sdk -lt [version]$config.WindowsSdk.MinimumVersion) { $failures.Add("Windows SDK $($config.WindowsSdk.MinimumVersion)+ is required; actual: $sdk") }

$lyraProject = Join-Path $LyraRoot $config.Lyra.ProjectFile
if (-not (Test-Path $lyraProject)) { $failures.Add("Lyra project missing: $lyraProject") }

$lfs = git lfs version
if ($LASTEXITCODE -ne 0 -or $lfs -notmatch '^git-lfs/3\.') { $failures.Add('Git LFS 3.x is required.') }
$drive = ([IO.Path]::GetPathRoot($Root)).TrimEnd('\').TrimEnd(':')
$freeGb = [math]::Floor((Get-PSDrive $drive).Free / 1GB)
if ($freeGb -lt [int]$config.Disk.MinimumFreeGb) { $failures.Add("120 GB free required; actual: $freeGb GB") }

if ($failures.Count) { $failures | ForEach-Object { Write-Error $_ }; exit 1 }
Write-Host 'PASS: VS, Windows SDK, UE 5.8, Lyra 5.8, Git LFS, and disk space are ready.'
Write-Host "UE58_ROOT=$EngineRoot"
Write-Host "LYRA58_ROOT=$LyraRoot"
```

- [ ] **Step 3: 运行检查并确认可读失败**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-DevelopmentEnvironment.ps1
```

Expected: `FAIL`，明确报告 Visual Studio、UE 5.8 和 Lyra 尚未安装，而不是脚本解析错误。

- [ ] **Step 4: 安装 Visual Studio 2026 Community**

在管理员 PowerShell 中运行：

```powershell
$installer = Join-Path $env:TEMP 'vs_community_2026.exe'
Invoke-WebRequest 'https://aka.ms/vs/18/stable/vs_community.exe' -OutFile $installer
Start-Process $installer -Wait -ArgumentList @('--add','Microsoft.VisualStudio.Workload.NativeGame','--includeRecommended','--passive','--wait','--norestart')
```

Expected: 安装 Game development with C++、MSVC x64/x86、Windows 11 SDK、Visual Studio Tools for Unreal Engine、Blueprint Debugger 与 Unreal Engine Test Adapter。安装结束后重启 Windows。

- [ ] **Step 5: 安装 Epic Games Launcher**

```powershell
winget install --id EpicGames.EpicGamesLauncher --exact --source winget --accept-package-agreements --accept-source-agreements
```

Expected: 安装成功，开始菜单出现 Epic Games Launcher。

- [ ] **Step 6: 安装 UE 5.8**

1. 启动 Epic Games Launcher 并登录；
2. 打开 **Unreal Engine → Library**；
3. 添加引擎槽位并选择最新 `5.8.x`；
4. 安装到 `C:\Program Files\Epic Games\UE_5.8`；
5. 保留 Core Components、Starter Content、Templates and Feature Packs；
6. 安装后点击 Verify。

Expected: `UE_5.8\Engine\Build\Build.version` 的 MajorVersion/MinorVersion 为 `5`/`8`。

- [ ] **Step 7: 获取 Lyra 5.8**

1. 在 Fab 搜索 **Lyra Starter Game**，确认发布者为 Epic Games；
2. Add to My Library；
3. 在 Launcher 中点击 Create Project；
4. Engine Version 选择 `5.8`；
5. Location 使用 `C:\Users\gao-menmen\Documents\Unreal Projects`；
6. Project Name 使用 `LyraStarterGame`；
7. 首次启动编辑器，等待初始化完成后正常关闭。

Expected: `C:\Users\gao-menmen\Documents\Unreal Projects\LyraStarterGame\LyraStarterGame.uproject` 存在。

- [ ] **Step 8: 固定环境变量并验证通过**

```powershell
[Environment]::SetEnvironmentVariable('UE58_ROOT','C:\Program Files\Epic Games\UE_5.8','User')
[Environment]::SetEnvironmentVariable('LYRA58_ROOT','C:\Users\gao-menmen\Documents\Unreal Projects\LyraStarterGame','User')
$env:UE58_ROOT = 'C:\Program Files\Epic Games\UE_5.8'
$env:LYRA58_ROOT = 'C:\Users\gao-menmen\Documents\Unreal Projects\LyraStarterGame'
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-DevelopmentEnvironment.ps1
```

Expected: 输出 `PASS: VS, Windows SDK, UE 5.8, Lyra 5.8, Git LFS, and disk space are ready.`

- [ ] **Step 9: 提交环境清单**

```powershell
git add Build/Environment/Toolchain.json Build/Scripts/Test-DevelopmentEnvironment.ps1
git commit -m "build: lock Unreal 5.8 development environment"
```

---

### Task 3: 安全导入 Lyra 5.8 基线并验证 LFS

**Files:**
- Create: `Build/Scripts/Import-LyraBaseline.ps1`
- Create: `Build/Scripts/Test-LyraImport.ps1`
- Create: `docs/architecture/lyra-baseline.md`
- Create: `UrbanSpear.uproject`
- Import: `Config/**`, `Content/**`, `Plugins/**`, `Source/LyraGame/**`, `Source/LyraGame.Target.cs`, `Source/LyraGameEditor.Target.cs`

- [ ] **Step 1: 写入会先失败的导入测试**

创建 `Build/Scripts/Test-LyraImport.ps1`：

```powershell
[CmdletBinding()]
param([string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path)
$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()
$required = @('UrbanSpear.uproject','Source\LyraGame.Target.cs','Source\LyraGameEditor.Target.cs','Source\LyraGame\LyraGame.Build.cs','Plugins\GameFeatures\ShooterCore\ShooterCore.uplugin','Content')
foreach ($relative in $required) {
    if (-not (Test-Path (Join-Path $Root $relative))) { $failures.Add("Missing Lyra baseline path: $relative") }
}
$visibility = gh repo view gao-menmen/3D_GAME --json visibility --jq '.visibility'
if ($visibility.Trim() -ne 'PRIVATE') { $failures.Add('Repository must be PRIVATE.') }
$asset = Get-ChildItem $Root -Recurse -File -Include *.uasset,*.umap | Select-Object -First 1
if (-not $asset) { $failures.Add('No Unreal binary asset was imported.') }
elseif ((git check-attr filter -- $asset.FullName) -notmatch 'filter: lfs') { $failures.Add("Asset is not covered by LFS: $($asset.FullName)") }
if ($failures.Count) { $failures | ForEach-Object { Write-Error $_ }; exit 1 }
Write-Host 'PASS: Lyra baseline, private-repository gate, and LFS coverage are valid.'
```

- [ ] **Step 2: 运行导入测试并确认失败**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-LyraImport.ps1
```

Expected: `FAIL`，报告项目描述、Lyra source/plugin 和 Unreal binary assets 缺失。

- [ ] **Step 3: 写入可重复执行的导入脚本**

创建 `Build/Scripts/Import-LyraBaseline.ps1`：

```powershell
[CmdletBinding()]
param(
    [string]$SourceRoot = $env:LYRA58_ROOT,
    [string]$DestinationRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($SourceRoot)) { throw 'LYRA58_ROOT is not set.' }
$sourceProject = Join-Path $SourceRoot 'LyraStarterGame.uproject'
if (-not (Test-Path $sourceProject)) { throw "Lyra project missing: $sourceProject" }
$visibility = gh repo view gao-menmen/3D_GAME --json visibility --jq '.visibility'
if ($visibility.Trim() -ne 'PRIVATE') { throw 'Refusing to import Lyra into a public repository.' }
$source = (Resolve-Path $SourceRoot).Path.TrimEnd('\')
$destination = (Resolve-Path $DestinationRoot).Path.TrimEnd('\')
if ($source -eq $destination) { throw 'Source and destination must differ.' }
& robocopy $source $destination /E /COPY:DAT /DCOPY:DAT /R:2 /W:2 /XD .git .vs Binaries DerivedDataCache Intermediate Saved /XF .gitignore .gitattributes README.md
if ($LASTEXITCODE -ge 8) { throw "Robocopy failed with exit code $LASTEXITCODE" }
$copied = Join-Path $destination 'LyraStarterGame.uproject'
$urban = Join-Path $destination 'UrbanSpear.uproject'
if (-not (Test-Path $copied)) { throw 'Copied project descriptor is missing.' }
Move-Item -LiteralPath $copied -Destination $urban -Force
$descriptor = Get-Content $urban -Raw | ConvertFrom-Json
$descriptor.EngineAssociation = '5.8'
$descriptor | ConvertTo-Json -Depth 100 | Set-Content $urban -Encoding UTF8
Write-Host "Imported Lyra baseline and created $urban"
```

- [ ] **Step 4: 执行导入并验证**

```powershell
$env:LYRA58_ROOT = [Environment]::GetEnvironmentVariable('LYRA58_ROOT','User')
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Import-LyraBaseline.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-LyraImport.ps1
```

Expected: 测试 `PASS`；出现 `Config`、`Content`、`Plugins`、`Source` 和 `UrbanSpear.uproject`，没有导入生成目录。

- [ ] **Step 5: 记录 Lyra 上游边界**

创建 `docs/architecture/lyra-baseline.md`：

```markdown
# Lyra Baseline Architecture

Project Urban Spear imports Lyra Starter Game 5.8 as its upstream foundation.

The descriptor is `UrbanSpear.uproject`. Lyra's `LyraGame` module, targets, plugins, and asset paths retain upstream names. Urban-owned code uses the `Urban` prefix and stays outside `Source/LyraGame`.

- Upstream: `Source/LyraGame`, Lyra plugins, and Lyra content.
- Urban Spear: `Source/UrbanSpear*.Target.cs` and `Plugins/UrbanFoundation`.
- Generated: `Binaries`, `DerivedDataCache`, `Intermediate`, `Saved`, `.vs`, and `Artifacts`; never committed.

Engine or Lyra upgrades occur on isolated branches. Urban-owned files are not inserted into Lyra source folders.
```

- [ ] **Step 6: 暂存 LFS 对象并提交**

```powershell
git add UrbanSpear.uproject Config Content Plugins Source docs/architecture/lyra-baseline.md Build/Scripts/Import-LyraBaseline.ps1 Build/Scripts/Test-LyraImport.ps1
git lfs status
git status --short
git commit -m "feat: import Lyra 5.8 foundation"
```

Expected: `.uasset`/`.umap` 位于 `Git LFS objects to be committed`；没有生成目录；提交成功。

---

### Task 4: 建立 UrbanSpear 项目标识与 Windows Target

**Files:**
- Create: `Build/Scripts/Test-ProjectIdentity.ps1`
- Create: `Source/UrbanSpear.Target.cs`
- Create: `Source/UrbanSpearEditor.Target.cs`
- Modify: `Config/DefaultGame.ini`

- [ ] **Step 1: 写入会先失败的项目标识测试**

创建 `Build/Scripts/Test-ProjectIdentity.ps1`：

```powershell
[CmdletBinding()]
param([string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path)
$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()
$projectPath = Join-Path $Root 'UrbanSpear.uproject'
if (-not (Test-Path $projectPath)) { $failures.Add('UrbanSpear.uproject is missing.') }
elseif ((Get-Content $projectPath -Raw | ConvertFrom-Json).EngineAssociation -ne '5.8') { $failures.Add('EngineAssociation must be 5.8.') }
$gameTarget = Join-Path $Root 'Source\UrbanSpear.Target.cs'
$editorTarget = Join-Path $Root 'Source\UrbanSpearEditor.Target.cs'
if (-not (Test-Path $gameTarget)) { $failures.Add('UrbanSpear game target is missing.') }
elseif ((Get-Content $gameTarget -Raw) -notmatch 'class UrbanSpearTarget : LyraGameTarget') { $failures.Add('Game target must inherit LyraGameTarget.') }
if (-not (Test-Path $editorTarget)) { $failures.Add('UrbanSpear editor target is missing.') }
elseif ((Get-Content $editorTarget -Raw) -notmatch 'class UrbanSpearEditorTarget : LyraGameEditorTarget') { $failures.Add('Editor target must inherit LyraGameEditorTarget.') }
$ini = Get-Content (Join-Path $Root 'Config\DefaultGame.ini') -Raw
foreach ($entry in @('ProjectName=Project Urban Spear','BuildTarget=UrbanSpear')) {
    if ($ini -notmatch [regex]::Escape($entry)) { $failures.Add("DefaultGame.ini missing: $entry") }
}
if ($failures.Count) { $failures | ForEach-Object { Write-Error $_ }; exit 1 }
Write-Host 'PASS: descriptor, metadata, and UrbanSpear targets are valid.'
```

- [ ] **Step 2: 运行测试并确认失败**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-ProjectIdentity.ps1
```

Expected: `FAIL`，报告两个 Target 和项目元数据缺失。

- [ ] **Step 3: 创建 Game Target**

创建 `Source/UrbanSpear.Target.cs`：

```csharp
using UnrealBuildTool;
public class UrbanSpearTarget : LyraGameTarget
{
    public UrbanSpearTarget(TargetInfo Target) : base(Target) { }
}
```

- [ ] **Step 4: 创建 Editor Target**

创建 `Source/UrbanSpearEditor.Target.cs`：

```csharp
using UnrealBuildTool;
public class UrbanSpearEditorTarget : LyraGameEditorTarget
{
    public UrbanSpearEditorTarget(TargetInfo Target) : base(Target) { }
}
```

- [ ] **Step 5: 写入项目展示和打包标识**

在 `Config/DefaultGame.ini` 末尾追加：

```ini
[/Script/EngineSettings.GeneralProjectSettings]
ProjectID=D9C68B2E4B734BBE924A3CD8657F2A11
ProjectName=Project Urban Spear
ProjectDisplayedTitle=NSLOCTEXT("UrbanSpear", "ProjectDisplayedTitle", "Project Urban Spear")
Description=Near-future tactical shooter prototype
CompanyName=Urban Spear Development

[/Script/UnrealEd.ProjectPackagingSettings]
BuildTarget=UrbanSpear
BuildConfiguration=PPBC_Development
FullRebuild=True
ForDistribution=False
```

- [ ] **Step 6: 验证标识、生成项目文件并提交**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-ProjectIdentity.ps1
& "$env:UE58_ROOT\Engine\Build\BatchFiles\GenerateProjectFiles.bat" -project="$PWD\UrbanSpear.uproject" -game -engine
git add Config/DefaultGame.ini Source/UrbanSpear.Target.cs Source/UrbanSpearEditor.Target.cs Build/Scripts/Test-ProjectIdentity.ps1
git commit -m "feat: add Urban Spear project targets"
```

Expected: 测试 `PASS`；生成 `UrbanSpear.sln`；提交成功。

---

### Task 5: 建立七个 Urban 模块边界和自动化测试模块

**Files:**
- Create: `Build/Manifests/FoundationModules.json`
- Create: `Build/Scripts/Test-FoundationScaffold.ps1`
- Create: `Build/Scripts/New-UrbanFoundationScaffold.ps1`
- Create: `Plugins/UrbanFoundation/UrbanFoundation.uplugin`
- Create: `Plugins/UrbanFoundation/Source/**`

- [ ] **Step 1: 写入模块清单**

创建 `Build/Manifests/FoundationModules.json`：

```json
{
  "Plugin": "UrbanFoundation",
  "RuntimeModules": ["UrbanCore", "UrbanCombat", "UrbanAI", "UrbanMission", "UrbanModes", "UrbanUI", "UrbanOnline"],
  "TestModule": "UrbanFoundationTests"
}
```

- [ ] **Step 2: 写入会先失败的脚手架测试**

创建 `Build/Scripts/Test-FoundationScaffold.ps1`：

```powershell
[CmdletBinding()]
param([string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path)
$manifest = Get-Content (Join-Path $Root 'Build\Manifests\FoundationModules.json') -Raw | ConvertFrom-Json
$pluginRoot = Join-Path $Root "Plugins\$($manifest.Plugin)"
$failures = [System.Collections.Generic.List[string]]::new()
$descriptorPath = Join-Path $pluginRoot 'UrbanFoundation.uplugin'
if (-not (Test-Path $descriptorPath)) { $failures.Add('Plugin descriptor missing.') }
else {
    $names = (Get-Content $descriptorPath -Raw | ConvertFrom-Json).Modules.Name
    foreach ($name in @($manifest.RuntimeModules) + $manifest.TestModule) { if ($name -notin $names) { $failures.Add("Descriptor missing: $name") } }
}
foreach ($name in $manifest.RuntimeModules) {
    foreach ($relative in @("Source\$name\$name.Build.cs","Source\$name\Private\${name}Module.cpp")) {
        if (-not (Test-Path (Join-Path $pluginRoot $relative))) { $failures.Add("Missing: $relative") }
    }
}
foreach ($relative in @('Source\UrbanFoundationTests\UrbanFoundationTests.Build.cs','Source\UrbanFoundationTests\Private\UrbanFoundationTestsModule.cpp','Source\UrbanFoundationTests\Private\UrbanFoundationAutomationTests.cpp')) {
    if (-not (Test-Path (Join-Path $pluginRoot $relative))) { $failures.Add("Missing: $relative") }
}
if ($failures.Count) { $failures | ForEach-Object { Write-Error $_ }; exit 1 }
Write-Host 'PASS: UrbanFoundation plugin and eight module layouts are present.'
```

- [ ] **Step 3: 运行脚手架测试并确认失败**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-FoundationScaffold.ps1
```

Expected: `FAIL`，报告插件和八个模块源文件缺失。

- [ ] **Step 4: 写入确定性模块生成器**

创建 `Build/Scripts/New-UrbanFoundationScaffold.ps1`：

```powershell
[CmdletBinding()]
param([string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path)
$manifest = Get-Content (Join-Path $Root 'Build\Manifests\FoundationModules.json') -Raw | ConvertFrom-Json
$pluginRoot = Join-Path $Root "Plugins\$($manifest.Plugin)"
New-Item -ItemType Directory -Force -Path $pluginRoot | Out-Null
$modules = @()
foreach ($name in $manifest.RuntimeModules) { $modules += [ordered]@{ Name=$name; Type='Runtime'; LoadingPhase='Default' } }
$modules += [ordered]@{ Name=$manifest.TestModule; Type='DeveloperTool'; LoadingPhase='PostEngineInit' }
[ordered]@{ FileVersion=3; Version=1; VersionName='0.1.0'; FriendlyName='Urban Spear Foundation'; Description='Shared runtime boundaries and tests.'; Category='Urban Spear'; EnabledByDefault=$true; CanContainContent=$false; Modules=$modules } |
  ConvertTo-Json -Depth 20 | Set-Content (Join-Path $pluginRoot 'UrbanFoundation.uplugin') -Encoding UTF8
foreach ($name in $manifest.RuntimeModules) {
    $moduleRoot = Join-Path $pluginRoot "Source\$name"
    New-Item -ItemType Directory -Force -Path (Join-Path $moduleRoot 'Private') | Out-Null
    "using UnrealBuildTool; public class $name : ModuleRules { public $name(ReadOnlyTargetRules Target) : base(Target) { PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs; PublicDependencyModuleNames.AddRange(new[] { `"Core`" }); } }" | Set-Content (Join-Path $moduleRoot "$name.Build.cs") -Encoding UTF8
    "#include `"Modules/ModuleManager.h`"`r`nIMPLEMENT_MODULE(FDefaultModuleImpl, $name)" | Set-Content (Join-Path $moduleRoot "Private\${name}Module.cpp") -Encoding UTF8
}
$testRoot = Join-Path $pluginRoot 'Source\UrbanFoundationTests'
New-Item -ItemType Directory -Force -Path (Join-Path $testRoot 'Private') | Out-Null
'using UnrealBuildTool; public class UrbanFoundationTests : ModuleRules { public UrbanFoundationTests(ReadOnlyTargetRules Target) : base(Target) { PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs; PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UrbanCore", "UrbanCombat", "UrbanAI", "UrbanMission", "UrbanModes", "UrbanUI", "UrbanOnline" }); } }' | Set-Content (Join-Path $testRoot 'UrbanFoundationTests.Build.cs') -Encoding UTF8
'#include "Modules/ModuleManager.h"
IMPLEMENT_MODULE(FDefaultModuleImpl, UrbanFoundationTests)' | Set-Content (Join-Path $testRoot 'Private\UrbanFoundationTestsModule.cpp') -Encoding UTF8
@"
#include "CoreMinimal.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUrbanModuleLoadTest, "UrbanSpear.Foundation.ModuleLoad", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FUrbanModuleLoadTest::RunTest(const FString& Parameters)
{
    const TCHAR* Names[] = { TEXT("UrbanCore"), TEXT("UrbanCombat"), TEXT("UrbanAI"), TEXT("UrbanMission"), TEXT("UrbanModes"), TEXT("UrbanUI"), TEXT("UrbanOnline") };
    for (const TCHAR* Name : Names)
    {
        TestTrue(FString::Printf(TEXT("%s registered"), Name), FModuleManager::Get().ModuleExists(Name));
        TestTrue(FString::Printf(TEXT("%s loads"), Name), FModuleManager::Get().LoadModulePtr<IModuleInterface>(Name) != nullptr);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUrbanProjectIdentityTest, "UrbanSpear.Foundation.ProjectIdentity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FUrbanProjectIdentityTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Project name"), FString(FApp::GetProjectName()), FString(TEXT("UrbanSpear")));
    TestTrue(TEXT("Lyra front-end map exists"), FPackageName::DoesPackageExist(TEXT("/Game/System/FrontEnd/Maps/L_LyraFrontEnd")));
    return true;
}
#endif
"@ | Set-Content (Join-Path $testRoot 'Private\UrbanFoundationAutomationTests.cpp') -Encoding UTF8
Write-Host 'Created UrbanFoundation runtime and test modules.'
```

- [ ] **Step 5: 生成插件、验证并重新生成项目文件**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\New-UrbanFoundationScaffold.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-FoundationScaffold.ps1
& "$env:UE58_ROOT\Engine\Build\BatchFiles\GenerateProjectFiles.bat" -project="$PWD\UrbanSpear.uproject" -game -engine
```

Expected: 脚手架测试 `PASS`；解决方案包含七个 Runtime 模块和 `UrbanFoundationTests`。

- [ ] **Step 6: 提交模块骨架**

```powershell
git add Build/Manifests/FoundationModules.json Build/Scripts/New-UrbanFoundationScaffold.ps1 Build/Scripts/Test-FoundationScaffold.ps1 Plugins/UrbanFoundation
git commit -m "feat: add Urban foundation module boundaries"
```

---

### Task 6: 编译 Editor Target 并运行 Unreal 自动化烟雾测试

**Files:**
- Create: `Build/Scripts/Invoke-UrbanBuild.ps1`
- Create: `Build/Scripts/Invoke-UrbanAutomation.ps1`

- [ ] **Step 1: 写入统一编译脚本**

创建 `Build/Scripts/Invoke-UrbanBuild.ps1`：

```powershell
[CmdletBinding()]
param(
    [ValidateSet('UrbanSpearEditor','UrbanSpear')][string]$Target = 'UrbanSpearEditor',
    [ValidateSet('Development','DebugGame','Shipping')][string]$Configuration = 'Development',
    [string]$EngineRoot = $env:UE58_ROOT,
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($EngineRoot)) { throw 'UE58_ROOT is not set.' }
$buildBat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$project = Join-Path $Root 'UrbanSpear.uproject'
if (-not (Test-Path $buildBat)) { throw "Build.bat missing: $buildBat" }
& $buildBat $Target Win64 $Configuration "-Project=$project" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw "Unreal build failed with exit code $LASTEXITCODE" }
Write-Host "PASS: $Target Win64 $Configuration compiled."
```

- [ ] **Step 2: 写入自动化烟雾测试脚本**

创建 `Build/Scripts/Invoke-UrbanAutomation.ps1`：

```powershell
[CmdletBinding()]
param(
    [string]$EngineRoot = $env:UE58_ROOT,
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($EngineRoot)) { throw 'UE58_ROOT is not set.' }
$editorCmd = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $Root 'UrbanSpear.uproject'
$report = Join-Path $Root 'Saved\Automation\Foundation'
New-Item -ItemType Directory -Force -Path $report | Out-Null
& $editorCmd $project -unattended -nop4 -nosplash -NullRHI '-ExecCmds=Automation RunTests UrbanSpear.Foundation; Quit' '-TestExit=Automation Test Queue Empty' "-ReportOutputPath=$report" -log
if ($LASTEXITCODE -ne 0) { throw "Automation tests failed with exit code $LASTEXITCODE" }
$index = Join-Path $report 'index.json'
if (-not (Test-Path $index)) { throw "Automation report missing: $index" }
$result = Get-Content $index -Raw | ConvertFrom-Json
if ([int]$result.failed -ne 0 -or [int]$result.succeeded -lt 2) { throw "Expected 2+ passing and 0 failing tests; succeeded=$($result.succeeded), failed=$($result.failed)" }
Write-Host "PASS: $($result.succeeded) UrbanSpear.Foundation tests succeeded."
```

- [ ] **Step 3: 验证两个脚本可解析**

```powershell
$files = @('.\Build\Scripts\Invoke-UrbanBuild.ps1','.\Build\Scripts\Invoke-UrbanAutomation.ps1')
foreach ($file in $files) {
  $tokens=$null; $errors=$null
  [void][Management.Automation.Language.Parser]::ParseFile((Resolve-Path $file),[ref]$tokens,[ref]$errors)
  if ($errors.Count) { $errors; exit 1 }
}
'PASS: build scripts parse.'
```

Expected: 输出 `PASS: build scripts parse.`。

- [ ] **Step 4: 编译 UrbanSpearEditor**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Invoke-UrbanBuild.ps1 -Target UrbanSpearEditor -Configuration Development
```

Expected: UnrealBuildTool 输出 `Result: Succeeded`，脚本输出 `PASS`。

- [ ] **Step 5: 运行基础自动化测试**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Invoke-UrbanAutomation.ps1
```

Expected: `UrbanSpear.Foundation.ModuleLoad` 和 `UrbanSpear.Foundation.ProjectIdentity` 通过；至少 2 成功、0 失败。

- [ ] **Step 6: 确认生成物未被跟踪并提交脚本**

```powershell
git status --short
git add Build/Scripts/Invoke-UrbanBuild.ps1 Build/Scripts/Invoke-UrbanAutomation.ps1
git commit -m "test: add Unreal build and foundation smoke tests"
```

Expected: 状态中没有 `Binaries`、`Intermediate`、`Saved`、`.vs` 或 `.sln`；提交成功。

---

### Task 7: 生成 Windows Development 包并执行启动测试

**Files:**
- Create: `Build/Scripts/Build-WindowsDevelopment.ps1`
- Create: `Build/Scripts/Test-PackagedBuild.ps1`
- Create: `docs/build/windows-development.md`

- [ ] **Step 1: 写入 Windows Development 打包脚本**

创建 `Build/Scripts/Build-WindowsDevelopment.ps1`：

```powershell
[CmdletBinding()]
param(
    [string]$EngineRoot = $env:UE58_ROOT,
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($EngineRoot)) { throw 'UE58_ROOT is not set.' }
$runUat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
$project = Join-Path $Root 'UrbanSpear.uproject'
$archive = Join-Path $Root 'Artifacts\Windows-Development'
if (Test-Path $archive) { Remove-Item -LiteralPath $archive -Recurse -Force }
New-Item -ItemType Directory -Force -Path $archive | Out-Null
& $runUat BuildCookRun "-project=$project" -target=UrbanSpear -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive "-archivedirectory=$archive" -utf8output
if ($LASTEXITCODE -ne 0) { throw "BuildCookRun failed with exit code $LASTEXITCODE" }
Write-Host "PASS: Windows Development build archived at $archive"
```

- [ ] **Step 2: 写入打包程序启动测试**

创建 `Build/Scripts/Test-PackagedBuild.ps1`：

```powershell
[CmdletBinding()]
param([string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path)
$ErrorActionPreference = 'Stop'
$archive = Join-Path $Root 'Artifacts\Windows-Development'
$exe = Get-ChildItem $archive -Recurse -File -Filter 'UrbanSpear.exe' | Select-Object -First 1
if (-not $exe) { throw "UrbanSpear.exe missing under $archive" }
$process = Start-Process -FilePath $exe.FullName -ArgumentList @('-nullrhi','-unattended','-nosplash','-NoSound','-ExecCmds=quit') -PassThru -WindowStyle Hidden
if (-not $process.WaitForExit(90000)) { Stop-Process -Id $process.Id -Force; throw 'Packaged build did not exit within 90 seconds.' }
if ($process.ExitCode -ne 0) { throw "Packaged build exited with code $($process.ExitCode)" }
Write-Host "PASS: packaged executable started and exited cleanly: $($exe.FullName)"
```

- [ ] **Step 3: 验证打包脚本可解析**

```powershell
$files = @('.\Build\Scripts\Build-WindowsDevelopment.ps1','.\Build\Scripts\Test-PackagedBuild.ps1')
foreach ($file in $files) {
  $tokens=$null; $errors=$null
  [void][Management.Automation.Language.Parser]::ParseFile((Resolve-Path $file),[ref]$tokens,[ref]$errors)
  if ($errors.Count) { $errors; exit 1 }
}
'PASS: package scripts parse.'
```

Expected: 输出 `PASS: package scripts parse.`。

- [ ] **Step 4: 打包并执行无界面启动测试**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Build-WindowsDevelopment.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-PackagedBuild.ps1
```

Expected: AutomationTool 输出 `BUILD SUCCESSFUL`；`Artifacts\Windows-Development` 包含 `UrbanSpear.exe`；启动测试在 90 秒内以退出码 0 结束。

- [ ] **Step 5: 在当前电脑执行一次 720p 低画质人工启动**

```powershell
$exe = Get-ChildItem .\Artifacts\Windows-Development -Recurse -File -Filter UrbanSpear.exe | Select-Object -First 1
Start-Process $exe.FullName -ArgumentList @('-windowed','-ResX=1280','-ResY=720','-sg.ViewDistanceQuality=0','-sg.AntiAliasingQuality=0','-sg.ShadowQuality=0','-sg.GlobalIlluminationQuality=0','-sg.ReflectionQuality=0','-sg.PostProcessQuality=0','-sg.TextureQuality=0','-sg.EffectsQuality=0','-sg.FoliageQuality=0')
```

Expected: Lyra 前端可见、键鼠有响应、无崩溃。本机只验收功能启动，不验收最终 Lumen/Nanite 或 1080p 60 FPS。

- [ ] **Step 6: 写入 Windows 构建说明**

创建 `docs/build/windows-development.md`：

```markdown
# Windows Development Build

Run the environment check before compiling:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-DevelopmentEnvironment.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Invoke-UrbanBuild.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Invoke-UrbanAutomation.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Build-WindowsDevelopment.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-PackagedBuild.ps1
```

Output is under `Artifacts/Windows-Development` and is not committed. The integrated-GPU workstation is approved for low-quality functionality checks only. Final visual and 1080p/60 FPS acceptance requires the dedicated-GPU test machine.
```

- [ ] **Step 7: 提交打包入口**

```powershell
git add Build/Scripts/Build-WindowsDevelopment.ps1 Build/Scripts/Test-PackagedBuild.ps1 docs/build/windows-development.md
git commit -m "build: add Windows development packaging pipeline"
```

---

### Task 8: 完善 README、路线图、PR 模板并提交实施 PR

**Files:**
- Modify: `README.md`
- Create: `docs/roadmap/implementation-sequence.md`
- Create: `.github/pull_request_template.md`

- [ ] **Step 1: 写入项目 README**

将 `README.md` 完整替换为：

```markdown
# Project Urban Spear

Project Urban Spear is a Windows 64-bit near-future tactical shooter built with Unreal Engine 5.8 and the Lyra Starter Game architecture.

## Current milestone

The current milestone provides a reproducible UE/Lyra project, seven Urban module boundaries, automated smoke tests, and a packaged Windows Development build.

## Requirements

- Unreal Engine 5.8.x
- Lyra Starter Game 5.8
- Visual Studio 2026 Community 18.x with Game development with C++
- Windows SDK 10.0.26100.0+
- Git LFS 3.x

## Validate, build, test, and package

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-RepositoryBaseline.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-DevelopmentEnvironment.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Invoke-UrbanBuild.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Invoke-UrbanAutomation.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Build-WindowsDevelopment.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-PackagedBuild.ps1
```

The repository remains private while it contains licensed source payload. Unreal binary assets use Git LFS. Generated folders, builds, secrets, and local settings are excluded from Git.
```

- [ ] **Step 2: 写入实施路线图**

创建 `docs/roadmap/implementation-sequence.md`：

```markdown
# Urban Spear Implementation Sequence

1. Foundation: UE 5.8, Lyra, modules, automation, Windows package.
2. Character and camera: shared first-person/third-person state and restrictions.
3. Combat: three weapons, ammunition, damage, armor, and four tactical items.
4. AI: four enemy classes, perception, cover, search, flanking, reinforcement.
5. Mission and save: objectives, extraction, failure, checkpoint, save, difficulty.
6. City blockout: checkpoint, garage, commercial center, metro, rooftop.
7. UI/audio/accessibility: menu, HUD, settings, subtitles, audio, readable errors.
8. Release/performance: license audit, quality tiers, dedicated-GPU 1080p/60 FPS target.

Every stage uses a feature branch, automated checks, a Windows milestone build, a pull request, and explicit approval before merge.
```

- [ ] **Step 3: 写入 Pull Request 模板**

创建 `.github/pull_request_template.md`：

```markdown
## Purpose

Describe one bounded change and link its approved design or implementation plan.

## Validation

- [ ] Repository baseline test passes
- [ ] Development environment test passes when toolchain files change
- [ ] UrbanSpearEditor Development compiles
- [ ] UrbanSpear.Foundation automation tests pass
- [ ] Windows Development package and packaged executable smoke test pass
- [ ] Git LFS and third-party asset registry were checked
- [ ] No generated files, credentials, receipts, or local absolute paths were committed

## Evidence

Record exact commands, pass counts, build configuration, and tested commit SHA.
```

- [ ] **Step 4: 运行全部门禁**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-RepositoryBaseline.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-DevelopmentEnvironment.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-LyraImport.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-ProjectIdentity.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-FoundationScaffold.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Invoke-UrbanBuild.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Invoke-UrbanAutomation.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Build-WindowsDevelopment.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-PackagedBuild.ps1
```

Expected: 五个静态/环境检查 `PASS`；Editor 编译成功；2+ 自动化测试成功且 0 失败；Windows 打包和启动测试 `PASS`。

- [ ] **Step 5: 扫描禁止提交的路径**

```powershell
$forbidden = git ls-files | Where-Object { $_ -match '(^|/)(Binaries|DerivedDataCache|Intermediate|Saved|Artifacts|\.vs)/' }
if ($forbidden) { $forbidden; exit 1 }
'PASS: no generated paths are tracked.'
```

Expected: 输出 `PASS: no generated paths are tracked.`。

- [ ] **Step 6: 提交最终文档**

```powershell
git add README.md docs/roadmap/implementation-sequence.md .github/pull_request_template.md
git commit -m "docs: add foundation build and review guide"
```

- [ ] **Step 7: 推送实施分支**

在 `feat/urban-spear-foundation` 分支运行：

```powershell
git status --short
git push -u origin feat/urban-spear-foundation
```

Expected: 推送前工作区干净；远端分支创建成功。

- [ ] **Step 8: 创建实施 Pull Request**

运行：

```powershell
$prBody = Join-Path $env:TEMP 'urban-spear-foundation-pr.md'
@"
## Summary
- install and verify UE 5.8 / VS 2026 / Lyra
- import Lyra with Git LFS and private-repository safeguards
- add UrbanSpear targets and seven Urban module boundaries
- add automation and Windows package smoke tests

## Validation
- repository, environment, import, identity, scaffold: PASS
- UrbanSpearEditor Win64 Development: PASS
- UrbanSpear.Foundation: 2+ passed, 0 failed
- Windows BuildCookRun and packaged startup: PASS

## Approval
Do not merge until the project owner approves this PR.
"@ | Set-Content $prBody -Encoding UTF8
gh pr create --base main --head feat/urban-spear-foundation --title "feat: establish Urban Spear Unreal foundation" --body-file $prBody
```

Expected: GitHub 返回 PR 地址；目标 `main`，来源 `feat/urban-spear-foundation`，保持未合并等待批准。

---

## 完成定义

1. 仓库为私有，授权源素材未暴露；
2. VS 2026、Windows SDK、UE 5.8、Lyra 5.8、Git LFS 和磁盘检查通过；
3. `UrbanSpear.uproject`、UrbanSpear Game/Editor Target 有效；
4. 七个 Urban Runtime 模块和测试模块可编译；
5. 至少两个基础自动化测试通过且 0 失败；
6. Windows 64 位 Development 包成功生成；
7. 打包程序无界面启动并以退出码 0 结束；
8. 当前电脑可在 720p/低画质进入前端；
9. README、构建说明、素材登记、路线图和 PR 模板完整；
10. 实施分支已推送并创建 PR，未经用户批准不合并。
