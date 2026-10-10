import json, traceback
from pathlib import Path
import unreal

report = Path('C:/Users/DJLeg/Documents/Unreal Projects/Quasicombo-level-design/Saved/ArenaDefense')
report.mkdir(parents=True, exist_ok=True)
try:
    mesh = unreal.load_asset('/Game/RadicalMike/Mesh/SKM_MegaMikeZ')
    skeleton = mesh.get_editor_property('skeleton')
    asset = unreal.load_asset('/Game/RadicalMike/Animations/Anim_ZMIKE_IdleAggro')
    names = list(map(str, unreal.AnimationLibrary.get_animation_track_names(asset)))
    data = {'names': names, 'poses': {}}
    for label in ['IdleAggro', 'UppercutL', 'UppercutR']:
        a = unreal.load_asset('/Game/RadicalMike/Animations/Anim_ZMIKE_' + label)
        data['poses'][label] = {}
        for t in [0, .3, .48, .6]:
            data['poses'][label][str(t)] = {name: str(unreal.AnimationLibrary.get_bone_pose_for_time(a, name, t, False)) for name in names}
    data['controllerMethods'] = [n for n in dir(asset.get_editor_property('controller')) if 'frame' in n or 'bone' in n]
    data['skeletonMethods'] = [n for n in dir(skeleton) if 'bone' in n or 'reference' in n]
    (report/'inspect.json').write_text(json.dumps(data, indent=2))
except Exception:
    (report/'inspect-error.txt').write_text(traceback.format_exc())
