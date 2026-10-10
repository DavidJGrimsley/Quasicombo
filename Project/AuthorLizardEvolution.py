"""Bake the Blender performance and add facial shapes to an exact Unreal mesh copy.

ExportLizardAuthoring.py -> Blender BuildLizardPerformance.py -> this script.
The animation JSON uses the lizard mesh bind pose, never the shared human idle.
"""
import json
from pathlib import Path
import unreal

def main():
    LIB=unreal.EditorAssetLibrary
    OUT=Path(unreal.Paths.project_saved_dir())/'LizardRebuild'
    FOLDER='/Game/Quasicombo/Boss/Lizard'
    original=unreal.load_asset('/Game/Lizardman_Berserker/Mesh/SeparatedMesh/SK_Body')
    body_path=FOLDER+'/SK_Lizard_CinematicBody'
    # Refresh only the project-owned facial copy. The vendor mesh remains read-only.
    body=unreal.load_asset(body_path) if LIB.does_asset_exist(body_path) else LIB.duplicate_asset(original.get_path_name(),body_path)
    assert body
    assert unreal.QuasicomboLizardAuthoringLibrary.add_face_shapes(original,body,str(OUT/'facial_shapes.json'))
    assert LIB.save_loaded_asset(body,False)
    data=json.loads((OUT/'performance.json').read_text())
    source=unreal.load_asset('/Game/Lizardman_Berserker/Demo/TestAnimations/ThirdPersonIdle')
    clips={}
    finish=unreal.QuasicomboLizardAuthoringLibrary.finish_animation_compression
    def set_clip(clip,prop,value):
        finish(clip)
        clip.set_editor_property(prop,value)
        finish(clip)
    for name,performance in data['clips'].items():
        path=FOLDER+'/Animations/AS_Lizard_'+name
        clip=unreal.load_asset(path) if LIB.does_asset_exist(path) else LIB.duplicate_asset(source.get_path_name(),path)
        set_clip(clip,'additive_anim_type',unreal.AdditiveAnimationType.AAT_NONE)
        controller=clip.get_editor_property('controller')
        controller.open_bracket('Baked lizard mesh-space performance: '+name,False)
        for n in list(clip.get_editor_property('data_model_interface').get_bone_track_names()):
            controller.remove_bone_track(n,False)
        controller.set_frame_rate(unreal.FrameRate(60,1),False)
        controller.set_number_of_frames(unreal.FrameNumber(len(performance['frames'])-1),False)
        for index,bone in enumerate(data['bones']):
            controller.add_bone_curve(bone,False)
            keys=[frame[index] for frame in performance['frames']]
            assert controller.set_bone_track_keys(bone,[unreal.Vector(*k[:3]) for k in keys],
                [unreal.Quat(*k[3:7]) for k in keys],[unreal.Vector(*k[7:]) for k in keys],False)
        controller.close_bracket(False)
        finish(clip)
        set_clip(clip,'enable_root_motion',False)
        clip.set_retarget_source_asset(body)
        finish(clip)
        clip.update_retarget_source_asset_data()
        finish(clip)
        unreal.AnimationLibrary.remove_all_animation_notify_tracks(clip)
        clips[name]=clip
    reaction=clips['ArmorAttach']
    # Unreal clears RefPoseSeq when additive mode is disabled or the base type is
    # Reference Pose. Set those modes first, then assign the reference last.
    set_clip(reaction,'additive_anim_type',unreal.AdditiveAnimationType.AAT_LOCAL_SPACE_BASE)
    set_clip(reaction,'ref_pose_type',unreal.AdditiveBasePoseType.ABPT_ANIM_FRAME)
    set_clip(reaction,'ref_frame_index',60)
    set_clip(reaction,'ref_pose_seq',clips['ArmorAssembly'])
    assert reaction.get_editor_property('ref_pose_seq')==clips['ArmorAssembly']
    for clip in clips.values():
        assert LIB.save_loaded_asset(clip,False)
        print('LIZARD_CLIP',clip.get_path_name(),clip.get_play_length())

    bp=unreal.load_asset('/Game/Quasicombo/Boss/BP_QC_Boss')
    cdo=unreal.get_default_object(LIB.load_blueprint_class(bp.get_path_name()))
    cdo.set_editor_property('base_health_hits',10)
    cdo.set_editor_property('required_hits',10)
    cdo.set_editor_property('max_hp',10.)
    cdo.set_editor_property('evolution_trigger_health',1)
    cdo.mesh.set_skeletal_mesh_asset(body)
    cdo.evolution.set_editor_property('hover_seconds',1.)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert 'ERROR' not in str(bp.get_editor_property('status')).upper()
    assert LIB.save_loaded_asset(bp,False)
    # Update only the boss actor, preserving its other settings and all unrelated edits.
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    levels.load_level('/Game/Maps/Lvl_Main')
    boss=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if a.get_actor_label()=='QC_Boss')
    print('PLACED_BOSS_BEFORE',boss.get_path_name(),boss.base_health_hits,boss.mesh.get_editor_property('skeletal_mesh_asset').get_path_name())
    boss.set_editor_property('base_health_hits',10)
    boss.set_editor_property('evolution_trigger_health',1)
    boss.set_editor_property('required_hits',10)
    boss.mesh.set_skeletal_mesh_asset(body)
    # Save only this external actor package, not the level or the other edited actors.
    assert unreal.EditorLoadingAndSavingUtils.save_packages([boss.get_package()],False)
    print('LIZARD_PERFORMANCE_IMPORTED')

main()
# Release Python UObject wrappers before the commandlet tears down the engine.
import gc
gc.collect()
