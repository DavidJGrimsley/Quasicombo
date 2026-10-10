# Boss approach lighting and music

The `time-evolution` workspace now has one music controller on the SideScrolling GameMode and a reusable `/Game/Quasicombo/Boss/BP_BossApproachVolume` actor. Forest ambience continues independently, with the same volume and three-second intro fade. It loops on the audio device so slow gameplay timers cannot leave gaps during stalls or time dilation.

## Placement and tuning

The reusable Blueprint is `/Game/Quasicombo/Boss/BP_BossApproachVolume`. Its placed instance was deleted from this branch during merge cleanup. After merging, drag a fresh instance into the main level, resize its box, and move its start/end widgets to the climb endpoints. Assign its `Boss` reference explicitly or leave it empty to find the nearest boss at BeginPlay.

Widget coordinates are local to the actor; translation, rotation and scale transform both endpoints. Progress projects the first local player's world X/Z position onto that line, ignoring Y, and clamps to 0-1. Sunlight changes linearly from 20 to 4, and route music gain changes from 1 to 0. Backtracking reverses both effects. A bottom exit restores the start values, an arena-end exit holds the end values, and a side exit holds the last values.

Combat takes ownership at intensity 4. Transformation fades to 0.2, then returns to `PostEvolutionIntensity` (default 1) when it completes or is interrupted. Retries, pickup and victory retain 1. Defeat or encounter removal restores the sunlight captured before approach dimming.

`Lvl_Main.umap` has no branch changes. The branch still contains an earlier saved change to the `QC_Boss` external actor at `Content/__ExternalActors__/Maps/Lvl_Main/3/MH/6SGRCGI04R822MHW1EI64W.uasset` from commit 73. Other pending level actor edits in this workspace are excluded from the cleanup commit. Inspect the Diversion merge preview before merging.

## Tracks

| Situation | SoundWave |
|---|---|
| Default / Route B | `/Game/Audio/Quantum_Ambience` |
| Route A | `/Game/Audio/Music/SW_RouteA` — Quantum Blur Low Shimmer |
| Route C | `/Game/Audio/Music/SW_RouteC` — Quantum Blur Low Selective |
| Boss combat, QTE and retries | `/Game/Audio/Music/SW_Boss` — Quantum Blur 01 |
| Transformation cinematic | `/Game/Audio/Music/SW_Evolution` — Quantum Blur 02 |

All five supplied MP3 originals are preserved unchanged under `Project/AudioSource/Music`; Downloads is unchanged. Four gameplay tracks have matching 48 kHz, stereo, 16-bit WAV sources and looping SoundWaves. TrailerMusic remains available as an MP3 source for later use.

The GameMode's `MusicController` exposes soft track references, `CrossfadeSeconds` (default 1), `EncounterVolume` (0.7) and `ApproachMusicGain` (1). Repeated requests for the current track do not restart it. Route changes happen only after an accepted commitment. A different route track seeks to the outgoing song's current audio-device timestamp, including when returning to a previously stopped track. If the incoming file is shorter, the timestamp wraps within that file's duration. Crossfades use elapsed real time, including during time dilation; audio pitch stays at 1. Silent route music keeps playing so backtracking does not restart it. Transformation starts its track at zero and takes priority over optional boss overrides and quantum callbacks. Completion or interruption returns to the boss track. Boss defeat fades music out over 0.8 seconds.

The temporary music clock and route trigger diagnostics were removed during merge cleanup. Audio-device sample positions still drive route seeks. Pausing freezes playback while gameplay time dilation leaves music pitch and playback speed unchanged.

Transformation music uses `EvolutionVolumeMultiplier` (default 0.5) on top of `EncounterVolume`, so its output gain is 0.35 instead of 0.7. Returning to boss combat restores gain 0.7.

`Project/ImportBossApproachMusic.py` imports the four WAVs and creates the approach Blueprint. It saves only these assets. To create a new WAV from a supplied source, use `ffmpeg -i "input.mp3" -ar 48000 -ac 2 -c:a pcm_s16le "output.wav"`.

## Validation

