"""Capture the normal map in dedicated, hidden PIE sessions; never save the map.

Run with -ExecutePythonScript=<this file>. Runtime actor positioning is only
validation setup. Normal PIE uses the encounter's live or local evolution state.
"""
import json
import gc
import os
import math
import time
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
out = Path(unreal.Paths.project_saved_dir()) / os.environ.get('QUASICOMBO_EVOLUTION_OUTPUT','EvolutionValidation')
out.mkdir(parents=True, exist_ok=True)
scenarios = [(0,-1,1920,1080),(1,-1,1920,1080),(2,-1,1920,1080),
             (0,1,1920,1080),(1,1,1920,1080),(2,1,1920,1080),(2,1,960,1080),(2,-1,1920,1080)]
if os.environ.get('QUASICOMBO_EVOLUTION_DETAIL'):
    scenarios = [(1,-1,1920,1080),(2,-1,1920,1080),(2,1,960,1080),(2,-1,1920,1080)]
if os.environ.get('QUASICOMBO_EVOLUTION_PROBE'):
    scenarios = [(1,-1,1920,1080),(2,-1,1920,1080)]
if os.environ.get('QUASICOMBO_EVOLUTION_FIRE'):
    scenarios = [(1,-1,1920,1080),(1,1,1920,1080),(1,1,960,1080),(1,-1,1920,1080)]
index, initialized, started, stage_started, snapshots = 0, False, 0.0, 0.0, []
prepared_at = None
seen, previous_stage, between, broke_at = set(), '', 0.0, None
callback = None
detail_camera = None
original_camera, original_mode, original_dilation, original_ground = None, None, 1.0, None
motion=[]
restorations=[]
return_path=[]
face_samples=[]
initial_eyes=None
face_reveals=0
face_was_awarded=False
face_final_captured=False
record_motion=bool(os.environ.get('QUASICOMBO_EVOLUTION_RECORD'))
if record_motion:(out/'motion').mkdir(exist_ok=True)

def finish():
    global original_camera,original_mode,original_ground,levels,editor,callback
    (out/'report.json').write_text(json.dumps(snapshots, indent=2))
    (out/'motion.json').write_text(json.dumps(motion,indent=2))
    (out/'restorations.json').write_text(json.dumps(restorations,indent=2))
    (out/'return_path.json').write_text(json.dumps(return_path,indent=2))
    (out/'face_samples.json').write_text(json.dumps(face_samples,indent=2))
    unreal.unregister_slate_post_tick_callback(callback)
    # Drop editor/world wrappers before Python and UObject shutdown run in different orders.
    callback=None
    original_camera=original_mode=original_ground=None
    levels=editor=None
    gc.collect()
    unreal.SystemLibrary.quit_editor()

def validate_camera_path(scenario):
    points=[frame for frame in return_path if frame['scenario']==scenario]
    assert len(points)>2, 'Missing camera return samples'
    start,end=points[0]['location'],points[-1]['location']
    direction=[b-a for a,b in zip(start,end)]
    length2=sum(value*value for value in direction)
    assert length2>100., 'Camera did not pull back'
    previous=-1.
    for frame in points:
        offset=[value-origin for value,origin in zip(frame['location'],start)]
        progress=sum(a*b for a,b in zip(offset,direction))/length2
        residual=math.sqrt(sum((a-progress*b)**2 for a,b in zip(offset,direction)))
        assert residual<.1, 'Camera bent away from the diagonal: '+str(residual)
        assert progress>=previous-.001, 'Camera moved backwards along the return'
        previous=progress
    assert any(abs(a-b)>.5 for a,b in zip(points[0]['rotation'],points[-1]['rotation'])), 'Camera did not rotate during return'
    print('EVOLUTION_DIAGONAL_CAMERA_VERIFIED',scenario,len(points))

