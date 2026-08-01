# Character and Camera Milestone Verification

This document records automated and manual verification for the Urban Spear first-person/third-person character-camera foundation. The test target is Windows 64-bit at 1280x720 Low on the current Intel Iris Xe development machine. It is a functional target, not a final 3A visual-performance claim.

## Test content

- Map: `/UrbanFoundation/Maps/L_UrbanCharacterCameraTest`
- Experience: `/UrbanFoundation/Experiences/B_UrbanCharacterCameraExperience`
- Pawn: `/UrbanFoundation/Characters/BP_UrbanTestPawn`
- Camera data: `/UrbanFoundation/Camera/DA_UrbanCamera_Default`
- Perspective switch: `V`
- Shoulder switch: `Q`

The route contains spawn and movement areas, a crouch obstacle, aiming/sprint/traversal/downed restrictions, a forced-first-person station, a death/reset station, and a wall for camera pull-in and muzzle-obstruction checks.

## Rebuild and verify the editor content

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Invoke-UrbanBuild.ps1 -Target UrbanSpearEditor -Configuration Development -EngineRoot "C:\Program Files\Epic Games\UE_5.8"

$root = (Resolve-Path .).Path
$editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
& $editor (Join-Path $root "UrbanSpear.uproject") "/UrbanFoundation/Maps/L_UrbanCharacterCameraTest" -unattended -nop4 -nosplash -NullRHI "-ExecutePythonScript=$(Join-Path $root 'Build\Editor\Configure-UrbanCharacterCameraMap.py')" -log
& $editor (Join-Path $root "UrbanSpear.uproject") "/UrbanFoundation/Maps/L_UrbanCharacterCameraTest" -unattended -nop4 -nosplash -NullRHI "-ExecutePythonScript=$(Join-Path $root 'Build\Editor\Verify-UrbanCharacterCameraAssets.py')" -log
```

Run the configure command twice when validating idempotency. Inspect the produced Unreal log for `LogPython: Error` and `Traceback`; the editor command can return zero even when a Python script fails.

## Automated verification

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-RepositoryBaseline.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Invoke-UrbanAutomation.ps1 -EngineRoot "C:\Program Files\Epic Games\UE_5.8"
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-CharacterCameraMilestone.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Build-WindowsDevelopment.ps1 -EngineRoot "C:\Program Files\Epic Games\UE_5.8"
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-PackagedBuild.ps1
```

### Current automated evidence — July 31, 2026

| Check | Status | Evidence |
| --- | --- | --- |
| UrbanSpearEditor Win64 Development | Pass | Unreal Build Tool reported `Result: Succeeded`. |
| Test-station integration automation | Pass | 1 succeeded, 0 failed, 0 not run, 0 in process. |
| Test-map configuration | Pass | Configuration script completed twice with no Python error or traceback. |
| Pawn, PawnData, experience, map route, semantic tags, and six station modes | Pass | Asset verification script emitted its PASS marker. |
| Full CharacterCamera automation suite | Pass | 12 succeeded, 0 failed, 0 not run, 0 in process, including delayed Lyra input binding. |
| Foundation automation suite | Pass | 2 required smoke tests succeeded. |
| Repository safeguards | Pass | Privacy, ignore rules, Git LFS, and asset registry gate passed. |
| Windows Development package | Pass | BuildCookRun completed successfully in 5 minutes 29 seconds and archived the Win64 Development package. |
| Packaged executable smoke | Pass | The fresh packaged executable started and exited cleanly; the explicit character-camera test-map launch also created its Development game window. |

## Manual Windows acceptance

Do not change a status to Pass unless the behavior was observed in the packaged Windows build at 1280x720 Low.

| Manual check | Status |
| --- | --- |
| Character spawns and can walk, turn, and crouch | Pending |
| `V` switches between first and third person while allowed | Pending |
| `Q` switches left/right shoulder in third person | Pending |
| Crouching still permits perspective switching | Pending |
| Aiming, sprinting, traversal, downed, and death states reject switching | Pending |
| Camera pulls in against the camera wall | Pending |
| First-person head/body visibility is correct, including looking down | Pending |
| Forced-first-person station applies and restores policy | Pending |
| Death/reset station respawns and restores a legal perspective | Pending |
| Muzzle-obstruction debug check does not pass through the wall | Pending |

### Manual test blocker

The packaged game was launched again for visual acceptance on August 1, 2026. Windows reported open desktop applications, but the captured game surface still showed the 9:25 lock screen and the message asking that the lock screen be closed before login. The game window therefore could not be observed or controlled honestly; all visual and input checks remain Pending until the Windows session is fully signed in to the desktop.

## Completion rule

Stage 2 is complete only after every automated gate passes, the packaged executable starts and exits cleanly, and the manual table contains recorded observations. Generated build directories and local reports must remain untracked.
