"""Versioned replacement of the fifth kick with a grounded, larger uppercut.
Run inside Unreal through AuthorQTECinematics.py. Ordinary animations are read only.
"""
import math
import unreal
import QTEChoreography as q
import QTEPoseAuthoring as pose

VERSION='1'

def mark_done(sequence, key):
    q.LIB.set_metadata_tag(sequence,key,VERSION)
    q.save(sequence)

def lizard_ground():
    return q.g(q.g(q.g(q.load(q.ROOT+'/DA_QTEPresentation'),'choreography')['Jump'],'samples')[0],'lizard_offset').z

def larger_path(t):
    # The opening few frames stay grounded. The uppercut drives a larger rise,
    # then settles into the shared apex for CannonLanding or prefix recovery.
    z=0 if t<=.48 else 145*q.smooth((t-.48)/.27) if t<=.75 else 145-45*q.smooth((t-.75)/.25)
    return unreal.Vector(40,0,z)

def reaction_clip(seconds, contact_frame):
    base=q.load(q.ROOT+'/Lizard/AS_QTE_UppercutReaction')
    path=q.ROOT+'/Lizard/AS_QTE_FinisherUppercutReaction'
    clip=pose.copy(base.get_path_name(),path)
    if q.LIB.get_metadata_tag(clip,'QTEGroundedReaction')==VERSION:
        return clip
    controller=q.g(clip,'controller'); frames=round(seconds*30)
    idle=q.load(q.ROOT+'/Baked/AS_QTE_JumpReady_Lizard')
    controller.open_bracket('Final uppercut reaction at Contact',False)
    try:
        controller.set_frame_rate(unreal.FrameRate(30,1),False)
        controller.set_number_of_frames(unreal.FrameNumber(frames),False)
        for bone in unreal.AnimationLibrary.get_animation_track_names(base):
            before=unreal.AnimationLibrary.get_bone_pose_for_time(idle,bone,0,False)
            samples=[]
            for i in range(frames+1):
                t=max(0,(i-contact_frame)/(frames-contact_frame))
                value=unreal.AnimationLibrary.get_bone_pose_for_time(base,bone,t*base.get_play_length(),False)
                alpha=q.smooth((i-contact_frame)/3)
                value.translation=before.translation+(value.translation-before.translation)*alpha
                value.rotation=unreal.MathLibrary.quat_slerp(before.rotation,value.rotation,alpha)
                samples.append(value)
            controller.set_bone_track_keys(bone,[p.translation for p in samples],[p.rotation for p in samples],[p.scale3d for p in samples],False)
    finally:
        controller.close_bracket(False)
    unreal.AnimationLibrary.remove_all_animation_notify_tracks(clip)
    mark_done(clip,'QTEGroundedReaction')
    return clip

def widen_camera(sequence):
    if q.LIB.get_metadata_tag(sequence,'QTEWideFinisherCamera')==VERSION:
        return
    q.VIEW.open_level_sequence(sequence); q.VIEW.refresh_current_level_sequence(); q.VIEW.set_current_time(0)
    binding,camera=q.bound_actor(sequence,'Camera')
    track=next(t for t in binding.get_tracks() if isinstance(t,unreal.MovieScene3DTransformTrack))
    channels=track.get_sections()[0].get_all_channels()
    for channel in channels[:6]:
        for key in list(channel.get_keys()): channel.remove_key(key)
    blend=next(t for t in binding.get_tracks() if isinstance(t,unreal.MovieSceneFloatTrack))
    for channel in blend.get_sections()[0].get_all_channels():
        for key in list(channel.get_keys()): key.set_value(.5)
    camera.set_editor_property('target_blend',.5)
    _,mike=q.bound_actor(sequence,'Mike'); _,lizard=q.bound_actor(sequence,'Lizard')
    for frame in range(sequence.get_playback_end()+1):
        q.VIEW.set_current_time(frame)
        target=(mike.get_actor_location()+lizard.get_actor_location())*.5+unreal.Vector(0,0,40)
        eye=target+unreal.Vector(-100,1050,220)
        rotation=unreal.MathLibrary.find_look_at_rotation(eye,target)
        for channel,value in zip(channels[:6],[eye.x,eye.y,eye.z,rotation.roll,rotation.pitch,rotation.yaw]):
            channel.add_key(unreal.FrameNumber(frame),value,interpolation=unreal.MovieSceneKeyInterpolation.LINEAR)
    lens=next(b for b in sequence.get_bindings() if b.get_name()=='Camera Lens')
    for channel in lens.get_tracks()[0].get_sections()[0].get_all_channels():
        for key in channel.get_keys(): key.set_value(58)
    mark_done(sequence,'QTEWideFinisherCamera')
    q.VIEW.close_level_sequence()

