# Lizardman evolution presentation

The boss defaults to **10 base HP**. The accepted hit that leaves it alive at **1 HP** locks combat and starts Stumble → Idle Recovery → Rise → Hover → Face Reveal → Armor Assembly → Roar/Descent → Return. This is nine one-point hits at default health (maximum base HP minus one). The editable `EvolutionTriggerHealth` uses remaining health rather than a fixed hit count. Evolution refills base HP to its configured maximum when the result is resolved; awarded shields stay separate. Earlier accepted hits do not consume the refilled phase's hit budget. The opening uses 0.4 seconds of the existing defeat animation, then 0.4 seconds of idle. The actor rises 165 cm over 1.25 seconds, hovers for one second, and keeps the first charge pose through the eye reveal and all armor impacts. Descent waits for every piece and the last 0.28-second attachment reaction. The roar plays over the 1.2-second descent, followed by a one-second diagonal camera return with rotation and FOV changing together. There is no smirk or separate middle assembly pose. The face stage uses three seconds of eye flickering followed by a 0.7-second close-up hold: 0.25 seconds to frame the face, then an accelerating exponential push from 135 cm to 100 cm. The new eye color flickers on fifteen times: brief flashes stay at 60 ms, while the fourteen old-color gaps shrink exponentially from approximately 306 ms to 38 ms. The fifteenth reveal stays on at full brightness during the added 0.7-second hold at the closest face framing. The same exponential charge curve ramps eye emissive from 10% to normal, including after the final color reveal locks. Project material copies expose an emissive multiplier without changing the original eye textures or glow color. Armor pullback begins from that final close-up, avoiding a jump in framing. Typical authored duration is about 10.0 / 10.8 / 11.5 seconds for levels 0 / 1 / 2, plus frame quantization.

## Visual test in PIE

Use the **Quasicombo-QTE-improvement** workspace and **QTE-improvement** branch for the combined evolution/QTE work. Open `/Game/Maps/Lvl_Main` and press Play. Normal combat starts transformation when the boss reaches one remaining base HP. The 0/1/2 developer shortcuts and automatic PIE preview mode have been removed. PIE now uses the same live or local evolution flow as normal gameplay.

Starting Tau selects skin 01-12 in equal twelfths. The encounter captures the validated braid state and rejects pending braid responses after that capture. If no state is ready, the existing route fixture supplies it; manually starting before completing the route fills unplayed fixture lanes with A. This does not commit route choices. Deterministic validation scripts configure their test fixtures directly without player shortcuts.

| Level | Absolute Tau change | Eyes after reveal | Aura | Shields |
| --- | --- | --- | --- | --- |
| 0 | < 0.05 | M_Eyes_03 | N_MagicalBarrier | 0 |
| 1 | 0.05 to < 0.15 | M_Eyes_02 | N_RingOfFlames | 1 |
| 2 | ≥ 0.15 | M_GlowEyes | N_TrailsOfDeath | 2 |

Eyes start at M_Eyes_01 and retain the awarded result after armor breaks. Aggression follows evolved Tau separately.

## Armor and combat

Each blue shield takes four accepted hits. The outer group drains first. Inner: cloth, waist belt, both arm guards, and left shoulder 01 for level 1 or 02 for level 2. Outer: torso belts/harness, right shoulder, both arm spikes, left shoulder spikes, skull, and waist bone. Left shoulder 02 stays after the outer break; shoulder 01 does not replace it.

A depleted shield detaches its posed pieces. They fall and rotate, collide with world-static ground, bounce, settle, ignore characters, and remain until encounter cleanup. Base health is separate from shields. Stagger resistance is 60% / 80% / 100% with zero / one / two shields. The resistance before a hit controls that hit's animation amplitude and stagger duration. Ordinary hits at full armor drain shields while attacks continue.

Boss health is HUD-only; the boss's overhead widget stays hidden. A bottom-centered panel orders the outer shield, inner shield, base health, **Lizardman Berserker | Braid Aggression (Tau): XX%**, and the **Attack Rate | Speed | Resistance** buff line. Empty shields collapse; the buff line always has reserved space before its reveal. Text fits on one line, with readable screen-space sizing in short and narrow windows and an eight-pixel bottom margin.

