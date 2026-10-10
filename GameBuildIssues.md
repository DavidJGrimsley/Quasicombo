# Game Build Issues

## Current Windows release — 2026-10-10

The October 10 build includes the merged menu and current level edits, How to Play, pause **Restart/Quit**, the **seven-second** combo countdown, countdown suspension during evolution cinematics and QTEs, boss enemy cleanup, rounds display, and the pending arena animations and controller changes. The previous September GitHub release and October 5 local build are retained.

Fresh **complete Development and Shipping cooks, builds, packages and archives passed with 0 errors and 4 imported-content warnings each**. Both cook logs explicitly report a full cook and deletion of previously cooked packages. The updated private Quantum configuration was saved before these builds: live braid and evolution requests passed in PIE without fallback or failure. An earlier authentication diagnostic was superseded by the owner's working key update; that earlier ZIP is preserved separately under `BeforeKeyUpdate` and is not the release build.

Validation evidence is under ignored `Saved/Releases/2026-10-10`:

- **44 unique native regressions passed** across 55 test executions, covering frontend, route/quantum, score/loadout, boss/QTE, evolution and victory behavior.
- **19 frontend PIE checks** passed for intro → Home → Start, saved preferences, customization/settings, death/retry and victory/replay.
- **8 pause/combo PIE checks** passed for seven-second expiry, pause holds, Restart state reset and retained preferences, pause after Restart, and Quit.
- **4 evolution countdown checks** passed. A 10.09-second evolution cinematic held the countdown at 7 seconds; it resumed afterward and expired normally. Native QTE interruption checks cover countdown suspension during QTE presentation.
- Live boss checks cleared **38 combat enemies**, preserved **16 scenery/trap actors**, and verified the top-center **“Rounds left: 2”** display.
- The saved main map reopened with **2,340 floating mesh instances in 13 components**, including both floater variants and secondary detail. All nine pending arena animations loaded with valid skeletons and nonzero durations. Four-position PIE checks retained the formations, and the landscape cache remains `SerializeOnlyAtCook` with `SerializeAll` contents.
- The Development executable completed intro → Home → Start. Required Mike tint materials loaded; landscape projection generated nonzero point sets up to **250 points**. Its runtime log has no errors, missing required materials, quantum failures or PCG landscape-cache serialization failures.
- A fresh Shipping extraction passed archive CRC and all **29 file SHA-256 checks**. Its desktop smoke test covered intro/Home/Start, How to Play and scrolling, movement/jump, visible landscape and floating scenery, pause/Resume, Restart, Quit, and music-setting persistence across executable relaunch. Original saved preferences were restored after testing.

The complete ZIP, including the runtime and Visual C++ prerequisite installer, is **`F:\UE_Builds\Quasicombo\2026-10-10\Quasicombo-Win64-2026-10-10.zip`**. Its verified C: original is under `Saved/Releases/2026-10-10`.

