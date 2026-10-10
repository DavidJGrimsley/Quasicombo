# Quasicombo frontend

The frontend lives in `/Game/UI/Menu`. Every screen inherits ordinary `UserWidget`, with its layout in the UMG Designer. Navigation, color controls, saving, waves, camera framing and transitions are Blueprint graphs. The existing native exit/victory screens remain independent.

The application opens `Lvl_QuasicomboIntro`, an empty presentation level using `BP_QuasicomboIntroMode`. It shows only narrative text and Continue/Skip over opaque black, with intro music. Its fourth page contains only the final question. Completion or Skip fades the intro audio and loads `Lvl_Main` into Home. `BP_QuasicomboSession.LaunchStorySeen` remembers this for the application launch. Playing directly from `Lvl_Main` in the editor also redirects to the intro on the first play, covering the camera with black before travel. Settings → Replay story uses the same full-screen black layout and returns to Settings.

## Editing the screens

| Asset | Edit here |
| --- | --- |
| `WBP_QuasicomboMenu` | Translucent menu `Panel`, opaque full-screen `StoryBlack`, and the `Pages` WidgetSwitcher. `ConfigureStoryLayout` switches between centered black story and the left menu panel. The embedded `Story` exposes `NarrativePage0`–`NarrativePage3`. |
| `WBP_QuasicomboHome` | Title, display font, Start/Customize/Settings buttons, vertical spacing and navigation hint. |
| `WBP_QuasicomboPause` | The copied Home layout with Resume/Customize/Settings. Resume continues the current run. |
| `WBP_QuasicomboStory` | Narrative body and text-only Continue/Skip controls. Heading and page counter are collapsed. `RefreshStory` selects the four narrative text properties. |
| `WBP_NYSQCustomization` | Body/Lights swatches, two rows per palette, RGB sliders and numeric labels, Reset colors and Back. Swatch `OnClicked` events contain their names and linear color values. |
| `WBP_QuasicomboSettings` | Replay story, Game info, Music/Jungle ambience sliders and Back. |
| `WBP_QuasicomboGameInfo` | Controls and route/combat explanation as Designer TextBlocks. |

The four narrative pages are copied verbatim from `Project/Narrative.txt`, split at paragraph boundaries; the fourth is “How will you traverse this chaos to escape?” To revise them, edit the embedded Story instance in the shell Designer; keep the standalone Story class defaults and source text consistent. Enter/A continues; Escape/B skips. Up/Down or the D-pad can scroll if edited text exceeds the available space. Each page opens at the top. The narrative source was preserved during this implementation.

Body text uses readable regular type, pale text and cyan focus accents. The Home title uses the existing Quasicombo display font. Buttons highlight on hover or keyboard/gamepad focus. The shell is focusable; page changes apply focus after the new page has been laid out. Select a slider with Enter/A, adjust with arrows/D-pad, then confirm with Enter/A; mouse dragging also saves on release.

## Director properties

Open `BP_QuasicomboFrontend`, then Class Defaults. Its exposed properties are grouped under `Menu`:

| Group | Properties and defaults |
| --- | --- |
| Camera | `CameraOffset = (-180, 700, 110)`, `CameraAimOffset = (-180, 0, 10)`, `CameraFOV = 45`. Offsets are world-space distances relative to the actual pawn. `UpdatePreviewCamera` follows the idle pawn until Start. |
| Timing | `FadeSeconds = 0.4`, `CameraBlendSeconds = 1.2`, `EmoteIntervalSeconds = 10`, `BlinkOffsetSeconds = 5`, `IdleCycleSeconds = 50`, `IdleAggroShare = 0.4`. Waves start at 10, 20, 30 seconds; blinks at 5, 15, 25 seconds. The idle cycle assigns 30 seconds to regular, then 20 to aggro, repeating (60% / 40%). |
| Animation | `WaveCompleteClip`, `BlinkClip`, `RegularIdleClip`, `AggroIdleClip`, played through the full-body `DefaultSlot`. Each gesture uses one complete sequence (3.8-second wave, 2.5-second blink), then resumes the selected looping idle. Gesture lengths are read from the assets. The old wave min/max and start/loop variables remain under Legacy/Unused for compatibility. |
| Audio | `IntroSound = SW_Intro`. It loops during story pages and fades out on completion or Skip. Replay story fades it back in. |
| Saving | `SaveSlot = QuasicomboPreferences`, user index 0. |

