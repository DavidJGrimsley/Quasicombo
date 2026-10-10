"""Bake one isolated victory finisher; never rewrite source or defense assets.

Run through the open editor with Project/MenuTools/editor_console.py.
The two existing CalibrationStretches plays precede this in-place clip.
"""
import hashlib, json, math, struct, traceback
from pathlib import Path
import unreal

PROJECT = Path('C:/Users/DJLeg/Documents/Unreal Projects/Quasicombo-level-design')
OUT = PROJECT / 'Saved/ArenaDefense'
REPORT = OUT / 'victory-authoring.json'
ASSET_PATH = '/Game/Quasicombo/Mike/Arena/AS_Mike_VictoryUppercut'
LIB = unreal.EditorAssetLibrary
sources = {}

def source(name):
    if name not in sources:
        sources[name] = unreal.load_asset('/Game/RadicalMike/Animations/Anim_ZMIKE_' + name)
        assert sources[name], name
    return sources[name]

def pose(name, seconds):
    asset = source(name)
    seconds = min(seconds, asset.get_play_length() - .00001)
    return {n: unreal.AnimationLibrary.get_bone_pose_for_time(asset, n, seconds, False) for n in names}

def mix(a, b, weight):
    result = {}
    for n in names:
        p = unreal.Transform()
        p.translation = a[n].translation + (b[n].translation - a[n].translation) * weight
        p.rotation = unreal.MathLibrary.quat_slerp(a[n].rotation, b[n].rotation, weight)
        p.scale3d = a[n].scale3d + (b[n].scale3d - a[n].scale3d) * weight
        result[n] = p
    return result

def smooth(t):
    t = max(0, min(1, t))
    return t * t * (3 - 2 * t)

def world(p, n, cache):
    if n not in cache:
        parent = parents.get(n)
        if parent in p:
            q, v = world(p, parent, cache)
            cache[n] = (q * p[n].rotation, v + q.rotate_vector(p[n].translation))
        else:
            cache[n] = (p[n].rotation, p[n].translation)
    return cache[n]

def aim(p, bone, child, direction):
    q, start = world(p, bone, {})
    _, end = world(p, child, {})
    a, b = end - start, direction
    a /= math.sqrt(a.x*a.x + a.y*a.y + a.z*a.z)
    b /= math.sqrt(b.x*b.x + b.y*b.y + b.z*b.z)
    cross = unreal.Vector(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x)
    delta = unreal.Quat(cross.x, cross.y, cross.z, 1+a.x*b.x+a.y*b.y+a.z*b.z)
    length = math.sqrt(delta.x**2+delta.y**2+delta.z**2+delta.w**2)
    delta = unreal.Quat(delta.x/length, delta.y/length, delta.z/length, delta.w/length)
    parent = world(p, parents[bone], {})[0]
    inverse = unreal.Quat(-parent.x, -parent.y, -parent.z, parent.w)
    p[bone].rotation = inverse * delta * q

def high_fist():
    p = pose('UppercutR', .53)
    # A fully extended celebratory sweep, distinct from the short combat attack.
    aim(p, 'upperarm_r', 'lowerarm_r', unreal.Vector(-4, 16, 25))
    aim(p, 'lowerarm_r', 'hand_r', unreal.Vector(-1, 10, 26))
    return p

def authored(seconds):
    idle = pose('IdleAggro', 0)
    finish = pose('CalibrationStretches', source('CalibrationStretches').get_play_length())
    if seconds < .30:
        return mix(finish, pose('UppercutR', .23), smooth(seconds / .30))
    if seconds < .65:
        t = (seconds - .30) / .35
        return mix(pose('UppercutR', .23 + .30*t), high_fist(), smooth(t))
    if seconds < 1.05:
        return high_fist()
    if seconds < 1.55:
        return mix(high_fist(), pose('UppercutR', 1.10), smooth((seconds - 1.05) / .50))
    return mix(pose('UppercutR', 1.10), idle, smooth((seconds - 1.55) / .45))

