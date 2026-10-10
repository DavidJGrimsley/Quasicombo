"""One-time QTE migration. All asset operations run inside the Unreal editor.
Sources own FK poses, capsule trajectories, markers and camera framing.
"""
import math
import unreal
import QTEPoseAuthoring as pose
ROOT = '/Game/Quasicombo/QTE'
LIB = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
EDITOR = unreal.get_editor_subsystem(unreal.LevelSequenceEditorSubsystem)
VIEW = unreal.LevelSequenceEditorBlueprintLibrary
LEGACY_LABELS = ['Approach', 'JumpReady', 'Jump', 'Uppercut', 'JumpApex', 'Chomp', 'Claw', 'FlyingKick', 'CannonLanding', 'Cannon', 'CannonFire', 'Recovery', 'Counter', 'Launch']
LABELS = ['Approach', 'JumpReady', 'Jump', 'Uppercut', 'JumpApex', 'Chomp', 'Claw', 'FinisherLanding', 'FinisherUppercut', 'CannonLanding', 'Cannon', 'CannonFire', 'Recovery', 'Counter', 'Launch']
BEATS = ['Jump', 'Uppercut', 'Chomp', 'Claw', 'FinisherUppercut']
MESHES = {'Mike': '/Game/RadicalMike/Mesh/SKM_MegaMikeZ', 'Lizard': '/Game/Quasicombo/Boss/Lizard/SK_Lizard_CinematicBody'}
BLUEPRINTS = {'Mike': '/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter', 'Lizard': '/Game/Quasicombo/Boss/BP_QC_Boss'}

def load(path):
    result = unreal.load_asset(path)
    assert result, path
    return result

def save(asset):
    assert LIB.save_loaded_asset(asset), asset.get_path_name()

def g(obj, name):
    return obj.get_editor_property(name)

def smooth(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)

def mesh(actor):
    return actor.get_editor_property('mesh') if isinstance(actor,unreal.Character) else actor.get_component_by_class(unreal.SkeletalMeshComponent)

def transform_track(binding, positions, frames):
    for track in list(binding.get_tracks()):
        if isinstance(track, unreal.MovieScene3DTransformTrack):
            binding.remove_track(track)
    track = binding.add_track(unreal.MovieScene3DTransformTrack)
    track.set_display_name('Capsule trajectory (cm)')
    section = track.add_section()
    section.set_range(0, frames + 1)
    channels = section.get_all_channels()
    assert len(channels) == 9
    for channel, value in zip(channels, [0, 0, 0, 0, 0, 180 if binding.get_name()=='Lizard' else 0, 1, 1, 1]):
        channel.set_default(value)
    for frame, position in enumerate(positions):
        for channel, value in zip(channels[:3], [position.x, position.y, position.z]):
            channel.add_key(unreal.FrameNumber(frame), value, interpolation=unreal.MovieSceneKeyInterpolation.LINEAR)
    return section

def float_track(binding, name, path, values, frames):
    track = binding.add_track(unreal.MovieSceneFloatTrack)
    track.set_property_name_and_path(name, path)
    section = track.add_section()
    section.set_range(0, frames + 1)
    channel = section.get_all_channels()[0]
    for frame, value in values:
        channel.add_key(unreal.FrameNumber(frame), value, interpolation=unreal.MovieSceneKeyInterpolation.LINEAR)

def proxy(who):
    actor = ACTORS.spawn_actor_from_class(unreal.Character, unreal.Vector())
    actor.set_actor_label('QTE ' + who + ' authoring proxy')
    mesh(actor).set_skeletal_mesh_asset(load(MESHES[who]))
    cdo = unreal.get_default_object(LIB.load_blueprint_class(BLUEPRINTS[who]))
    mesh(actor).set_relative_transform(cdo.get_editor_property('mesh').get_relative_transform(), False, True)
    mesh(actor).set_update_animation_in_editor(True)
    actor.set_actor_rotation(unreal.Rotator(yaw=0 if who == 'Mike' else 180), False)
    actor.set_actor_location(unreal.Vector(0 if who == 'Mike' else 125, 0, 0), False, False)
    return actor

