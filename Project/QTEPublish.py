"""Stage every QTE export, validate seams, then publish clips and presentation data."""
import math
import unreal
from QTEChoreography import ROOT, LIB, TOOLS, VIEW, LABELS, BEATS, MESHES, load, save, bound_actor, g

def raw_frames(clip,frames,rate=30):
    options=unreal.AnimPoseEvaluationOptions()
    options.should_retarget=False
    options.evaluation_type=unreal.AnimDataEvalType.RAW
    return [unreal.AnimPoseExtensions.get_anim_pose_at_time(clip,min(i/rate,clip.get_play_length()),options) for i in range(frames+1)]

def normalize_in_place(clip):
    controller = clip.get_editor_property('controller')
    names = list(map(str, unreal.AnimationLibrary.get_animation_track_names(clip)))
    frames = round(clip.get_play_length() * 30)
    raw=raw_frames(clip,frames)
    for name in ['root', 'sk_zmike']:
        if name not in names:
            continue
        poses = [unreal.AnimPoseExtensions.get_bone_pose(p,name,unreal.AnimPoseSpaces.LOCAL) for p in raw]
        assert controller.set_bone_track_keys(name, [poses[0].translation] * (frames + 1), [p.rotation for p in poses], [p.scale3d for p in poses], False)
    unreal.AnimationLibrary.remove_all_animation_notify_tracks(clip)
    clip.set_editor_property('enable_root_motion', False)

def copy_animation(staged, destination):
    controller = destination.get_editor_property('controller')
    rate = staged.get_editor_property('controller').get_model_interface().get_frame_rate()
    frames = round(staged.get_play_length() * rate.numerator / rate.denominator)
    raw=raw_frames(staged,frames,rate.numerator/rate.denominator)
    controller.open_bracket('Publish QTE Sequencer pose', False)
    try:
        controller.set_frame_rate(rate, False)
        controller.set_number_of_frames(unreal.FrameNumber(frames), False)
        for name in map(str, unreal.AnimationLibrary.get_animation_track_names(staged)):
            poses = [unreal.AnimPoseExtensions.get_bone_pose(p,name,unreal.AnimPoseSpaces.LOCAL) for p in raw]
            controller.add_bone_curve(name, False)
            assert controller.set_bone_track_keys(name, [p.translation for p in poses], [p.rotation for p in poses], [p.scale3d for p in poses], False), name
    finally:
        controller.close_bracket(False)
    normalize_in_place(destination)

