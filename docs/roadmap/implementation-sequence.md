# Urban Spear Implementation Sequence

1. Foundation: UE 5.8, Lyra, modules, automation, Windows package.
2. Character and camera: shared first-person/third-person state and restrictions — automated gates and Windows package pass; manual packaged-game acceptance pending.
3. Combat: three weapons, ammunition, damage, armor, and four tactical items — core data, ammo, damage, armor, and automated rules are in progress; playable integration remains.
4. AI: four enemy classes, perception, cover, search, flanking, reinforcement.
5. Mission and save: objectives, extraction, failure, checkpoint, save, difficulty.
6. City blockout: checkpoint, garage, commercial center, metro, rooftop.
7. UI/audio/accessibility: menu, HUD, settings, subtitles, audio, readable errors.
8. Release/performance: license audit, quality tiers, dedicated-GPU 1080p/60 FPS target.

Every stage uses a feature branch, automated checks, a Windows milestone build, a pull request, and explicit approval before merge.
