"""Export manually edited Blender actions without regenerating their poses.

blender LizardTransformation.blend --background --python this.py -- EvolutionRoar
With no clip arguments, export Rise, Hover, ArmorAttach, and Roar.
Then run ImportLizardPerformance.py in the Unreal Python commandlet.
"""
import bpy
import json
import sys
from pathlib import Path
from mathutils import Vector, Matrix, Quaternion

PROJECT = Path(__file__).resolve().parents[1]
OUT = PROJECT / 'Saved/LizardSequence'
OUT.mkdir(parents=True, exist_ok=True)
rig = next(obj for obj in bpy.context.scene.objects if obj.type == 'ARMATURE')
reference = json.loads((PROJECT/'Project/AnimationSource/LizardMeshReference.json').read_text(encoding='utf-8-sig'))['bones']
rest = {bone.name: bone.matrix_local.copy() for bone in rig.data.bones}
ue_rest = {bone['name']: Matrix.LocRotScale(Vector(bone['t']), Quaternion((bone['q'][3], *bone['q'][:3])), Vector((1, 1, 1))) for bone in reference}
reflection = Matrix.Diagonal((1., -1., 1., 1.))
names = sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else ['EvolutionRise', 'EvolutionHover', 'ArmorAttach', 'EvolutionRoar']
clips = {}
for name in names:
    action = bpy.data.actions['AS_Lizard_'+name]
    rig.animation_data.action = action
    rig.animation_data.action_slot = action.slots[0]
    first, last = (round(value) for value in action.frame_range)
    frames = []
    for frame in range(first, last+1):
        bpy.context.scene.frame_set(frame)
        bpy.context.view_layer.update()
        worlds = {}
        local = []
        for bone in reference:
            n = bone['name']
            posed = rig.pose.bones.get(n)
            world = reflection @ posed.matrix @ rest[n].inverted() @ reflection @ ue_rest[n] if posed else ue_rest[n]
            worlds[n] = world
            matrix = worlds[bone['parent']].inverted() @ world if bone['parent'] else world
            translation, rotation, scale = matrix.decompose()
            local.append([*translation, rotation.x, rotation.y, rotation.z, rotation.w, *scale])
        frames.append(local)
    fps = bpy.context.scene.render.fps / bpy.context.scene.render.fps_base
    clips[name] = dict(seconds=(last-first)/fps, frames=frames)
    print('EXPORTED_EDITED_ACTION', name, len(frames))
(OUT/'performance.json').write_text(json.dumps(dict(fps=round(fps), bones=[bone['name'] for bone in reference], clips=clips)), encoding='utf-8')
