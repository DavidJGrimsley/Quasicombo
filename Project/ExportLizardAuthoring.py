"""Export the untouched lizard and reference poses for the Blender source rig."""
import json
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_saved_dir()) / 'LizardRebuild'
out.mkdir(parents=True, exist_ok=True)
mesh = unreal.load_asset('/Game/Lizardman_Berserker/Mesh/SeparatedMesh/SK_Body')
assert unreal.QuasicomboLizardAuthoringLibrary.export_reference_pose(mesh,str(out/'mesh_reference.json'))
idle = unreal.load_asset('/Game/Lizardman_Berserker/Demo/TestAnimations/ThirdPersonIdle')
task = unreal.AssetExportTask()
task.object = mesh
task.filename = str(out / 'LizardBody.fbx')
task.automated = True
task.prompt = False
task.replace_identical = True
task.options = unreal.FbxExportOption()
task.options.set_editor_property('level_of_detail', False)
task.options.set_editor_property('collision', False)
assert unreal.Exporter.run_asset_export_task(task)
pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(idle, 0.0, unreal.AnimPoseEvaluationOptions())
def transform(t):
    return dict(t=[t.translation.x,t.translation.y,t.translation.z],
                q=[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w],
                s=[t.scale3d.x,t.scale3d.y,t.scale3d.z])
bones = list(map(str,unreal.AnimPoseExtensions.get_bone_names(pose)))
data = {b: dict(idle_local=transform(unreal.AnimPoseExtensions.get_bone_pose(pose,b,unreal.AnimPoseSpaces.LOCAL)),
                idle_world=transform(unreal.AnimPoseExtensions.get_bone_pose(pose,b,unreal.AnimPoseSpaces.WORLD)),
                ref_local=transform(unreal.AnimPoseExtensions.get_ref_bone_pose(pose,b,unreal.AnimPoseSpaces.LOCAL)),
                ref_world=transform(unreal.AnimPoseExtensions.get_ref_bone_pose(pose,b,unreal.AnimPoseSpaces.WORLD))) for b in bones}
(out/'unreal_reference.json').write_text(json.dumps(data,indent=2))
print('LIZARD_EXPORT_COMPLETE',len(bones),out)
