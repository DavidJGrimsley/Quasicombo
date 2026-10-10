# QTE choreography: editing and PIE checks

Workspace: **Quasicombo-QTE-improvement**, Diversion branch **QTE-improvement**. The earlier airborne implementation is saved in Diversion commit **87**. Open this workspace's `Braided_Quanta2026.uproject`. Implementation uses the connected Unreal 5.8 editor and dedicated QTE assets. Ordinary animations, montages, notifies and playback settings are preserved.

## Edit in Sequencer

Sources are in `/Game/Quasicombo/QTE/Sources`; open the `LS_QTE_` sequence named below. **Mike** and **Lizard** have editable FK Control Rig pose tracks and separate **Capsule trajectory (cm)** transform tracks. **Camera** owns transform, target blend and focus offset; **Camera Lens** owns FOV. Camera cuts enable Sequencer preview.

| Source | Edit here |
| --- | --- |
| JumpReady / Jump | Held squat at A, then movement-only takeoff. Keep the squat/takeoff seam. |
| JumpApex | Shared airborne pose and short transitions. It never adds a prompt or combo point. |
| Uppercut / Chomp / Claw | Anticipation, contact and follow-through. Uppercut ends at apex; Chomp and Claw start and end there. |
| FinisherLanding | Descend from Claw's ending apex, land and blend into the beginning of the final uppercut. Finish before the fifth B opens. |
| FinisherUppercut | Larger grounded uppercut. Prompt holds the very beginning, Contact uses Mike's right hand and the lizard's head, and follow-through rises higher than the first uppercut. Contact starts the lizard ragdoll launch. |
| CannonLanding | Descend from the final uppercut, land and settle into grounded aim. Finish before X opens. |
| Cannon | Aim anticipation at Prompt, then charge toward the ending pose. Charge duration follows the existing input rules. |
| CannonFire | Start from Cannon's ending pose and fire. The published range controls shot travel duration. |
| Recovery | Descend and recover after a prefix or failure. Runtime offsets its start to Mike's current position to avoid a snap. |
| Launch | Frame zero, marked **Ragdoll seed**, supplies the spread-arm recoil pose for the fifth uppercut's slow-motion ragdoll. Edit Lizard FK keys, trajectory and actor yaw here; the canonical yaw is 120 degrees and mirrors to 60 when facing the other way. Later FK keys preview relaxation; gameplay physics supplies the loose movement after the seed. |
| Counter / Approach | Paired counter; opening clip/camera. Approach distance and speed remain adaptive at runtime. |

Keep source ranges at **30 fps**, starting at frame zero. The playback end sets the published duration. **Prompt** marks the evaluated pose and placement held for input. **Contact** triggers impact and the paired reaction; keep `0 < Prompt < Contact < end` on attacks. JumpReady and Cannon use Prompt. A successful press resumes the held attack from its cursor. Move reaction keys together with Contact when retiming it.

Author in the canonical arena: Mike's approach endpoint is `(0,0,0)` in capsule space, lizard is at X=125 with its capsule height aligned to the same floor, +X is forward, +Z is height and negative Y is background. Actor transform tracks own travel; FK bone tracks own in-place poses. Opposite facing mirrors forward travel and rotates Mike's contact correction with his pose; lizard launch and camera background depth stay in the scene frame. Match adjacent endpoints. FinisherUppercut's Contact is fitted to the evaluated `hand_r` and lizard `head` target. The earlier FlyingKick assets remain as inactive historical sources; gameplay and publishing use FinisherLanding and FinisherUppercut.

Camera transforms, including rotation, and FOV are published. **TargetBlend** selects the tracking focus between Mike (0) and lizard (1); **FocusOffset** adjusts it. Runtime preserves the authored framing relative to live subjects, then applies camera obstruction handling. Final uppercut, landing, charge and fire use wide framing at 58 degrees, with no charge zoom. The lizard recoils with arms spread and knees bent, then flies in slow-motion ragdoll while Mike lands. It freezes at X, stays frozen through Hold and Release, then resumes flight for the tracking shot. Launch pose changes must be published before testing gameplay.

Save source edits, stop PIE, then choose **Tools → Quasicombo → Publish QTE Choreography**. The presentation asset also has a Publish Choreography action. The operation stages all fifteen paired exports, validates bindings, skeletons, timing, in-place roots and pose/path seams, then saves the dedicated bakes and `/Game/Quasicombo/QTE/DA_QTEPresentation`. It retains source binding IDs and FK keys. Failures appear in Output Log under Python/QTE and leave the presentation unpublished. Published timing/path fields are generated; edit the source instead. Effects, sounds, approach speed and other presentation settings remain independent.

Runtime runs baked clips only. `AQuasicomboQTEDirector` owns montage cursors, actor placement and camera evaluation on one presentation clock. Sources do not play alongside it. Automatic montage blend-out is disabled before playback initializes, so prompts retain full slot weight. Input deadlines remain real-time; hit stops, final slow impact, quantum selection, Xbox patterns and cleanup retain their existing rules.

## Three short PIE checklists

Select the placed boss, enable **QTE Preview**, choose **Preview QTE Rounds** and **Preview QTE Outcome** (0 Vacuum, 1 Tau). Plain number keys 2–6 select the final prefix before boss depletion; restart PIE between selections. They do not start QTE. Six temporarily equips the cannon. Disable preview to restore quantum selection.