def fit_hand_contact(sequence):
    if q.LIB.get_metadata_tag(sequence,'QTEFinisherHandContact')=='2':
        return
    import QTEPublish
    q.VIEW.open_level_sequence(sequence); q.VIEW.refresh_current_level_sequence()
    frame=next(m.frame_number.value for m in sequence.get_marked_frames() if m.label=='Contact')
    end=sequence.get_playback_end()
    def bone_world(who,bone):
        binding,actor=q.bound_actor(sequence,who)
        template=q.load(q.ROOT+'/Mike/AS_QTE_JumpApex') if who=='Mike' else q.load(q.ROOT+'/Baked/AS_QTE_JumpReady_Lizard')
        clip=unreal.QuasicomboQTEPresentation.create_staged_clip(template)
        options=unreal.AnimSeqExportOption()
        options.use_custom_time_range=True; options.custom_display_rate=unreal.FrameRate(30,1)
        options.custom_start_frame=unreal.FrameNumber(0); options.custom_end_frame=unreal.FrameNumber(end)
        assert unreal.ControlRigSequencerLibrary.export_anim_sequence_from_sequencer(clip,options,binding,False)
        QTEPublish.normalize_in_place(clip)
        options=unreal.AnimPoseEvaluationOptions(); options.optional_skeletal_mesh=q.load(q.MESHES[who])
        p=unreal.AnimPoseExtensions.get_anim_pose_at_time(clip,frame/30,options)
        bone=unreal.AnimPoseExtensions.get_bone_pose(p,bone,unreal.AnimPoseSpaces.WORLD)
        q.VIEW.set_current_time(frame)
        return unreal.MathLibrary.compose_transforms(bone,q.mesh(actor).get_world_transform()).translation
    q.VIEW.set_current_time(frame)
    target=bone_world('Lizard','head'); hand=bone_world('Mike','hand_r')
    _,mike=q.bound_actor(sequence,'Mike')
    correction=target-hand+mike.get_actor_location()-larger_path(frame/end)
    correction.z=0
    print('QTE_FINISHER_FIT_INPUT',target,hand,correction,larger_path(frame/end))
    # Contact fitting does not change grounded anticipation or either endpoint.
    binding=next(b for b in sequence.get_bindings() if b.get_name()=='Mike')
    track=next(t for t in binding.get_tracks() if isinstance(t,unreal.MovieScene3DTransformTrack))
    channels=track.get_sections()[0].get_all_channels()
    for i in range(end+1):
        t=i/end
        weight=q.smooth((t-.12)/(frame/end-.12)) if i<=frame else 1-q.smooth((i-frame)/(end-frame))
        position=larger_path(t)+correction*weight
        position.z=max(0,position.z)
        assert position.z>=-.01,('Finisher path below floor',position)
        for channel,value in zip(channels[:3],[position.x,position.y,position.z]):
            next(k for k in channel.get_keys() if k.get_time().frame_number.value==i).set_value(value)
    print('QTE_FINISHER_CONTACT_FIT',target,hand,correction)
    q.LIB.set_metadata_tag(sequence,'QTEFinisherHandContact','2'); q.save(sequence); q.VIEW.close_level_sequence()

