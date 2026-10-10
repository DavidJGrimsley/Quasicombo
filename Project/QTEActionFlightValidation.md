# Action buttons and uppercut flyback validation

Verified Unreal MCP connection: Unreal 5.8, this workspace's exact `Braided_Quanta2026.uproject`, branch **QTE-improvement** over commit **92**, `/Game/Maps/Lvl_Main`, PIE stopped. Editor content and maps were clean before implementation. Authoring and preview used an unsaved Entry world; the editor was returned to Lvl_Main with no dirty content/maps before final automation. No arena or ordinary animation assets were saved.

Both quantum outcomes now use **A → B → Y → RT → B → X**. Existing Interact Started routes Y/E to Chomp once per press; ChargedAttack Started routes RT/right mouse to Claw. Releasing RT during QTE cannot start an ordinary charged attack. Quantum encounter/round selection and existing charge, release, failure and combo rules remain intact.

`LS_QTE_Launch` supplies an editable spread-arm, bent-knee recoil at frame zero, marked **Ragdoll seed**. The dedicated `AS_QTE_LaunchRecoil` was authored through MCP using the cinematic mesh's actual reference pose, then imported into the Lizard FK track and published to `AS_QTE_Launch_Lizard`. Source binding identities, Mike keys, camera and trajectory tracks were retained. Versioned authoring skips migrated sources on routine reruns. Gameplay evaluates this pose before starting restrained slow-motion physics; X freezes the full ragdoll through Hold/Release. Launch yaw is published from Sequencer and mirrored at contact; counters restore combat facing. Ordinary death ragdoll damping is unchanged.

Validation:

- Editor Development and Win64 Development builds passed. Logs: `Saved/QTEActionFlightEditorBuild.log`, `Saved/QTEActionFlightGameBuild.log`.
- All **10 QTE automation tests passed**, including actual Y/RT pawn callbacks and glyphs, failure cues, both facing directions, prefixes 2–6, nonfinal launches/counters, combo accounting, contact, grounded recovery and reward/equipment behavior.
- Live animation tests verify spread-arm width, hands near shoulder height, bent knees, slow physics, mirrored facing, uninterrupted held attacks and frozen head/hands at X.
- Full Quasicombo suite: **61/62 passed**. Detailed results: `Saved/QTEActionFlightAutomation.json`.
- Retained baseline `Quasicombo.Boss.LizardRigAndTailAnimation` **passed**. Retained baseline `Quasicombo.Buster.ThunderParticleColors` **failed** its two existing early-particle assertions; neither was removed or weakened.
- Asset validation passed for all fifteen paired sources and thirty gameplay bakes. Publishing fixtures verified FK pose, Prompt marker, actor trajectory, lizard yaw and camera FOV changes, binding identities and unrelated presentation settings; disposable assets were deleted.
- Inspected the published recoil in Unreal's editor viewport without PIE. Actual mesh evaluation at frame zero gives a **146.1 cm** hand span, hands **8.4 cm** below the shoulders, and visibly bent knees. User PIE/controller checks remain pending.
- All **11 pre-existing pending files** and **47 protected ordinary/source files** retained their starting hashes. Separate evolution workspace and main were untouched.

Editing instructions and the three PIE checklists are in `Project/QTECinematics.md`. Check Y for Chomp and RT for Claw, then the spread-arm slow flyback and complete X freeze; also verify nonfinal counter facing. Music remains at 40%, with full-volume impact/charge/shot effects.