def replace_pose(sequence, binding, clip, frames):
    binding = EDITOR.convert_to_possessable(binding)
    for track in list(binding.get_tracks()):
        if isinstance(track, (unreal.MovieSceneSkeletalAnimationTrack, unreal.MovieSceneControlRigParameterTrack)):
            binding.remove_track(track)
    track = binding.add_track(unreal.MovieSceneSkeletalAnimationTrack)
    section = track.add_section()
    section.set_range(0, frames)
    params = section.get_editor_property('params')
    params.animation = clip
    section.set_editor_property('params', params)
    VIEW.refresh_current_level_sequence()
    VIEW.set_current_time(0)
    options = unreal.AnimSeqExportOption()
    options.use_custom_time_range = True
    options.custom_display_rate = unreal.FrameRate(30, 1)
    options.custom_start_frame = unreal.FrameNumber(0)
    options.custom_end_frame = unreal.FrameNumber(frames - 1)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert unreal.ControlRigSequencerLibrary.bake_to_control_rig(world, sequence, unreal.FKControlRig, options, False, 0.001, binding, True)
    for track in binding.get_tracks():
        if isinstance(track, unreal.MovieSceneSkeletalAnimationTrack):
            for section in track.get_sections():
                section.set_is_active(False)
        if isinstance(track, unreal.MovieSceneControlRigParameterTrack):
            track.set_display_name(binding.get_name() + ' editable FK pose')
            for section in track.get_sections():
                section.set_range(0, frames + 1)
    name = binding.get_name()
    converted = EDITOR.convert_to_spawnable(binding)
    assert converted
    return next((b for b in converted if b.get_name() == name))

