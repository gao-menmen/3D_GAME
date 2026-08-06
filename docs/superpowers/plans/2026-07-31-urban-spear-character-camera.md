# Urban Spear Character and Camera Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a server-authoritative first-person/third-person character camera foundation on Lyra 5.8, including shoulder switching, state restrictions, camera collision, configuration, and automated Windows verification.

**Architecture:** Keep Lyra source unchanged. Put rules, replicated state, camera mode, and integration components in `UrbanCore`; use a single Urban Lyra default camera mode that reads the replicated perspective component, so Lyra ability camera overrides remain compatible. Separate pure decision logic from UObject components so restrictions and policies can be tested without loading a map.

**Tech Stack:** Unreal Engine 5.8, Lyra Starter Game 5.8, C++20 through Unreal Build Tool, Gameplay Tags, Gameplay Ability System integration, Enhanced Input, Unreal Automation Tests, PowerShell build gates.

---

## File map

- `Plugins/UrbanFoundation/Source/UrbanCore/Public/Character/UrbanPerspectiveTypes.h`: enums, state snapshot, decision result, and pure rules API.
- `Plugins/UrbanFoundation/Source/UrbanCore/Private/Character/UrbanPerspectiveTypes.cpp`: policy and state transition evaluator.
- `Plugins/UrbanFoundation/Source/UrbanCore/Public/Character/UrbanCharacterStateComponent.h`: pawn component exposing view-relevant character state.
- `Plugins/UrbanFoundation/Source/UrbanCore/Private/Character/UrbanCharacterStateComponent.cpp`: Gameplay Tag observation and snapshot production.
- `Plugins/UrbanFoundation/Source/UrbanCore/Public/Character/UrbanViewPolicyComponent.h`: replicated game-mode view policy.
- `Plugins/UrbanFoundation/Source/UrbanCore/Private/Character/UrbanViewPolicyComponent.cpp`: authority checks and policy replication.
- `Plugins/UrbanFoundation/Source/UrbanCore/Public/Character/UrbanPerspectiveComponent.h`: accepted perspective, saved preference, shoulder selection, RPCs, and delegates.
- `Plugins/UrbanFoundation/Source/UrbanCore/Private/Character/UrbanPerspectiveComponent.cpp`: server validation and transition lifecycle.
- `Plugins/UrbanFoundation/Source/UrbanCore/Public/Camera/UrbanCameraSettings.h`: validated camera tuning values.
- `Plugins/UrbanFoundation/Source/UrbanCore/Private/Camera/UrbanCameraSettings.cpp`: safe defaults and value clamping.
- `Plugins/UrbanFoundation/Source/UrbanCore/Public/Camera/LyraCameraMode_UrbanPerspective.h`: Lyra-compatible combined camera mode.
- `Plugins/UrbanFoundation/Source/UrbanCore/Private/Camera/LyraCameraMode_UrbanPerspective.cpp`: first-person, shoulder offset, interpolation, and Lyra penetration avoidance.
- `Plugins/UrbanFoundation/Source/UrbanCore/Public/Input/UrbanPerspectiveInputComponent.h`: Enhanced Input bindings for perspective and shoulder requests.
- `Plugins/UrbanFoundation/Source/UrbanCore/Private/Input/UrbanPerspectiveInputComponent.cpp`: local input forwarding.
- `Plugins/UrbanFoundation/Source/UrbanCore/Public/Character/UrbanPerspectivePresentationComponent.h`: owner mesh/arms visibility and muzzle-obstruction query.
- `Plugins/UrbanFoundation/Source/UrbanCore/Private/Character/UrbanPerspectivePresentationComponent.cpp`: local visibility application and obstruction trace.
- `Plugins/UrbanFoundation/Source/UrbanFoundationTests/Private/UrbanPerspectiveRulesTests.cpp`: pure rule tests.
- `Plugins/UrbanFoundation/Source/UrbanFoundationTests/Private/UrbanPerspectiveComponentTests.cpp`: component and replication-oriented automation tests.
- `Plugins/UrbanFoundation/Source/UrbanFoundationTests/Private/UrbanCameraSettingsTests.cpp`: configuration fallback tests.
- `Build/Manifests/CharacterCameraMilestone.json`: exact source and test inventory for static validation.
- `Build/Scripts/Test-CharacterCameraMilestone.ps1`: milestone file, dependency, generated-output, and test-report gate.
- `docs/build/character-camera-verification.md`: build, automation, package, and manual test instructions.

