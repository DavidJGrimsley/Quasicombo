# Background creatures, larvae trap, wall slide, and regeneration

Workspace: `Quasicombo-HUD`, Diversion branch `quasicombo-menu`. Completed work is committed with Diversion after validation, as requested by the user.

## Placement controls

Four reusable Blueprints live in `/Game/Quasicombo/Background`: `BP_QC_Background_StoneGolem`, `BP_QC_Background_WoodMonster`, `BP_QC_Background_Megaspikan`, and `BP_QC_Background_Larvae`.

On a new Blueprint placement, **Quasicombo → Background → Idle Only** appears directly on the actor. On an existing scenery placement, select its **BackgroundRoaming** component to find the same checkbox. Blueprint component defaults and individual placements are editable. Ordinary scenery actors default to unchecked. The current level has 15 roamers, one stationary roaring adult, and the separate larvae trap; the user's conversion of one adult to the stationary Blueprint is preserved.

Checking Idle Only loops the selected idle clip and stops walking, turning, terrain probes, and the pause clock. It preserves the complete transform and materials. Roaming controls gray out; Idle Animation stays available. Toggling during PIE freezes at the current location and resumes around the original anchor. It does not reset the placement.

Unchecking starts local random wandering with gradual turns and pauses. Adult Megaspikans use the existing Turn45Left/Turn45Right step animations for larger heading changes; rotation progresses through each full clip. Optional turn-animation references are on BackgroundRoaming. Creatures without a matching turn clip continue walking forward along a curved path, slowing for tight turns and arrival. A blocked creature does not rotate its idle pose. Idle Only also cancels an active turn immediately.

Base tuning is 600 cm radius, 90 cm/s, and 1–3 second pauses. Turn Rate defaults to 90 degrees/second for walking arcs, and Turn Animation Angle defaults to 45 degrees. Destinations leave room for curved turns within the original roaming radius. The authoring script scales speed, radius, probe size, and allowed ground step for larger creatures, with a sixfold cap. Animation playback accounts for actual movement speed and mesh scale. Unsupported terrain, obstacles, steep ground, and abrupt height changes reject movement; a blocked creature stays idle and tries again. Adult Megaspikan prefabs preserve the marketplace Blueprint's multipart attachments. Original placed actors retain their identities.

## Stationary roaring Megaspikan

`/Game/Quasicombo/Background/BP_QC_MegaspikanRoarer` is a separate reusable Blueprint for placement. It retains the adult's multipart appearance and stays at its authored position and scale. Its **AmbientRoar** component exposes idle/roar/left/right animation references, Minimum/Maximum Roar Interval, Readjust Interval, and Readjust Angle under **Quasicombo → Background Roar**.

It starts idle and schedules a random roar every 5–20 seconds, playing the complete six-second clip before returning to idle. About every 20 seconds it readjusts its legs using Turn45Left, then Turn45Right on the next cycle. Each stepping clip gradually turns the actor through 45 degrees; the right turn returns it to its original facing. Position, authored tilt, scale, and materials remain unchanged. Roars and turns do not overlap or interrupt each other; due actions wait for the full active clip to finish, so an actual start can be slightly later than its scheduled interval. It has no roaming component or combat registration.

## Nearby larvae trap

`ANIM_MegaspikanLarvae_Roar` at approximately `(-2410, 930, 704)` owns **LarvaeTrap** instead of BackgroundRoaming. The reusable prefab is `/Game/Quasicombo/Background/BP_QC_LarvaeTrap`. Its component exposes its animation references, trigger radius, mesh facing offset, release time, flight duration, and rearm cooldown under **Quasicombo → Trap**.

The default radius is 600 cm. Menus, completed runs, and boss/QTE cinematics block activation. Entry captures a live Mike once, cancels his attacks/dash/wall grip, and holds movement and gameplay input with a separate fatal-trap lock. GetOutOfTheGround plays for 2 seconds, followed by SpitAttack. At 0.4 seconds into SpitAttack, green spit travels from the jaw bones to Mike over 0.2 seconds. Scripted impact applies his remaining HP as damage through normal player death handling. The spit clip finishes, emergence reverses, and the creature hides underground. A 3-second cooldown rearms it; exit and fresh entry are required. Interruption and actor teardown release the lock and remove the spit effect. Reload resets the trap. It is cosmetic scenery with no enemy-clear registration or damageable combat interface.

## Wall assistance

Mike uses `QuasicomboPlayerMovement`. Its wall settings and observable state are on CharacterMovement under **Side Scrolling → Wall Slide**. Descending near a vertical blocking surface on either side grips for 0.2 seconds, then limits descent to 120 cm/s. Characters and tagged background creatures cannot serve as walls. Pressing away releases grip. Jump works with neutral horizontal input and for 0.12 seconds after losing contact. Existing horizontal/vertical jump impulses remain; input/reattachment lockout is 0.2 seconds. Double jump and dash remain available.

`AN_Mike_WallSlide` is an isolated one-second looping braced pose using Mike's skeleton. `ABP_Mike_SideScroller` blends it before the full-body montage slot; the native single-node animation fallback uses the same sequence. Landing, attacks, dash, hit reaction, QTE capture, trap capture, and death clear wall state.

## Combat regeneration

Actual received damage and accepted outgoing melee, charged, charged-dash, Buster, and QTE contacts refresh one combat clock. Regeneration begins after 3 seconds at 10% of maximum HP per second. A frame crossing the delay boundary heals only for its eligible fraction. Misses, charging, duplicate callbacks, and rejected enemy strikes leave the clock unchanged. QTE and fatal-trap capture suspend regeneration.

## Mike's defeat pose