def build_source(label, clips, seconds, mike_path, lizard_path, offset, fov=48, prompt=None, contact=None, focus=0.5):
    path = ROOT + '/Sources/LS_QTE_' + label
    sequence = load(path) if LIB.does_asset_exist(path) else TOOLS.create_asset('LS_QTE_' + label, ROOT + '/Sources', unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    if LIB.get_metadata_tag(sequence, 'QTEAirborneVersion') == '1':
        return sequence
    frames = round(seconds * 30)
    sequence.set_display_rate(unreal.FrameRate(30, 1))
    sequence.set_playback_start(0)
    sequence.set_playback_end(frames)
    assert VIEW.open_level_sequence(sequence)
    for who, clip in clips.items():
        bindings = [b for b in sequence.get_bindings() if b.get_name() == who]
        if bindings:
            binding = bindings[0]
        else:
            binding = sequence.add_possessable(proxy(who))
            binding.set_name(who)
        if clip:
            binding = replace_pose(sequence, binding, clip, frames)
        template = binding.get_object_template()
        if template:
            cdo = unreal.get_default_object(LIB.load_blueprint_class(BLUEPRINTS[who]))
            mesh(template).set_relative_transform(cdo.get_editor_property('mesh').get_relative_transform(), False, True)
            template.set_actor_rotation(unreal.Rotator(yaw=0 if who == 'Mike' else 180), False)
        transform_track(binding, [(mike_path if who == 'Mike' else lizard_path)(i / frames) for i in range(frames + 1)], frames)
    for binding in sequence.get_bindings():
        for track in binding.get_tracks():
            if isinstance(track, unreal.MovieSceneSkeletalAnimationTrack):
                for section in track.get_sections():
                    section.set_is_active(False)
    for binding in list(sequence.get_bindings()):
        if binding.get_name() in ['Camera', 'Camera Lens']:
            binding.remove()
    camera = ACTORS.spawn_actor_from_class(unreal.QuasicomboQTEAuthoringCamera, unreal.Vector())
    camera.set_editor_property('target_blend', focus)
    camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(fov)
    binding = sequence.add_possessable(camera)
    binding.set_name('Camera')
    positions = []
    for i in range(frames + 1):
        t = i / frames
        weight = t if label == 'CannonFire' else focus
        positions.append(mike_path(t) * (1 - weight) + lizard_path(t) * weight + unreal.Vector(0, 0, 40) + offset)
    transform_track(binding, positions, frames)
    float_track(binding, 'TargetBlend', 'TargetBlend', [(0, focus), (frames, 1 if label == 'CannonFire' else focus)], frames)
    lens = sequence.add_possessable(camera.get_component_by_class(unreal.CameraComponent))
    lens.set_name('Camera Lens')
    lens.set_parent(binding)
    float_track(lens, 'FieldOfView', 'FieldOfView', [(0, fov), (frames, fov)], frames)
    EDITOR.convert_to_spawnable(binding)
    sequence.delete_marked_frames()
    for name, fraction in [('Prompt', prompt), ('Contact', contact)]:
        if fraction is not None:
            sequence.add_marked_frame(unreal.MovieSceneMarkedFrame(frame_number=unreal.FrameNumber(round(fraction * frames)), label=name))
    LIB.set_metadata_tag(sequence, 'QTEAirborneVersion', '1')
    save(sequence)
    VIEW.close_level_sequence()
    print('QTE_AIRBORNE_SOURCE', label)
    return sequence

def bound_actor(sequence, name):
    binding = next((b for b in sequence.get_bindings() if b.get_name() == name))
    objects = VIEW.get_bound_objects(sequence.get_binding_id(binding))
    actor=next((o for o in objects if isinstance(o, unreal.Actor)))
    component=actor.get_component_by_class(unreal.SkeletalMeshComponent)
    if component: component.set_update_animation_in_editor(True)
    return binding,actor

def fit_kick_contact():
    sequence = load(ROOT + '/Sources/LS_QTE_FlyingKick')
    if LIB.get_metadata_tag(sequence, 'QTEContactFitted') == '6':
        return
    VIEW.open_level_sequence(sequence)
    VIEW.refresh_current_level_sequence()
    end = sequence.get_playback_end()
    frame = next((m.frame_number.value for m in sequence.get_marked_frames() if m.label == 'Contact'))
    VIEW.set_current_time(frame)
    binding, mike = bound_actor(sequence, 'Mike')
    _, lizard = bound_actor(sequence, 'Lizard')
    def bone_world(who,actor,bone):
        b=next(b for b in sequence.get_bindings() if b.get_name()==who)
        import QTEPublish
        template=load(ROOT+'/Mike/AS_QTE_JumpApex') if who=='Mike' else load(ROOT+'/Baked/AS_QTE_JumpReady_Lizard')
        clip=unreal.QuasicomboQTEPresentation.create_staged_clip(template)
        options=unreal.AnimSeqExportOption()
        options.use_custom_time_range=True
        options.custom_display_rate=unreal.FrameRate(30,1)
        options.custom_start_frame=unreal.FrameNumber(0)
        options.custom_end_frame=unreal.FrameNumber(end)
        assert unreal.ControlRigSequencerLibrary.export_anim_sequence_from_sequencer(clip,options,b,False)
        QTEPublish.normalize_in_place(clip)
        evaluation=unreal.AnimPoseEvaluationOptions()
        evaluation.optional_skeletal_mesh=load(MESHES[who])
        p=unreal.AnimPoseExtensions.get_anim_pose_at_time(clip,frame/30,evaluation)
        local=unreal.AnimPoseExtensions.get_bone_pose(p,bone,unreal.AnimPoseSpaces.WORLD)
        VIEW.set_current_time(frame)
        return unreal.MathLibrary.compose_transforms(local,mesh(actor).get_world_transform()).translation
    target = bone_world('Lizard',lizard,'spine_02')
    foot = bone_world('Mike',mike,'foot_r')
    VIEW.set_current_time(frame)
    print('QTE_CONTACT_EXPORT',target,foot,mike.get_actor_location(),lizard.get_actor_location())
    base = unreal.Vector(40 + 25 * math.sin(math.pi * frame / end), 0, 100)
    delta = target - foot + mike.get_actor_location() - base
    track = next((t for t in binding.get_tracks() if isinstance(t, unreal.MovieScene3DTransformTrack)))
    channels = track.get_sections()[0].get_all_channels()
    positions=[]
    for i in range(end + 1):
        t = i / end
        weight = smooth(i / frame) if i <= frame else 1 - smooth((i - frame) / (end - frame))
        pos = unreal.Vector(40 + 25 * math.sin(math.pi * t), 0, 100) + delta * weight
        assert pos.z > 30, ('Flying kick must remain airborne', pos)
        positions.append(pos)
    for i,pos in enumerate(positions):
        for channel, value in zip(channels[:3], [pos.x, pos.y, pos.z]):
            next((k for k in channel.get_keys() if k.get_time().frame_number.value == i)).set_value(value)
    print('QTE_FLYING_CONTACT_FIT', target, foot, delta)
    LIB.set_metadata_tag(sequence, 'QTEContactFitted', '6')
    save(sequence)
    VIEW.close_level_sequence()

def align_contact_reactions():
    for label, reaction in [('Uppercut','Uppercut'),('Chomp','Chomp'),('Claw','Claw'),('FlyingKick','Kick')]:
        sequence=load(ROOT+'/Sources/LS_QTE_'+label)
        if LIB.get_metadata_tag(sequence,'QTEReactionAligned')=='2':
            continue
        frames=sequence.get_playback_end()
        contact=next(m.frame_number.value for m in sequence.get_marked_frames() if m.label=='Contact')
        original=load(ROOT+'/Lizard/AS_QTE_'+reaction+'Reaction')
        clip=pose.copy(original.get_path_name(),ROOT+'/Lizard/AS_QTE_'+label+'Contact')
        controller=g(clip,'controller')
        controller.open_bracket('Align reaction to Contact marker',False)
        controller.set_frame_rate(unreal.FrameRate(30,1),False)
        controller.set_number_of_frames(unreal.FrameNumber(frames),False)
        idle=load(ROOT+'/Baked/AS_QTE_JumpReady_Lizard')
        for bone in unreal.AnimationLibrary.get_animation_track_names(original):
            poses=[unreal.AnimationLibrary.get_bone_pose_for_time(original,bone,
                max(0,(i-contact)/(frames-contact))*original.get_play_length(),False) for i in range(frames+1)]
            before=unreal.AnimationLibrary.get_bone_pose_for_time(idle,bone,0,False)
            for i,p in enumerate(poses):
                alpha=smooth((i-contact)/3)
                p.translation=before.translation+(p.translation-before.translation)*alpha
                p.rotation=unreal.MathLibrary.quat_slerp(before.rotation,p.rotation,alpha)
            controller.set_bone_track_keys(bone,[p.translation for p in poses],[p.rotation for p in poses],[p.scale3d for p in poses],False)
        controller.close_bracket(False)
        unreal.AnimationLibrary.remove_all_animation_notify_tracks(clip)
        save(clip)
        VIEW.open_level_sequence(sequence)
        binding=next(b for b in sequence.get_bindings() if b.get_name()=='Lizard')
        replace_pose(sequence,binding,clip,frames)
        LIB.set_metadata_tag(sequence,'QTEReactionAligned','2')
        save(sequence)
        VIEW.close_level_sequence()
    sequence=load(ROOT+'/Sources/LS_QTE_Launch')
    if LIB.get_metadata_tag(sequence,'QTELaunchContact')!='1':
        clip=pose.pose_clip('LaunchContact',ROOT+'/Lizard/AS_QTE_KickReaction',sequence.get_playback_end()/30,
            mesh=MESHES['Lizard'],blend_from=ROOT+'/Baked/AS_QTE_JumpReady_Lizard',blend_to=ROOT+'/Lizard/AS_QTE_LizardAirborne',author_version='contact1')
        VIEW.open_level_sequence(sequence)
        binding=next(b for b in sequence.get_bindings() if b.get_name()=='Lizard')
        replace_pose(sequence,binding,clip,sequence.get_playback_end())
        LIB.set_metadata_tag(sequence,'QTELaunchContact','1')
        save(sequence); VIEW.close_level_sequence()

def preview_cameras():
    for label in LABELS:
        sequence=load(ROOT+'/Sources/LS_QTE_'+label)
        if LIB.get_metadata_tag(sequence,'QTECameraPreview')=='1': continue
        VIEW.open_level_sequence(sequence)
        VIEW.refresh_current_level_sequence()
        VIEW.set_current_time(0)
        binding,camera=bound_actor(sequence,'Camera')
        _,mike=bound_actor(sequence,'Mike')
        _,lizard=bound_actor(sequence,'Lizard')
        track=next(t for t in binding.get_tracks() if isinstance(t,unreal.MovieScene3DTransformTrack))
        channels=track.get_sections()[0].get_all_channels()
        for frame in range(sequence.get_playback_end()+1):
            VIEW.set_current_time(frame)
            w=g(camera,'target_blend')
            target=mike.get_actor_location()*(1-w)+lizard.get_actor_location()*w+g(camera,'focus_offset')
            rotation=unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),target)
            for channel,value in zip(channels[3:6],[rotation.roll,rotation.pitch,rotation.yaw]):
                channel.add_key(unreal.FrameNumber(frame),value,interpolation=unreal.MovieSceneKeyInterpolation.LINEAR)
        cuts=[t for t in sequence.get_tracks() if isinstance(t,unreal.MovieSceneCameraCutTrack)]
        cut=cuts[0] if cuts else sequence.add_track(unreal.MovieSceneCameraCutTrack)
        section=cut.get_sections()[0] if cut.get_sections() else cut.add_section()
        section.set_range(0,sequence.get_playback_end()+1)
        section.set_camera_binding_id(sequence.get_binding_id(binding))
        LIB.set_metadata_tag(sequence,'QTECameraPreview','1')
        save(sequence)
        VIEW.close_level_sequence()

