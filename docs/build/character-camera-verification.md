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

### Current automated evidence - August 2, 2026

| Check | Status | Evidence |
| --- | --- | --- |
| UrbanSpearEditor Win64 Development | Pass | Unreal Build Tool reported `Result: Succeeded`. |
| First-person dynamic-cosmetic visibility regression | Pass | `UrbanSpear.CharacterCamera.Presentation.OwnerVisibility`: 1 succeeded, 0 failed, 0 warnings. Report: `Saved/Automation/PresentationOwnerVisibility-VisibilityOwner-Green-20260802-2136/index.json`. |
| Full CharacterCamera automation suite | Pass | 13 succeeded, 0 failed, 0 not run, 0 in process. Report: `Saved/Automation/CharacterCamera-VisibilityOwner-Green-20260802-215955/index.json`. |
| Character-camera milestone gate | Pass | Manifest paths, dependencies, repository state, and all 13 required automation tests passed. |
| Foundation automation suite | Pass | 2 required smoke tests succeeded. Report: `Saved/Automation/Foundation/index.json`. |
| Repository safeguards | Pass | Privacy, ignore rules, Git LFS, and asset registry gate passed in the final verification run. |
| Windows Development package | Pass | BuildCookRun completed successfully and archived `Artifacts/Windows-Development/UrbanSpear.exe`. |
| Packaged executable smoke | Pass | The packaged executable started and exited cleanly. Its log contains no missing Urban input assets, `Error:`, fatal error, or assertion; only optional performance-analysis DLL warnings were observed. |

## Manual Windows acceptance

Do not change a status to Pass unless the behavior was observed in the packaged Windows build at 1280x720 Low.

| Manual check | Status | Observation |
| --- | --- | --- |
| Character spawns and the third-person body is visible | Pass | The packaged build spawned the player with the full cosmetic body visible in third person. |
| Walking and turning remain functional | Pending | Not separately recorded during this acceptance pass. |
| Crouch enters and exits correctly | Pass | `Ctrl` visibly changed between standing and crouched third-person poses. |
| `V` switches between first and third person while allowed | Pass | Both directions were observed in the packaged build. |
| `Q` switches left/right shoulder in third person | Pass | The player body moved from the left side of the frame to the right side after `Q`. |
| Crouching still permits perspective switching | Pass | While crouched, `V` changed from third person to first person and back without losing the crouched pose. |
| Aiming, sprinting, traversal, downed, and death states reject switching | Pending | State-policy stations were not completed in this pass. |
| Camera pulls in against the camera wall | Pending | The dedicated camera-wall check was not completed in this pass. |
| First-person head/body visibility is correct, including looking down | Pass | Looking down showed the weapon and ground without the player's full-body cosmetic obscuring the right side of the view. |
| Forced-first-person station applies and restores policy | Pending | The dedicated station was not completed in this pass. |
| Death/reset station respawns and restores a legal perspective | Pending | Bot-driven respawn occurred, but the dedicated death/reset station was not completed. |
| Muzzle-obstruction debug check does not pass through the wall | Pending | The dedicated wall check was not completed in this pass. |

### Manual acceptance notes - August 2, 2026

The packaged Windows Development build was tested at 1280x720 Low on `/UrbanFoundation/Maps/L_UrbanCharacterCameraTest`. The previous Windows lock-screen blocker no longer applied. `god` was enabled after respawn to keep bot fire from interrupting the crouch and perspective checks.

The test map still displays Lyra sample-content script/audio warnings, including `B_WeaponInstance_Base`, `W_RespawnTimer`, and the optional `Urban.FirstPersonArms` tag. These warnings did not prevent the recorded character-camera checks, but they remain visible test-content noise and are not marked as resolved by this milestone.

## Completion rule

Stage 2 is complete only after every automated gate passes, the packaged executable starts and exits cleanly, and the manual table contains recorded observations. Generated build directories and local reports must remain untracked.
