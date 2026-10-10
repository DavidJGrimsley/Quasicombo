"""Retarget the boss stumble/death and bake a looped QTE daze on its Epic rig."""
import math
import unreal

LIB = unreal.EditorAssetLibrary
ANIMS = '/Game/Quasicombo/Boss/Lizard/Animations'
sources = [
    '/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Med_01',
    '/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_02',
]
expected = ['Lizard_MM_HitReact_Front_Med_01', 'Lizard_MM_Death_Front_02']
missing = [source for source, name in zip(sources, expected) if not LIB.does_asset_exist(ANIMS + '/' + name)]
if missing:
    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.set_editor_property('assets_to_retarget', [LIB.find_asset_data(path) for path in missing])
    inputs.set_editor_property('source_mesh', unreal.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'))
    inputs.set_editor_property('target_mesh', unreal.load_asset('/Game/Lizardman_Berserker/Mesh/SeparatedMesh/SK_Body'))
    inputs.set_editor_property('ik_retarget_asset', unreal.load_asset('/Game/Quasicombo/Boss/Lizard/RTG_Manny_LizardBoss'))
    inputs.set_editor_property('target_path', ANIMS)
    inputs.set_editor_property('prefix', 'Lizard_')
    inputs.set_editor_property('include_referenced_assets', False)
    inputs.set_editor_property('overwrite_existing_files', False)
    created = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    for data in created:
        LIB.save_loaded_asset(data.get_asset())
    print('BQ_REACTION_RETARGETED', [str(data.package_name) for data in created])

react = unreal.load_asset(ANIMS + '/Lizard_MM_HitReact_Front_Med_01')
assert react, 'Lizard hit reaction did not retarget'
idle = unreal.load_asset('/Game/Lizardman_Berserker/Demo/TestAnimations/ThirdPersonIdle')
dazed_path = ANIMS + '/AS_Lizard_DazedIdle'
dazed = unreal.load_asset(dazed_path) if LIB.does_asset_exist(dazed_path) else LIB.duplicate_asset(idle.get_path_name(), dazed_path)
assert dazed, 'Could not create dazed loop'
controller = dazed.get_editor_property('controller')
frames = 48
tracks = [str(name) for name in unreal.AnimationLibrary.get_animation_track_names(idle)]
poses = {}
for name in tracks:
    poses[name] = [unreal.AnimationLibrary.get_bone_pose_for_time(idle, name, i / frames * idle.get_play_length(), False)
                   for i in range(frames + 1)]
controller.open_bracket('Bake looped dazed sway', False)
controller.set_number_of_frames(unreal.FrameNumber(frames), False)
existing = set(map(str, unreal.AnimationLibrary.get_animation_track_names(dazed)))
for name, samples in poses.items():
    if name not in existing:
        controller.add_bone_curve(name, False)
    positions, rotations, scales = [], [], []
    for i, pose in enumerate(samples):
        position = pose.translation
        rotation = pose.rotation
        sway = math.sin(2 * math.pi * i / frames)
        if name == 'root':
            position = samples[0].translation
        elif name == 'pelvis':
            position = unreal.Vector(position.x, position.y, position.z - 5.0 + 2.0 * sway)
            rotation = unreal.Rotator(pitch=9.0 + 3.0 * sway).quaternion() * rotation
        elif name in ('spine_01', 'spine_02'):
            rotation = unreal.Rotator(pitch=6.0 + 3.0 * sway).quaternion() * rotation
        elif name == 'head':
            rotation = unreal.Rotator(pitch=-7.0 + 4.0 * sway).quaternion() * rotation
        positions.append(position)
        rotations.append(rotation)
        scales.append(pose.scale3d)
    assert controller.set_bone_track_keys(name, positions, rotations, scales, False), name
controller.close_bracket(False)
dazed.set_editor_property('enable_root_motion', False)
dazed.set_editor_property('force_root_lock', False)
unreal.AnimationLibrary.remove_all_animation_notify_tracks(dazed)
assert LIB.save_loaded_asset(dazed)

bp = unreal.load_asset('/Game/Quasicombo/Boss/BP_QC_Boss')
cdo = unreal.get_default_object(LIB.load_blueprint_class('/Game/Quasicombo/Boss/BP_QC_Boss'))
cdo.set_editor_property('hit_reaction_animation', react)
cdo.set_editor_property('dazed_animation', dazed)
cdo.set_editor_property('defeat_animation', unreal.load_asset(ANIMS + '/Lizard_MM_Death_Front_02'))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert LIB.save_loaded_asset(bp)
print('BQ_LIZARD_ANIMS_READY', react.get_path_name(), dazed.get_path_name(), cdo.get_editor_property('defeat_animation').get_path_name())
