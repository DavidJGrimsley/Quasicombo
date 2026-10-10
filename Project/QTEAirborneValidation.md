# Airborne QTE implementation and validation

Validated October 3, 2026 in the exact `Quasicombo-QTE-improvement/Braided_Quanta2026.uproject` workspace with Unreal 5.8 through Unreal MCP. Diversion remains on **QTE-improvement**, over commit **74**. PIE was stopped before authoring. Changes remain uncommitted; no merge was performed. The separate time-evolution workspace, detached over commit 85 with pending changes, was left untouched.

## Delivered

- Fourteen editable paired Sequencer sources and twenty-eight dedicated baked animation outputs. JumpApex is duplicated into the QTE namespace on Mike's existing skeleton.
- Uppercut → apex → Chomp → apex → Claw → apex → FlyingKick. FlyingKick uses knee chamber, one-foot extension/contact and airborne follow-through; every earned fifth contact launches the lizard, including nonfinal rounds.
- Prefixes end through recovery from the current pose and placement. Six completes CannonLanding before X, then maintains charge/fire pose continuity and continuing lizard flight.
- The director alone evaluates baked animation, trajectories and cameras from the presentation clock. Prompt holds retain full montage weight and placement; accepted input resumes the existing montage cursor. Input deadlines and existing QTE rules remain unchanged.
- **Tools → Quasicombo → Publish QTE Choreography** stages exports, validates bindings, skeletons, timing and continuity, publishes paths/cameras/markers, and explicitly saves only QTE outputs and presentation data. Routine publication retains source bindings, FK edits and unrelated presentation settings.

Editing instructions and the three manual test checklists: [QTECinematics.md](QTECinematics.md).

## Automated results

| Check | Result | Evidence |
| --- | --- | --- |
| Editor Development build | Passed | `Saved/QTEAirborneEditorBuild.log` |
| Win64 Development build | Passed | `Saved/QTEAirborneGameBuild.log` |
| QTE automation | **9/9 passed** | `Saved/QTEAirborneTargeted.json` |
| Full Quasicombo automation | **45/47 passed** | `Saved/QTEAirborneAutomation.json` |
| Asset validation | Passed | `QTE_ASSET_VALIDATION_PASSED` in editor Output Log |
| Actual Tools menu publication | Passed, 14 segments | `QTE_PUBLISH_COMPLETE` at 00:31 UTC October 4 |
| Disposable publication fixture | Passed, including fixture removal | `QTE_PUBLISH_INTEGRATION_PASSED` at 00:41:04 UTC October 4 |
| Preservation audit | **49 files checked, zero changed** from implementation starting state | `Saved/QTEAirbornePreservedHashes.json` |

The live world/animation tests cover both facing directions; continued airborne placement; shared apex transitions without extra combo; actual montage weight and evaluated pose holds; uninterrupted accepted-input playback; evaluated kick foot within 35 cm of the lizard contact target; reaction/contact order; nonfinal fifth launches and counters; grounded cannon prompt; charge/release/obliteration; airborne failure recovery; interruption cleanup; and grounded reward accessibility. The old uppercut landing expectation is replaced by separate airborne continuation and deliberate ending-prefix recovery checks.

The disposable fixture edits an FK pose, Prompt marker, actor trajectory and camera FOV, then verifies each published output. It also verifies source binding IDs and unrelated approach-speed/explosion-scale settings remain intact. The fixture publishes into its own temporary namespace and removes its assets afterward. Validation scripts do not replace gameplay outputs.

### Retained baseline failures

These assertions were retained and reported separately; neither was weakened:

1. **Quasicombo.Boss.LizardRigAndTailAnimation**: `Imported claw geometry is above capsule bottom in Blueprint pose` fails.
2. **Quasicombo.Buster.ThunderParticleColors**: early light-core and body-electricity particle activity assertions fail. Visual acceptance remains manual.

These account for both failures in the full suite. There are no failing QTE tests.

## Preservation and remaining manual checks

The preservation audit includes ordinary animations and the existing pending Jump animation, Lvl_Main external-actor changes and TODO file. Existing MCP configuration changes were retained. Authoring used an empty `/Engine/Maps/Entry` workspace and explicit QTE saves; no Save All was used. The editor is returned to `/Game/Maps/Lvl_Main` for handback, with PIE stopped and no dirty map or content packages.

PIE visual and controller checks are intentionally left to the user: airborne/contact choreography, rounds/combo, and charge/rumble/obliteration. Do not merge before these checks and the evolution integration review.

An Unreal Python shutdown/finalization crash was observed during authoring editor restarts after assets had been saved. It involved `python311.dll` / PythonScriptPlugin; it is not claimed fixed by this change. Fresh editor builds, MCP publication and automation completed successfully. Temporary authoring fixture recovery was declined on restart; user packages were not discarded.