- Size: **2,617,042,375 bytes**.
- SHA-256: **`40b747e058ea6c9a487f903e0a4af3f6fdc0fb9836e61d69397acbb0ec9aba60`**.
- GitHub release: [release-2026-10-10](https://github.com/DavidJGrimsley/Quasicombo/releases/tag/release-2026-10-10). Download both numbered parts and follow `README.txt` or `Reassemble.ps1`; the full ZIP exceeds GitHub's per-asset limit.
- `ReleaseManifest.json` records the Diversion and GitHub revisions, ZIP and part hashes, build configuration and validation limits. `SourceManifest.json` and `PublicExportManifest.json` record the matching private build inputs and sanitized public source hashes.

All pending non-ignored project work is included in the release source commit. The public GitHub export includes Source, Config, the Quantum API plugin source, Project scripts/docs, documentation and the project descriptor; configured credentials, generated output, editable Content assets and raw audio/animation source files are excluded. Direct Quantum API integration remains selected in the private runtime.

### Remaining limits and later cleanup

World Partition streaming remains **disabled** in `Lvl_Main` (`bEnableStreaming=False`). The floating actor is non-spatially loaded, but these checks verify availability with all cells loaded; they do not establish cell unloading/reloading. Its latest placement and controls documented below were preserved.

The four cook warnings below remain nonfatal. Development runtime also retains imported PCG partition warnings about offline generation, an empty-intro null-class spawn warning, and existing Niagara activation warnings. No runtime errors accompanied the smoke run. Shipping logging is compiled out, so log inspection used Development. Packaged-game testing was a short smoke test; the detailed boss/QTE, death/retry and victory/replay regressions used native automation and PIE.

The older status sections below are historical records; use the October 10 ZIP and GitHub release for the current build.

## Project update — 2026-10-06

The pause page now includes **Restart** and **Quit**. Restart reloads the current gameplay map with fresh health, combo and run state, preserving preferences and the current loadout. Quit saves preferences and exits the game. Both controls use the existing menu styling and focus/hover feedback.

The hit-combo countdown is now **7 seconds** in both the native character default and `BP_SideScrollingCharacter`. The HUD reads the character's remaining time directly. The existing QTE interruption regression now waits for the configured remaining timer, including restored slow-motion dilation.

Live PIE checks passed for the seven-second countdown, expiry beyond the old five-second boundary, frozen combo/run clocks while paused, Restart state reset and preference retention, pausing after Restart, and the Quit button ending the game. The Editor C++ rebuild and all **13 focused frontend/QTE native regressions** passed. Evidence is under ignored `Saved/RequestedFixes/2026-10-06`.

**At the time of this update, the October 5 Shipping ZIP had not been rebuilt.** The complete October 10 build above now includes these changes and the previously pending level/controller work.

## Windows release status — 2026-10-05

Workspace: `Quasicombo-level-design`, Diversion branch `level-design`, based on the completed menu merge `dv.commit.108`.

The full post-merge diagnostic cook reproduced the earlier **23 errors** (13 warnings). The final Development and Shipping builds, complete cooks and packages now pass with **0 errors and 4 warnings** each. The final ZIP passed CRC validation, and all 29 archived files matched their SHA-256 hashes after extraction into a fresh C: folder. The extracted Shipping game passed its short manual smoke test and restart/persistence checks.

“A clean full cook still fails on existing ControlRig and demo-content dependencies” meant that imported assets referred to modules or content unavailable to the packaged game. Editor play can succeed because the editor loads more modules than the game target. The post-merge cook reproduced those existing failures; it did not identify new menu cook errors.

## Release blockers fixed

- **ControlRig:** removed its Editor-only target restriction in `Braided_Quanta2026.uproject`. The game target can now load the ControlRig classes used by imported animations and rigs.
- **PCG Geometry Script Interop:** enabled `PCGGoemetryScriptInterop` for the imported PCG assets.
- **GameFeatureData:** added the standard runtime Asset Manager entry with class `/Script/GameFeatures.GameFeatureData`, `bIsEditorOnly=False` and `AlwaysCook`. Existing Map and PrimaryAssetLabel entries were retained. No Game Feature plugins are currently discovered or enabled.
- **RVT demo-map dependency:** cleared `BoundsAlignActor` on both main-level Runtime Virtual Texture volumes. Their transforms, dimensions and RVT textures were verified unchanged. These stale references had pulled `ExampleLevel_Jungle01` and its example player dependencies into the main map cook.
- **Quantum run-end error:** `ApplyFixture()` now returns when the component is frozen or the run is inactive. Ending a run before reaching a boss no longer tries to initialize a state that validation correctly rejects. State validation and asynchronous callback guards remain intact. Two native regression tests cover ended-run rejection and active-state freeze/retry behavior.
- **PCG manager reload errors:** added native Blueprint `Is Valid` guards around Landscape-dependent construction and size calculation in `BP_PCG_Manager`. A missing Landscape reference safely skips those operations. Existing nodes, calculations, actor transforms and spawning controls were preserved.
- **Packaged Mike customization materials:** added `/Game/Quasicombo/Buster/Materials` to `DirectoriesToAlwaysCook`. The two tint instances are loaded from C++ path strings, so the original cook did not discover them. The required material instances and their master now cook with the game.

## Validation completed

- Editor C++ target rebuild passed.
- A fresh, complete Development cook of the intro and main maps, followed by staging, packaging and archiving, passed. The logs explicitly identify a full cook and removal of previously cooked packages; no failed-cook subset was reused as proof of success.
- All **37 focused native tests passed**, covering frontend, score, music, loadout, route behavior, quantum, boss combat, QTEs and victory UI.
- **19 PIE frontend checks** covered intro → Home → Start, customization/audio SaveGame persistence, failed/invalid saves, death/retry and victory/replay.
- **22 PIE pause checks** covered keyboard pause, frozen gameplay, settings/customization, story replay, Resume transitions, attack continuity, retry pause and the real-time hold during slow motion.
- Direct Quantum API braid and evolution requests passed in PIE using live responses, with no braid fallback or evolution failure. The selected direct API integration is retained.
- Floating scenery remained present at the start, at X=45,000 and X=120,000 cm, and after returning. It has **2,340 mesh instances in 13 components**, including both floater variants and secondary detail.
- The previously saved PCG cache remains `SerializeOnlyAtCook` with `SerializeAll` contents.
- The updated Development package completed an automated intro → Home → Start smoke run. Both Mike tint materials loaded successfully; native landscape projection produced nonzero point sets (up to 250 points), using the serialized landscape cache. Its runtime log contains no errors, missing required tint materials, quantum-state errors or landscape-cache serialization failures.
- The freshly extracted Shipping game passed intro → Home → Start, visible landscape/background scenery, jump input, keyboard pause, frozen run timer and Resume. A cyan body color (RGB 85/215/230) and 70% music volume survived closing and restarting the executable. Original default colors and 100% music volume were restored afterward. This was a short smoke test; full death/retry, victory/replay, boss and QTE coverage came from the native and PIE checks above. Shipping logging is compiled out, so runtime log inspection used the Development package.
- Existing editor preferences were restored after testing.

## Current level configuration retained

The floating actor's latest saved placement is **(11,130, −78,275, −3,830) cm**. Its current controls are radius **40,608.31640625**, density **0.008**, horizontal offset **15,771**, scale range **0.5–4**, altitude offsets **−2,500/+2,500**, and editor generation on demand. It remains non-spatially loaded. These are the user's later level edits, so the earlier PCG repair defaults were not reapplied.

**World Partition streaming is currently disabled in Lvl_Main (`bEnableStreaming=False`).** Movement checks therefore verified scenery availability with all cells loaded; they do not prove cell unloading/reloading. This working configuration was preserved for today's release. Retest scenery, landscape PCG, routes and bosses before enabling streaming in a later release.

## Remaining imported-content warnings

The successful full cook reports four nonfatal warnings:

1. Deprecated `Initialize` usage in `PCGE_LandscapeMasker`.
2. A duplicate `Out` pin in the imported `PCG_S_Excluder` graph, which PCG removes.
3. Missing `u_SideLegArmor_r` transform-curve bone in imported Lizardman demo animation.
4. Missing `u_SideLegArmor_l` transform-curve bone in that demo animation.

The cook also lists stale imported Scifi Outpost subgraph and larvae texture dependencies at Display verbosity. PIE reports duplicate PCG partition actors among existing pending level saves. These were preserved for later cleanup. A successful cook does not by itself establish whether every unused soft reference is needed; packaged runtime logs are checked separately for required-asset failures.

## Delivery and reproducibility — historical 2026-10-05

The earlier build was delivered as **`F:\UE_Builds\Quasicombo\2026-10-05\Quasicombo-Win64-2026-10-05.zip`**. It contains the complete Win64 Shipping runtime and prerequisite installers. Extract the entire ZIP and run its root `Braided_Quanta2026.exe`. `Backspace` opens pause; `Alt+F4` exits. Use the October 10 release above for the current controls and gameplay updates.

- ZIP size: **2,608,024,039 bytes** (2.608 GB / 2.429 GiB).
- SHA-256: **`2c92ffc4c8aa1fc02a71b69985f54f879e200dc0ef9e4d11e682dfe6a00d75dc`**.
- ZIP64 support enabled; archive CRC and extracted-file hashes verified.
- The F: copy's checksum matches the verified C: original.

Temporary outputs, the C: ZIP backup and evidence are under ignored `Saved/Releases/2026-10-05`. The fresh smoke-test extraction is `ExtractedShippingFinal`. Previous builds and drive/security settings on F: were preserved.

`SourceManifest.json` records merge revision `dv.commit.108` and SHA-256 hashes of all 83 changed runtime source/content files used for the build: seven release fixes and 76 pre-existing pending level files. Their combined manifest hash is `c19fd4502f3ffecf1fd8c53554fc38d34875ad6bb93918a15deb9eeeb433185b`. The release commit is scoped to the seven runtime fixes plus this document; the pre-existing saves remain pending. Consequently, the release commit alone does not reconstruct all current level content. `ReleaseManifest.json` beside the ZIP records the actual release-fix commit, checksum and validation results; retain both manifests with the build.

Evidence includes `BaselineCook.log`, `EditorBuild.log`, `DevelopmentBuildCookRun.log`, `ShippingBuildCookRun.log`, `DevelopmentProjectionFinal.log`, `NativeTests.json`, `FrontendRuntime.json`, `PauseRuntime.json`, `LiveQuantumPIE.json`, `StreamingPIE.json`, `BackgroundAfterBuild.json`, `ShippingSmoke.json`, `PackageManifest.json`, and `EditorValidation.log`.

Historical PCG repair evidence remains under `Saved/PCGChecks`, with the original repair in `dv.commit.106`. Its old **34 formations / 3,198 instances** described the earlier settings, not the current release scenery. Those earlier focused cooked tests used a subset of a failed cook and were never a complete packaging pass.
