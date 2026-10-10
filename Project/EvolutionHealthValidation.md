# Evolution trigger and health refill

The default ten-HP boss now evolves after nine accepted hits, at **one remaining base HP**. `EvolutionTriggerHealth` defaults to 1 in native code, `BP_QC_Boss`, and both authoring scripts. The placed `QC_Boss` in `/Game/Maps/Lvl_Main` inherits that default; its existing external actor file was not saved or changed.

Resolving evolution refills base health to `BaseHealthHits`, then adds the separately awarded shields. Combat stays locked until the existing evolution presentation finishes. Cumulative accepted-strike accounting is retained, with the remaining hit budget extended for the healed phase so earlier damage cannot make the boss invulnerable. The boss evolves once; depleting the healed phase and shields still enters the existing QTE.

Verified through Unreal MCP on Unreal 5.8 in this exact workspace, branch **QTE-improvement** over commit **93**, PIE stopped. The editor was restarted after rebuilding and confirmed both the Blueprint default and placed boss have ten base HP and a one-HP trigger. Editor content and maps were clean before implementation and after validation. Only `BP_QC_Boss` was explicitly saved for this asset change.

Validation:

- Editor Development and Win64 Development builds passed. Logs: `Saved/EvolutionHealthEditorBuild.log` and `Saved/EvolutionHealthGameBuild.log`.
- Full Quasicombo suite: **61/62 passed**. Results: `Saved/EvolutionHealthAutomation.json`.
- All evolution tests and all ten QTE tests passed. Coverage includes ten- and twelve-HP trigger cases, rejection of hits during evolution, full-health refill with and without shields, damage throughout the healed phase, no second evolution, projectile damage, retry behavior and QTE entry.
- Retained baseline `Quasicombo.Boss.LizardRigAndTailAnimation` passed. Retained baseline `Quasicombo.Buster.ThunderParticleColors` failed the same two early-particle color assertions; neither assertion was removed or weakened.
- The eleven files already pending when this task began retained their starting hashes. Ordinary animation/source files also retained their protected hashes. The separate evolution workspace and main were untouched.

For the user PIE check, hit the default boss nine times: it should transform at one HP and return to combat with a full base-health bar plus the awarded shields. Break the shields and deplete ten more base hits to reach QTE. Controller and visual checks remain with the user.
