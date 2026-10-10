# Cinematic Lizardman Time Evolution

## Summary

Replace the instant transformation with a roughly 8–10 second sequence: stumble into idle, levitate and power up, reveal the eye change from the front, hold the first charge pose while armor attaches, roar while descending, and return the camera along one diagonal with rotation. High aggression then opens combat with a charge and punch.

## Appearance and results

- Map starting Tau across all 12 skin palettes: `min(11, floor(Tau × 12))`, selecting palettes 01–12 in order. Capture the starting braid state when the encounter begins; use the existing route fixture if that state is unavailable. Keep the selected skin throughout the encounter.

- Start with `M_Eyes_01`. Change eyes during the face close-up using this mapping:

| Evolution level | Eyes | Aura | Armor shields |
|---|---|---|---|
| 0 | `M_Eyes_03` | `N_MagicalBarrier` | None |
| 1 | `M_Eyes_02` | `N_RingOfFlames` | One |
| 2 | `M_GlowEyes` | `N_TrailsOfDeath` | Two |

- Retain the current result thresholds: absolute Tau change below 5% gives level 0, 5%–below 15% gives level 1, and 15% or more gives level 2.

- Keep eyes tied to the awarded result after armor breaks. Aggression continues to follow evolved Tau independently.

## Sequence and combat

- Add a dedicated evolution presentation component with stages for **Stumble → Idle Recovery → Rise → Hover → Face Reveal → Armor Assembly → Roar/Descent → Return**. Default base health is 10 HP; start when an accepted hit leaves the living boss at 4 HP or less, retaining the combat lock.

- Begin the API request immediately. Play the first 0.4 seconds of the defeat animation, recover to idle for 0.4 seconds, then rise approximately 165 cm over 1.25 seconds, then hover for one second. Start the selected aura when the outcome is available and the rise has begun; early results still wait for the reveal.

- Use a dedicated frontal camera targeting the head. Take 0.25 seconds to frame the face, then push closer with an accelerating exponential curve during a three-second face reveal. Use fifteen new-color reveals with fixed 60ms flickers and exponentially shortening old-color gaps; the fifteenth reveal stays on. Ramp new-eye emissive from 10% to normal using the camera's same charge curve. Hold the closest face framing and full-bright eyes for 0.7 additional seconds, then pull back just far enough to frame the whole body. Follow the moving boss and preserve the viewport’s aspect ratio.

- Author baked animations on the lizard skeleton for rising, hovering with power-up tension, armor attachment reactions, a roar that plays during controlled descent. Use the existing full-body animation slot; control elevation through the presentation component.

- Fly solid armor pieces along staggered curved paths from outside the frame. Keep pose alignment with the body during flight, play an attachment reaction on impact, and lock each piece into place. Add small upward body pulses during assembly.

- Center both Ring of Flames copies at the body's midpoint and tilt them by +45° / −45° around the boss's forward axis, creating a crossed X pattern that follows the moving boss.

- Hold the first charge pose until every armor piece has attached and the last impact reaction finishes. Roar while floating down, taper the aura, then move the camera directly to the gameplay view while rotating along the path. Restore player control at the end. Remove the separate middle pose and smirk. At evolved Tau ≥⅔, telegraph a charge and punch with the existing attack windup, then use normal AI. Lower Tau resumes normal AI immediately.

- Cleanup on interruption, retry, defeat, or level exit must restore transforms, movement, camera, and time settings.

## Armor, resistance, and HUD

- Keep two shield groups and four accepted hits per shield. Preserve the partial/full armor sets, but define membership explicitly instead of switching visibility by remaining bar count.

- The inner group contains cloth, waist belt, arm guards, and the selected left shoulder. The outer group contains the torso harness, right shoulder, spikes, skull, and waist bone. Level 1 uses left shoulder 01; level 2 uses left shoulder 02, which remains after the outer group breaks.

- Drain the outer shield first. When a shield empties, detach its pieces as posed debris with gravity, rotation, ground collision, and a settling bounce. Debris ignores characters and remains until encounter cleanup.

- Apply resistance values of 0.6/0.8/1.0 for zero/one/two shields to boss stagger duration and animation strength. At full armor, ordinary hits still damage shields but do not interrupt attacks. Use resistance before the hit for that hit’s reaction; shield breaks affect subsequent hits.

- Show both shields in blue above the boss health bar; collapse empty bars and keep base health separate.

- Keep all boss information at the bottom: outer shield, inner shield, health, name and Tau, then a reserved one-line Attack Rate | Speed | Resistance row. Hide the overhead boss widget. Show a one-second paused "One more chance" announcement on retry instead of a persistent retry count. Reveal these during the transformation and retain them during combat, updating resistance when armor falls.

- Expose animation and sound slots plus stage events for attachment, landing, and roar. Bespoke audio production remains outside this first visual pass.

## API, preview, and validation

- Add a backward-compatible per-request timeout override to `FQuantumApiRequestOptions`; boss quantum requests use two seconds. Enforce the evolution deadline with real elapsed time.

- Snapshot the complex starting state, Hamiltonian coefficients, and evolution time at the trigger. On timeout, request failure, or offline mode, compute `exp(-iHt)` locally for the existing one-qubit `H = aX + bZ`. Feed the validated result through the normal outcome calculation and invalidate late callbacks.

- Distinguish live, local, and preview results in diagnostics. Keep API errors out of the cinematic HUD.

- Provide an editor-only preview setting, enabled for the initial visual test. Each PIE session defaults to outcome 0; keys **0/1/2** select the forced outcome before the hit reaching four HP. Use preview Tau values 0.2/0.5/0.8 to exercise low/medium/high aggression. Bypass boss API calls in preview.

- PlayerStart positioning remains manual. Preview selection does not teleport actors or start the sequence; normal combat triggers it.

- Validate all skin-band boundaries, outcome thresholds, local calculation reference vectors, two-second fallback, and rejection of late callbacks.

- Test shield absorption, debris creation, resistance changes, full-armor attack continuity, and the high-aggression opening. Verify camera and control restoration through interruption and retry.

- Build and run the relevant Unreal automation suites, then inspect all three outcomes in the time-evolution project’s PIE session. Check frontal framing from both facing directions, armor alignment, landing contact, and HUD readability at 1080p and in a narrow window.

- Implement in the dedicated Diversion time-evolution workspace and branch, and commit every implemented iteration. Further reviews and merging wait for the user to request them.