def main():
    global names, parents
    OUT.mkdir(parents=True, exist_ok=True)
    original = Path('F:/ReactNativeApps/xr-showcase-i2Workspace/xr-showcase-phase-2-arena/assets/arena/Mike_Player1_Blue_AR.glb').read_bytes()
    count = struct.unpack_from('<I', original, 12)[0]
    nodes = json.loads(original[20:20+count])['nodes']
    parents = {nodes[c]['name'].lower(): n.get('name', '').lower() for n in nodes for c in n.get('children', [])}
    names = list(map(str, unreal.AnimationLibrary.get_animation_track_names(source('IdleAggro'))))
    protected = list((PROJECT/'Content/RadicalMike').rglob('*.uasset'))
    protected += [p for p in (PROJECT/'Content/Quasicombo/Mike/Arena').glob('*.uasset') if p.stem != 'AS_Mike_VictoryUppercut']
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
    report = {'status': 'running', 'sourceHashes': hashes, 'exports': []}
    REPORT.write_text(json.dumps(report, indent=2))
    asset = unreal.load_asset(ASSET_PATH) if LIB.does_asset_exist(ASSET_PATH) else LIB.duplicate_asset(source('UppercutR').get_path_name(), ASSET_PATH)
    assert asset
    samples = {n: [] for n in names}
    idle = pose('IdleAggro', 0)
    for frame in range(61):
        p = authored(frame / 30)
        for n in ['root', 'sk_zmike']:
            p[n].translation = idle[n].translation
        for n in names:
            samples[n].append(p[n])
    c = asset.get_editor_property('controller')
    c.open_bracket('Bake in-place victory Uppercut', False)
    c.set_frame_rate(unreal.FrameRate(30, 1), False)
    c.set_number_of_frames(unreal.FrameNumber(60), False)
    for n, poses in samples.items():
        assert c.set_bone_track_keys(n, [p.translation for p in poses], [p.rotation for p in poses], [p.scale3d for p in poses], False), n
    c.close_bracket(False)
    unreal.AnimationLibrary.remove_all_animation_notify_tracks(asset)
    asset.set_editor_property('enable_root_motion', False)
    asset.set_editor_property('force_root_lock', False)
    LIB.set_metadata_tag(asset, 'ArenaSourcePolicy', 'Derived UppercutR; source and defense assets read only')
    LIB.set_metadata_tag(asset, 'ArenaAuthorVersion', 'Victory1')
    assert LIB.save_loaded_asset(asset)
    options = unreal.GLTFExportOptions()
    for key, value in {'export_preview_mesh': False, 'export_vertex_skin_weights': True, 'export_animation_sequences': True,
                       'export_uniform_scale': .01, 'use_mesh_quantization': False, 'default_level_of_detail': 0, 'make_skinned_meshes_root': False}.items():
        options.set_editor_property(key, value)
    raw = OUT/'VictoryUppercut.glb'
    assert unreal.GLTFExporter.export_to_gltf(asset, str(raw), options, set()) is not None
    assert raw.exists()
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in hashes.items())
    report.update(status='complete', sourceAssetsByteIdentical=True, defenseAssetsByteIdentical=True,
                  exports=[{'name': 'VictoryUppercut', 'sourceAsset': ASSET_PATH, 'file': raw.name, 'durationSeconds': asset.get_play_length(), 'loopRecommended': False}],
                  choreography={'blendFromCalibration': .30, 'uppercutSweepEnds': .65, 'raisedFistHoldEnds': 1.05, 'returnToIdleEnds': 2.0})
    REPORT.write_text(json.dumps(report, indent=2))

try:
    main()
except Exception:
    (OUT/'victory-authoring-error.txt').write_text(traceback.format_exc())
    unreal.log_error(traceback.format_exc())
