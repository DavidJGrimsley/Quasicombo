# Fresh-session prompt: airborne boss QTE through Unreal MCP

Implement the following changes in the existing QTE workspace. Use the Unreal MCP connection to inspect and author assets in the editor. First discover the actual tools, verify the connected editor's project path, and report whether you can manipulate Sequencer, animation assets and Control Rig. If Unreal MCP is not exposed, tell me what connection is missing before substituting a different editor workflow. Do not claim shell/Python operations are MCP access.

## Workspace and current work

- Work in `C:/Users/DJLeg/Documents/Unreal Projects/Quasicombo-QTE-improvement/Braided_Quanta2026.uproject`, Unreal 5.8. Read this workspace's `AGENTS.md`. Use Diversion (`dv`), not Git.
- At handoff, branch `QTE-improvement` is over commit 74. Existing review: https://app.diversion.dev/repo/dv.repo.a4601f20-a84b-4d1a-8eb3-e8348e3b27c8/reviews/dv.review.3c5b00de-3e9a-44a7-962a-ea247ba79121 . Recheck status before editing.
- Existing user edits at handoff: `Content/RadicalMike/Animations/Anim_ZMIKE_Jump.uasset`, two external actors in `Content/__ExternalActors__/Maps/Lvl_Main/` (0/U8/FJZBVZ3YOR87Z1N06UPRVY and 3/MH/6SGRCGI04R822MHW1EI64W), and `Project/TODO.md`. Preserve them; inspect any additional changes. Do not commit unrelated edits or merge without authorization.
- The separate Time Evolution workspace is `C:/Users/DJLeg/Documents/Unreal Projects/Quasicombo-time-evolution`; its plan is `Project/TimeEvolutionImprovementPlan.md`. Its current status must be checked rather than inferred from older documentation. Do not modify that workspace. Integration remains a separate review checkpoint.
- Read `Project/QTECinematics.md` for the existing rules, implementation and validation. Its grounded uppercut/running kick description is superseded by the airborne design below.

## Requested visual change

The opening stays: approach at a believable pace, squat and hold at the A prompt, A starts JUST a jump, then the early uppercut anticipation holds at the B prompt. B continues the uppercut and makes the first attack contact. No jump damage or reaction. Preserve the recent fix that holds the actual evaluated pose without fading to idle.

After the uppercut, Mike should remain airborne. Use a dedicated QTE copy of `/Game/RadicalMike/Animations/Anim_ZMIKE_JumpApex` as the airborne transition/holding pose between the remaining moves: uppercut → apex → chomp → apex → claw → apex → flying kick. Do not add an apex button or combo point. Blend the attack poses into and out of the apex pose without ground idle, landing snaps or abrupt restarts. Prompts should appear at meaningful authored transition/anticipation points; hold the visible pose and placement while awaiting input, then continue smoothly. Input deadlines still run in real time.

Replace the fifth move's running kick with a dedicated flying kick authored from a COPY of JumpApex on Mike's skeleton. Mike is already in the air: chamber one leg, extend one foot into a clear impact, then follow through. Remove the fifth move's running approach and ground placement. Coordinate Mike's trajectory, contact height, lizard reaction and camera so the foot visibly connects in both facing directions. Do not change Mike's ordinary locomotion, jump, combat animations, montages, notifies or playback settings.

Land/recover deliberately when the selected prefix ends or the failure/counter flow requires it. A two-prompt result still ends with the uppercut; three ends with chomp; four with claw. Five earns the flying kick launch. There is NO automatic finishing kick. Six adds the cannon finish after the kick launches the lizard upward, backward and slightly into the background. Preserve the charge timing and tracking shot; give Mike a coherent transition into his charge/aim/fire pose rather than a forced ground snap.

## Make manual editing understandable

I want to adjust the choreography in Unreal's editor through Sequencer/Control Rig. Leave clearly named editable sources and explain exactly which assets I should open to change apex transitions, flying kick, charge/fire, contact timing and camera framing.

The existing Sequencer sources are AUTHORING assets. Current gameplay does NOT play them as Level Sequences: it plays their baked AnimSequences using dynamic montages, while C++ drives actor placement and cameras. Editing a source only affects gameplay after rebaking. Make that distinction explicit.

Prefer an editor workflow where the editable sequence is the source of truth for choreography. Reuse the existing QTE input/quantum/health/combo logic. You may connect editor-authored sequence segments to that logic if it provides a clear, reliable workflow; do not silently implement a second competing system. Ensure only one system owns each actor's transform and animation at a time. If retaining baked playback, expose the airborne timing/path settings for manual tuning and provide a straightforward editor bake/update operation. Do not force me to edit C++ every time I move a contact pose or change a kick.

## Existing assets and code to inspect