The persistent retry count has been removed. The first encounter failure instead shows a large centered **One more chance** announcement, pauses the game, and restarts after a one-second real-time deadline. Game time, physics, and movement stop during that announcement. The boss can tick while paused to release its own pause; retry, run end, camera/time restoration, and level exit clear that ownership without clearing a pre-existing external pause.

The flame outcome uses two copies of **N_RingOfFlames** crossing at the moving boss's body midpoint. They tilt by **+45° / −45°** around his forward axis to form an X from the frontal camera, rather than separating toward his neck and ankles. `FireRingTiltDegrees` exposes the angle on the evolution component. Both rings follow his position and facing during rise, armor assembly, and descent, then taper and clean up together.

## Authoring hooks

`EncounterLighting` captures the level's authored sunlight before approach dimming or combat, then smoothly fades the Directional Light to **4 lux over one second** at encounter start. The health-triggering hit starts a second one-second fade to **0.2 lux** during transformation. Completion or interruption fades it back to **1 lux**, retained through shield breaks, QTE, retry, Mike's Buster pickup animation, and the victory screen. Victory is recorded before Mike's pickup animation and retains the dim lighting. Defeat, actor removal, and level exit restore the captured intensity. The saved `Lvl_Main` light is movable at **20 lux**; this runtime effect does not modify the map.

The component exposes combat, transformation and post-transformation intensities and fade durations on the boss. Its optional **Directional Light** reference can bind a particular sun; otherwise it finds the brightest visible directional light whose mobility allows runtime changes. The fades follow game time and freeze during the retry announcement.

`EvolutionPresentation` exposes an optional Stumble clip (otherwise the boss defeat clip), Rise, Hover, Attachment, and Roar slots; sound hooks; aura slots; and stage, armor-attached, landed, and roar events. Baked clips live in `/Game/Quasicombo/Boss/Lizard/Animations/AS_Lizard_*`. The landing event now occurs during the roar descent. Armor follows the animated skeleton, so the middle assembly pose is not needed for alignment. Only the roar entry was changed to start from the first charge pose; the retained rise, hover, impact reaction, and remaining roar keys are unchanged in this pass. The original facial copy remains neutral; its expression morphs are not driven.

For manual editing, open `Project/AnimationSource/LizardTransformation.blend`. It has a skinned deformation armature and keyed actions, rather than custom IK animator controls. `Project/AnimationSource/README.md` gives the edit/export/import commands. Use `ExportLizardPerformance.py` and `ImportLizardPerformance.py` for edited actions; `BuildLizardPerformance.py` regenerates and overwrites the source poses. Elevation and armor flight remain controlled by the presentation component. Bespoke audio is still a later pass.

## Quantum fallback and validation

Boss braid and evolution HTTP requests use a two-second per-request override; other plugin requests retain their configured timeout. The evolution deadline uses real elapsed time. At the health trigger, the complex state, X/Z coefficients, and evolution time are copied into a request snapshot. Offline mode, failure, invalid responses, or deadline expiry use the exact local `exp(-i (aX + bZ) t)` calculation, then the normal validated outcome logic. Diagnostics label Live, Local, or Preview; late callbacks cannot replace a completed result. API errors stay in diagnostics.

Build `Braided_Quanta2026Editor Win64 Development`. Relevant automation filters are `Quasicombo.Boss`, `Quasicombo.Evolution`, `Quasicombo.Quantum`, `Quasicombo.Braid`, `Quasicombo.Buster.SingleHitAndBossEvolution`, and `QuantumApi`. Use rendering for the VFX checks rather than NullRHI.

`Project/ValidateLizardEvolution.py` runs dedicated offscreen PIE sessions for all outcomes and both facing directions, including 1920×1080 and 960×1080. Launch a separate editor with `-RenderOffScreen -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -ExecutePythonScript=<absolute script path>`. Its temporary actor positioning is test setup only and it never saves the map or PlayerStart. Captures and diagnostics are written under `Saved/EvolutionValidation`.

## Verified on October 1, 2026

