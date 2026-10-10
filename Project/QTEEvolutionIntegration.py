"""One-time alignment to the merged lizard mesh; retain FK, markers and camera keys."""
import unreal, importlib
import QTEChoreography as q
import QTEPublish as publish
importlib.reload(q)
importlib.reload(publish)

def integrate_if_needed():
    assert not unreal.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE before migration'
    final=q.load(q.ROOT+'/Sources/LS_QTE_FinisherUppercut')
    if q.LIB.get_metadata_tag(final,'QTEEvolutionMeshFit')=='2':
        return
    boss=q.ACTORS.spawn_actor_from_class(q.LIB.load_blueprint_class(q.BLUEPRINTS['Lizard']),unreal.Vector(5000,0,0))
    try:
        actual=q.mesh(boss).get_relative_transform()
        cdo=unreal.get_default_object(q.LIB.load_blueprint_class(q.BLUEPRINTS['Lizard']))
        delta=actual.translation-q.mesh(cdo).get_relative_transform().translation
        for name in q.LABELS:
            source=q.load(q.ROOT+'/Sources/LS_QTE_'+name)
            previous=q.LIB.get_metadata_tag(source,'QTEEvolutionMesh')
            if previous=='2':
                continue
            binding=next(b for b in source.get_bindings() if b.get_name()=='Lizard')
            template=binding.get_object_template()
            assert template, (name,'Lizard spawnable required')
            component=q.mesh(template)
            old=component.get_relative_transform()
            component.set_skeletal_mesh_asset(q.load(q.MESHES['Lizard']))
            if not previous: old.translation=old.translation+delta
            component.set_relative_transform(old,False,True)
            q.LIB.set_metadata_tag(source,'QTEEvolutionMesh','2'); q.save(source)
    finally:
        q.ACTORS.destroy_actor(boss)
    q.VIEW.open_level_sequence(final); q.VIEW.refresh_current_level_sequence()
    frame=next(m.frame_number.value for m in final.get_marked_frames() if m.label=='Contact')
    end=final.get_playback_end()
    def bone_world(who,bone):
        q.VIEW.set_current_time(frame)
        binding,actor=q.bound_actor(final,who)
        template=q.load(q.ROOT+'/Mike/AS_QTE_JumpApex') if who=='Mike' else q.load(q.ROOT+'/Baked/AS_QTE_JumpReady_Lizard')
        clip=unreal.QuasicomboQTEPresentation.create_staged_clip(template)
        options=unreal.AnimSeqExportOption()
        options.use_custom_time_range=True; options.custom_display_rate=unreal.FrameRate(30,1)
        options.custom_start_frame=unreal.FrameNumber(0); options.custom_end_frame=unreal.FrameNumber(end)
        assert unreal.ControlRigSequencerLibrary.export_anim_sequence_from_sequencer(clip,options,binding,False)
        publish.normalize_in_place(clip)
        options=unreal.AnimPoseEvaluationOptions(); options.optional_skeletal_mesh=q.load(q.MESHES[who])
        pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(clip,frame/30,options)
        transform=unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.WORLD)
        q.VIEW.set_current_time(frame)
        return unreal.MathLibrary.compose_transforms(transform,q.mesh(actor).get_world_transform()).translation
    target=bone_world('Lizard','head'); hand=bone_world('Mike','hand_r')
    correction=target-hand
    binding=next(b for b in final.get_bindings() if b.get_name()=='Mike')
    channels=next(t for t in binding.get_tracks() if isinstance(t,unreal.MovieScene3DTransformTrack)).get_sections()[0].get_all_channels()
    prompt=next(m.frame_number.value for m in final.get_marked_frames() if m.label=='Prompt')
    for i in range(end+1):
        weight=q.smooth((i-prompt)/(frame-prompt)) if i<=frame else 1-q.smooth((i-frame)/(end-frame))
        for channel,amount in zip(channels[:3],[correction.x,correction.y,correction.z]):
            key=next(k for k in channel.get_keys() if k.get_time().frame_number.value==i)
            key.set_value(key.get_value()+amount*weight)
    q.LIB.set_metadata_tag(final,'QTEEvolutionMeshFit','2'); q.save(final)
    q.VIEW.close_level_sequence()
    print('QTE_EVOLUTION_MESH_FIT',delta,correction)

if __name__=='__main__':
    integrate_if_needed()
    publish.publish()
