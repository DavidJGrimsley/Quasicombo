"""Author eight isolated Mike Arena clips, then export raw animation-only GLBs.

Run in the open editor via Project/MenuTools/editor_console.py. Original assets
are read only. App motion owns horizontal travel and whole-body airborne lift.
"""
import json, math, hashlib, traceback, struct
from pathlib import Path
import unreal

ROOT = '/Game/Quasicombo/Mike/Arena'
OUT = Path('C:/Users/DJLeg/Documents/Unreal Projects/Quasicombo-level-design/Saved/ArenaDefense')
OUT.mkdir(parents=True, exist_ok=True)
LIB = unreal.EditorAssetLibrary
packed = Path('F:/ReactNativeApps/xr-showcase-i2Workspace/xr-showcase-phase-2-arena/assets/arena/Mike_Player1_Blue_AR.glb').read_bytes()
count = struct.unpack_from('<I',packed,12)[0]
nodes = json.loads(packed[20:20+count])['nodes']
parents = {nodes[c]['name'].lower():n.get('name','').lower() for n in nodes for c in n.get('children',[])}
sources = {}
def source(name):
    if name not in sources:
        sources[name] = unreal.load_asset('/Game/RadicalMike/Animations/Anim_ZMIKE_'+name)
        assert sources[name], name
    return sources[name]
def pose(name, t):
    a = source(name)
    return {n: unreal.AnimationLibrary.get_bone_pose_for_time(a,n,min(t,a.get_play_length()-.00001),False) for n in names}
def mix(a,b,t):
    out = {}
    for n in names:
        p = unreal.Transform()
        p.translation = a[n].translation + (b[n].translation-a[n].translation)*t
        p.rotation = unreal.MathLibrary.quat_slerp(a[n].rotation,b[n].rotation,t)
        p.scale3d = a[n].scale3d + (b[n].scale3d-a[n].scale3d)*t
        out[n] = p
    return out
def smooth(t):
    t = max(0,min(1,t)); return t*t*(3-2*t)
def normalize(v):
    return v / math.sqrt(v.x*v.x+v.y*v.y+v.z*v.z)
def between(a,b):
    a,b = normalize(a),normalize(b)
    cross = unreal.Vector(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x)
    q = unreal.Quat(cross.x,cross.y,cross.z,1+a.x*b.x+a.y*b.y+a.z*b.z)
    length = math.sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w)
    return unreal.Quat(q.x/length,q.y/length,q.z/length,q.w/length)
def inverse(q):
    return unreal.Quat(-q.x,-q.y,-q.z,q.w)
def world(p,n,cache):
    if n not in cache:
        parent = parents.get(n)
        if parent in p:
            r,v = world(p,parent,cache)
            cache[n] = (r*p[n].rotation, v+r.rotate_vector(p[n].translation))
        else:
            cache[n] = (p[n].rotation,p[n].translation)
    return cache[n]
def aim(p,bone,child,direction):
    cache = {}; q,v = world(p,bone,cache); _,end = world(p,child,cache)
    pq = world(p,parents[bone],cache)[0]
    p[bone].rotation = inverse(pq) * between(end-v,direction) * q
def guard(t=0, hit=0):
    # Source uppercut fists and wrist orientation; held FK arms are authored separately.
    p = pose('IdleAggro',0)
    for side, label in [('l','UppercutL'),('r','UppercutR')]:
        ref = pose(label,.48)
        for n in names:
            if n.endswith('_'+side) and any(n.startswith(k) for k in ['index_','pinky_','thumb_','hand_']):
                p[n].rotation = ref[n].rotation
        sign = 1 if side == 'l' else -1
        breath = .25*math.sin(t*2*math.pi)
        aim(p,'upperarm_'+side,'lowerarm_'+side,unreal.Vector(sign*2,20-hit*3,8+breath))
        aim(p,'lowerarm_'+side,'hand_'+side,unreal.Vector(-sign*4,14-hit*2,14))
        # Keep the knuckles from the source uppercut, with both fists upright.
        cache={}; rq = world(ref,'hand_'+side,cache)[0]
        pq = world(p,'lowerarm_'+side,{})[0]
        p['hand_'+side].rotation = inverse(pq)*rq
    return p
