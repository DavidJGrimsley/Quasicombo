# Mike player character

## Architecture

`BP_SideScrollingCharacter` remains the playable pawn in `Lvl_Main`. Its C++
base owns movement, dash velocity and duration, melee damage, charged attacks,
health, QTE, boss interaction, and run defeat. Mike supplies the skeletal mesh
and presentation. Radical Buster gameplay is outside this pass.

The marketplace package lives at `/Game/RadicalMike`. The active mesh is
`SKM_MegaMikeZ` with `SK_MegaMikeZ` and `PA_MegaMikeZ`. Its confirmed combat
bones are `hand_r`, `hand_l`, and `pelvis`. Mike-specific authored assets live
under `/Game/Quasicombo/Mike` and leave the source animation sequences intact.

## Animation assets

- `ABP_Mike_SideScroller` uses the side-scroller movement state machine,
  `BS_Mike_Locomotion`, Mike jump start, airborne/apex, and landing clips, and
  the full-body `DefaultSlot`. The template's Manny foot Control Rig is bypassed
  because it targets a different skeleton. Root motion is disabled on the Mike
  source clips; Character Movement stays authoritative. The blend space has 15
  samples and 16 interpolation triangles. Directly writing samples without
  rebuilding those triangles produces a frozen walk pose.
- `AM_Mike_Combo1` through `Combo5` play right punch, left uppercut, right
  punch, left punch, and right uppercut. The copied sequences trace from the
  matching hand bones at 0.30 seconds for punches and 0.48 for uppercuts.
  Combo checks occur at 0.52 and 0.78 seconds respectively. Punch montages are
  trimmed to 0.65 seconds and play at rate 2.4; uppercuts are trimmed to 0.90
  seconds and play at the earlier 2.0 rate. Four rapid follow-up presses can be
  buffered, and a fresh combo always starts with the right punch. Montage
  blend-in/out times are 0.04/0.06 seconds.
- Mike's base melee damage is 1.0, with `MikeComboDamageStep` set to 0.25. The
  five stages deal 1.0, 1.25, 1.5, 1.75, and 2.0 damage by default. Both the
  base damage and step can be adjusted in `BP_SideScrollingCharacter` under
  **Combat > Melee Attack > Damage**. Charged attacks retain base melee damage.
  Side-scrolling enemies and the boss currently require a set number of accepted
  strikes, so this damage increase does not shorten their required hit counts.
- The right punch plays `SFX_Mike_Punch1`, the left punch plays `Punch2`, and
  both uppercuts play `Punch3` at their trace frames. These SoundWaves come
  from the supplied `punch_01/02/03.ogg` effects, converted to 48 kHz PCM WAV
  in `Project/AudioSource/Mike` for Unreal import. All three use
  `/Game/Quasicombo/Mike/Sounds/SC_Mike_Punches`; open that Sound Class in the
  Content Browser and change **Volume** in its Details panel to tune every
  Mike punch together. Its starting value is 0.8. Rerunning the import script
  preserves later edits to this Sound Class volume.
- `AM_Mike_ChargeHold` loops IdleAggro while held. `AM_Mike_ChargeRelease`
  plays a distinct ClawR strike with a `hand_r` trace at 0.40 seconds, at rate 1.5.
- `AM_Mike_Dash` loops Run_Faster at rate 1.0 until its gameplay timer ends.
  Dash duration is 0.97 seconds, matching the original platforming montage's
  roughly 0.967-second length. This timer remains authoritative if the montage
  cannot play.
- Two copies of the platforming `NS_Jump_Trail` attach to Mike's feet during
  jumps and dashes. They use the Niagara `User.Color` parameter set to the
  default Mike body material's blue light color. Trails deactivate on landing.
- The melee gamepad button is face-right (`B`); mouse punch remains left click.
- Directional hit montages play the front, back, left, or right Mike reaction.
  Nonfatal hits interrupt attacks without partial ragdoll. Fatal damage disables
  movement, plays the 0.9-second death clip, and starts physics ragdoll just
  before the clip ends. Player death extends the level reload to 2.6 seconds so
  the fall and ragdoll are visible. Other defeat paths retain their 1.5-second
  reload.

The existing native animation and timer-based melee fallbacks remain available
when the AnimBP or a montage cannot play. The old Manny montage properties are
still present for the non-Mike character path.

## Rebuild and verification

The Unreal Python authoring scripts are in `Project/`:

1. `InspectMikeAssets.py` checks the relocated source assets.
2. `CreateMikeLocomotion.py` creates the blend space and AnimBP.
3. `ImportMikePunchSounds.py` imports the three WAV sources as SoundWaves.
4. `CreateMikeCombatAnimations.py` creates the copied notify sequences and
   montages, including sound notifies at each combo impact.
5. `WireMikePlayer.py` assigns the assets to the player Blueprint.
6. `VerifyMikePlayer.py` reloads and audits the saved mesh, skeleton, AnimBP,
   montage sections, slots, sequences, sound notifies, and Blueprint references.

Run these through Unreal Editor's `-ExecutePythonScript=` option from the
project root after building the editor target. `CreateMikeLocomotion.py` calls
the editor-only `Quasicombo.RebuildMikeBlendSpace` command to triangulate and
save the samples. `CreateMikeCombatAnimations.py` calls
`Quasicombo.RefreshMikeMontageLengths` to save exact clip endpoints.
`CompileQuasicomboBlueprints.py` also checks the player and enemy Blueprints.
In the editor, build the `Braided_Quanta2026Editor` Win64
Development target before PIE. If another Unreal session has Live Coding open,
use a normal editor build with `-NoHotReload -NoHotReloadFromIDE`.

Automated checks completed on 2026-09-27: editor and game target builds, Mike
saved-asset audit, player Animation Blueprint compilation, four combat tests,
and three route tests. The Mike test verifies moving leg poses, all three combo
hits after rapid taps, the distinct charged release, foot trail components,
the dash timer, and death-to-ragdoll timing. Level alignment and feel still
require PIE judgment. Restart an already open editor before checking; it may
hold the old blend space and compiled code in memory.

The punch SoundWaves, Sound Class, sequence notifies, and five combo montage
links passed the saved-asset audit on 2026-09-27. The Mike player animation
test also passed with five buffered taps in order. The final sound mix, damage
balance, and uppercut timing still need a brief PIE check.

## Two quick PIE checks in `Lvl_Main`

1. Stand and move in both directions. Confirm Mike's size, facing, and feet
   align with the floor and capsule. The mesh currently inherits location
   `(0, 0, -90)`, yaw `-90`, and scale `1` from the existing player Blueprint.
   Adjust the mesh component only if alignment needs correction.
2. Try idle/walk/run, jump and landing, a full gap jump with dash, five quick
   `B`/left-click taps, charged hold and release, a nonfatal hit, and death.
   Check the blue foot trails, the five moves in order, impact timing, the
   softer punch sounds, chaining, dash distance, and whether both the death
   clip and ragdoll are visible before reset.

If a trace or physics bone needs diagnosis in PIE, run `DumpCharacterRigInfo`
in the Unreal console and inspect the Output Log.