The Development editor build succeeded. All 13 tests in the relevant filters passed (eight reported warnings); the report is `Saved/EvolutionRelevantAutomation/index.json`. These cover skin bands, outcome thresholds, reference quantum states, the real two-second deadline and late-response rejection, shield absorption and posed debris, resistance and attack continuity, interruption/retry, and the high-aggression opening.

Seven dedicated PIE sessions produced 53 captures in `Saved/EvolutionValidation/report.json`. All three outcomes were inspected from both facing directions, with 1920×1080 and 960×1080 framing. The completed sequences took approximately 6.4–7.7 seconds. Camera, player visibility and movement, boss height, and time dilation restoration passed in all seven sessions, including cancellation during the rise. Shield breaks produced five inner or seven outer debris pieces and updated the HUD resistance. The player and legacy combo HUD are temporarily hidden during the cinematic to keep the frontal reveal clear, then restored.

This validation covers the relevant suites. A broader exploratory run failed Buster particle checks and crashed the `Quasicombo.Combat.EnemyResistance` fixture, so the entire repository suite is not certified. Authenticated live HTTP was not exercised; transport mocks, deadline handling, and local quantum results were tested. Bespoke audio production remains outside this visual pass; assign audio through the exposed sound slots.

## HUD iteration validation

`Project/ValidateLizardHUD.py` exercises all three outcomes, shield collapse, the empty reserved buff row, stacked flames, a paused retry, and level exit during retry. Its latest run passed five PIE sessions and produced 31 captures at 1920×1080, 1860×550, and 960×1080 under `Saved/EvolutionHUDValidation`. The updated relevant automation report is `Saved/EvolutionHUDAutomation/index.json`: 13 passed, zero failed. Each completed requested iteration is committed to the `time-evolution` branch; further review requests wait until the visual pass is ready.

## Lighting iteration validation

The Development editor build succeeded. All 14 relevant automation tests passed, with nine tests reporting warnings and zero failures. `Quasicombo.Boss.LightingFadesAndRestores` checks intermediate intensities during both fades, their final targets, persistence after evolution and retry, and restoration on run end and actor removal during a fade. The report is `Saved/EvolutionLightingAutomation/index.json`.

The updated HUD validator passed five PIE sessions with 31 captures covering all three auras at 1920×1080, 1860×550, and 960×1080. Every pre-evolution capture recorded 10 lux; every later capture, including the paused retry and resumed combat, recorded 1 lux. The editor light remained at its authored 20 lux between sessions. Captures and intensity records are in `Saved/EvolutionHUDValidation/report.json`; the execution log is `Saved/EvolutionLightingPIE.log`.

## Pickup lighting regression

Collecting the Buster reward records victory before Mike's acquisition animation begins. Victory cleanup now retains the encounter's dim sunlight through the animation and victory screen. Defeat and level-exit cleanup still restore the authored intensity.

The Development editor build and all seven tests under `Quasicombo.Boss` and `Quasicombo.Buster.Acquisition` passed, with six tests reporting warnings and zero failures. `BaseHealthToVictory` now collects the actual pickup, checks sunlight throughout the complete acquisition animation and at the victory screen, and verifies restoration on boss removal. Results are in `Saved/EvolutionPickupLightingAutomation/index.json`. `Project/ValidateLizardPickupLighting.py` follows the same flow in the rendered map without saving assets.

The rendered 1920×1080 PIE check passed with eight captures. Sunlight remained at 1 lux throughout Lift, Orbit, Equip, Land, Victory Wait, Complete, and the victory screen; the editor light remained at 20 lux after ending PIE. Captures and intensity records are in `Saved/EvolutionPickupLightingValidation/report.json`.

## Animation rebuild checkpoint (user testing)

The rebuilt Development editor build succeeded. Eight rendered PIE scenarios completed with 69 captures in `Saved/LizardRebuild/PIE2/report.json`: all outcomes, both facings, a narrow viewport, and interruption. Per-frame bone-scale and position checks passed, as did camera, movement, visibility, height, and time restoration. The attachment reference-pose bug was corrected after the first visual run. The mesh authoring check verified 15,389 unchanged neutral vertices and their original skin weights, with 143 smirk and 306 eyelid deltas added to the copy.