### Task 1: Add pure perspective rules with red-green automation tests

**Files:**
- Create: `Plugins/UrbanFoundation/Source/UrbanCore/Public/Character/UrbanPerspectiveTypes.h`
- Create: `Plugins/UrbanFoundation/Source/UrbanCore/Private/Character/UrbanPerspectiveTypes.cpp`
- Create: `Plugins/UrbanFoundation/Source/UrbanFoundationTests/Private/UrbanPerspectiveRulesTests.cpp`
- Modify: `Plugins/UrbanFoundation/Source/UrbanCore/UrbanCore.Build.cs`

- [ ] **Step 1: Expand UrbanCore dependencies**

Set public dependencies to `Core`, `CoreUObject`, `Engine`, `GameplayTags`, `ModularGameplay`, and `LyraGame`; set private dependencies to `GameplayAbilities`, `NetCore`, and `EnhancedInput`. Run the Editor build and expect `Result: Succeeded`.

- [ ] **Step 2: Write failing policy and restriction tests**

Create automation cases under `UrbanSpear.CharacterCamera.Rules` that construct `FUrbanCharacterViewState` and verify:

```cpp
TestEqual(TEXT("free choice accepts third person"),
    FUrbanPerspectiveRules::Evaluate(EUrbanPerspective::FirstPerson, EUrbanPerspective::ThirdPersonRight,
        EUrbanViewPolicy::FreeChoice, FUrbanCharacterViewState()).AcceptedPerspective,
    EUrbanPerspective::ThirdPersonRight);

FUrbanCharacterViewState AimingState;
AimingState.bAiming = true;
TestEqual(TEXT("aiming rejects change"),
    FUrbanPerspectiveRules::Evaluate(EUrbanPerspective::FirstPerson, EUrbanPerspective::ThirdPersonRight,
        EUrbanViewPolicy::FreeChoice, AimingState).Reason,
    EUrbanPerspectiveBlockReason::Aiming);

TestEqual(TEXT("server policy forces first person"),
    FUrbanPerspectiveRules::Evaluate(EUrbanPerspective::ThirdPersonRight, EUrbanPerspective::ThirdPersonLeft,
        EUrbanViewPolicy::FirstPersonOnly, FUrbanCharacterViewState()).AcceptedPerspective,
    EUrbanPerspective::ForcedFirstPerson);
```

Run `Build/Scripts/Invoke-UrbanBuild.ps1` and expect compilation to fail because the Urban perspective types do not exist.

- [ ] **Step 3: Implement the minimal pure model**

Define `UENUM(BlueprintType)` values `FirstPerson`, `ThirdPersonRight`, `ThirdPersonLeft`, `ForcedFirstPerson`; policy values `FreeChoice`, `FirstPersonOnly`, `ThirdPersonOnly`, `Disabled`; block reasons `None`, `Aiming`, `Sprinting`, `Traversal`, `Downed`, `Dead`, `Transitioning`, `Policy`, `Uninitialized`, `InvalidRequest`. Define a `USTRUCT(BlueprintType) FUrbanCharacterViewState` containing the six approved restriction booleans and a transition boolean. Implement `FUrbanPerspectiveRules::Evaluate` with priority: invalid request, uninitialized, server policy, dead, downed, traversal, sprinting, aiming, transition, accept.

- [ ] **Step 4: Run build and focused automation**

Run:

```powershell
& .\Build\Scripts\Invoke-UrbanBuild.ps1 -Target UrbanSpearEditor -Configuration Development -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
& .\Build\Scripts\Invoke-UrbanAutomation.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
```

Expected: Editor build succeeds and the automation report includes the original two foundation tests plus the new rules tests with zero failures.

- [ ] **Step 5: Commit**

Commit message: `feat: add perspective policy rules`.

### Task 2: Add character-state and server policy components

**Files:**
- Create: `Plugins/UrbanFoundation/Source/UrbanCore/Public/Character/UrbanCharacterStateComponent.h`
- Create: `Plugins/UrbanFoundation/Source/UrbanCore/Private/Character/UrbanCharacterStateComponent.cpp`
- Create: `Plugins/UrbanFoundation/Source/UrbanCore/Public/Character/UrbanViewPolicyComponent.h`
- Create: `Plugins/UrbanFoundation/Source/UrbanCore/Private/Character/UrbanViewPolicyComponent.cpp`
- Modify: `Plugins/UrbanFoundation/Source/UrbanFoundationTests/Private/UrbanPerspectiveComponentTests.cpp`

