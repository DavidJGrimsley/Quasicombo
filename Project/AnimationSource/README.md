# Editing the lizard performance manually

Open `LizardTransformation.blend` in Blender. Select the armature named `root`, enter Pose Mode, and open the Dope Sheet in Action Editor mode. Choose an action named `AS_Lizard_*`, move to a frame, rotate the relevant bones, and insert rotation keys. Use the Graph Editor to adjust timing and transitions. Save the blend file before exporting.

This is a skinned deformation skeleton with baked keys at 60 FPS. It supports manual bone posing, but it does not yet have a custom animator control rig, IK handles, or facial controls. An animator can add those controls and bake their result onto this skeleton. Root elevation and armor flight are controlled by the Unreal presentation component.

The active performance uses `AS_Lizard_EvolutionRise`, `AS_Lizard_EvolutionHover`, `AS_Lizard_ArmorAttach`, and `AS_Lizard_EvolutionRoar`. The hover action continues through the eye reveal and all armor impacts. `ArmorAttach` is an additive reaction; its reference is the old `ArmorAssembly` frame 60. Keep that reference unchanged when editing the impact reaction. The old assembly, landing, and smirk actions are retained as source history and are not played in this sequence. The stumble uses the opening of the existing Unreal defeat animation, followed by the existing idle.

To export an edited action, run this from the time-evolution project directory (replace the final clip name as needed):

```powershell
& 'C:/Program Files/Blender Foundation/Blender 5.0/blender.exe' 'Project/AnimationSource/LizardTransformation.blend' --background --python Project/ExportLizardPerformance.py -- EvolutionRoar
```

Close the time-evolution Unreal editor before importing, then run:

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' "$PWD/Braided_Quanta2026.uproject" -run=pythonscript "-script=$PWD/Project/ImportLizardPerformance.py" '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities' -unattended -NullRHI
```

The exporter writes only the selected actions to `Saved/LizardSequence/performance.json`. The importer updates those existing Unreal animation assets; it preserves the body, other clips, and map. With no clip arguments, the exporter selects the four active authored clips. The checked-in mesh reference JSON preserves the lizard's actual proportions during conversion.

`BuildLizardPerformance.py` is the original procedural generator. Running it regenerates the poses and overwrites the blend file, so use `ExportLizardPerformance.py` for manually edited work.