Stopped at the user's request for hands-on testing. Remaining: judge the charge, landing, roar, and restrained smirk visually; inspect the recorded motion and narrow captures in detail; rerun the final rendered automation suite, including the newly added attachment regression assertions. The earlier automation run passed 14 of 15 tests; the Buster test's two particle checks failed under NullRHI. Normal rendered sequences took about 8.3–9.7 seconds; frame-by-frame screenshot recording stretched one level-2 run to 10.55 seconds because stage transitions discard frame overshoot. The authoring commandlet saved all assets but crashed in Python shutdown on its last run; wrapper cleanup was added but has not been rerun. No further review or merge is requested.

## Opening and ending iteration — October 2, 2026

This iteration supersedes the preceding animation checkpoint. It adds the short defeat-clip stumble and idle recovery, removes the separate assembly pose and smirk, preserves armor impacts while hovering, and plays the existing roar during descent. Only the roar's first 17 frames were adjusted to enter from the retained charge pose; comparison of the exported keys confirms that frame 17 onward is unchanged. The body mesh, rise, hover, and impact assets were not edited. The manual export/import workflow successfully saved the edited roar and exited cleanly.

The Development editor build passed. All **15 relevant rendered automation tests passed**, with ten reporting warnings and zero failures (`Saved/LizardSequence/FinalAutomation/index.json`). An artificial controller camera fixture crashed during development; camera-path assertions now run in the actual PIE map instead.

Eight final PIE sessions produced **71 captures** (`Saved/LizardSequence/FinalPIE/report.json`). All outcomes and both facings were checked at 1920×1080, with an additional 960×1080 full-armor session and cancellation during rise. All seven completed camera returns passed per-frame straight-path and simultaneous-rotation checks. Player visibility, movement, camera, ground height, and time settings restored in all eight scenarios. Completed sequences took about 8.0 / 8.7 / 9.4 seconds. Armor assembly remained aloft, the roar descended, and the player stayed hidden until the camera return completed. Further pose polish remains for hands-on evaluation; no new review or merge is requested.

## Crossed flame iteration — October 2, 2026

The editor build passed. Four targeted fire-outcome PIE sessions produced 35 captures under `Saved/LizardSequence/CrossedFirePIE`: both facing directions at 1920×1080, a 960×1080 viewport, and cancellation during rise. Both rings share their center, use opposing 45-degree tilts, and follow the body through the sequence. The rendered crossing was inspected during armor assembly and descent. Camera, visibility, movement, height, and time restoration passed in all four sessions. Set `QUASICOMBO_EVOLUTION_FIRE=1` when running the evolution validator to select these scenarios. The HUD validator now checks crossed rings instead of vertical spacing.

## Five eye reveals and continued camera push — October 2, 2026

The Development editor build and all five `Quasicombo.Evolution` automation tests passed (four tests reported fixture warnings, zero failures). Eight rendered PIE sessions produced 106 captures in `Saved/LizardSequence/FiveEyesPIE/report.json`. All three outcomes and both facing directions, plus a 960×1080 full-armor view, showed exactly five eye reveals over the extended three-second face stage. Per-frame samples verified continued camera approach from approximately 135 cm to 100 cm after initial face framing. The fifth reveal stayed on, armor assembly resumed, and camera, controls, visibility, boss height, and time settings restored in all eight sessions, including cancellation during rise. Completed sequences took about 9.4 / 10.2 / 10.9 seconds.

The offscreen validation editor returned exit code `0xC0000005` after completing every check and closing its shutdown log. A short repeat completed both a full sequence and interruption with the same shutdown code. Dropping retained Python editor/world wrappers before shutdown did not resolve it. No error appeared during PIE and no new Unreal crash report was generated; the shutdown cause is unresolved. Evidence is retained in `Saved/LizardSequence/FiveEyesPIE.log` and `FiveEyesCleanup.log`.

## Exponential charge rhythm — October 2, 2026

