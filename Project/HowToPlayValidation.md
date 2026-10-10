# Main-menu How to Play validation

Validated 2026-10-06 in the running UE 5.8 editor, workspace
`Quasicombo-level-design`, Diversion branch `level-design`, based on commit 113.

## Changes

- Replaced the copied customization tree in `WBP_HowToPlay` with the approved
  gameplay paragraph, nine keyboard/controller controls, Buster unlock/replay
  note, and QTE instructions. Regular 22-point body text and wrapped cells.
- Preserved the existing jungle background, menu colors, heading font and Back
  button style. Page 6 uses panel anchors `(0.08, 0.06)` to `(0.92, 0.94)`;
  heading and Back sit outside the scroll box.
- Appended How to Play after the existing six pages. Home's existing button has
  click, hover and focus behavior. Opening resets scrolling and focuses Back.
- Up/Down and D-pad scroll by 80 Slate units, clamped to the content range.
  Back, Escape and controller B return to Home. Enter/controller A activate the
  focused Back button. Other pages restore their original panel layout.
- Removed all copied color controls, their event handlers, and customization
  function graphs. Only the new page's Construct, Tick and Back events remain.
- Asset editing used registered Unreal MCP tools; no binary files were edited
  on disk. No gameplay code or new C++ APIs.

## Verification

- All four edited Blueprints compiled and saved. Final compilation used
  `warnings_as_errors=True` and passed for `WBP_HowToPlay`,
  `WBP_QuasicomboHome`, `WBP_QuasicomboMenu`, and `BP_QuasicomboFrontend`.
- Confirmed How to Play has 54 widgets, one EventGraph, and no remaining copied
  swatch/slider/reset controls, color setters, refresh functions or preference
  save handlers.
- MCP-driven floating PIE at configured 1280x720 and 1200x550: verified exact
  paragraph/table/footnote, font size/typeface, panel anchors, page index 6,
  seven pages, initial zero offset and focused Back.
- Mouse click, keyboard arrows/Enter, and simulated Xbox A/D-pad events opened
  the page. Escape, Xbox B and mouse Back returned Home. Reopening reset scroll.
- Up/Down and Xbox D-pad scrolled both directions. Repeated Down stopped at the
  content end; a single Up moved back immediately. Top, bottom and scrolled-up
  screenshots were inspected at both window sizes: all content is reachable,
  columns wrap, and heading/Back remain visible and fixed.
- Real Windows mouse-wheel input through the computer-use skill scrolled down
  (offset 0 to approximately 160) and back to zero. Native MCP Slate snapshots
  also verified fixed heading/Back positions at the content end.
- Keyboard Enter and Xbox A activated the focused Back button. Home's How to
  Play focus and hover colors were checked against the existing highlight.
- Existing Customize, Settings, Game info and Story pages opened and returned
  correctly at both sizes; their existing page indices and anchors are retained.
- 39 runtime observations are recorded under `Saved/HowToPlayValidation/`,
  with screenshots and final compiler results. These local artifacts are ignored.
- PIE was stopped and original play-window dimensions/position restored.

## Scope

Task assets: `Content/UI/Menu/{WBP_HowToPlay,WBP_QuasicomboHome,
WBP_QuasicomboMenu,BP_QuasicomboFrontend}.uasset`, authoring/validation helpers,
and this note.

Unreal automatically compiled and resaved `BP_SideScrollingPlayerController`
before the first PIE run (editor log timestamp 02:41:47 UTC). No controller graph
was authored by this task. That incidental asset change is preserved outside
the task commit. No merge performed.

## User revision review - 2026-10-07

- Reviewed the user's revised `WBP_HowToPlay` through the running editor's
  Unreal MCP connection. Preserved the user's content, section order and
  controls-table edits.
- `BasicsHeading`, `ControlsHeading` and `QuantumBraidHeading` are distinct
  TextBlock widgets with Is Variable enabled. Each has the same wrapped,
  28-point Bold heading style and cyan color. Exposing these widgets as
  variables is harmless; it only makes their references available to graphs.
- The page has one EventGraph: frontend setup, Back click and Back highlight.
  None of the three section headers is referenced by that graph. There are no
  additional function graphs or copied customization handlers.
- `WBP_HowToPlay` compiled with `warnings_as_errors=True` and was saved.
- MCP-driven PIE at configured 1280x720 and 1200x550 confirmed that the three
  headers resolve to separate runtime widgets and retain their own labels.
  Both instruction paragraphs matched the user's editor text before and after
  scrolling. Mouse opening, keyboard scrolling, Escape return and mouse Back
  return passed; Back retained focus and remained fixed outside the scroll area.
- Inspected top and bottom screenshots at both sizes: text wraps readably and
  the complete Quantum Braid paragraph is reachable. Six runtime observations
  and screenshots are under `Saved/HowToPlayValidation/HeaderReview/` (ignored).
- Stopped review PIE and restored the original play-window settings. The
  existing unrelated player-controller asset change remains outside this commit.
