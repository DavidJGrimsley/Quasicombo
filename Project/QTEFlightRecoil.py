"""Versioned Launch recoil migration. Binary writes run inside Unreal MCP.

Launch frame zero is the editable seed for the slow-motion ragdoll. Reruns
retain manual FK edits, camera keys, paths, markers and binding identities.
"""
import json
import math
import unreal
import QTEChoreography as q
import QTEPoseAuthoring as pose

VERSION = '3'


def author_if_needed():
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE before authoring'
    sequence = q.load(q.ROOT + '/Sources/LS_QTE_Launch')
    if q.LIB.get_metadata_tag(sequence, 'QTEFlightRecoil') == VERSION:
        return
    data = q.load(q.ROOT + '/DA_QTEPresentation')
    mesh = q.load(q.MESHES['Lizard'])
    filename = unreal.Paths.project_saved_dir() + 'QTELizardReference.json'
    assert unreal.QuasicomboLizardAuthoringLibrary.export_reference_pose(mesh, filename)
    with open(filename) as stream:
        bones = json.load(stream)['bones']
    def transform(translation, rotation, scale=None):
        value = unreal.Transform()
        value.translation = translation
        value.rotation = rotation
        if scale is not None:
            value.scale3d = scale
        return value
    worlds = {b['name']: transform(unreal.Vector(*b['t']), unreal.Quat(*b['q'])) for b in bones}
    parents = {b['name']: b['parent'] for b in bones}
    reference = {name: unreal.MathLibrary.make_relative_transform(value, worlds[parents[name]]) if parents[name] else value for name, value in worlds.items()}
    clip = pose.copy(data.get_editor_property('lizard_flight').get_path_name(), q.ROOT + '/Lizard/AS_QTE_LaunchRecoil')
    frames = sequence.get_playback_end()
    controller = clip.get_editor_property('controller')
    names = list(map(str, unreal.AnimationLibrary.get_animation_track_names(clip)))
    # Shared-skeleton helper bones absent from this mesh keep their prior local
    # tracks; only the visible mesh's actual reference pose defines the recoil.
    for name in names:
        if name not in reference:
            reference[name] = unreal.AnimationLibrary.get_bone_pose_for_time(clip, name, 0, False)
            parents[name] = ''
    # Mesh-space axes: +Y is forward. Open the A-pose into a near T-pose;
    # lean the chest back, lift the knees and let the feet hang behind them.
    angles = {
        'spine_01': [('x', 10)], 'spine_02': [('x', 3)], 'neck_01': [('x', -5)],
        'upperarm_l': [('y', -28)], 'upperarm_r': [('y', 28)],
        'lowerarm_l': [('y', -8)], 'lowerarm_r': [('y', 8)],
        'thigh_l': [('x', 20), ('y', -8)], 'thigh_r': [('x', 20), ('y', 8)],
        'calf_l': [('x', -45)], 'calf_r': [('x', -45)],
        'foot_l': [('x', 15)], 'foot_r': [('x', 15)]}
    samples = {name: [] for name in names}
    for frame in range(frames + 1):
        t = frame / frames
        values = {name: transform(reference[name].translation, reference[name].rotation, reference[name].scale3d) for name in names}
        rotations = {}
        def rotation(name):
            if name not in rotations:
                parent = parents[name]
                pq = rotation(parent) if parent in values else unreal.Quat(0, 0, 0, 1)
                value = values[name]
                for axis_name, degrees in angles.get(name, []):
                    axis = pq.unrotate_vector(unreal.Vector(1, 0, 0) if axis_name == 'x' else unreal.Vector(0, 1, 0))
                    # A slight authored relaxation keeps the Sequencer preview
                    # readable; gameplay starts physics from the first key.
                    half = math.radians(degrees * (1 - .12 * q.smooth(t))) / 2
                    adjustment = unreal.Quat(axis.x * math.sin(half), axis.y * math.sin(half), axis.z * math.sin(half), math.cos(half))
                    value.rotation = adjustment * value.rotation
                rotations[name] = pq * value.rotation
            return rotations[name]
        for name in names:
            rotation(name)
            samples[name].append(values[name])
    controller.open_bracket('Spread-arm uppercut flyback recoil', False)
    try:
        controller.set_frame_rate(unreal.FrameRate(30, 1), False)
        controller.set_number_of_frames(unreal.FrameNumber(frames), False)
        for name, values in samples.items():
            assert controller.set_bone_track_keys(name, [p.translation for p in values], [p.rotation for p in values], [p.scale3d for p in values], False), name
    finally:
        controller.close_bracket(False)
    unreal.AnimationLibrary.remove_all_animation_notify_tracks(clip)
    clip.set_editor_property('enable_root_motion', False)
    clip.set_editor_property('retarget_source_asset', mesh)
    q.save(clip)
    guid_key = lambda b: tuple(b.get_id().get_editor_property(k) for k in ['a', 'b', 'c', 'd'])
    before = {b.get_name(): guid_key(b) for b in sequence.get_bindings()}
    q.VIEW.open_level_sequence(sequence)
    q.VIEW.refresh_current_level_sequence()
    binding = next(b for b in sequence.get_bindings() if b.get_name() == 'Lizard')
    q.replace_pose(sequence, binding, clip, frames)
    binding = next(b for b in sequence.get_bindings() if b.get_name() == 'Lizard')
    track = next(t for t in binding.get_tracks() if isinstance(t, unreal.MovieScene3DTransformTrack))
    yaw = track.get_sections()[0].get_all_channels()[5]
    yaw.set_default(120)
    for key in yaw.get_keys():
        key.set_value(120)
    if not any(m.label == 'Ragdoll seed' for m in sequence.get_marked_frames()):
        sequence.add_marked_frame(unreal.MovieSceneMarkedFrame(frame_number=unreal.FrameNumber(0), label='Ragdoll seed'))
    after = {b.get_name(): guid_key(b) for b in sequence.get_bindings()}
    assert before == after, ('Launch binding identities changed', before, after)
    q.LIB.set_metadata_tag(sequence, 'QTEFlightRecoil', VERSION)
    q.save(sequence)
    q.VIEW.close_level_sequence()
    print('QTE_FLIGHT_RECOIL_AUTHORED', clip.get_path_name(), 'bindings retained')
