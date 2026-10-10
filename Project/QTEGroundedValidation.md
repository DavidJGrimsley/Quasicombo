# Grounded uppercut and charge revision — validation

Validated October 3, 2026 (EDT), using Unreal 5.8 through Unreal MCP in `Quasicombo-QTE-improvement/Braided_Quanta2026.uproject`. Diversion branch: `QTE-improvement`. The preceding airborne/Sequencer implementation was committed separately as **87** before this revision.

## Implemented behavior

- Claw now transitions through `LS_QTE_FinisherLanding` into `LS_QTE_FinisherUppercut`. Fifth B holds frame 3 of 36 at ground height, then resumes the same uppercut. Its authored rise reaches 145 cm versus the first uppercut's 100 cm. Contact is frame 17 and uses `hand_r` against the lizard's `head`.
- Fifth contact launches a real ragdoll along the authored Launch trajectory. Five retains the launch/death ending; six completes CannonLanding before X. Ragdoll flight freezes at X, remains frozen throughout Hold/Release, and resumes when the tracking shot fires. Nonfinal fifth attacks recover into the existing counter/reset.
- Final uppercut, landing, Cannon and CannonFire use wide camera framing at 58 degrees. Charge no longer selects a close view of Mike's muzzle/head.
- X displays **Hold**, then **Release** after the one-second charge. Release allows 1.2 seconds. Button glyph outlines and progress strokes are thicker; prompt-count dots are removed.
- The reward shares the current cannon's material instances and Mike's light color. Prefixes 2–4 finish the full death clip at full montage weight before physics. Cinematic ragdolls use a transient physics-asset copy with self collision disabled, bounded depenetration and zero initial velocity; the ordinary physics asset is restored for counters/cancellation.
- Fifteen paired Sequencer sources publish thirty active baked outputs. Legacy FlyingKick assets remain inactive. Versioned authoring and routine publication preserve editable source tracks and unrelated presentation settings.

## Results

| Check | Result |
| --- | --- |
| Win64 Development Editor | Passed; `Saved/QTEGroundedEditorBuild.log` |
| Win64 Development game | Passed; `Saved/QTEGroundedGameBuild.log` |
| QTE automation | **9 / 9 passed** in the final full-suite run; `Saved/QTEGroundedTargeted.json` |
| Full Quasicombo automation | **45 / 47 passed**; only the two retained baseline tests failed; `Saved/QTEGroundedFullSuite.json` |
| Asset validation | Passed all fifteen paired sources, thirty active bakes, grounded prompt, larger rise, contact metadata and wide camera checks |
| Publish integration | Passed disposable FK pose, Prompt marker, trajectory and FOV changes; bindings and unrelated settings retained; fixtures removed |
| Preservation | SHA-256 comparison: **49 protected files, zero changes** from the recorded starting state |
| Final editor | Exact project verified, `/Game/Maps/Lvl_Main` loaded, PIE stopped, no dirty content or map packages |

The final full-suite run emitted **no Invalid Simulate Options warnings**. Test-world warnings for the absent game viewport, Niagara activation after construction and deliberately offline evolution remain.

Expanded live world/animation coverage runs prefixes 2–6, a failed airborne prefix and an earned nonfinal fifth attack in both facing directions. It checks airborne continuation through Claw, grounded fifth anticipation, evaluated hand/head contact, actual ragdoll entry, frozen head pose/trajectory during X and charge, wide FOV, Hold/Release text, release resumption, full death montage weight, bounded corpse motion, combo increments, recovery and cleanup. Existing prompt-hold, interruption, round, sequence and projectile tests remain. Reward validation compares each pickup material slot with the live cannon.

## Retained baseline failures

1. `Quasicombo.Boss.LizardRigAndTailAnimation`: imported claw geometry is below the capsule bottom in the Blueprint pose.
2. `Quasicombo.Buster.ThunderParticleColors`: no qualifying active core/electricity particle data at the early sample.

Their assertions remain intact. Neither failure comes from the new finisher tests.

## Handback

The editing guide and three short PIE/controller checklists are in `Project/QTECinematics.md`. Visual pose quality, precise framing, controller rumble and explosion coverage are the user's tests; automation does not replace them.

Only intended QTE assets were saved. Ordinary animation/montage assets, existing map edits, MCP configuration changes, the handoff document and TODO changes remain outside the scoped commits. The separate time-evolution workspace was left untouched. No merge to main was performed.