1. **Airborne/contact choreography:** Try prefixes 2–6 in both facing directions. Check held squat/A → takeoff → uppercut/B, then airborne apex transitions through Chomp and Claw. Each prompt holds its pose and placement; accepted input continues it. After Claw, Mike lands before fifth B and holds the beginning of a larger uppercut. Check hand/head contact and launch. Prefixes 2–4 finish the lizard death animation and settle into ragdoll without being thrown upward; five retains its launch/death ending. Six lands again before X. Check ordinary movement after cleanup.
2. **Rounds/combo:** Try three rounds and final count 6 (4 → 5 → 6). Both quantum outcomes use **A → B → Y → RT → B → X**: jump, uppercut, Chomp, Claw, larger uppercut, cannon. Quantum selection still determines the encounter and round counts. Keyboard Chomp uses E; Claw uses right mouse. Each accepted prompt adds one combo point; transitions, contacts, hold repeats and release add none. An earned nonfinal fifth uppercut launches, then recovers into counter/reset. Success preserves combo; wrong input or timeout clears it, halves current HP and repeats that round. Check restored boss health/shields, interruption cleanup and reachable rewards matching Mike's current palette. Check the thicker button outline and absence of count dots.
3. **Charge/rumble/obliteration:** Complete six prompts. The lizard rises in ragdoll while Mike lands, then freezes when X appears. Start X within 1.5 seconds; **Hold** for one second, then **Release** within the following **1.2 seconds**. Ragdoll stays frozen through both windows and resumes on fire. Check wide framing, increasing rumble (about 15–60%), charge/fire pose continuity, tracking shot and explosion coverage. Try early release, overhold, wrong input, cancel, pause/disconnect, disabled vibration and quitting PIE during charge/explosion. Verify restored control/camera/time and ordinary projectile behavior.

Visual contact quality, framing, vibration and explosion coverage remain the user's PIE/controller checks.

## Scripts and validation

Run editor scripts through Unreal MCP or the editor Python console using absolute paths. `AuthorQTECinematics.py` performs the versioned initial migration, including `QTEGroundedFinisher.py` and `QTEFlightRecoil.py`; reruns retain migrated sources. `BakeQTECinematics.py` uses the same publish operation as the menu and preserves manual edits. `ValidateQTECinematics.py` checks all fifteen paired sources and thirty active bakes. `TestQTEPublishing.py` modifies a disposable FK pose, Prompt marker, trajectory, lizard yaw and FOV, publishes to disposable outputs, verifies the changes/bindings and deletes its fixtures. Neither validation script replaces gameplay outputs.

Current build logs: `Saved/QTEActionFlightEditorBuild.log`, `Saved/QTEActionFlightGameBuild.log`. The current validation record is `Project/QTEActionFlightValidation.md`; `QTEGroundedValidation.md` and `QTEAirborneValidation.md` retain the earlier checkpoints.

Retained baseline assertions: **Quasicombo.Boss.LizardRigAndTailAnimation** checks imported mesh clearance against the capsule; it passes after evolution integration. **Quasicombo.Buster.ThunderParticleColors** checks for active core/electricity particles at 0.05 seconds and remains a known failure. Neither assertion was removed.

## Evolution integration checkpoint

Main **86**, including evolution **85**, is merged into QTE-improvement in **89**. The combined code retains evolution debris cleanup before restoring shields, stored skin/eyes, resistance, anchored HUD, deadlines/fallbacks and Blueprint assets. QTE reset does not rerun evolution assembly. Plain 2–6 remain QTE preview counts. The separate detached evolution workspace and its pending changes were left untouched. Do not merge QTE-improvement into main before visual checks.

QTE sources now preview the merged `SK_Lizard_CinematicBody` at its runtime mesh height. `QTEEvolutionIntegration.py` performs the one-time source alignment and fits the final uppercut's actor path to the new head target without replacing FK pose keys, markers, camera tracks or bindings. Its migration tags make subsequent authoring runs preserve manual source edits; ordinary rebaking uses the saved sources.

Music fades to **40% of the current encounter volume** during QTE, while each attack retains its own full-volume punch effect. Nonfinal counters and interruptions restore encounter music; final victory fades it out. Tune **Music Controller → Music / QTE → QTE Volume Multiplier** and **QTE Duck Fade Seconds** on the side-scrolling game mode (defaults `0.4` and `0.25` seconds). Ducking preserves music playback position and does not affect sound effects or forest ambience. Add a listening check to the three PIE checklists above: quieter boss music under every prompt, audible hits, restored music between rounds and a quiet victory ending.

The sixth-beat explosion transfers encounter lighting to an owner that survives through the reward scene; removing the lizard does not brighten sunlight or release the QTE camera early. Final integration results and logs are recorded in `Project/QTEEvolutionMergeValidation.md`.

Final victories spawn a Buster Cannon pickup only when Mike does not already have it equipped. Without the cannon, collect it and let the full acquisition/calibration animation finish before the victory screen. With it equipped, victory opens after the selected QTE's death/recovery or explosion outro and camera/input cleanup; no duplicate pickup or acquisition animation plays. Nonfinal rounds and failures continue into combat.

After the death stumble, prefixes 2–4 hand the lizard to gravity-driven ragdoll. The old timed gravity removal and forced sleep are removed. Physics can settle naturally against the arena floor; the existing protection against self-collision launches remains. In PIE, wait beyond six seconds and check that the corpse collapses normally rather than freezing in the stumble pose. For the equipment check, try a short prefix with and without the cannon, then count six with it equipped. The reward/corpse/music validation record is `Project/QTERewardCorpseValidation.md`.

Wrong inputs, expired prompts and failed cannon hold/release show the expected button in bright red, with a red ring and **Miss** label for **0.4 seconds**, then proceed into the existing landing/recovery and lizard counterattack. The feedback holds the current pose, placement and camera, blocks further inputs and adds no combo. Pausing preserves the flash; interruption clears it. Tune **Failure Feedback Seconds** on the QTE director class. In PIE, deliberately miss A, B, Y, RT and X; B's red ring, enlarged button and Miss label distinguish failure from its normal red face-button color.