def validate_face_reveal(scenario):
    points=[frame for frame in face_samples if frame['scenario']==scenario]
    assert face_reveals==15, 'Expected fifteen eye reveals, got '+str(face_reveals)
    assert points and points[-1]['age']>=2.9, 'Face reveal was not extended to three seconds'
    pushing=[frame for frame in points if frame['age']>=.3]
    assert len(pushing)>3, 'Missing camera push-in samples'
    assert pushing[0]['distance']-pushing[-1]['distance']>30., 'Camera did not continue closer during flashes'
    for a,b in zip(pushing,pushing[1:]):
        assert b['distance']<=a['distance']+.5, 'Camera backed away during eye flashes'
    halfway=min(pushing,key=lambda frame:abs(frame['age']-1.625))
    early=pushing[0]['distance']-halfway['distance']
    late=halfway['distance']-pushing[-1]['distance']
    assert late>early*2.5, 'Camera push does not accelerate with the charge'
    colors=[]
    for frame in points:
        if not colors or frame['awarded']!=colors[-1][0]:
            colors.append((frame['awarded'],frame['age']))
    flashes=[b[1]-a[1] for a,b in zip(colors,colors[1:]) if a[0]]
    gaps=[b[1]-a[1] for a,b in zip(colors,colors[1:]) if not a[0] and a!=colors[0]]
    assert len(flashes)==14 and len(gaps)==14, 'Missing flash or gap timing samples'
    assert all(abs(seconds-.06)<.03 for seconds in flashes), 'New-color flickers did not stay at 60ms'
    assert gaps[0]>gaps[-1]*5., 'Old-color gaps did not become much shorter'
    ratio=.125**(1./13.)
    assert all(abs(b-a*ratio)<.04 for a,b in zip(gaps,gaps[1:])), 'Old-color gaps do not decay exponentially'
    bright=[frame for frame in points if frame['awarded']]
    assert bright[0]['emissive']<.15 and bright[-1]['emissive']>.95, 'Missing low-to-normal emissive ramp'
    for frame in bright:
        alpha=max(0.,min(1.,(135.-frame['distance'])/35.))
        assert abs(frame['emissive']-(.1+.9*alpha))<.03, 'Camera and emissive do not share the charge curve'
    assert points[-1]['awarded'], 'Final eye reveal did not stay on'
    hold=[frame for frame in points if frame['age']>=3.]
    assert len(hold)>3 and hold[-1]['age']-hold[0]['age']>.6, 'Missing 0.7-second close-up hold'
    assert all(frame['awarded'] and abs(frame['emissive']-1.)<.001 for frame in hold), 'Eyes changed during the hold'
    assert all(abs(frame['distance']-100.)<.25 for frame in hold), 'Camera left its closest framing during the hold'
    print('EVOLUTION_FACE_HOLD_VERIFIED',scenario,hold[-1]['age']-hold[0]['age'])
    print('EVOLUTION_FIFTEEN_FLICKERS_VERIFIED',scenario,flashes,gaps,early,late)

def validate_fire_rings(boss):
    first,second=boss.evolution.get_active_aura(),boss.evolution.get_stacked_aura()
    if not first: return
    assert second, 'Missing second fire ring'
    center,extent,_=unreal.SystemLibrary.get_component_bounds(boss.mesh)
    for ring in (first,second):
        # Skeletal bounds refresh after the presentation tick; allow one frame of pose motion.
        error=(ring.get_world_location()-center).length()
        assert error<max(1.,extent.z*.2), 'Fire ring left the middle tenth of the body: '+str(error)
    assert (first.get_world_location()-second.get_world_location()).length()<.01, 'Fire rings have separated'
    a,b=first.get_up_vector(),second.get_up_vector()
    expected=math.cos(math.radians(2*boss.evolution.get_editor_property('fire_ring_tilt_degrees')))
    assert abs(a.x*b.x+a.y*b.y+a.z*b.z-expected)<.001, 'Fire rings do not have opposing tilts'

def capture(world, boss, level, side, label, elapsed):
    if index == len(scenarios)-1: label = 'cancel_'+label
    _,_,width,height=scenarios[index]
    filename = 'level%d_side%d_%dx%d_%s.png' % (level,side,width,height,label)
    unreal.SystemLibrary.execute_console_command(world,'Quasicombo.EvolutionValidationShot "%s"' % (out/filename))
    snapshots.append(dict(level=level,side=side,stage=label,elapsed=elapsed,
        skin=boss.mesh.get_material(0).get_path_name(),eyes=boss.mesh.get_material(1).get_path_name(),
        base_health=boss.get_base_health_fraction(),body=boss.mesh.get_editor_property('skeletal_mesh_asset').get_path_name(),
        shields=boss.get_armor_bars(),resistance=boss.get_resistance(),debris=boss.evolution.get_debris_count(),file=filename))
    snapshots[-1]['height']=boss.get_actor_location().z-original_ground.z
    aura=boss.evolution.get_active_aura()
    if aura:
        snapshots[-1]['aura_active']=aura.is_active()
        snapshots[-1]['aura_location']=str(aura.get_world_location())
        snapshots[-1]['aura_asset']=aura.get_editor_property('asset').get_path_name()
        snapshots[-1]['aura_bounds']=str(unreal.SystemLibrary.get_component_bounds(aura))
    print('EVOLUTION_CAPTURE',snapshots[-1])