`SetFrontendScreen` uses Home=0, Story=1, Customization=2, Settings=3, Game info=4, Pause=5. `FrontendStartup` chooses the intro or jungle initialization. `IntroOnly` is computed from the current level name. Before Start, the mesh faces the camera continuously on every jungle menu page. `UpdateMenuAnimation` owns the shared preview clock; Home, Customization, Settings and Game info retain the same gesture and schedule when switching. Replay story stops animation and pauses this clock while its black screen is shown. Returning resumes the chosen idle; pressing Start calls `RestoreGameplayPose`, stopping preview montages and restoring the cached gameplay mesh rotation.

`StartFrontendRun` immediately disables the shell, saves settings, cancels the wave and starts the camera blend. `UpdateFrontend` fades the shell, and `FinishFrontendRun` releases input/HUD/music only once the camera duration has elapsed. The input handoff flushes held keys.

## Preferences and audio

`BP_QuasicomboPreferences` is an ordinary Blueprint SaveGame containing version 1, `SavedBody`, `SavedLights`, `SavedMusic` and `SavedAmbience`. Colors are linear `LinearColor` values; displayed RGB is sRGB 0–255. The control graphs perform the standard sRGB transfer conversion in both directions and quantize slider input to 8-bit RGB.

Live appearance changes call the existing loadout/body and Mike light APIs, preserving material, cannon and VFX updates. Defaults are body `(1, 0.08, 0.5)` and lights `(1, 0.82, 0.05)` in linear space. Reset colors changes only appearance. Save points include swatches, capture completion, keyboard adjustment completion, Reset, Back and Start. Invalid version/ranges or unreadable saves use defaults; failed writes show a notice and retain the working session values.

Music and ambience default to 100%. They multiply the existing mixes: gameplay/boss music retains its encounter crossfades and QTE ducking, and jungle ambience retains its original base level. Intro music uses the same music preference.

Settings refreshes its sliders and percentage labels from the live preferences on construction and whenever the page opens. The pause implementation retains the existing saved values; it does not reset audio or colors. The earlier initial validation explicitly saved 100% audio, as recorded below, so those saves can explain a change from previously tested lower values. A missing or rejected preference save also falls back to 100%.

The source `TrailerMusic_Quantum Blur Low Conserved.mp3` was preserved as `Project/AudioSource/Menu/Intro.mp3`. `SW_Intro.wav` is a 48 kHz stereo PCM16 conversion used by the imported `/Game/UI/Menu/SW_Intro`. Audio source copies are kept for reimport.

## Gameplay connection

`BP_SideScrollingGameMode.FrontendDirectorClass` points to `BP_QuasicomboFrontend`. Native `ShouldShowFrontend`, `StartGameplay`, run-start/session state, HUD/health visibility and music multiplier hooks connect the Blueprints to existing gameplay. Before Start, the run clock stays zero, gameplay input/combat is locked, route commits are rejected, enemies cannot advance combat and the boss cannot begin an encounter. The jungle world and forest ambience continue ticking while its menu is open. The empty intro world contains no gameplay pawn or jungle actors and cannot start a run.

`Config/DefaultEngine.ini` selects the intro as the game default map and `BP_QuasicomboSession` as GameInstance; the editor startup map remains `Lvl_Main`. Packaging settings include both intro and main maps for the Blueprint travel by name.

A run marks the GameInstance session as started. Map reloads for death and victory therefore bypass the frontend. Existing colors and earned Buster Cannon state remain in the existing loadout subsystem; a fresh application/PIE session starts without the cannon.

Backspace or gamepad Start/Menu opens the copied Home menu during an active run. The camera blends into the same close view as Home, using the director's `CameraOffset`, `CameraAimOffset`, `CameraFOV` and `CameraBlendSeconds`. Mike keeps his gameplay facing and current action; Pause only freezes the world and moves the camera. It does not rotate the mesh or replace its animation. Resume, Backspace or gamepad Start/Menu starts a return pan to the previous gameplay view. The menu remains visible with its controls disabled throughout the pan. After the pan finishes, the menu disappears; gameplay remains paused for another 0.25 real-time seconds before input and the HUD return with the cursor hidden. Repeated Resume requests during that sequence are ignored. Escape/B backs out of submenus and resumes from the main Pause page. Customize and Settings remain available; Replay story returns to Settings. There is no on-screen gameplay Pause button.

