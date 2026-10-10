"""Author isolated QTE clips and paired, editable FK Control Rig sequences.

Run with UnrealEditor-Cmd <project> /Engine/Maps/Entry -ExecutePythonScript=<this file>.
Writes only /Game/Quasicombo/QTE. Never modifies a source animation or map.
Marked authored sources are retained on reruns. Only explicitly versioned
opening clips and sources are rebuilt when their choreography changes.
"""
import math
import unreal

ROOT = '/Game/Quasicombo/QTE'
LIB = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MIKE_MESH = '/Game/RadicalMike/Mesh/SKM_MegaMikeZ'
LIZARD_MESH = '/Game/Lizardman_Berserker/Mesh/SeparatedMesh/SK_Body'


def load(path):
    value = unreal.load_asset(path)
    assert value, path
    return value


def copy(source, path):
    if LIB.does_asset_exist(path):
        return load(path)
    value = LIB.duplicate_asset(source, path)
    assert value, path
    return value


def save(asset):
    assert LIB.save_loaded_asset(asset), asset.get_path_name()


def actor(mesh):
    a = ACTORS.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector())
    a.skeletal_mesh_component.set_skeletal_mesh_asset(load(mesh))
    return a


def curve(keys, t):
    for (a, x), (b, y) in zip(keys, keys[1:]):
        if t <= b:
            u = max(0, min(1, (t-a)/(b-a)))
            u = u*u*(3-2*u)
            return x+(y-x)*u
    return keys[-1][1]


def pose_clip(label, source, seconds, angles=None, loop=False, static=False, mesh=MIKE_MESH,
              start_time=0, end_time=None, blend_from=None, blend_to=None, blend_from_time=0, hold_last_frame=False, author_version='1'):
    """Bake component-axis joint adjustments into dedicated local bone tracks."""
    path = ROOT + ('/Mike/' if mesh == MIKE_MESH else '/Lizard/') + 'AS_QTE_' + label
    if LIB.does_asset_exist(path) and LIB.get_metadata_tag(load(path),'QTEAuthorVersion') == author_version:
        return load(path)
    base = load(source)
    out = copy(source, path)
    names = list(map(str, unreal.AnimationLibrary.get_animation_track_names(base)))
    rate = out.get_editor_property('controller').get_model_interface().get_frame_rate()
    frames = round(seconds*rate.numerator/rate.denominator)
    angles = angles or {}
    a = actor(mesh)
    parent = {name: str(a.skeletal_mesh_component.get_parent_bone(name)) for name in names}
    ACTORS.destroy_actor(a)
    samples = {name: [] for name in names}
    last_time = base.get_play_length() if end_time is None else end_time
    before = load(blend_from) if blend_from else None
    after = load(blend_to) if blend_to else None
    for i in range(frames+1):
        t = 1 if hold_last_frame and i>=frames-1 else i/frames
        poses = {name: unreal.AnimationLibrary.get_bone_pose_for_time(
            base, name, 0 if static else min(start_time+t*(last_time-start_time), base.get_play_length()-.0001), False)
            for name in names}
        # Local joint blending gives the takeoff and uppercut a shared airborne pose.
        blending_start = bool(before and t < .15)
        blend = before if blending_start else after if after and t > .7 else None
        if blend:
            alpha = curve([(0,0),(1,1)], t/.15 if blending_start else (t-.7)/.3)
            for name in names:
                other = unreal.AnimationLibrary.get_bone_pose_for_time(blend,name,blend_from_time if blending_start else 0,False)
                a,b = (other,poses[name]) if blending_start else (poses[name],other)
                blended = unreal.Transform()
                blended.translation = a.translation+(b.translation-a.translation)*alpha
                blended.rotation = unreal.MathLibrary.quat_slerp(a.rotation,b.rotation,alpha)
                blended.scale3d = a.scale3d+(b.scale3d-a.scale3d)*alpha
                poses[name] = blended
        global_rot = {}

        def rotation(name):
            if name not in global_rot:
                p = parent.get(name, 'None')
                pq = rotation(p) if p in poses else unreal.Quat()
                q = poses[name].rotation
                if name in angles:
                    degrees = curve(angles[name], t)
                    # Mesh forward is +Y; rotating around +X lifts a knee forward.
                    axis = pq.unrotate_vector(unreal.Vector(1, 0, 0))
                    half = math.radians(degrees)/2
                    q = unreal.Quat(axis.x*math.sin(half), axis.y*math.sin(half),
                                    axis.z*math.sin(half), math.cos(half))*q
                    poses[name].rotation = q
                global_rot[name] = pq*q
            return global_rot[name]

        for name in names:
            rotation(name)
            # In-place copies: director owns jumps, approaches and flight.
            if name in ('root', 'sk_zmike'):
                poses[name].translation = unreal.AnimationLibrary.get_bone_pose_for_time(base, name, 0, False).translation
            samples[name].append(poses[name])
    controller = out.get_editor_property('controller')
    controller.open_bracket('Bake QTE choreography at 30 fps', False)
    controller.set_number_of_frames(unreal.FrameNumber(frames), False)
    for name, poses in samples.items():
        assert controller.set_bone_track_keys(name, [p.translation for p in poses],
                                             [p.rotation for p in poses], [p.scale3d for p in poses], False), name
    controller.close_bracket(False)
    unreal.AnimationLibrary.remove_all_animation_notify_tracks(out)
    out.set_editor_property('enable_root_motion', False)
    out.set_editor_property('force_root_lock', False)
    LIB.set_metadata_tag(out,'QTEAuthorVersion',author_version)
    save(out)
    print('QTE_CLIP', path, frames)
    return out


def mike(name):
    return '/Game/RadicalMike/Animations/Anim_ZMIKE_' + name


