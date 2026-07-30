# Lyra Baseline Architecture

Project Urban Spear imports Lyra Starter Game 5.8 as its upstream foundation.

The project descriptor is `UrbanSpear.uproject`. Lyra's `LyraGame` and `LyraEditor` modules, targets, plugins, configuration, and asset paths retain their upstream names. The Fab 5.8 sample uses `Lyra.uproject` as its source descriptor and `LyraEditor.Target.cs` as its editor target; the import scripts preserve those real upstream names.

- Upstream: `Source/LyraGame`, `Source/LyraEditor`, Lyra targets, Lyra plugins, configuration, and Lyra content.
- Urban Spear: future `Source/UrbanSpear*.Target.cs` targets and the `Plugins/UrbanFoundation` plugin.
- Generated: `Binaries`, `DerivedDataCache`, `Intermediate`, `Saved`, `.vs`, and `Artifacts`; never imported or committed.

Engine or Lyra upgrades occur on isolated branches. Urban-owned files are not inserted into Lyra source folders. The repository remains private while it contains Epic/Fab source content, and Unreal binary assets are stored through Git LFS.
