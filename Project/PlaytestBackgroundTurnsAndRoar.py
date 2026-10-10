"""Observe turning and a temporary roaring Blueprint in PIE; never edits the map."""
import json,math,time,traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
report = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / 'BackgroundGameplayValidation/turns-roar-pie.json'
# Editor-world queries are unavailable during PIE. Compare the play world to the
# saved-placement audit instead, including intentional stationary conversions.
audit = json.loads((report.parent / 'asset-audit.json').read_text())
assert audit['success'], 'Run AuditBackgroundGameplay.py before this playtest'
expected_roamers = {p['label'] for p in audit['placements'] if 'radius' in p}
stage = 'setup'
began = time.monotonic()
world = None
creatures = []
previous = {}
roarer = None
roar_component = None
placement = None
observing_since = 0
last_roar_start = 0
was_roaring = False
starts = []
returns = 0
curved_steps = 0
stepped_turn_frames = 0
pose_changes = 0
last_head = None
initial_rotation = None
was_readjusting = False
last_turn_start = 0
readjusts = []
turn_returns = 0

def spawn(cls, transform):
    lib = unreal.get_default_object(unreal.GameplayStatics)
    actor = lib.call_method('BeginDeferredActorSpawnFromClass',args=(world,cls,transform,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    return lib.call_method('FinishSpawningActor',args=(actor,transform,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))

def yaw_delta(a,b):
    return abs((a-b+180)%360-180)

def playing(actor):
    instance = actor.skeletal_mesh_component.get_anim_instance()
    return instance.get_animation_asset() if isinstance(instance,unreal.AnimSingleNodeInstance) else None

def finish(success,error=None):
    unreal.unregister_slate_post_tick_callback(callback)
    data = dict(success=success,error=error,curved_steps=curved_steps,stepped_turn_frames=stepped_turn_frames,
                roar_starts=starts,idle_returns=returns,roar_pose_changes=pose_changes,roaming_creatures=len(creatures),
                readjusts=readjusts,turn_returns=turn_returns,
                multipart_components=len(roarer.get_components_by_class(unreal.SkeletalMeshComponent)) if roarer else 0)
    report.write_text(json.dumps(data,indent=2))
    print('BACKGROUND_TURNS_ROAR_PIE_FINISHED',data)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)

def step():
    global stage,world,creatures,roarer,roar_component,placement,observing_since,last_roar_start,was_roaring,returns,curved_steps,stepped_turn_frames,pose_changes,last_head,initial_rotation,was_readjusting,last_turn_start,turn_returns
    current = editor.get_game_world()
    if not current: return
    cls = unreal.load_class(None,'/Game/UI/Menu/BP_QuasicomboFrontend.BP_QuasicomboFrontend_C')
    frontend = unreal.GameplayStatics.get_actor_of_class(current,cls)
    if frontend and not frontend.get_editor_property('MenuInitialized'): return
    if frontend and frontend.get_editor_property('IntroOnly'):
        if not frontend.get_editor_property('MenuStarting'): frontend.call_method('StorySkip')
        return
    run = unreal.QuasicomboRunSubsystem.get_run(current)
    if stage == 'setup':
        if frontend: frontend.call_method('StartFrontendRun')
        elif not run.has_run_started(): run.start_run()
        stage = 'start'
        return
    if stage == 'start':
        if not run.has_run_started(): return
        world = current
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SkeletalMeshActor):
            for component in actor.get_components_by_class(unreal.QuasicomboBackgroundRoamingComponent):
                creatures.append((actor,component))
                previous[actor.get_path_name()] = (actor.get_actor_location(),actor.get_actor_rotation().yaw)
        assert {actor.get_actor_label() for actor,component in creatures}==expected_roamers and expected_roamers
        focal = next(a for a,c in creatures if a.get_actor_label()=='ThirdPersonWalk')
        location = focal.get_actor_location()+unreal.Vector(500,0,0)
        roar_class = unreal.load_class(None,'/Game/Quasicombo/Background/BP_QC_MegaspikanRoarer.BP_QC_MegaspikanRoarer_C')
        roarer = spawn(roar_class,unreal.Transform(location=location))
        roarer.set_actor_scale3d(unreal.Vector(.5,.5,.5))
        roarer.set_actor_rotation(unreal.Rotator(0,180,0),False)
        placement = roarer.get_actor_transform()
        initial_rotation = roarer.get_actor_rotation()
        roar_component = roarer.get_editor_property('ambient_roar')
        assert not roarer.get_components_by_class(unreal.QuasicomboBackgroundRoamingComponent)
        assert len(roarer.get_components_by_class(unreal.SkeletalMeshComponent))==14
        body = roarer.skeletal_mesh_component
        body.set_editor_property('visibility_based_anim_tick_option',unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
        assert not roar_component.is_roaring() and playing(roarer)==roar_component.idle_animation
        camera = spawn(unreal.CameraActor.static_class(),unreal.Transform(location=location+unreal.Vector(-250,-1600,450)))
        camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),location+unreal.Vector(-150,0,180)),False)
        unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(camera,0)
        observing_since = last_roar_start = unreal.GameplayStatics.get_time_seconds(world)
        last_turn_start = observing_since
        stage = 'observe'
        print('BACKGROUND_TURNS_ROAR_PIE_OBSERVING')
        return
    assert current==world, 'Play world changed during preview'
    for actor,component in creatures:
        old_location,old_yaw = previous[actor.get_path_name()]
        location,yaw = actor.get_actor_location(),actor.get_actor_rotation().yaw
        if yaw_delta(yaw,old_yaw)>.01:
            clip = playing(actor)
            if clip and clip in [component.turn_left_animation,component.turn_right_animation]:
                stepped_turn_frames += 1
            else:
                assert (location-old_location).length()>.01 and clip==component.walk_animation, actor.get_actor_label()+' rotated an idle pose'
                curved_steps += 1
        assert math.hypot(location.x-component.get_roaming_anchor().x,location.y-component.get_roaming_anchor().y)<=component.roaming_radius+1
        previous[actor.get_path_name()] = (location,yaw)
    now = unreal.GameplayStatics.get_time_seconds(world)
    transform=roarer.get_actor_transform()
    assert transform.translation==placement.translation and transform.scale3d==placement.scale3d
    rotation=roarer.get_actor_rotation()
    offset=(rotation.yaw-initial_rotation.yaw+180)%360-180
    assert -45.2<=offset<=.2 and abs(rotation.pitch-initial_rotation.pitch)<.01 and abs(rotation.roll-initial_rotation.roll)<.01
    roaring = roar_component.is_roaring()
    body = roarer.skeletal_mesh_component
    if roaring and not was_roaring:
        interval = now-last_roar_start
        assert 4.8<=interval<=23.5, interval
        starts.append(dict(time=now-observing_since,interval=interval))
        last_roar_start = now
        assert playing(roarer)==roar_component.roar_animation
        print('BACKGROUND_LIVE_ROAR',starts[-1])
    if not roaring and was_roaring:
        assert 5.8<=now-last_roar_start<=6.6
        assert playing(roarer)==roar_component.idle_animation
        returns += 1
    readjusting=roar_component.is_readjusting()
    assert not (roaring and readjusting)
    if readjusting and not was_readjusting:
        interval=now-last_turn_start
        assert 19.8<=interval<=27,interval
        expected=roar_component.turn_left_animation if len(readjusts)%2==0 else roar_component.turn_right_animation
        assert playing(roarer)==expected
        readjusts.append(dict(time=now-observing_since,interval=interval,clip=expected.get_name()))
        last_turn_start=now
    if not readjusting and was_readjusting:
        turn_returns+=1
        assert playing(roarer)==roar_component.idle_animation
        assert abs(offset-(-45 if turn_returns%2 else 0))<.1
    was_readjusting=readjusting
    head = body.get_socket_transform('Megaspikan_Head')
    if roaring and last_head is not None and head!=last_head: pose_changes += 1
    last_head = head
    was_roaring = roaring
    if now-observing_since>=60:
        assert len(starts)>=2 and returns>=2 and curved_steps>0 and stepped_turn_frames>0 and pose_changes>0 and len(readjusts)>=2 and turn_returns>=2
        finish(True)

def tick(delta):
    try:
        if time.monotonic()-began>80: raise RuntimeError('Timed out at '+stage)
        step()
    except Exception: finish(False,traceback.format_exc())

callback = unreal.register_slate_post_tick_callback(tick)
print('BACKGROUND_TURNS_ROAR_PIE_STARTED')
