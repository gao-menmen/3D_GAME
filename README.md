# Project Urban Spear

Project Urban Spear is a Windows 64-bit near-future tactical shooter built with Unreal Engine 5.8 and the Lyra Starter Game architecture.

## Current milestone

The current development branch starts the combat foundation after the completed character-camera milestone. It adds data-driven defaults for three weapon archetypes and four tactical items, deterministic ammunition and reload rules, a replicated server-authoritative ammo component, hit-region damage, torso armor absorption, and 9 combat automation tests. Playable ShooterCore weapon integration is the next combat increment.

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

## Character and camera milestone

- [Design specification](docs/superpowers/specs/2026-07-31-urban-spear-character-camera-design.md)
- [Implementation plan](docs/superpowers/plans/2026-07-31-urban-spear-character-camera.md)
- [Build and acceptance record](docs/build/character-camera-verification.md)
- Test map: `/UrbanFoundation/Maps/L_UrbanCharacterCameraTest`
- Controls: `V` perspective, `Q` shoulder

Run the character-camera gate after the automation suite:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-CharacterCameraMilestone.ps1
```
## Combat foundation milestone

- [Build and verification record](docs/build/combat-foundation-verification.md)
- Three weapon archetypes: assault rifle, submachine gun, tactical pistol
- Four tactical items: frag grenade, smoke grenade, medkit, recon drone
- Rules: server-authoritative ammo mutation, reload lifecycle, hit regions, torso armor

Run the combat foundation gate after the `UrbanSpear.Combat` automation suite:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-CombatFoundationMilestone.ps1
```
