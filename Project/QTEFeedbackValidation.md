# QTE music and failed-input feedback

Validated October 4, 2026, on Diversion `QTE-improvement`, over commit 91. Unreal MCP confirmed this workspace's exact project and `/Game/Maps/Lvl_Main`, with PIE stopped.

QTE music now fades to **40%** of encounter volume in both the native component default and the live `BP_SideScrollingGameMode` asset. The fade remains 0.25 seconds, preserves playback position and affects only music. Punch, charge and shot effects retain their own volume. Nonfinal rounds and interruption restore music; victory keeps its existing fade-out.

A failed input retains the expected button before the sequence cancels it. The HUD displays that button in bright red with a full red ring, a small enlargement and **Miss** for **0.4 seconds**, then the director runs the existing recovery and counterattack. The cue also covers timeout and early cannon release. The evaluated poses, placement and camera remain held during feedback; extra inputs are consumed without adding combo. Game pause retains the cue, and interruption clears it and releases input/camera ownership. `Failure Feedback Seconds` is editable on the director class.

| Check | Result |
| --- | --- |
| Editor Win64 Development | Passed; `Saved/QTEFeedbackEditorBuild.log` |
| Game Win64 Development | Passed; `Saved/QTEFeedbackGameBuild.log` |
| Full Quasicombo suite | **61 / 62 passed**; `Saved/QTEFeedbackFullSuite.json` |
| QTE suite | **10 / 10 passed** |
| Music suite | **3 / 3 passed** |
| Failure feedback | Correct red A/B/RB/X, wrong input while charging, early release, timeout, held pose/placement/camera, blocked extra inputs, delayed counter damage, pause and interruption passed |
| Live defaults after editor restart | Music multiplier 0.4; fade 0.25 seconds; failure feedback 0.4 seconds |

The only failure remains `Quasicombo.Buster.ThunderParticleColors` (the two known particle-color assertions). `Quasicombo.Boss.LizardRigAndTailAnimation` passes. Existing test-world viewport/Niagara and roar armor-bone warnings remain. The retry test now waits for the completed feedback/counter rather than assuming a two-second total; the standalone pause fixture explicitly supplies a world pause owner and asserts the world is paused.

Binary editing used Unreal MCP and saved only the game-mode asset plus the already-dirty user level actor needed to preserve its unsaved edit before rebuilding. The user actor is backed up under `Saved/QTEFeedbackBackups`, stays outside this commit, and remains a pending user change. All 11 pending user files match the baseline captured after that save. The other 47 files match the earlier protection snapshot; its two differences are the saved user actor and the user's earlier TODO edit. Ordinary animation assets and Sequencer sources were not edited. No actual editor content or map is dirty after validation; remaining dirty `/Temp/Untitled_*` worlds belong to automation and were not saved.

PIE/controller checks remain the user's: compare music against normal combat; intentionally press the wrong button on A/B/RB/X and check the correct button flashes red before the counter; try early cannon release and a timeout. B is already normally red, so its brighter flash, red ring, enlargement and Miss label make failure distinct. The editor is left on `Lvl_Main` with PIE stopped.