- [ ] **Step 1: Write failing component tests**

Create transient pawns with `NewObject<UUrbanCharacterStateComponent>` and `NewObject<UUrbanViewPolicyComponent>`. Verify default snapshot is initialized and unrestricted; applying the tags `Status.Aiming`, `Status.Sprinting`, `Status.Traversal`, `Status.Downed`, and `Status.Death` sets the corresponding snapshot fields; verify a non-authority policy mutation returns false.

- [ ] **Step 2: Verify the tests fail to compile**

Run the Editor build. Expected: missing component class errors.

- [ ] **Step 3: Implement character-state observation**

Derive `UUrbanCharacterStateComponent` from `UPawnComponent`. Cache the pawn ability system through `ULyraAbilitySystemComponent`; register tag-count delegates during `BeginPlay`; unregister in `EndPlay`; expose `FUrbanCharacterViewState GetViewState() const`. Use native tags defined in the `.cpp` for the five approved restrictions, while crouch and normal movement remain allowed states.

- [ ] **Step 4: Implement policy authority**

Derive `UUrbanViewPolicyComponent` from `UActorComponent`, set replication by default, replicate `EUrbanViewPolicy Policy` with `OnRep_Policy`, and implement `bool SetPolicy(EUrbanViewPolicy NewPolicy)` that only mutates when `GetOwner()->HasAuthority()` is true. Broadcast `OnPolicyChanged` after authority changes and replication.

- [ ] **Step 5: Build, run automation, and commit**

Expected: zero automation failures. Commit message: `feat: add character view state and policy components`.

### Task 3: Add the replicated perspective state machine

**Files:**
- Create: `Plugins/UrbanFoundation/Source/UrbanCore/Public/Character/UrbanPerspectiveComponent.h`
- Create: `Plugins/UrbanFoundation/Source/UrbanCore/Private/Character/UrbanPerspectiveComponent.cpp`
- Modify: `Plugins/UrbanFoundation/Source/UrbanFoundationTests/Private/UrbanPerspectiveComponentTests.cpp`

- [ ] **Step 1: Write failing state-machine tests**

Verify default accepted and preferred states are `FirstPerson`; a legal request changes both accepted and preferred values; forced first person changes only accepted state; ending the force restores the saved preference; a rejected aiming request is not replayed after aiming clears; shoulder requests only work from third person.

- [ ] **Step 2: Verify red state**

Run the Editor build and expect missing `UUrbanPerspectiveComponent` symbols.

- [ ] **Step 3: Implement authoritative requests**

Derive from `UPawnComponent`. Replicate `AcceptedPerspective` owner-only and expose `ServerRequestPerspective(EUrbanPerspective Requested)` as a reliable server RPC. Resolve `UUrbanCharacterStateComponent` and `UUrbanViewPolicyComponent`; call the pure evaluator; update `PreferredPerspective` only for accepted free-choice requests; never queue a rejected request. Use `GetLifetimeReplicatedProps` and `DOREPLIFETIME_CONDITION`.

- [ ] **Step 4: Implement transition and respawn hooks**

Expose `BeginTransition`, `FinishTransition`, `RestoreAfterRespawn`, `RequestTogglePerspective`, and `RequestToggleShoulder`. During a transition, the state snapshot reports `bTransitioning=true`. A server policy change immediately reevaluates accepted state and can interrupt transition.

- [ ] **Step 5: Run tests and commit**

Expected: rules and component tests pass. Commit message: `feat: add replicated perspective state machine`.

### Task 4: Add validated camera settings and the combined Lyra camera mode

**Files:**
- Create: `Plugins/UrbanFoundation/Source/UrbanCore/Public/Camera/UrbanCameraSettings.h`
- Create: `Plugins/UrbanFoundation/Source/UrbanCore/Private/Camera/UrbanCameraSettings.cpp`
- Create: `Plugins/UrbanFoundation/Source/UrbanCore/Public/Camera/LyraCameraMode_UrbanPerspective.h`
- Create: `Plugins/UrbanFoundation/Source/UrbanCore/Private/Camera/LyraCameraMode_UrbanPerspective.cpp`
- Create: `Plugins/UrbanFoundation/Source/UrbanFoundationTests/Private/UrbanCameraSettingsTests.cpp`