def author_if_needed():
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False),'Stop PIE before authoring'
    data=q.load(q.ROOT+'/DA_QTEPresentation')
    ground=lizard_ground(); idle=q.load(q.ROOT+'/Baked/AS_QTE_JumpReady_Lizard')
    apex=q.ROOT+'/Mike/AS_QTE_JumpApex'
    finisher_path=q.ROOT+'/Sources/LS_QTE_FinisherUppercut'
    if not q.LIB.does_asset_exist(finisher_path):
        seconds=1.2; frames=36; prompt=3; contact=17
        upper=pose.pose_clip('FinisherUppercut',pose.mike('UppercutR'),seconds,end_time=1.05,
            blend_to=apex,hold_last_frame=True,author_version='grounded1',angles={
                'spine_01':[(0,0),(.12,-5),(.45,-12),(.65,-5),(1,0)],
                'upperarm_r':[(0,0),(.15,0),(.45,12),(.65,8),(1,0)],
                'lowerarm_r':[(0,0),(.45,-8),(.65,-4),(1,0)]})
        landing=pose.pose_clip('FinisherLanding',pose.mike('JumpEnd'),.7,
            blend_from=q.ROOT+'/Baked/AS_QTE_Claw_Mike',
            blend_from_time=q.load(q.ROOT+'/Baked/AS_QTE_Claw_Mike').get_play_length(),
            blend_to=upper.get_path_name(),hold_last_frame=True,author_version='grounded1')
        q.build_source('FinisherLanding',{'Mike':landing,'Lizard':idle},.7,
            lambda t:unreal.Vector(40,0,100*(1-q.smooth(t/.75))),
            lambda t:unreal.Vector(125,0,ground),unreal.Vector(-100,1050,220),58)
        reaction=reaction_clip(seconds,contact)
        sequence=q.build_source('FinisherUppercut',{'Mike':upper,'Lizard':reaction},seconds,larger_path,
            lambda t:unreal.Vector(125+70*q.smooth((t-contact/frames)/.4),-15*q.smooth((t-contact/frames)/.4),ground+100*q.smooth((t-contact/frames)/.4)),
            unreal.Vector(-100,1050,220),58,prompt/frames,contact/frames)
    sequence=q.load(finisher_path)
    fit_hand_contact(sequence)
    if q.LIB.get_metadata_tag(data,'QTEGroundedFinisher')!=VERSION:
        beats=list(q.g(data,'beats')); beats[4].set_editor_property('mike_contact_bone','hand_r'); beats[4].set_editor_property('lizard_contact_bone','head')
        data.set_editor_property('beats',beats); q.save(data)
        mark_done(data,'QTEGroundedFinisher')
    launch=q.load(q.ROOT+'/Sources/LS_QTE_Launch')
    if q.LIB.get_metadata_tag(launch,'QTEUppercutLaunchPath')!=VERSION:
        lizard=next(b for b in launch.get_bindings() if b.get_name()=='Lizard')
        frames=launch.get_playback_end()
        q.transform_track(lizard,[unreal.Vector(125+260*q.smooth(i/frames),-60*q.smooth(i/frames),ground+360*math.sin(math.pi*i/frames)) for i in range(frames+1)],frames)
        mark_done(launch,'QTEUppercutLaunchPath')
    for name in ['FinisherLanding','FinisherUppercut','CannonLanding','Cannon','CannonFire','Launch']:
        sequence=q.load(q.ROOT+'/Sources/LS_QTE_'+name)
        widen_camera(sequence)
        # Camera cuts are already retained on old sources; add them to new ones.
        if not any(isinstance(t,unreal.MovieSceneCameraCutTrack) for t in sequence.get_tracks()):
            binding=next(b for b in sequence.get_bindings() if b.get_name()=='Camera')
            section=sequence.add_track(unreal.MovieSceneCameraCutTrack).add_section()
            section.set_range(0,sequence.get_playback_end()+1)
            section.set_camera_binding_id(sequence.get_binding_id(binding)); q.save(sequence)
    print('QTE_GROUNDED_FINISHER_AUTHORED')
