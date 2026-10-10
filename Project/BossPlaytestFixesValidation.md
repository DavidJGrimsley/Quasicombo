# Enemy cleanup and boss rounds display

Validated on 2026-10-06 in `Quasicombo-level-design`, Diversion branch `level-design`, Unreal Engine 5.8.

## Changes

- The first accepted `StartEncounter` lethally damages all living `ACombatEnemy` actors except bosses. Hidden route enemies use the same death notifications, gate bookkeeping, ragdoll behavior, and five-second removal timer as ordinary kills. Rejected and repeated starts do not perform cleanup. Players, background creatures, and larvae traps are preserved.
- Added Blueprint-pure `GetAdditionalQTERoundsRemaining()`. It returns `max(0, FinisherRounds - FinisherRound)` after the first finisher snapshot and zero before the snapshot or after the encounter/run ends.
- Added an independent top-center native HUD counter, exactly `Rounds left: X`, in white with a dark outline and viewport-scaled padding. It survives the boss panel being hidden during QTEs and remains visible between rounds and during retries. Zero collapses it; a three-round encounter displays **2 → 1 → hidden**.
- Pit behavior was already correct and was left unchanged. The earlier live collision check reduced an enemy's HP from 3 to 0; the retained pit automation also passes.

## Build and automation results

Both `Braided_Quanta2026Editor Win64 Development` and `Braided_Quanta2026 Win64 Development` builds succeeded.

Through the running Unreal MCP automation toolset, **17 selected tests passed, 0 failed, 0 skipped**:

- Six boss tests: `ApproachStartsCombat`, `ArmorAndSharedRetry`, `ArmorAndUncertainty`, `BaseHealthToVictory`, `EncounterClearsCombatEnemies`, and `RewardEligibilityAfterDeath`.
- Ten QTE tests: `AirborneChainContactAndRecovery`, `ChargeBoundaries`, `CombatRoundsComboAndObliteration`, `FailureFeedbackBeforeCounter`, `Interruption`, `JumpThenUppercut`, `LivePromptPoseHolds`, `PresentationInterruption`, `ProgressiveRounds`, and `PromptSequence`.
- `Quasicombo.Route.RuntimeGatesPitArena`, including the existing enemy pit-death regression.

The new cleanup test exercises direct encounter start, arena-entry overlap, and proximity start. It checks living/hidden/already-dead enemies, selected-route gate notifications, ordinary ragdoll removal, excluded actor survival, rejected starts, and repeated starts. The extended QTE integration test checks one-, two-, and three-round counter states, retry preservation, failed and interrupted attempts, successful nonfinal decrement, and final-round hiding.

## Live editor checks

MCP-driven PIE in `Lvl_Main` killed all **38** remaining combat enemies on encounter start while preserving **16** background creatures and traps, the player, and the boss. Deterministic PIE preview settings were applied only to runtime actors.

The three-round QTE displayed `Rounds left: 2` while its boss information panel was hidden. MCP screenshots confirmed readable top-center placement at actual viewport sizes **1400×853** and **1200×553**, including the short viewport's cinematic framing. Original editor play settings were restored, PIE was stopped, and no project content or map packages were left dirty.

Local evidence is retained under the ignored `Saved/BossFixValidation/` directory: `EditorBuild.log`, `DevelopmentBuild.log`, `FinalAutomation.json`, `NormalViewport.json`, `ShortViewport.json`, and both viewport PNG screenshots. Existing asset and TODO edits remain outside this task's commit.