- [ ] **Step 1: Write failing settings tests**

Verify defaults: first-person FOV `90`, third-person FOV `85`, right shoulder Y `55`, left shoulder Y `-55`, transition `0.2`; verify invalid FOV, distance, collision radius, and transition values clamp to documented safe ranges.

- [ ] **Step 2: Implement `FUrbanCameraSettings`**

Use a `USTRUCT(BlueprintType)` with safe constructor defaults and `Sanitize()` using `FMath::Clamp`: FOV `60..120`, distance `80..450`, collision radius `4..30`, transition `0.15..0.30`.

- [ ] **Step 3: Implement a single Lyra default camera mode**

Derive `ULyraCameraMode_UrbanPerspective` from `ULyraCameraMode_ThirdPerson`. In `UpdateView`, read `UUrbanPerspectiveComponent` from `GetTargetActor()`. For third person, select signed shoulder Y and call the base third-person view/collision path. For first person, build the view from pawn eye location and control rotation with first-person FOV. Interpolate location and FOV over the sanitized transition time. Do not change `ULyraHeroComponent`; the Urban pawn data will use this class as its default mode, preserving Lyra ability camera overrides.

- [ ] **Step 4: Build, automate, and commit**

Expected: settings tests pass and existing Lyra camera code remains unmodified. Commit message: `feat: add urban first and third person camera mode`.

### Task 5: Bind perspective and shoulder input without hard-coded character logic

**Files:**
- Create: `Plugins/UrbanFoundation/Source/UrbanCore/Public/Input/UrbanPerspectiveInputComponent.h`
- Create: `Plugins/UrbanFoundation/Source/UrbanCore/Private/Input/UrbanPerspectiveInputComponent.cpp`
- Modify: `Plugins/UrbanFoundation/UrbanFoundation.uplugin`
- Create through Unreal Editor: `Plugins/UrbanFoundation/Content/Input/Actions/IA_UrbanTogglePerspective.uasset`
- Create through Unreal Editor: `Plugins/UrbanFoundation/Content/Input/Actions/IA_UrbanToggleShoulder.uasset`
- Create through Unreal Editor: `Plugins/UrbanFoundation/Content/Input/Mappings/IMC_UrbanPerspective_KBM.uasset`

- [ ] **Step 1: Enable plugin content and write input binding test**

Set `CanContainContent` to `true`. Add an automation assertion that the three package paths exist and that the mapping context binds `V` to perspective and `Q` to shoulder.

- [ ] **Step 2: Verify missing assets fail**

Run automation and expect the input package test to fail before asset creation.

- [ ] **Step 3: Implement the input component**

Derive from `UPawnComponent`. On local player input readiness, add the mapping context through `UEnhancedInputLocalPlayerSubsystem`; bind Triggered events to `RequestTogglePerspective` and `RequestToggleShoulder`; remove bindings and mapping context in `EndPlay`.

- [ ] **Step 4: Create assets using an unattended Unreal Editor utility script**

Add `Build/Editor/Create-UrbanPerspectiveInputAssets.py`, run UnrealEditor with `-ExecutePythonScript`, save the two boolean input actions and mapping context, then remove only the temporary editor script if the generated asset registry test is sufficient. Keep the reproducible script if asset regeneration is required by review.

- [ ] **Step 5: Automate and commit**

Expected: the asset package test passes. Commit message: `feat: add perspective input actions`.

### Task 6: Apply first-person presentation and muzzle-obstruction boundaries

**Files:**
- Create: `Plugins/UrbanFoundation/Source/UrbanCore/Public/Character/UrbanPerspectivePresentationComponent.h`
- Create: `Plugins/UrbanFoundation/Source/UrbanCore/Private/Character/UrbanPerspectivePresentationComponent.cpp`
- Modify: `Plugins/UrbanFoundation/Source/UrbanFoundationTests/Private/UrbanPerspectiveComponentTests.cpp`

- [ ] **Step 1: Write failing presentation tests**