def migrate_capsule_proxies():
    for label in LABELS:
        sequence=load(ROOT+'/Sources/LS_QTE_'+label)
        version=LIB.get_metadata_tag(sequence,'QTECapsuleProxy')
        if version=='2': continue
        if version=='1':
            for binding in sequence.get_bindings():
                template=binding.get_object_template()
                if isinstance(template,unreal.Character):
                    binding.set_name('Mike' if mesh(template).get_skeletal_mesh_asset()==load(MESHES['Mike']) else 'Lizard')
            LIB.set_metadata_tag(sequence,'QTECapsuleProxy','2')
            save(sequence)
            continue
        VIEW.open_level_sequence(sequence)
        VIEW.refresh_current_level_sequence()
        for who in ['Mike','Lizard']:
            binding=next(b for b in sequence.get_bindings() if b.get_name()==who)
            binding=EDITOR.convert_to_possessable(binding)
            VIEW.refresh_current_level_sequence(); VIEW.set_current_time(0)
            _,old=bound_actor(sequence,who)
            replacement=proxy(who)
            EDITOR.replace_binding_with_actors([replacement],binding)
            converted=EDITOR.convert_to_spawnable(binding)
            assert converted
            converted[0].set_name(who)
            if old!=replacement and isinstance(old,unreal.SkeletalMeshActor): ACTORS.destroy_actor(old)
        LIB.set_metadata_tag(sequence,'QTECapsuleProxy','2')
        save(sequence); VIEW.close_level_sequence()

