# QTE reward eligibility, corpse physics and music

Validated on October 4, 2026, in `Quasicombo-QTE-improvement` on Diversion branch `QTE-improvement`, over integration commit 90. Unreal MCP verified the exact workspace project and `/Game/Maps/Lvl_Main`; PIE was stopped. Binary assets and existing pending user files were not edited.

Final successful QTEs check Mike's equipped cannon before spawning a reward. When equipped, the selected death/recovery or explosion outro completes and the director releases animation, camera and input before ending the run. When unequipped, the existing palette-matched pickup and full acquisition/calibration animation remain; the victory screen waits for that animation. Nonfinal rounds and failures still return to combat. The legacy death callback applies the same equipment check after the complete death duration.

The lizard now continues into gravity-driven ragdoll after its death animation. Removed `UpdateCorpseSettle` and its timed gravity removal, zeroing and forced body sleep. Kept world-floor collision, bounded handoff velocity, damping and the actor-local protection against imported physics-body self-collision launches. Ordinary animations and physics assets were not changed. The sixth-beat X/Hold/Release freeze is retained during the input windows; it is separate from completed corpse behavior.

Music ducking was already implemented in commit 90. This change verifies it through actual boss entry, nonfinal counter and interrupted-QTE transitions, including no song restart. The live game mode defaults remain a **0.1** multiplier and **0.25-second** fade. Punch/contact, charge, fire and impact sounds stay outside the music gain; the live charge component retains volume 1. Final victory preserves the existing fade-out. The presentation asset has all four punch effects and dedicated charge/fire/impact sounds assigned.

| Check | Result |
| --- | --- |
| Editor Win64 Development | Passed; `Saved/QTERewardCorpseEditorBuild.log` |
| Game Win64 Development | Passed; `Saved/QTERewardCorpseGameBuild.log` |
| Full Quasicombo suite | **60 / 61 passed**; `Saved/QTERewardCorpseFullSuite.json` |
| QTE suite | **9 / 9 passed** |
| Music suite | **3 / 3 passed** |
| Reward eligibility | Passed for equipped and unequipped endings, both facings, nonfinal rounds, failures and the legacy death callback |
| Corpse physics | Passed through six seconds after death with a physical floor, retained gravity, simulation and bounded motion; short-prefix deaths also lower the head by more than 10 cm, proving the visible stumble collapses |
| Acquisition | Existing complete pickup/calibration and deferred-victory checks passed |
| Preservation | All **49** protected ordinary files and **11** pending user files retained their starting SHA-256 hashes |

The only failure remains `Quasicombo.Buster.ThunderParticleColors`: early active core/electricity particle-color assertions. `Quasicombo.Boss.LizardRigAndTailAnimation` passes. No Invalid Simulate Options warnings occurred; existing test-world viewport/Niagara and incoming roar armor-bone warnings remain. Test fixtures use fixed 20 ms steps for gameplay timers and allow the standard small walking clearance after collision is restored; airborne holds and authored contact checks retain their strict tolerances.

For PIE, check prefixes 2–5 without the cannon for a matching pickup and complete acquisition before victory. Repeat with the cannon equipped: no pickup, no duplicate acquisition, and victory after the QTE outro. Count six should keep the full explosion/camera ending, then show victory without a pickup. Watch the short-prefix death stumble continue into a natural collapse; check music is quiet under prompts while hits, charge and fire remain audible, and restores between rounds or on interruption. The editor is left on `Lvl_Main`; visual/controller checks remain the user's.