Build `Braided_Quanta2026Editor Win64 Development`. Focused automation filters are `Quasicombo.Approach`, `Quasicombo.Music`, `Quasicombo.Boss`, `Quasicombo.Evolution` and `Quasicombo.Buster.Acquisition`.

The full rendered regression is `Project/ValidateBossApproachMusic.py`. It requires a placed approach volume using the original test placement: actor `(27000, 1000, 100)`, box extents `(1550, 400, 1300)`, local start `(-1500, 0, -850)` and end `(1500, 0, 1100)`. Import it in the editor Python console, then start a floating PIE session through Unreal MCP. When the first session ends, start PIE again for the interrupted transformation and defeat scenario. The script changes runtime actors only and saves no assets. It checks accepted/rejected route changes, a natural Route C audio loop, approach endpoints/midpoint/backtracking/exits, encounter takeover, cinematic priority, QTE, retry, pickup/victory darkness and defeat restoration. It writes its report and captures under `Saved/BossApproachValidation/Rendered`; automation results are under `Saved/BossApproachValidation/Automation`. For the current level without a placed box, use `ValidateBossLighting.py` and the automation approach fixtures.

The Development Editor build and all 16 focused automation tests passed. The rendered report contains 28 passing checkpoints. Route C played past its 81.048-second duration without the controller restarting it. All pickup stages and victory stayed at intensity 1, and defeat restored intensity 20. The 12 baseline map, actor, project/config and task files matched their original hashes after validation.

A separate final PIE check reused the piano as a boss override without restarting playback, verified encounter volume 0.7, and observed forest ambience past its 28.633-second loop during 0.4× time dilation. Music pitch remained 1. Its result is `Saved/BossApproachValidation/Rendered/shared_override.json`.

The route timestamp regression is `Project/ValidateRouteMusicTimestamp.py`: run it from the editor Python console, then start a fresh rendered PIE session through MCP. It checks actual route seek positions, returning to Route B, a natural loop boundary (seeked near its end), 0.4× time dilation, pause, silence/backtracking, and the clean gameplay HUD. Reports and captures are under `Saved/BossApproachValidation/RouteTimestamp`; the corresponding automation test is `Quasicombo.Music.RoutePlaybackContinuity`.

After the timestamp correction, the Development Editor build and all 17 focused automation tests passed, along with 14 rendered timestamp checkpoints. Accepted A/B/C changes sought to the outgoing audio position, including returning to the piano and switching after a loop. The clock froze during pause, ran at normal speed during 0.4× gameplay time dilation, and continued through stair silence without restarting. All 12 baseline files remained unchanged.

After lowering the approach/combat target to 6 and halving transformation music, the Development Editor build and all 17 focused tests passed again. Seven rendered checkpoints verified approach sunlight 20/13/6, combat 6 with boss gain 0.7, evolution 1 with gain 0.35, return to boss gain 0.7, and encounter removal restoring sunlight 20. Results are under `Saved/BossApproachValidation/LightingMix`. The placed Blueprint inherited the new defaults without saving any actor or map package; the 12 baseline files remained unchanged.

The final lighting targets are approach/combat 4, transformation 0.2, and post-transformation 1. `Project/ValidateBossLighting.py` checks those values and the unchanged half-volume transformation track. It also works when no approach volume is placed. `Quasicombo.Approach.InitialOverlapAndPlayerFilter` spawns the actual reusable Blueprint and verifies it finds the boss and inherits intensity 4. `Quasicombo.Route.ThreeSectionsPhysicalOverlaps` covers all three final lanes through genuine capsule/box overlaps.

Merge cleanup passed the Development Editor build and all 22 focused approach, music, boss, evolution, pickup and route tests (17 had fixture warnings, zero failed). Four rendered lighting/music checkpoints verified combat 4, cinematic 0.2 at gain 0.35, return to 1 at boss gain 0.7, and removal restoring 20. All 14 route timestamp checkpoints passed again with no music debug widget. Combat, transformation and route screenshots show the normal HUD without music, lighting or evolution diagnostics. The 21 preserved workspace files retained their baseline hashes, and the editor had no dirty map or content packages after validation.