def align_capsule_ground():
    mike=unreal.get_default_object(LIB.load_blueprint_class(BLUEPRINTS['Mike']))
    lizard=unreal.get_default_object(LIB.load_blueprint_class(BLUEPRINTS['Lizard']))
    height=g(lizard,'capsule_component').get_scaled_capsule_half_height()-g(mike,'capsule_component').get_scaled_capsule_half_height()
    for label in LABELS:
        sequence=load(ROOT+'/Sources/LS_QTE_'+label)
        if LIB.get_metadata_tag(sequence,'QTEGroundAligned')=='1': continue
        binding=next(b for b in sequence.get_bindings() if b.get_name()=='Lizard')
        track=next(t for t in binding.get_tracks() if isinstance(t,unreal.MovieScene3DTransformTrack))
        for key in track.get_sections()[0].get_all_channels()[2].get_keys(): key.set_value(key.get_value()+height)
        LIB.set_metadata_tag(sequence,'QTEGroundAligned','1')
        save(sequence)

def lower_kick_extension():
    sequence=load(ROOT+'/Sources/LS_QTE_FlyingKick')
    if LIB.get_metadata_tag(sequence,'QTEKickExtension')=='1': return
    rig_clip=pose.pose_clip('FlyingKick',ROOT+'/Mike/AS_QTE_JumpApex',sequence.get_playback_end()/30,
        static=True,hold_last_frame=True,author_version='extension2',angles={
        'thigh_r':[(0,0),(.22,40),(.42,75),(.64,45),(.78,40),(1,0)],
        'calf_r':[(0,0),(.22,-85),(.42,-115),(.64,-4),(.78,-8),(1,0)],
        'foot_r':[(0,0),(.42,12),(.64,-20),(.78,-18),(1,0)],
        'thigh_l':[(0,0),(.42,-15),(.64,-25),(.78,-20),(1,0)],
        'calf_l':[(0,0),(.42,-35),(.64,-45),(.78,-40),(1,0)],
        'spine_01':[(0,0),(.42,5),(.64,18),(.78,14),(1,0)]})
    VIEW.open_level_sequence(sequence)
    binding=next(b for b in sequence.get_bindings() if b.get_name()=='Mike')
    replace_pose(sequence,binding,rig_clip,sequence.get_playback_end())
    LIB.set_metadata_tag(sequence,'QTEKickExtension','1')
    save(sequence); VIEW.close_level_sequence()