- `/Game/Quasicombo/QTE/DA_QTEPresentation`: editable presentation references, beat definitions, prompt/contact fractions, durations and camera settings.
- `/Game/Quasicombo/QTE/Sources/LS_QTE_*`: eleven paired editable FK Control Rig sources, including Uppercut, Chomp, Claw, RunningKick, Cannon and CannonFire.
- `/Game/Quasicombo/QTE/Baked/AS_QTE_*`: twenty-two dedicated gameplay clips. Fifth beat currently points to `AS_QTE_RunningKick_Mike`.
- Existing cannon charge is `AS_QTE_Cannon_Mike`, based on `Anim_ZMIKE_BusterIdle_R` with subtle motion; fire is `AS_QTE_CannonFire_Mike`, based on `Anim_ZMIKE_FireR`. They are separate charge and release clips. Inspect and improve continuity with the new airborne choreography.
- Runtime directory: `Source/Braided_Quanta2026/Quasicombo/QTE/`. Inspect `QuasicomboQTEDirector.cpp`, particularly `BeginAnticipation`, `SetMikePose`, `Accepted`, `UpdateMikeActionPosition`, `Contact`, `UpdateFlight`, `Result`, `Finish` and `Cleanup`.
- `UpdateMikeActionPosition` currently lowers Mike to the ground after uppercut and puts the fifth kick on the ground. This MUST change; an airborne clip swap alone cannot solve it.
- Preserve the montage fix: disable automatic blend-out BEFORE `Montage_Play` initializes the instance. Mike's montage cursor and actor placement currently share the director's real-time clock; prompts freeze the live pose at full slot weight.
- `QuasicomboQTEPresentation.h` defines the data asset. `QuasicomboQTEComponent`, `QuasicomboQTESequence` and `QuasicomboQTERules` own input/charge/selection behavior.
- `Project/AuthorQTECinematics.py` creates sources, bakes and default references. Rerunning it can reassign data asset references/timing, so preserve manual work and update its defaults deliberately. `Project/BakeQTECinematics.py` rebakes retained sources. `Project/ValidateQTECinematics.py` checks assets. Do not modify binary assets outside Unreal.

## Rules that must continue working

- Quantum rounds: uncertainty `1 - abs(2 * Tau - 1)`; thresholds at one-third/two-thirds give 1/2/3 rounds. Final prompt count `clamp(2 + floor((maximum - 1) * Tau), 2, maximum)`; maximum 5, or 6 with cannon. Earlier rounds `max(2, finalCount - roundsRemaining)`. Snapshot once; failures repeat count/pattern.
- Xbox glyphs only. Vacuum pattern A → B → B → A → B → X; Tau A → B → B → RB → B → X. Ordinary deadlines 1.5 real-time seconds. Rapid input during actions cannot skip moves.
- PIE preview defaults to two prompts/one round. Plain 2–6 choose count BEFORE boss health depletion; they do not trigger the QTE. Six equips cannon temporarily for that PIE run. Editor properties choose 1–3 rounds and button family. Restart clears overrides; packaged game has none.
- Pause combo expiry for the whole QTE. Every accepted prompt adds exactly one point. Apex transitions, contacts, cannon release and impact add none. Cannon hold-start counts once. Failure clears combo. Successful nonfinal counter preserves it.
- Successful nonfinal rounds advance; failures repeat. Both counter outcomes halve Mike's current HP, restore full boss health and stored shields/armor/resistance/strike counters, then return to combat. Do not rerun evolution/API to restore armor.
- Cannon: 1.5 seconds to begin holding, one-second charge, then 0.6-second release window. Early release, overholding, wrong input and cancellation fail. Holding ramps rumble about 15%→60%; disabled vibration stays disabled. Stop rumble on release/failure/pause/disconnection/interruption.
- Normal projectile visual diameter 114 cm; cinematic starts at 342 cm with exposed multiplier. Ordinary damage/collision/cadence stay unchanged. Only successful final six destroys the lizard at peak explosion coverage. Reward and cleanup remain independent of boss lifetime; grounded reward must be reachable.
- Every exit restores camera, time, movement, collision, animation ownership, effects, timers and rumble. Preserve the existing distance-based opening pace (220 cm/s maximum), hit stops and final slow impact.

## Validation and handback

I will perform the PIE visual and controller tests myself. Implement and run appropriate builds/automation, then give me exact setup and expected behavior for short checks: (1) apex transitions/flying kick/contact, (2) rounds/combo, (3) charge/rumble/obliteration. Do not spend time driving PIE with computer use to judge animation quality for me.

Add meaningful checks for staying airborne after uppercut, apex transition/hold continuity, no fade to idle, valid contact, no extra combo increments, both facing directions and recovery/cleanup. Update existing tests that currently EXPECT a landing after uppercut. Retain the live world/animation evaluation checks, not just montage cursor checks.

Before this new change, Editor and Win64 Development builds passed; all eight QTE tests passed; full Quasicombo automation passed 44/46. Two failures reproduced in baseline: `Quasicombo.Boss.LizardRigAndTailAnimation` (imported mesh bounds versus capsule-bottom clearance) and `Quasicombo.Buster.ThunderParticleColors` (no active core/electricity particle data at its 0.05-second sample, not wrong color values). Do not weaken those assertions. Reports are under `Saved/QTEPoseHold*` and `Saved/QTEBaselineAutomation.log`.

Finish with a simple explanation of which sequence/clip to edit, how to publish that edit to gameplay, where to adjust prompt/contact timing, and what remains for my visual test. Keep the explanation concrete and short. Do not merge to main until visual checks and evolution integration have been reviewed.