The initial body-to-face zoom now takes 0.25 seconds. The three-second reveal uses new-color durations of approximately 60 / 120 / 240 / 480 ms followed by the permanent fifth reveal, with intervening old-color gaps of 480 / 240 / 120 / 60 ms. `QuasicomboEvolutionRules` provides the shared exponential curve for flash weights, continued camera approach, and emissive scaling. Eye brightness starts at 10% and reaches the established normal appearance before armor assembly. The fifth reveal locks color while brightness continues to rise. `AuthorLizardEyeCharge.py` creates three project material copies with an emissive multiplier; the original materials remain the source of their textures and colors.

The Development editor build and all five evolution automation tests passed, with zero failures and four fixture warnings (`Saved/LizardSequence/ChargeAutomation/index.json`). Eight rendered PIE sessions produced 78 captures under `Saved/LizardSequence/ChargePIE`. Per-frame samples verified five reveals, increasing new-color durations, shrinking old-color gaps, accelerating camera approach, and camera/emissive agreement for all outcomes and both facings, plus a 960×1080 view. Camera/control/visibility/height/time restoration passed in every session, including interruption. The previously reported offscreen-editor shutdown exit code `0xC0000005` still occurs after all checks complete.

## Fifteen short eye flickers — October 2, 2026

This iteration supersedes the preceding flash rhythm. Each new-color flicker uses a fixed 60ms window; only the fourteen old-color gaps shorten exponentially, from approximately 306ms to 38ms. The fifteenth reveal stays on and reaches full brightness at the end of the three-second face stage. The 0.25-second initial face zoom and shared exponential camera/emissive ramp are retained.

The Development editor build and all five `Quasicombo.Evolution` automation tests passed, with zero failures and four tests reporting fixture warnings (`Saved/LizardSequence/FifteenAutomation/index.json`). Eight final rendered PIE sessions produced 71 captures under `Saved/LizardSequence/FifteenFinalPIE`. All seven completed face stages showed fifteen reveals, fixed brief flashes within frame quantization, exponentially shortening gaps, accelerating camera approach, and matching emissive progression. These cover all outcomes and both facing directions at 1920×1080, plus a 960×1080 full-armor view. Camera, controls, visibility, height, and time settings restored in all eight sessions, including cancellation during rise.

Face screenshots are now captured after the final color lock: a readback stall during the earlier old-color gap cut a flicker short in the first validation run. Per-frame timing checks remain active through the entire reveal. The final run completed every check without a Python assertion; the previously reported offscreen-editor shutdown code `0xC0000005` remains unresolved and still occurs after the shutdown log closes (`Saved/LizardSequence/FifteenFinalPIE.log`).

## One-second close-up hold — October 2, 2026

After the existing three-second eye reveal, the camera holds its closest face framing for one additional second with the awarded eyes at full brightness. The charge animation continues through the hold, then the usual armor pullback and remaining sequence resume. `FaceHoldSeconds` is editable on the evolution component and defaults to 1.0; it does not stretch the flicker schedule.

The Development editor build and all five evolution automation tests passed, with zero failures and four tests reporting fixture warnings (`Saved/LizardSequence/FaceHoldAutomation/index.json`). Eight final rendered PIE sessions produced 71 captures in `Saved/LizardSequence/FaceHoldFinalPIE`. All seven completed face stages retained fifteen flickers and held the camera 100cm from its head target at full eye brightness for the final second. Checks cover all outcomes and both facing directions at 1920×1080, plus 960×1080 framing. Camera, controls, visibility, height, and time settings restored in every session, including interruption during rise. Screenshots now occur within the hold so readback does not hide its starting timing samples. All final assertions passed; the existing offscreen-editor shutdown code `0xC0000005` still follows completion (`Saved/LizardSequence/FaceHoldFinalPIE.log`).

## Close-up hold shortened to 0.7 seconds — October 2, 2026

`FaceHoldSeconds` now defaults to 0.7. The Development editor build and the existing `Quasicombo.Evolution.HealthTriggerAndPerformance` automation test passed; the test reported fixture warnings and zero failures (`Saved/LizardSequence/ShortHoldAutomation/index.json`). Its full sequences verify the new hold duration, full eye brightness, fifteen reveals, armor assembly, and return to combat. The PIE validator's timing expectation and screenshot point were updated for the shorter hold.