def tick_impl(_delta):
    global index, initialized, started, seen, previous_stage, stage_started, between, broke_at
    global original_camera, original_mode, original_dilation, original_ground
    global detail_camera
    global prepared_at
    global initial_eyes,face_reveals,face_was_awarded,face_final_captured
    world = editor.get_game_world()
    if not world:
        if index >= len(scenarios):
            print('EVOLUTION_VISUAL_VALIDATION_COMPLETE',len(snapshots))
            finish()
            return
        if time.monotonic() >= between:
            initialized, seen, previous_stage, broke_at, detail_camera = False, set(), '', None, None
            prepared_at=None
            _, _, width, height = scenarios[index]
            unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),
                'Quasicombo.EvolutionValidationPIE %d %d' % (width,height))
            between = time.monotonic()+30
        return
    player = unreal.GameplayStatics.get_player_pawn(world,0)
    boss = unreal.GameplayStatics.get_actor_of_class(world,unreal.QuasicomboBoss)
    if not player or not boss: return
    level, side, width, height = scenarios[index]
    now = unreal.GameplayStatics.get_time_seconds(world)
    if not initialized:
        # Give lighting and animation a few frames before reducing base health to the evolution threshold.
        if now < (5.0 if index == 0 else 2.0): return
        if prepared_at is None:
            player.set_actor_location(boss.get_actor_location()+unreal.Vector(side*420,0,0),False,True)
            player.character_movement.stop_movement_immediately()
            boss.set_actor_rotation(unreal.Rotator(yaw=0 if side>0 else 180),False)
            boss.quantum.initialize_preview(.2)
            boss.quantum.evolve_preview((.2,.5,.8)[level])
            boss.set_editor_property('preview_pre_evolution_tau',.2)
            boss.set_editor_property('preview_tau',(.2,.5,.8)[level])
            boss.set_editor_property('preview_armor_bars',level)
            prepared_at=now
            return
        if now-prepared_at<.5: return # Let the normal camera follow the test teleport before combat locks it.
        boss.start_encounter(player)
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        original_camera=pc.get_view_target()
        original_mode=player.character_movement.get_editor_property('movement_mode')
        original_dilation=unreal.GameplayStatics.get_global_time_dilation(world)
        original_ground=boss.get_actor_location()
        initial_eyes=boss.mesh.get_material(1).get_path_name()
        face_reveals,face_was_awarded=0,False
        face_final_captured=False
        for _ in range(boss.base_health_hits - boss.evolution_trigger_health): boss.apply_damage(1.0,None,boss.get_actor_location(),unreal.Vector())
        started, initialized = now, True
        return
    stage = str(boss.evolution.get_stage()).split('.')[-1].split(':')[0]
    elapsed = now-started
    if 'INACTIVE' not in stage:
        for bone in ['root','pelvis','spine_03','head','hand_l','hand_r','foot_l','foot_r']:
            transform=boss.mesh.get_socket_transform(bone,unreal.RelativeTransformSpace.RTS_COMPONENT)
            assert all(abs(v-1.)<.01 for v in [transform.scale3d.x,transform.scale3d.y,transform.scale3d.z]), 'Invalid bone scale: '+bone
            assert transform.translation.length()<350., 'Invalid bone position: '+bone
        assert boss.mesh.get_morph_target('EvolutionSmirk')==0., 'Smirk should no longer play'
        if level==1: validate_fire_rings(boss)
        if 'FACE' in stage:
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            view=pc.get_view_target()
            anchor='EvolutionEyes' if boss.mesh.does_socket_exist('EvolutionEyes') else 'head'
            head=boss.mesh.get_socket_location(anchor)
            awarded=boss.mesh.get_material(1).get_path_name()!=initial_eyes
            if awarded and not face_was_awarded:
                face_reveals+=1
            face_was_awarded=awarded
            face_age=now-stage_started if stage==previous_stage else 0.0
            # Sample the flickers and the start of the hold before screenshot readback.
            if face_reveals==15 and face_age>3.2 and not face_final_captured:
                capture(world,boss,level,side,'FACE_hold',elapsed)
                face_final_captured=True
            material=boss.mesh.get_material(1)
            emissive=material.get_scalar_parameter_value('EvolutionEmissiveScale') if awarded else None
            face_samples.append(dict(scenario=index,age=face_age,awarded=awarded,
                emissive=emissive,distance=(view.get_actor_location()-head).length()))
        if 'ARMOR' in stage:
            assert boss.get_actor_location().z-original_ground.z>=boss.evolution.lift_height-.1, 'Boss descended before armor finished'
        if 'RETURN' in stage:
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            view=pc.get_view_target()
            assert player.get_editor_property('hidden'), 'Player blocks the camera during its return'
            rotation=view.get_actor_rotation()
            return_path.append(dict(scenario=index,time=elapsed,location=[view.get_actor_location().x,view.get_actor_location().y,view.get_actor_location().z],rotation=[rotation.pitch,rotation.yaw,rotation.roll]))
    if record_motion and index==2 and 'INACTIVE' not in stage:
        file='motion/frame_%04d.png'%len(motion)
        unreal.SystemLibrary.execute_console_command(world,'Quasicombo.EvolutionValidationShot "%s"'%(out/file))
        motion.append(dict(file=file,time=elapsed,stage=stage,height=boss.get_actor_location().z-original_ground.z))
    if index == len(scenarios)-1 and 'RISE' in stage and elapsed>1.3:
        boss.evolution.cancel()
        stage='INACTIVE'
    if stage != previous_stage:
        previous_stage, stage_started = stage, now
    delay = 0.2 if 'STUMBLE' in stage or 'IDLE' in stage else 0.4 if 'FACE' in stage or 'ROAR' in stage else 1.1 if 'ARMOR' in stage else 0.25
    if stage not in seen and now-stage_started > delay:
        seen.add(stage)
        # Capture the face only after its final reveal. Readback stalls can skip
        # part of a 60ms flicker even when requested during an old-color gap.
        if 'FACE' not in stage:
            capture(world,boss,level,side,stage,elapsed)
    if 'INACTIVE' in stage and (elapsed > 6.0 or index == len(scenarios)-1):
        if broke_at is None:
            if index!=len(scenarios)-1:
                validate_camera_path(index)
                validate_face_reveal(index)
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            assert pc.get_view_target()==original_camera, 'Gameplay camera not restored'
            assert player.character_movement.get_editor_property('movement_mode')==original_mode, 'Player movement not restored'
            assert unreal.GameplayStatics.get_global_time_dilation(world)==original_dilation, 'Time dilation not restored'
            assert not player.get_editor_property('hidden'), 'Player visibility not restored'
            assert abs(boss.get_actor_location().z-original_ground.z)<10, 'Boss did not land on original ground'
            print('EVOLUTION_RESTORATION_VERIFIED',index,elapsed)
            restorations.append(dict(scenario=index,level=level,side=side,width=width,height=height,elapsed=elapsed))
            if level:
                for _ in range(4): boss.apply_damage(1.0,None,boss.get_actor_location(),unreal.Vector())
            broke_at = now
        elif level and detail_camera is None and now-broke_at > 1.0:
            unreal.SystemLibrary.execute_console_command(world,'Quasicombo.EvolutionValidationDebrisView')
            detail_camera=True
            broke_at=now
        elif now-broke_at > (0.3 if detail_camera else 1.0):
            if level: capture(world,boss,level,side,'debris',elapsed)
            levels.editor_request_end_play()
            index += 1
            between=time.monotonic()+2.0
    if elapsed > 45.0: raise RuntimeError('Evolution did not return to combat: '+stage)

def tick(delta):
    try: tick_impl(delta)
    except Exception:
        import traceback
        unreal.log_error(traceback.format_exc())
        levels.editor_request_end_play()
        finish()

levels.load_level('/Game/Maps/Lvl_Main')
callback=unreal.register_slate_post_tick_callback(tick)