def authored(label,t):
    idle = pose('IdleAggro',0)
    if label == 'BlockEnter': return mix(idle,guard(),smooth(t))
    if label == 'BlockExit': return mix(guard(),idle,smooth(t))
    if label == 'BlockHit': return guard(0,math.sin(t*math.pi))
    if label == 'BlockHold': return guard(t)
    if label.startswith('Block'):
        name={'BlockAdvance':'Walk','BlockStrafeLeft':'WalkLeft','BlockStrafeRight':'WalkRight'}[label]
        lower=pose(name,t*source(name).get_play_length())
        upper=guard(t)
        # Preserve source gait and spine balance; replace raised-arm rotations.
        for n in names:
            if any(n.startswith(k) for k in ['clavicle_','upperarm_','lowerarm_','hand_','index_','pinky_','thumb_','prop_','buster_']):
                lower[n].rotation=upper[n].rotation
        return lower
    # A short anticipation, held airborne silhouette, and planted landing.
    seconds=t*.4
    takeoff=pose('JumpStart',.5)
    air=pose('JumpApex',.5)
    landing=pose('JumpEnd',.0)
    if seconds < 2/30: p=mix(idle,takeoff,smooth(seconds/(2/30)))
    elif seconds < 10/30:
        u=(seconds-2/30)/(8/30)
        p=mix(takeoff,air,smooth(u/.3)) if u<.5 else mix(air,landing,smooth((u-.7)/.3))
    else: p=mix(landing,idle,smooth((seconds-10/30)/(2/30)))
    # Remove the original jump's body translation; app adds the 2.5 cm hop arc.
    p['pelvis'].translation=idle['pelvis'].translation
    return p

def main():
    global names
    names=list(map(str,unreal.AnimationLibrary.get_animation_track_names(source('IdleAggro'))))
    clips={'BlockEnter':3,'BlockHold':30,'BlockExit':3,'BlockHit':4,
           'BlockAdvance':44,'BlockStrafeLeft':44,'BlockStrafeRight':44,'JumpBack':12}
    source_files=list(Path('C:/Users/DJLeg/Documents/Unreal Projects/Quasicombo-level-design/Content/RadicalMike').rglob('*.uasset'))
    hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files}
    report={'status':'running','authorVersion':'1','exports':[],'sourceHashes':hashes}
    (OUT/'authoring.json').write_text(json.dumps(report,indent=2))
    options=unreal.GLTFExportOptions()
    for key,value in {'export_preview_mesh':False,'export_vertex_skin_weights':True,'export_animation_sequences':True,
                      'export_uniform_scale':.01,'use_mesh_quantization':False,'default_level_of_detail':0,'make_skinned_meshes_root':False}.items():
        options.set_editor_property(key,value)
    for label,frames in clips.items():
        path=ROOT+'/AS_Mike_'+label
        asset=unreal.load_asset(path) if LIB.does_asset_exist(path) else LIB.duplicate_asset(source('IdleAggro').get_path_name(),path)
        assert asset, path
        samples={n:[] for n in names}
        for i in range(frames+1):
            p=authored(label,i/frames)
            for n in ['root','sk_zmike']: p[n].translation=pose('IdleAggro',0)[n].translation
            for n in names: samples[n].append(p[n])
        c=asset.get_editor_property('controller');c.open_bracket('Bake Arena defense in place',False)
        c.set_frame_rate(unreal.FrameRate(30,1),False)
        c.set_number_of_frames(unreal.FrameNumber(frames),False)
        for n,poses in samples.items():
            assert c.set_bone_track_keys(n,[p.translation for p in poses],[p.rotation for p in poses],[p.scale3d for p in poses],False),n
        c.close_bracket(False)
        unreal.AnimationLibrary.remove_all_animation_notify_tracks(asset)
        asset.set_editor_property('enable_root_motion',False);asset.set_editor_property('force_root_lock',False)
        LIB.set_metadata_tag(asset,'ArenaAuthorVersion','1');LIB.set_metadata_tag(asset,'ArenaSourcePolicy','Derived; original assets read only')
        assert LIB.save_loaded_asset(asset)
        raw=OUT/(label+'.glb')
        assert unreal.GLTFExporter.export_to_gltf(asset,str(raw),options,set()) is not None
        assert raw.exists()
        report['exports'].append({'name':label,'sourceAsset':path,'file':raw.name,'durationSeconds':asset.get_play_length(),
                                  'loopRecommended':label in ['BlockHold','BlockAdvance','BlockStrafeLeft','BlockStrafeRight']})
        (OUT/'authoring.json').write_text(json.dumps(report,indent=2))
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in hashes.items())
    report['status']='complete';report['sourceAssetsByteIdentical']=True
    (OUT/'authoring-error.txt').write_text('Latest authoring run completed successfully.\n')
    (OUT/'authoring.json').write_text(json.dumps(report,indent=2))

try: main()
except Exception:
    (OUT/'authoring-error.txt').write_text(traceback.format_exc());unreal.log_error(traceback.format_exc())