`BP_SideScrollingPlayerController.ToggleQuasicomboPause` reuses the initial frontend actor. On a retry that bypassed Home, it creates `BP_QuasicomboPauseSession`, a lightweight subclass whose BeginPlay constructs the menu without running the introduction. `EnsurePauseCamera` recreates the menu camera destroyed after Start and reuses it for subsequent pauses. The controller temporarily enables full ticking while paused so camera blends and `UpdatePauseResume` advance, then restores its previous setting in `FinishPauseResume`. Gameplay actors and score time remain paused throughout the pan and final hold. `RestorePauseCamera` captures the visible camera location, rotation and FOV into the camera actor and blends with an unlocked outgoing target, avoiding Unreal's stale last-frame camera cache in a paused world. The final hold uses `GetRealTimeSeconds`, so slow motion does not stretch it. Pause is unavailable before Start and after defeat/victory.

## Verification

Validation scripts are in `Project/MenuTools`; reports and screenshots are under `Saved/MenuValidation` and are intentionally untracked. They use the running editor's Unreal MCP endpoint and its registered Blueprint/UMG/Slate tools. Do not rerun the initial authoring/repair scripts over subsequent manual Designer edits.

- `validate_navigation.py` exercises real mouse, keyboard and gamepad story/submenu navigation.
- `validate_runtime.py` checks the empty black intro, narrative text, held run state, the complete wave and alternating blink, interruption, RGB conversion, Blueprint saving/failure handling, repeated Start, fade/camera release and death/victory/cannon retries.
- `validate_layouts.py` captures every screen at 1280×720, 1920×1080 and 2560×1080 and checks actual swatch clicks.
- `validate_native.py` runs focused frontend, score, music and Buster regression tests.
- `validate_sliders.py` checks actual mouse dragging, keyboard RGB changes and gamepad audio adjustment, including the saved values.
- `validate_menu_animation.py` observes a full 50-second idle cycle, gesture onsets and uninterrupted montage positions across every jungle menu, Replay story pause, and repeated Start during a wave.
- `validate_pause.py` checks pause availability, real Backspace/focused-button input through Slate MCP, frozen run state and attack animation, unchanged Mike facing, submenu/replay navigation, camera interpolation, menu removal after the pan, the 0.25-second real-time hold, repeated Resume, retained audio and a fresh retry in slow motion. Its progress report requests the host to send Backspace or Enter at the corresponding `ready_*` stages.

The native build target is `Braided_Quanta2026Editor Win64 Development` for UE 5.8. The original frontend pass built successfully. All 10 focused native regressions, 19 runtime assertions, 16 navigation checks and three slider input/save checks passed. Layout verification captured seven views at each of 1280×720, 1920×1080 and 2560×1080, including the black story and separate final question. Saved colors/audio also survived an editor relaunch. Those validation preferences were restored to default colors and 100% audio.

The subsequent menu-animation update compiled through MCP and passed 25 focused PIE checks across 1,892 samples. The observed gesture starts were within 0.022 seconds of the 5/10-second schedule. Both actual idle assets played, switching at 30 and 50 seconds; no gesture montage restarted across submenu changes. Replay story paused the clock, repeated Start during a wave stopped preview animation, and gameplay facing/camera/music restored after the transition. The run clock stayed zero during the preview. This update changed only the director Blueprint and tooling/documentation, preserving subsequent Designer edits and current saved preferences.

The staged Resume implementation passed 22 focused checks through Unreal MCP in PIE. Observed intermediate camera views confirmed a pan instead of a cut. The menu stayed visible through the pan, and score time, position, health and routes stayed frozen until at least 0.25 real-time seconds after menu removal. The sequence passed through Backspace, the Resume button and a fresh retry, including repeated Resume requests and slow motion. Mike retained his mesh rotation and active charged-attack montage throughout Pause and the return pan, and the same attack continued after Resume. The on-screen Pause button remains removed. Settings retains the existing saved percentages; no default percentages were changed.

This workspace uses Diversion, on branch `quasicombo-menu`; the pre-existing `.uproject` and narrative edits were preserved. The menu implementation is committed separately from those existing edits.
