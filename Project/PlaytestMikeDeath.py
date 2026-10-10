"""Observe Mike's death, corpse persistence, and normal retry through Unreal MCP PIE."""
import json,time,traceback
from pathlib import Path
import unreal

assert unreal.Paths.project_dir().replace('\\','/').rstrip('/').endswith('/Quasicombo-HUD')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
report=Path(unreal.Paths.project_saved_dir())/'BackgroundGameplayValidation/mike-death-pie.json'
began=time.monotonic();stage='menu';pawn=None;death_time=0;standing_height=0
samples=[];checks=[];shot_requested=False
montage=unreal.load_asset('/Game/Quasicombo/Mike/AM_Mike_Death')

def finish(success,error=None):
    unreal.unregister_slate_post_tick_callback(callback)
    report.write_text(json.dumps(dict(success=success,error=error,checks=checks,samples=samples),indent=2))
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    print('MIKE_DEATH_PIE_FINISHED',success,error)

def step():
    global stage,pawn,death_time,standing_height,shot_requested
    assert time.monotonic()-began<60,'Timeout at '+stage
    world=editor.get_game_world()
    if not world:return
    frontend=unreal.GameplayStatics.get_actor_of_class(world,unreal.load_class(None,'/Game/UI/Menu/BP_QuasicomboFrontend.BP_QuasicomboFrontend_C'))
    run=unreal.QuasicomboRunSubsystem.get_run(world)
    if stage=='menu':
        if frontend and not frontend.get_editor_property('MenuInitialized'):return
        if frontend and frontend.get_editor_property('IntroOnly'):
            if not frontend.get_editor_property('MenuStarting'):frontend.call_method('StorySkip')
            return
        if frontend:frontend.call_method('StartFrontendRun')
        stage='start';return
    if stage=='start':
        if not run.has_run_started():return
        pawn=unreal.GameplayStatics.get_player_character(world,0)
        mesh=pawn.mesh
        mesh.set_editor_property('visibility_based_anim_tick_option',unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
        # Let a rendered living pose establish the reference before inflicting damage.
        death_time=unreal.GameplayStatics.get_time_seconds(world);stage='living';return
    if stage=='living':
        if unreal.GameplayStatics.get_time_seconds(world)-death_time<.5:return
        assert pawn.mesh.get_bone_index('head_01')>=0
        standing_height=pawn.mesh.get_socket_location('head_01').z-pawn.get_actor_location().z
        pawn.character_movement.stop_movement_immediately()
        unreal.GameplayStatics.apply_damage(pawn,pawn.get_current_hp(),None,None,unreal.DamageType)
        assert pawn.get_current_hp()==0 and run.get_outcome()==unreal.QuasicomboRunOutcome.DEFEAT
        assert pawn.mesh.get_anim_instance().montage_is_playing(montage)
        checks.append('Normal lethal damage starts the death montage and declares defeat')
        death_time=unreal.GameplayStatics.get_time_seconds(world);stage='death'
        print('MIKE_DEATH_PIE_OBSERVING');return
    if stage=='death':
        current=unreal.GameplayStatics.get_player_character(world,0)
        if current!=pawn:
            assert len(samples)>=3,'No corpse observations before reload'
            stage='retry';return
        mesh=pawn.mesh;t=unreal.GameplayStatics.get_time_seconds(world)-death_time
        h=mesh.get_socket_location('head_01').z-pawn.get_actor_location().z
        sample=dict(time=t,hp=pawn.get_current_hp(),simulating=mesh.is_simulating_physics(),paused=mesh.get_editor_property('pause_anims'),head_height=h,standing_height=standing_height)
        samples.append(sample)
        assert pawn.get_current_hp()==0
        if t>=1:
            assert mesh.is_simulating_physics() and mesh.get_editor_property('pause_anims'),sample
            assert mesh.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
            assert pawn.capsule_component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
            assert pawn.character_movement.movement_mode==unreal.MovementMode.MOVE_NONE
            assert h<standing_height-20,sample
        if t>=1.3 and not shot_requested:
            shot_requested=True
            path=(report.parent/'mike-corpse.png').resolve().as_posix()
            unreal.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename=\"'+path+'\"')
        if t>=2.1 and 'Corpse stays down throughout RUN LOST until reload' not in checks:
            checks.append('Corpse stays down throughout RUN LOST until reload')
            print('MIKE_CORPSE_PERSISTENCE_PASS',sample)
        return
    if stage=='retry':
        current=unreal.GameplayStatics.get_player_character(world,0)
        if not current or unreal.GameplayStatics.get_time_seconds(world)<.6:return
        assert current!=pawn and current.get_current_hp()>0 and run.has_run_started()
        assert not current.mesh.is_simulating_physics() and not current.mesh.get_editor_property('pause_anims')
        assert current.capsule_component.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
        assert current.character_movement.movement_mode!=unreal.MovementMode.MOVE_NONE
        checks.append('Automatic retry creates a living animated pawn with normal collision and movement')
        assert len(checks)==3,checks
        finish(True)

def tick(delta):
    try:step()
    except Exception:finish(False,traceback.format_exc())
callback=unreal.register_slate_post_tick_callback(tick)
print('MIKE_DEATH_PIE_STARTED')
