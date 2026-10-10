"""Read-only validation of retained sources and published airborne gameplay data."""
import pathlib
import sys
import math
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
import unreal
from QTEChoreography import ROOT, LABELS, MESHES, g, load

data=load(ROOT+'/DA_QTEPresentation')
segments=g(data,'choreography')
assert set(map(str,segments.keys()))==set(LABELS)
for label in LABELS:
    segment=segments[label]
    sequence=load(ROOT+'/Sources/LS_QTE_'+label)
    bindings={b.get_name():b for b in sequence.get_bindings()}
    assert {'Mike','Lizard','Camera','Camera Lens'}.issubset(bindings),(label,'bindings')
    for who in MESHES:
        rig=[t for t in bindings[who].get_tracks() if isinstance(t,unreal.MovieSceneControlRigParameterTrack)]
        assert rig and any(t.get_sections() for t in rig),(label,who,'editable FK')
        clip=g(segment,who.lower())
        assert clip and clip.get_play_length()>=.19
        assert g(clip,'skeleton')==g(load(MESHES[who]),'skeleton')
        assert not g(clip,'enable_root_motion')
        assert not unreal.AnimationLibrary.get_animation_notify_events(clip)
        names=set(map(str,unreal.AnimationLibrary.get_animation_track_names(clip)))
        assert len(names)>20
        for root in ['root','sk_zmike']:
            if root in names:
                poses=[unreal.AnimationLibrary.get_bone_pose_for_time(clip,root,t*clip.get_play_length(),False) for t in [0,.5,1]]
                assert all((p.translation-poses[0].translation).length()<.01 for p in poses),'no doubled actor motion'
    samples=g(segment,'samples')
    assert len(samples)>=7 and g(samples[0],'fraction')==0 and g(samples[-1],'fraction')==1
    assert all(math.isfinite(g(s,'lizard_yaw')) for s in samples),(label,'finite lizard facing')
    if label in ['JumpApex','Chomp','Claw']:
        assert min(g(s,'mike_offset').z for s in samples)>30,(label,'airborne')
    print('QTE_VALIDATED',label)
beats=g(data,'beats')
assert len(beats)==5
for i,beat in enumerate(beats):
    assert g(beat,'produces_contact')==(i>0)
    if i: assert 0<g(beat,'prompt_fraction')<g(beat,'contact_fraction')<1
assert 'FinisherUppercut' in g(beats[4],'mike').get_name()
assert str(g(beats[4],'mike_contact_bone'))=='hand_r'
assert str(g(beats[4],'lizard_contact_bone'))=='head'
finisher=segments['FinisherUppercut']; samples=g(finisher,'samples')
assert abs(g(samples[round(g(finisher,'prompt_fraction')*(len(samples)-1))],'mike_offset').z)<.01
assert max(g(s,'mike_offset').z for s in samples)>max(g(s,'mike_offset').z for s in g(segments['Uppercut'],'samples'))
assert all(g(s,'fov')>=50 and abs(g(s,'camera_offset').y)>=800 for s in g(segments['Cannon'],'samples'))
assert abs(g(g(segments['CannonLanding'],'samples')[-1],'mike_offset').z)<.01
launch=segments['Launch']
assert any(m.label=='Ragdoll seed' and m.frame_number.value==0 for m in g(launch,'source').get_marked_frames()),'editable launch seed marker'
assert g(data,'cinematic_diameter_multiplier')>=2
assert g(data,'approach_speed')==220
pose=unreal.AnimationLibrary.get_bone_pose_for_time
ready=g(data,'mike_jump_ready'); jump=g(beats[0],'mike')
for name in ['pelvis','thigh_r','calf_r','foot_r','upperarm_r','lowerarm_r']:
    assert pose(ready,name,ready.get_play_length(),False).rotation.equals(pose(jump,name,0,False).rotation,.035),(name,'squat/takeoff seam')
print('QTE_ASSET_VALIDATION_PASSED')