Mike's death montage holds its final pose instead of automatically blending into locomotion. The native death handoff enables mesh physics collision and full ragdoll blending, pauses animation, and disables the living capsule. This keeps the corpse down through RUN LOST until the normal reload creates a new living pawn. Boss retry restoration also clears the death timer and restores the mesh attachment, collision, and animation state. `Project/CreateMikeCombatAnimations.py` preserves the death montage's hold setting when reauthoring.

The October 5 follow-up built both targets, compiled the player and AnimBP through Unreal MCP, and passed the extended `Quasicombo.Combat.MikePlayerAnimation` regression. The broader suite remained at 50/51 with only the documented Buster particle-color failure. `Project/PlaytestMikeDeath.py` passed three live checks: death montage/defeat, corpse persistence through reload, and a new living pawn with normal animation/collision/movement. Its head-bone observations and corpse screenshot are in `Saved/BackgroundGameplayValidation/`.

## Reproduction and validation

After building the editor target and reopening this workspace's project, use Unreal MCP's console to execute `Project/AuthorBackgroundGameplay.py` for idempotent asset authoring, followed by `Project/AuthorBackgroundTurnsAndRoar.py` for the stationary roar Blueprint and turn references. The second script can run on its own to update turns without resetting existing instance overrides. Both explicitly save World Partition external actor packages. `Project/AuditBackgroundGameplay.py` audits saved placements and compiles the affected Blueprints. Run `python -B Project/ValidateBackgroundGameplay.py --focused` for the nine focused native tests, or omit the flag for player/combat/Buster/QTE/route/frontend regressions. In PIE, execute `Project/PlaytestBackgroundGameplay.py` through the same MCP console for the live-level integration check. `Project/PlaytestBackgroundTurnsAndRoar.py` observes curved/stepped turns and a temporary roaring Blueprint for 60 seconds; its preview actor/camera disappear when PIE stops. Both integration scripts wait for menu initialization and the normal Start transition before testing gameplay.

Reports are written to `Saved/BackgroundGameplayValidation/` (ignored by version control).

Verified on October 5, 2026 in UE 5.8:

- Win64 Development Editor and game builds passed. Native code was reloaded by closing and reopening this project, with Live Coding/hot reload disabled.
- All **9 focused tests passed**: Idle Only defaults/toggling/placement, unsupported ground, large probes on a walkable slope with separate obstacles, stepping left/right turns and curved walking with no idle swivel, repeated random roar intervals and full-clip return to idle, wall grip/slide/jump on both sides, combat regeneration, lethal impact at full/partial HP, and trap fresh entry/rearming/interruption/teardown.
- The broader player/combat/Buster/QTE/route/frontend suite passed **50 of 51 tests**. `Quasicombo.Buster.ThunderParticleColors` still fails because the travel effect exposes no active core/spark particle data in the test world. Its assets and test were unchanged; the same failure reproduced in isolation. Muzzle/impact color checks and the remaining Buster regressions passed.
- All **8 affected Blueprints compiled**. The initial saved asset audit matched all **17 original actor identities, transforms, materials, and skeletal component counts**. The current audit preserves the user's subsequent stationary-adult conversion and authored placement while checking the other original placements against their baseline. The two adult Megaspikans retain their 14-part assemblies. All four PlayerStarts are outside the trap radius (closest approximately 896 cm).
- The instance Idle Only checkbox saved across a map reload, then saved unchecked again with the exact placement intact. Its category, disabled roaming fields, and enabled idle-animation picker were inspected in the editor. The -90-degree mesh-facing offsets were also checked after saving and reloading the map.
- The initial live-level check passed all **7 integration checks**. All 16 ordinary creatures moved within their bounds; Idle Only froze/resumed the complete transform with the original anchor. The trap faced Mike despite its authored tilt, captured him once, blocked gameplay actions, killed him at impact, declared normal defeat, and reset on reload. The saved prefab audit confirms all six scenery Blueprint defaults have their animation references and settings.
- The follow-up 50-second PIE check passed: 286 curved walking samples and 137 stepping-turn samples had no idle swivel, with all 16 roamers staying inside their bounds. A temporary `BP_QC_MegaspikanRoarer` retained all 14 skeletal parts and its exact transform, started roars after 15.33, 10.67, and 10.33 seconds, and returned to idle after each complete six-second clip. The temporary preview was removed by stopping PIE.
- The subsequent 60-second PIE check passed with the 5–20 second roar schedule and alternating readjustments: 363 curved walking samples and 80 stepping-turn samples had no idle swivel across the current 15 roamers. The stationary preview played Turn45Left at 21.00 seconds and Turn45Right at 41.00 seconds, reached both 45-degree endpoints, returned to idle, and preserved position, scale, tilt, and all 14 skeletal parts. Five roars began at intervals of 14.67, 11.33, 6.67, 11.33, and 11.67 seconds; four completed within the observation window. No roar overlapped or interrupted a stepping turn. Both native targets built again, all nine focused tests passed again, and the saved audit confirmed the placed stationary adult's settings after map reload.
- The braced wall clip was inspected, and Mike's wall-animation flag activated in live PIE. Green spit was visually confirmed in flight and its green material parameters inspected.

The QTE automation fixture now holds its transient HUD widget with a strong reference during world ticks. This prevents the widget from being collected between scripted prompt checks; all QTE regressions passed. `.dvignore` now explicitly includes the Content root so Diversion tracks the seven new assets as well as existing content.

Physical keyboard/controller feel still needs manual acceptance. Automated tests exercise neutral wall jumps, both wall directions, contact grace, detachment cooldown, and capture cleanup, but do not establish controller ergonomics. Native computer input remained unavailable after the Escape interruption; same-frame MCP key injection was insufficient for a reliable hardware playtest.
