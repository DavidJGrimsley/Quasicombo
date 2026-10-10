"""Editor integration test using disposable source/output assets; no gameplay writes."""
import importlib
import pathlib
import sys
import traceback
import uuid
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
import unreal
import QTEChoreography as q
import QTEPublish as p
importlib.reload(p)

def run(fixture_root):
    source=q.load(q.ROOT+'/Sources/LS_QTE_FinisherUppercut')
    fixture=q.LIB.duplicate_asset(source.get_path_name(),fixture_root+'/LS_Fixture')
    data=unreal.new_object(unreal.QuasicomboQTEPresentation)
    original=q.load(q.ROOT+'/DA_QTEPresentation')
    data.set_editor_property('beats',q.g(original,'beats'))
    data.set_editor_property('approach_speed',237)
    data.set_editor_property('explosion_scale',4.75)
    try:
        baseline=q.g(original,'choreography')['FinisherUppercut']
        marks=fixture.get_marked_frames()
        prompt_index=next(i for i,m in enumerate(marks) if m.label=='Prompt')
        # Moving Prompt earlier stays on the grounded anticipation. A later
        # marker now enters the merged mesh's contact rise and must be rejected.
        prompt_frame=marks[prompt_index].frame_number.value-1
        fixture.set_marked_frame(prompt_index,unreal.FrameNumber(prompt_frame))
        binding=next(b for b in fixture.get_bindings() if b.get_name()=='Mike')
        path=next(t for t in binding.get_tracks() if isinstance(t,unreal.MovieScene3DTransformTrack))
        section=path.get_sections()[0]; channel=section.get_all_channels()[0]
        key=next(k for k in channel.get_keys() if k.get_time().frame_number.value==18)
        key.set_value(key.get_value()+3)
        lens=next(b for b in fixture.get_bindings() if b.get_name()=='Camera Lens')
        fov_channel=lens.get_tracks()[0].get_sections()[0].get_all_channels()[0]
        for k in fov_channel.get_keys(): k.set_value(k.get_value()+4)
        lizard=next(b for b in fixture.get_bindings() if b.get_name()=='Lizard')
        lizard_path=next(t for t in lizard.get_tracks() if isinstance(t,unreal.MovieScene3DTransformTrack))
        yaw=lizard_path.get_sections()[0].get_all_channels()[5]
        yaw.set_default(yaw.get_default()+15)
        for k in yaw.get_keys(): k.set_value(k.get_value()+15)
        q.VIEW.open_level_sequence(fixture); q.VIEW.refresh_current_level_sequence(); q.VIEW.set_current_time(18)
        rig=next(r.control_rig for r in unreal.ControlRigSequencerLibrary.get_control_rigs(fixture)
                 if r.track in binding.get_tracks())
        names=[str(k.name) for k in rig.get_hierarchy().get_controls()]
        control=next(n for n in names if 'upperarm_l' in n)
        transform=unreal.ControlRigSequencerLibrary.get_local_control_rig_euler_transform(fixture,rig,control,unreal.FrameNumber(18))
        transform.rotation.pitch+=8
        unreal.ControlRigSequencerLibrary.set_local_control_rig_euler_transform(fixture,rig,control,unreal.FrameNumber(18),transform,set_key=True)
        guid_key=lambda b: tuple(b.get_id().get_editor_property(k) for k in ['a','b','c','d'])
        binding_ids=[guid_key(b) for b in fixture.get_bindings()]
        result=p.publish(data,False,{'FinisherUppercut':fixture},fixture_root+'/Baked')['FinisherUppercut']
        assert abs(q.g(result,'prompt_fraction')-prompt_frame/fixture.get_playback_end())<.0001,'marker publishes'
        samples=q.g(result,'samples'); old=q.g(baseline,'samples')
        assert abs(q.g(samples[18],'mike_offset').x-q.g(old[18],'mike_offset').x-3)<.01,'path publishes'
        assert abs(q.g(samples[0],'fov')-q.g(old[0],'fov')-4)<.01,'camera publishes'
        yaw_delta=(q.g(samples[0],'lizard_yaw')-q.g(old[0],'lizard_yaw'))%360
        assert abs(yaw_delta-15)<.01,'lizard flight facing publishes'
        bone=unreal.AnimationLibrary.get_bone_pose_for_time
        before=bone(q.g(baseline,'mike'),'upperarm_l',.6,False)
        after=bone(q.g(result,'mike'),'upperarm_l',.6,False)
        assert not before.rotation.equals(after.rotation,.025),'edited FK pose publishes'
        assert binding_ids==[guid_key(b) for b in fixture.get_bindings()],'source bindings retained'
        assert q.g(data,'approach_speed')==237 and q.g(data,'explosion_scale')==4.75,'unrelated settings retained'
    finally:
        q.VIEW.close_level_sequence()

fixture_root=q.ROOT+'/PublishTest_'+uuid.uuid4().hex
try:
    run(fixture_root)
except Exception as error:
    # Failed assertions must also release Unreal wrappers before deleting the
    # disposable packages; Python's traceback otherwise retains run() locals.
    traceback.clear_frames(error.__traceback__)
    raise
finally:
    # Release the function's fixture/rig wrappers before forcing asset deletion.
    assert fixture_root.startswith(q.ROOT+'/PublishTest_')
    assert q.LIB.delete_directory(fixture_root), 'disposable fixture cleanup failed'
    assert not q.LIB.does_asset_exist(fixture_root+'/LS_Fixture'), 'fixture remains registered'
print('QTE_PUBLISH_INTEGRATION_PASSED: pose, marker, trajectory, flight facing, camera, bindings, unrelated settings; fixtures removed')