def align_cannon_fire():
    sequence=load(ROOT+'/Sources/LS_QTE_CannonFire')
    if LIB.get_metadata_tag(sequence,'QTECannonSeam')!='1':
        charge=load(ROOT+'/Baked/AS_QTE_Cannon_Mike')
        clip=pose.pose_clip('CannonFireContinuous',ROOT+'/Mike/AS_QTE_CannonFire',sequence.get_playback_end()/30,
            blend_from=charge.get_path_name(),blend_from_time=charge.get_play_length(),hold_last_frame=True,author_version='cannon2')
        VIEW.open_level_sequence(sequence)
        binding=next(b for b in sequence.get_bindings() if b.get_name()=='Mike')
        replace_pose(sequence,binding,clip,sequence.get_playback_end())
        LIB.set_metadata_tag(sequence,'QTECannonSeam','1')
        save(sequence)
        VIEW.close_level_sequence()
    cannon=load(ROOT+'/Sources/LS_QTE_Cannon')
    if LIB.get_metadata_tag(cannon,'QTECannonPrompt')!='1':
        cannon.add_marked_frame(unreal.MovieSceneMarkedFrame(frame_number=unreal.FrameNumber(round(cannon.get_playback_end()*.1)),label='Prompt'))
        LIB.set_metadata_tag(cannon,'QTECannonPrompt','1')
        save(cannon)