def publish(presentation=None, save_outputs=True, sources_override=None, output_root=None):
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE before publishing QTE'
    data = presentation or load(ROOT + '/DA_QTEPresentation')
    sources = {label: load(ROOT + '/Sources/LS_QTE_' + label) for label in LABELS}
    for label, segment in g(data, 'choreography').items():
        if str(label) in sources and g(segment, 'source'):
            value=g(segment,'source')
            sources[str(label)]=value if isinstance(value,unreal.LevelSequence) else load(str(value))
    sources.update(sources_override or {})
    output_root=output_root or ROOT+'/Baked'
    previous = VIEW.get_current_level_sequence()
    previous_time = VIEW.get_current_time() if previous else 0
    staged = {}
    segments = {}
    try:
        for label, sequence in sources.items():
            rate = sequence.get_display_rate()
            fps = rate.numerator / rate.denominator
            assert fps == 30, (label, 'QTE sources use 30 fps')
            start = sequence.get_playback_start()
            end = sequence.get_playback_end()
            frames = end - start
            assert frames > 0
            bindings = {b.get_name(): b for b in sequence.get_bindings()}
            assert {'Mike', 'Lizard', 'Camera', 'Camera Lens'}.issubset(bindings), (label, 'required bindings')
            assert start==0,(label,'sources begin at frame zero')
            mark = {m.label: m.frame_number.value for m in sequence.get_marked_frames()}
            if label in BEATS[1:]:
                assert start < mark.get('Prompt', -1) < mark.get('Contact', -1) < end, (label, 'Prompt before Contact')
            if label == 'JumpReady':
                assert start < mark.get('Prompt', -1) < end
            if label == 'Cannon':
                assert start < mark.get('Prompt', -1) < end
            segment = unreal.QuasicomboQTESegment()
            segment.set_editor_property('source', sequence)
            segment.set_editor_property('duration', frames / fps)
            segment.set_editor_property('prompt_fraction', (mark['Prompt'] - start) / frames if 'Prompt' in mark else -1)
            segment.set_editor_property('contact_fraction', (mark['Contact'] - start) / frames if 'Contact' in mark else -1)
            VIEW.open_level_sequence(sequence)
            VIEW.refresh_current_level_sequence()
            VIEW.set_current_time(start)
            mike_binding, mike = bound_actor(sequence, 'Mike')
            lizard_binding, lizard = bound_actor(sequence, 'Lizard')
            _, camera = bound_actor(sequence, 'Camera')
            samples = []
            for frame in range(start, end + 1):
                VIEW.set_current_time(frame)
                m = mike.get_actor_location()
                b = lizard.get_actor_location()
                c = camera.get_actor_location()
                sample = unreal.QuasicomboQTESample()
                sample.set_editor_property('fraction', (frame - start) / frames)
                sample.set_editor_property('mike_offset', m)
                sample.set_editor_property('lizard_offset', b)
                sample.set_editor_property('lizard_yaw', lizard.get_actor_rotation().yaw)
                assert math.isfinite(g(sample, 'lizard_yaw')), (label, 'finite lizard facing')
                sample.set_editor_property('target_blend', g(camera, 'target_blend'))
                focus=m*(1-g(camera,'target_blend'))+b*g(camera,'target_blend')+g(camera,'focus_offset')
                # Retain authored camera rotation while tracking the live subjects.
                aim=c+camera.get_actor_forward_vector()*(focus-c).length()
                sample.set_editor_property('focus_offset', g(camera,'focus_offset')+aim-focus)
                sample.set_editor_property('camera_offset', c - (m * (1 - g(sample, 'target_blend')) + b * g(sample, 'target_blend') + g(sample, 'focus_offset')))
                sample.set_editor_property('fov', g(camera.get_component_by_class(unreal.CameraComponent), 'field_of_view'))
                assert math.isfinite(g(sample, 'fov')) and 5 < g(sample, 'fov') < 170
                assert all((math.isfinite(x) for x in [m.x, m.y, m.z, b.x, b.y, b.z, c.x, c.y, c.z]))
                samples.append(sample)
            segment.set_editor_property('samples', samples)
            for who, binding in [('Mike', mike_binding), ('Lizard', lizard_binding)]:
                template=load(ROOT+'/Mike/AS_QTE_JumpApex') if who=='Mike' else load(ROOT+'/Baked/AS_QTE_JumpReady_Lizard')
                clip=unreal.QuasicomboQTEPresentation.create_staged_clip(template)
                options = unreal.AnimSeqExportOption()
                options.record_in_world_space = False
                options.use_custom_time_range = True
                options.custom_display_rate = rate
                options.custom_start_frame = unreal.FrameNumber(start)
                options.custom_end_frame = unreal.FrameNumber(end)
                assert unreal.ControlRigSequencerLibrary.export_anim_sequence_from_sequencer(clip, options, binding, False), (label, who)
                normalize_in_place(clip)
                assert g(clip, 'skeleton') == g(load(MESHES[who]), 'skeleton')
                assert len(unreal.AnimationLibrary.get_animation_track_names(clip)) > 20
                staged[label, who] = clip
            segments[label] = segment
            print('QTE_PUBLISH_STAGED', label)
        for left, right in [('Jump', 'Uppercut'), ('Uppercut', 'JumpApex'), ('JumpApex', 'Chomp'), ('Chomp', 'JumpApex'), ('JumpApex', 'Claw'), ('Claw', 'FinisherLanding'), ('FinisherLanding', 'FinisherUppercut'), ('FinisherUppercut', 'CannonLanding'), ('CannonLanding', 'Cannon'), ('Cannon','CannonFire')]:
            a, b = (staged[left, 'Mike'], staged[right, 'Mike'])
            for bone in ['calf_r', 'thigh_r', 'upperarm_r', 'pelvis']:
                p = unreal.AnimationLibrary.get_bone_pose_for_time(a, bone, a.get_play_length(), False)
                q = unreal.AnimationLibrary.get_bone_pose_for_time(b, bone, 0, False)
                assert p.rotation.equals(q.rotation, 0.07), (left, right, bone, 'pose seam')
            end_position=g(g(segments[left],'samples')[-1],'mike_offset')
            start_position=g(g(segments[right],'samples')[0],'mike_offset')
            assert (end_position-start_position).length()<.1,(left,right,'trajectory seam')
        for label in ['JumpApex', 'Chomp', 'Claw']:
            assert all((g(s, 'mike_offset').z > 30 for s in g(segments[label], 'samples'))), (label, 'airborne path')
        assert abs(g(g(segments['CannonLanding'], 'samples')[-1], 'mike_offset').z) < 0.01
        finisher=segments['FinisherUppercut']
        samples=g(finisher,'samples')
        prompt_index=round(g(finisher,'prompt_fraction')*(len(samples)-1))
        assert abs(g(samples[prompt_index],'mike_offset').z)<.01,'fifth prompt must be grounded'
        assert max(g(s,'mike_offset').z for s in samples)>max(g(s,'mike_offset').z for s in g(segments['Uppercut'],'samples')),'finisher rises higher'
        assert all(g(s,'fov')>=50 and abs(g(s,'camera_offset').y)>=800 for s in g(segments['Cannon'],'samples')),'charge framing must stay wide'
        for (label, who), clip in staged.items():
            path = output_root + '/AS_QTE_' + label + '_' + who
            if LIB.does_asset_exist(path):
                destination = load(path)
            else:
                factory = unreal.AnimSequenceFactory()
                factory.target_skeleton = g(clip, 'skeleton')
                factory.preview_skeletal_mesh = load(MESHES[who])
                destination = TOOLS.create_asset('AS_QTE_' + label + '_' + who, output_root, unreal.AnimSequence, factory)
            copy_animation(clip, destination)
            segments[label].set_editor_property(who.lower(), destination)
        beats = list(g(data, 'beats'))
        for i, label in enumerate(BEATS):
            segment = segments[label]
            beat = beats[i]
            beat.set_editor_property('mike', g(segment, 'mike'))
            beat.set_editor_property('lizard', g(segment, 'lizard'))
            beat.set_editor_property('duration', g(segment, 'duration'))
            if i > 0:
                beat.set_editor_property('prompt_fraction', g(segment, 'prompt_fraction'))
                beat.set_editor_property('contact_fraction', g(segment, 'contact_fraction'))
            beat.set_editor_property('camera_offset', g(g(segment, 'samples')[0], 'camera_offset'))
            beat.set_editor_property('fov', g(g(segment, 'samples')[0], 'fov'))
        data.set_editor_property('beats', beats)
        data.set_editor_property('choreography', segments)
        data.set_editor_property('mike_approach', g(segments['Approach'], 'mike'))
        data.set_editor_property('mike_jump_ready', g(segments['JumpReady'], 'mike'))
        data.set_editor_property('jump_ready_duration', g(segments['JumpReady'], 'duration'))
        data.set_editor_property('jump_ready_prompt_fraction', g(segments['JumpReady'], 'prompt_fraction'))
        for field, label, who in [('mike_charge', 'Cannon', 'Mike'), ('mike_fire', 'CannonFire', 'Mike'), ('mike_recover', 'Recovery', 'Mike'), ('lizard_recover', 'Recovery', 'Lizard'), ('mike_counter_reaction', 'Counter', 'Mike'), ('lizard_counter', 'Counter', 'Lizard'), ('lizard_flight', 'Launch', 'Lizard')]:
            data.set_editor_property(field, g(segments[label], who.lower()))
        if save_outputs:
            for segment in segments.values():
                save(g(segment, 'mike'))
                save(g(segment, 'lizard'))
            save(data)
        print('QTE_PUBLISH_COMPLETE', len(segments), 'segments; sources retained')
        return segments
    finally:
        VIEW.close_level_sequence()
        if previous:
            VIEW.open_level_sequence(previous)
            VIEW.set_current_time(previous_time)