Use test skeletal mesh components tagged `Urban.WorldBody`, `Urban.FirstPersonArms`, and `Urban.FirstPersonWeapon`. Verify first person hides owner head/upper-body presentation and shows first-person arms; third person restores world presentation. Verify an obstruction trace between camera aim point and muzzle reports a blocking wall.

- [ ] **Step 2: Implement local-only visibility**

Subscribe to accepted-perspective changes. Apply owner-only visibility flags and hidden bone configuration without disabling collision, replication, shadow, or non-owner world mesh rendering. Missing optional arms produce a warning once and do not block character spawn.

- [ ] **Step 3: Implement obstruction query**

Expose `FUrbanMuzzleObstructionResult TraceMuzzleToAim(const FVector& Muzzle, const FVector& AimPoint) const`. Use a visibility channel trace ignoring the owner. Return `bObstructed`, impact point, and hit actor; do not spawn projectiles or apply damage in UrbanCore.

- [ ] **Step 4: Build, test, and commit**

Commit message: `feat: add perspective presentation boundaries`.

### Task 7: Add milestone gates and playable integration assets

**Files:**
- Create: `Build/Manifests/CharacterCameraMilestone.json`
- Create: `Build/Scripts/Test-CharacterCameraMilestone.ps1`
- Create through Unreal Editor: `Plugins/UrbanFoundation/Content/Characters/BP_UrbanTestPawn.uasset`
- Create through Unreal Editor: `Plugins/UrbanFoundation/Content/Camera/DA_UrbanCamera_Default.uasset`
- Create through Unreal Editor: `Plugins/UrbanFoundation/Content/Experiences/B_UrbanCharacterCameraExperience.uasset`
- Create through Unreal Editor: `Plugins/UrbanFoundation/Content/Maps/L_UrbanCharacterCameraTest.umap`
- Create: `docs/build/character-camera-verification.md`

- [ ] **Step 1: Write the failing static milestone gate**

The PowerShell gate must fail if required source files or assets are missing, plugin content is disabled, generated directories are tracked, UrbanCore dependencies are absent, or the latest automation report lacks every `UrbanSpear.CharacterCamera` test.

- [ ] **Step 2: Create the test pawn and experience**

Use the Lyra pawn as base, add Character State, View Policy, Perspective, Perspective Input, and Presentation components, and set `ULyraCameraMode_UrbanPerspective` as the pawn data default camera mode. Configure the experience to spawn the test pawn in the test map.

- [ ] **Step 3: Build the wall/corridor test map**

Include a spawn area, open movement lane, crouch obstacle, sprint lane, traversal trigger, death/reset trigger, and a wall positioned to validate camera pull-in and muzzle obstruction. Use only project-owned or Lyra-licensed assets already covered by repository safeguards.

- [ ] **Step 4: Run complete verification**

Run static gates, Editor build, all `UrbanSpear.CharacterCamera` automation, `Build-WindowsDevelopment.ps1`, and `Test-PackagedBuild.ps1`. Expected: zero test failures and packaged executable starts and exits cleanly.

- [ ] **Step 5: Perform manual Windows acceptance**

Launch at 1280x720 Low. Verify `V`, `Q`, crouch allowance, aiming/sprint/traversal/downed/death rejection, wall collision, owner body visibility, forced-first-person policy, respawn restoration, and muzzle obstruction. Record results in the verification document.

- [ ] **Step 6: Commit**

Commit message: `test: add character camera milestone verification`.

### Task 8: Final review, push, and pull request

**Files:**
- Modify: `README.md`
- Modify: `docs/roadmap/implementation-sequence.md`
- Modify: `docs/build/character-camera-verification.md`

- [ ] **Step 1: Update documentation**

Link the design, plan, verification commands, test map, and Windows launcher. Mark roadmap stage 2 complete only after all automated and manual checks pass.

- [ ] **Step 2: Run final clean verification**

Run `git diff --check`, repository safeguards, character-camera milestone gate, Editor build, automation, Windows package, and package smoke. Confirm `git status --short` contains no generated outputs.

- [ ] **Step 3: Review the branch diff**

Compare against `origin/main`; confirm no Lyra source files changed, no unlicensed assets were added, no generated build directories are tracked, and every design requirement maps to code or a test.

- [ ] **Step 4: Push and create PR**

Push `feat/character-camera`. Create a PR targeting `main` with summary, test evidence, manual acceptance, performance limitation, and a statement that merge requires explicit user approval.