def author_airborne_if_needed():
    data = load(ROOT + '/DA_QTEPresentation')
    beats = list(g(data, 'beats'))
    apex = ROOT + '/Mike/AS_QTE_JumpApex'
    if not LIB.does_asset_exist(apex):
        assert LIB.duplicate_asset('/Game/RadicalMike/Animations/Anim_ZMIKE_JumpApex', apex)

    def seed(label, source, seconds, **kwargs):
        return pose.pose_clip(label, source, seconds, author_version='airborne3', hold_last_frame=True, **kwargs)
    seeds = {'Uppercut': seed('UppercutAirborne', g(beats[1], 'mike').get_path_name(), g(beats[1], 'duration'), blend_to=apex), 'Chomp': seed('ChompAirborne', g(beats[2], 'mike').get_path_name(), g(beats[2], 'duration'), blend_from=apex, blend_to=apex), 'Claw': seed('ClawAirborne', g(beats[3], 'mike').get_path_name(), g(beats[3], 'duration'), blend_from=apex, blend_to=apex), 'JumpApex': seed('ApexHold', apex, 0.2, static=True), 'FlyingKick': seed('FlyingKick', apex, g(beats[4], 'duration'), static=True, blend_to=apex, angles={'thigh_r': [(0, 0), (0.22, 40), (0.42, 75), (0.64, 90), (0.78, 85), (1, 0)], 'calf_r': [(0, 0), (0.22, -85), (0.42, -115), (0.64, -4), (0.78, -8), (1, 0)], 'foot_r': [(0, 0), (0.42, 12), (0.64, -20), (0.78, -18), (1, 0)], 'thigh_l': [(0, 0), (0.42, -15), (0.64, -25), (0.78, -20), (1, 0)], 'calf_l': [(0, 0), (0.42, -35), (0.64, -45), (0.78, -40), (1, 0)], 'spine_01': [(0, 0), (0.42, 5), (0.64, 18), (0.78, 14), (1, 0)]}), 'CannonLanding': seed('CannonLanding', pose.mike('JumpEnd'), 0.8, blend_from=apex, blend_to=g(data, 'mike_charge').get_path_name()), 'CannonFire': seed('CannonFireContinuous', g(data, 'mike_fire').get_path_name(), 0.6, blend_from=g(data, 'mike_charge').get_path_name()), 'Recovery': seed('AirborneRecovery', pose.mike('JumpEnd'), 0.6, blend_from=apex)}
    reaction = seed('LaunchReaction', ROOT + '/Lizard/AS_QTE_KickReaction', 1.8, mesh=MESHES['Lizard'], blend_to=g(data, 'lizard_flight').get_path_name())
    stationary = lambda t: unreal.Vector(125, 0, 0)
    airborne = lambda t: unreal.Vector(40, 0, 100)
    for label in LABELS:
        source = ROOT + '/Sources/LS_QTE_' + label
        exists = LIB.does_asset_exist(source)
        if exists and LIB.get_metadata_tag(load(source), 'QTEAirborneVersion') == '1':
            continue
        duration = load(source).get_playback_end() / 30 if exists else {'JumpApex': 0.2, 'FlyingKick': g(beats[4], 'duration'), 'CannonLanding': 0.8, 'Launch': 1.8}[label]
        if label == 'Recovery':
            duration = 0.6
        mike_clip = seeds.get(label)
        lizard_clip = reaction if label == 'Launch' else None
        if not exists:
            mike_clip = mike_clip or seeds['JumpApex']
            lizard_clip = lizard_clip or load(ROOT + '/Baked/AS_QTE_JumpReady_Lizard')
        prompt = contact = None
        offset = unreal.Vector(-80, 450, 110)
        fov = 48
        focus = 0.5
        mike_path = airborne
        lizard_path = stationary
        if label in BEATS:
            beat = beats[BEATS.index(label)]
            duration = g(beat, 'duration')
            prompt = g(beat, 'prompt_fraction') if label != 'Jump' else None
            contact = g(beat, 'contact_fraction') if label != 'Jump' else None
            offset = g(beat, 'camera_offset')
            fov = g(beat, 'fov')
        if label in ['Approach', 'JumpReady']:
            mike_path = lambda t: unreal.Vector(0, 0, 0)
        if label == 'JumpReady':
            prompt = g(data, 'jump_ready_prompt_fraction')
        if label == 'Jump':
            mike_path = lambda t: unreal.Vector(40 * smooth((t - 0.15) / 0.85), 0, 70 * smooth((t - 0.15) / 0.85))
        if label == 'Uppercut':
            mike_path = lambda t: unreal.Vector(40, 0, 70 + 30 * smooth(t / g(beats[1], 'contact_fraction')))
        if label == 'FlyingKick':
            mike_path = lambda t: unreal.Vector(40 + 25 * math.sin(math.pi * t), 0, 100)
        if label == 'CannonLanding':
            mike_path = lambda t: unreal.Vector(40, 0, 100 * (1 - smooth(t / 0.65)))
        if label in ['Cannon', 'CannonFire']:
            mike_path = lambda t: unreal.Vector(40, 0, 0)
            focus = 0
        if label == 'Cannon':
            offset = unreal.Vector(90, 180, 45)
            fov = 36
        if label == 'CannonFire':
            offset = unreal.Vector(-100, 650, 150)
            fov = 52
        if label == 'Recovery':
            mike_path = lambda t: unreal.Vector(40, 0, 100 * (1 - smooth(t)))
        if label == 'Counter':
            mike_path = lambda t: unreal.Vector(40 - 220 * smooth(t), 0, 65 * math.sin(math.pi * t))
        if label == 'Launch':
            lizard_path = lambda t: unreal.Vector(125 + 650 * smooth(t), -220 * smooth(t), 280 * math.sin(math.pi * t))
        build_source(label, {'Mike': mike_clip, 'Lizard': lizard_clip}, duration, mike_path, lizard_path, offset, fov, prompt, contact, focus)
    # Complete the initial migration's shared-pose blend. Never repeat after
    # the corrected source has been saved; subsequent FK keys remain authoritative.
    for label in ['Chomp', 'Claw']:
        sequence = load(ROOT + '/Sources/LS_QTE_' + label)
        if LIB.get_metadata_tag(sequence, 'QTEApexSeamVersion') != '3':
            VIEW.open_level_sequence(sequence)
            binding = next(b for b in sequence.get_bindings() if b.get_name() == 'Mike')
            replace_pose(sequence, binding, seeds[label], sequence.get_playback_end())
            LIB.set_metadata_tag(sequence, 'QTEApexSeamVersion', '3')
            save(sequence)
            VIEW.close_level_sequence()
    align_contact_reactions()
    migrate_capsule_proxies()
    align_capsule_ground()
    lower_kick_extension()
    fit_kick_contact()
    preview_cameras()
    align_cannon_fire()

def author_if_needed():
    global LABELS, BEATS
    # Retain the original migration for fresh workspaces. Existing revised
    # sources are authoritative and must not be reseeded by routine author runs.
    if LIB.get_metadata_tag(load(ROOT+'/Sources/LS_QTE_Claw'),'QTEAirborneVersion')!='1':
        active_labels,active_beats=LABELS,BEATS
        try:
            LABELS=LEGACY_LABELS
            BEATS=['Jump','Uppercut','Chomp','Claw','FlyingKick']
            author_airborne_if_needed()
        finally:
            LABELS,BEATS=active_labels,active_beats
    import importlib
    import QTEGroundedFinisher
    importlib.reload(QTEGroundedFinisher)
    QTEGroundedFinisher.author_if_needed()

