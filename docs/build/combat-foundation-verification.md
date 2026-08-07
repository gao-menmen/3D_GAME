# Combat foundation verification

- Date: August 6, 2026
- Branch: `feat/combat-foundation`
- Scope: combat rules foundation; playable ShooterCore integration is the next increment.

## Delivered architecture

- `FUrbanWeaponTuning` defines the assault rifle, submachine gun, and tactical pistol parameters, supported fire modes, spread, recoil, range, damage, magazine, and reserve ammunition.
- `FUrbanAmmoState` provides deterministic fire, reload, partial reload, cancel, and reserve-transfer rules.
- `UUrbanAmmoComponent` owns replicated ammunition state and rejects mutation without server authority.
- `FUrbanDamageProfile` and `UUrbanDamageModel` calculate head, torso, and limb damage, torso armor absorption, armor break-through, lethal clamping, and invalid negative input handling.
- `FUrbanTacticalItemTuning` defines the frag grenade, smoke grenade, medkit, and time/range-limited recon drone.
- Weapon and tactical item definitions are exposed as data assets so later content tuning does not require changing rule code.

## Automated verification

Editor Development compilation passed on August 6, 2026. The full `UrbanSpear` regression suite passed 28/28 tests. A fresh Windows Development package was created and its executable passed the unattended launch/exit smoke test.

The `UrbanSpear.Combat` automation suite passed 9/9 tests with zero errors and zero warnings:

1. Ammo component authority/replication contract.
2. Partial reload and cancellation.
3. Ammo state lifecycle.
4. Four tactical item catalog entries.
5. Three weapon archetype catalog entries.
6. Weapon tuning sanitization.
7. Head/torso/limb damage multipliers.
8. Lethal damage and negative-input clamping.
9. Torso armor absorption and break-through.

Report: `Saved/Automation/Combat-Development/index.json` (generated locally and not tracked).

## Remaining combat work

- Bind the new rules to Lyra/ShooterCore weapon instances and gameplay abilities.
- Create three project-owned weapon data assets and equip them on the test pawn.
- Add fire-mode switching, reload timing, recoil presentation, hit-region extraction, HUD ammunition/armor display, tactical-item gameplay, and a dedicated combat test route.
- Produce and smoke-test the next Windows Development package after playable integration.
