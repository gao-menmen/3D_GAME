# Windows Development Build

Run these commands from the repository root in Windows PowerShell 5.1. Complete the environment, compile, and automation checks before packaging:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-DevelopmentEnvironment.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Invoke-UrbanBuild.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Invoke-UrbanAutomation.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Build-WindowsDevelopment.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\Build\Scripts\Test-PackagedBuild.ps1
```

`Build-WindowsDevelopment.ps1` always archives to `Artifacts/Windows-Development` under the resolved repository root. It removes only that fixed directory after rejecting reparse points, records build provenance, and requires the packaged launcher at `Artifacts/Windows-Development/UrbanSpear.exe`. Unreal also places the larger runtime binary under `UrbanSpear/Binaries/Win64`; both files are expected. `Artifacts/` is ignored and must not be committed.

`Test-PackagedBuild.ps1` accepts only a recent package produced for the same repository and fixed archive. The default maximum age is 240 minutes and can be adjusted for a deliberate delayed test, for example `-MaxBuildAgeMinutes 480`. It launches the archive-root launcher without a visible window, waits up to 90 seconds, checks its exit code, and terminates it on timeout.

## Manual 720p functionality check

The integrated-GPU workstation is approved only for a 720p, low-quality **functionality check**. This is not final visual-quality, Lumen/Nanite, or 1080p/60 FPS acceptance.

Use the archive-root launcher (not the nested runtime binary):

```powershell
$exe = Get-Item -LiteralPath .\Artifacts\Windows-Development\UrbanSpear.exe -ErrorAction Stop
if ($exe -isnot [System.IO.FileInfo]) { throw "Packaged launcher is not a file: $($exe.FullName)" }
```

Then start it visibly at low settings:

```powershell
$manualProcess = Start-Process -FilePath $exe.FullName -ArgumentList @(
  '-windowed', '-ResX=1280', '-ResY=720',
  '-sg.ViewDistanceQuality=0', '-sg.AntiAliasingQuality=0', '-sg.ShadowQuality=0',
  '-sg.GlobalIlluminationQuality=0', '-sg.ReflectionQuality=0', '-sg.PostProcessQuality=0',
  '-sg.TextureQuality=0', '-sg.EffectsQuality=0', '-sg.FoliageQuality=0'
) -PassThru
```

Confirm that the frontend is visible, keyboard and mouse input respond, and no crash occurs. When the check is complete, exit the game normally and confirm `$manualProcess.HasExited`. Do not automate this visible check and do not intentionally leave the process running in the background.
