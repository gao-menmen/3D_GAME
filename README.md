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
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-LyraImport.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-ProjectIdentity.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-FoundationScaffold.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Invoke-UrbanBuild.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Invoke-UrbanAutomation.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Build-WindowsDevelopment.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-PackagedBuild.ps1
```

The repository remains private while it contains licensed source payload. Unreal binary assets use Git LFS. Generated folders, builds, secrets, and local settings are excluded from Git.
