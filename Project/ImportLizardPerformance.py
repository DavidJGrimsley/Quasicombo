"""Import selected manually edited actions; preserve other assets and poses."""
import gc
import json
from pathlib import Path
import unreal

def main():
    folder = '/Game/Quasicombo/Boss/Lizard'
    lib = unreal.EditorAssetLibrary
    data = json.loads((Path(unreal.Paths.project_saved_dir())/'LizardSequence/performance.json').read_text())
    body = unreal.load_asset(folder+'/SK_Lizard_CinematicBody')
    finish = unreal.QuasicomboLizardAuthoringLibrary.finish_animation_compression
    imported = []
    for name, performance in data['clips'].items():
        clip = unreal.load_asset(folder+'/Animations/AS_Lizard_'+name)
        assert clip, 'Missing animation '+name
        finish(clip)
        clip.set_editor_property('additive_anim_type', unreal.AdditiveAnimationType.AAT_NONE)
        finish(clip)
        controller = clip.get_editor_property('controller')
        controller.open_bracket('Import edited lizard action: '+name, False)
        controller.set_frame_rate(unreal.FrameRate(data['fps'], 1), False)
        controller.set_number_of_frames(unreal.FrameNumber(len(performance['frames'])-1), False)
        for index, bone in enumerate(data['bones']):
            keys = [frame[index] for frame in performance['frames']]
            assert controller.set_bone_track_keys(bone, [unreal.Vector(*key[:3]) for key in keys],
                [unreal.Quat(*key[3:7]) for key in keys], [unreal.Vector(*key[7:]) for key in keys], False)
        controller.close_bracket(False)
        finish(clip)
        clip.set_retarget_source_asset(body)
        finish(clip)
        clip.update_retarget_source_asset_data()
        finish(clip)
        imported.append(clip)
    if 'ArmorAttach' in data['clips']:
        # Its existing reference keeps the impact delta independent of the body pose.
        reaction = unreal.load_asset(folder+'/Animations/AS_Lizard_ArmorAttach')
        for prop, value in [('additive_anim_type', unreal.AdditiveAnimationType.AAT_LOCAL_SPACE_BASE),
                            ('ref_pose_type', unreal.AdditiveBasePoseType.ABPT_ANIM_FRAME),
                            ('ref_frame_index', 60),
                            ('ref_pose_seq', unreal.load_asset(folder+'/Animations/AS_Lizard_ArmorAssembly'))]:
            reaction.set_editor_property(prop, value)
            finish(reaction)
    for clip in imported:
        assert lib.save_loaded_asset(clip, False)
        print('LIZARD_EDITED_CLIP_IMPORTED', clip.get_path_name(), clip.get_play_length())
    print('LIZARD_MANUAL_IMPORT_COMPLETE')

main()
gc.collect()
