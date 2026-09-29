# Placing Quasicombo enemies

Place `/Game/Quasicombo/Enemies/BP_QC_Enemy` in `Lvl_Main`. Select each placed actor and set **Quasicombo → Variant → Species** to Wood Monster or Stone Golem and **Tier** to Light, Medium, or Heavy. The Blueprint applies its mesh, materials, capsule fit, animations, scale, hit count, and damage from those two settings in the editor and at play start.

For each lane, select its entry actor in the World Outliner (for example `QC_A1_Entry`). In **Quasicombo → Encounter → Enemies**, remove the empty slot left by the deleted enemy, then add a reference to every **placed actor** assigned to that lane. Drag each actor from the World Outliner into an array slot, or use the eyedropper. Do not assign the Blueprint asset itself. Save `Lvl_Main` after wiring.

The existing route links are already set: each entry's **Section** points to `QC_Section_1`, `QC_Section_2`, or `QC_Section_3`; **Forward Barrier** points to its lane's `QC_*_ExitGate`; each section's **Legs** contains A, B, and C; and the final `QC_*3_ArenaTransfer` has its matching **Final Leg**. Check these links only if you change route actors.

Assigned enemies are inactive until their lane is selected. The selected exit opens after every assigned enemy dies or is destroyed. An entry with no valid enemies opens immediately on selection. An intro enemy, if placed, is independent of the route-entry arrays.

## Size and combat tuning

The current light/medium/heavy actor scales are **1.1 / 1.2375 / 1.375**, a 10% increase over the original tier sizes. Hit counts are wood 3/4/5 hits and golem 4/5/6 hits. Melee damage scales by tier: medium deals 2× light and heavy deals 3× light (wood 1.0 / 2.0 / 3.0 damage, golem 1.5 / 3.0 / 4.5 damage). Both species use a screen-space health bar above the head. Hit reactions use the original light/heavy enemies' pelvis anchor, physics-body mesh collision, and damage effect. Player punches hit the capsule. A surviving enemy gets the original single-hit hop; subsequent hits cannot add another upward launch while it is airborne. Death uses the original full ragdoll and incoming impulse.

Distances below are Unreal centimeters; times are seconds.

| Setting | Current value | Meaning |
|---|---:|---|
| Required Hits | By species/tier | Accepted player strikes needed to defeat this enemy. |
| Aggro Range | 800 | Horizontal distance at which it notices the player on its lane. |
| Attack Range | 150 | Horizontal distance at which it starts a melee attack. |
| Patrol Half Width | 700 | Pursuit boundary on either side of its spawn X position. |
| Attack Cooldown | 1.0 | Minimum interval between attack starts; animation and recovery can extend it. |
| Attack Windup | 0.2 | Fallback strike delay if the animation notify does not fire. |
| Charged Attack Windup | 0.9 | Fallback delay for a charged attack; these enemies currently use normal strikes. |
| Recovery Duration | 0.25 | Pause after an attack finishes. |
| Hit Reaction Duration | 0.3 | Pause after receiving a strike. |
| Attack Timeout | 2.5 | Safety limit that ends a stuck attack. |

Wood and golem strike montages play at **1.2×** their original speed. The red, green, blue, and white rings around a selected actor in the editor are the rotation gizmo; they do not show aggro or attack range.
