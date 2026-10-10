"""PIE integration checks; edits only the play world, then observes normal reload."""
import json
import math
import time
import traceback
from pathlib import Path
import unreal
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
report = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / 'BackgroundGameplayValidation'
stage = 'setup'
stage_time = time.monotonic()
started = stage_time
checks = []
world = None
creatures = []
trap = None
toggle = None
frozen = None
original_world = None

def record(message, **values):
    checks.append(dict(check=message, **values))
    print('BACKGROUND_PIE_CHECK', checks[-1])

def distance2d(vector):
    return math.hypot(vector.x,vector.y)

def advance(next_stage):
    global stage, stage_time
    stage, stage_time = next_stage, time.monotonic()

def finish(success, error=None):
    unreal.unregister_slate_post_tick_callback(callback)
    data = dict(success=success, checks=checks, error=error)
    (report/'pie.json').write_text(json.dumps(data,indent=2))
    print('BACKGROUND_PIE_FINISHED',data)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)

def step():
    global world, creatures, trap, toggle, frozen, original_world
    current = editor.get_game_world()
    if not current: return
    age = time.monotonic() - stage_time
    if unreal.GameplayStatics.get_current_level_name(current)=='Lvl_QuasicomboIntro':
        cls = unreal.load_class(None,'/Game/UI/Menu/BP_QuasicomboFrontend.BP_QuasicomboFrontend_C')
        frontend = unreal.GameplayStatics.get_actor_of_class(current,cls)
        if frontend and frontend.get_editor_property('MenuInitialized'):
            frontend.call_method('StorySkip')
        return
    player = unreal.GameplayStatics.get_player_pawn(current,0)
    if not player: return
    run = next(o for o in unreal.ObjectIterator(unreal.QuasicomboRunSubsystem) if o.get_outer()==current)
    if stage == 'setup':
        cls = unreal.load_class(None,'/Game/UI/Menu/BP_QuasicomboFrontend.BP_QuasicomboFrontend_C')
        frontend = unreal.GameplayStatics.get_actor_of_class(current,cls)
        if frontend and not frontend.get_editor_property('MenuInitialized'): return
        world = current
        actors = unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SkeletalMeshActor)
        for a in actors:
            components = a.get_components_by_class(unreal.QuasicomboBackgroundRoamingComponent)
            if components:
                c = components[0]
                creatures.append((a,c,c.get_roaming_anchor(),a.get_actor_scale3d()))
            traps = a.get_components_by_class(unreal.QuasicomboLarvaeTrapComponent)
            if traps: trap = (a,traps[0])
        assert len(creatures)==16 and trap
        assert not player.is_in_fatal_trap() and player.get_current_hp()>0
        assert (player.get_actor_location()-trap[0].get_actor_location()).length()>trap[1].trigger_radius
        record('Saved main level has 16 roamers and one spawn-safe trap',spawn_distance=(player.get_actor_location()-trap[0].get_actor_location()).length())
        if frontend: frontend.call_method('StartFrontendRun')
        elif not run.has_run_started(): run.start_run()
        advance('start_transition')
    elif stage == 'start_transition':
        if not run.has_run_started(): return
        advance('wander')
    elif stage == 'wander':
        if age<12: return
        moved=[]
        for a,c,anchor,scale in creatures:
            assert c.get_roaming_anchor()==anchor
            assert a.get_actor_scale3d()==scale
            distance = distance2d(a.get_actor_location()-anchor)
            assert distance<=c.roaming_radius+1
            moved.append({'actor':a.get_actor_label(),'distance':distance,'walking':c.is_roaming()})
            if a.get_actor_label()=='ThirdPersonWalk': toggle=(a,c,anchor)
        assert any(x['distance']>5 for x in moved)
        record('Live wandering remains bounded and retains scale',creatures=moved)
        assert toggle
        toggle[1].set_editor_property('idle_only',True)
        frozen=toggle[0].get_actor_transform()
        advance('idle')
    elif stage == 'idle':
        if age<1: return
        assert toggle[0].get_actor_transform()==frozen
        assert not toggle[1].is_roaming()
        assert toggle[1].get_roaming_anchor()==toggle[2]
        record('PIE checkbox immediately freezes full transform without teleport')
        toggle[1].set_editor_property('idle_only',False)
        advance('resume')
    elif stage == 'resume':
        if age<5: return
        assert toggle[1].get_roaming_anchor()==toggle[2]
        assert distance2d(toggle[0].get_actor_location()-toggle[2])<=toggle[1].roaming_radius+1
        record('PIE checkbox resumes with original anchor')
        assert run.has_run_started()
        player.set_actor_location(trap[0].get_actor_location()+unreal.Vector(400,0,80),False,False)
        advance('capture')
    elif stage == 'capture':
        if age<.1: return
        assert player.is_in_fatal_trap()
        assert trap[1].state==unreal.QuasicomboLarvaeTrapState.EMERGING
        forward=trap[0].get_actor_transform().rotation.rotate_vector(unreal.Vector(0,1,0))
        direction=player.get_actor_location()-trap[0].get_actor_location()
        assert (forward.x*direction.x+forward.y*direction.y)/(distance2d(forward)*distance2d(direction))>.99
        assert not trap[1].try_trigger(player)
        hp=player.get_current_hp()
        player.do_jump_start(); player.do_dash(); player.do_combo_attack_start(); player.fire_buster()
        assert player.get_editor_property('character_movement').movement_mode==unreal.MovementMode.MOVE_NONE
        record('Entry commits Mike once and blocks jump/dash/attack/Buster',hp=hp)
        advance('impact')
    elif stage == 'impact':
        if player.get_current_hp()>0: return
        assert trap[1].has_delivered_lethal_hit()
        assert run.get_outcome()==unreal.QuasicomboRunOutcome.DEFEAT
        assert not player.is_in_fatal_trap()
        original_world=world
        record('Visible spit lands a normal fatal hit and defeat')
        advance('reload')
    elif stage == 'reload':
        if current==original_world: return
        assert player.get_current_hp()==player.get_max_hp() and not player.is_in_fatal_trap()
        actors=unreal.GameplayStatics.get_all_actors_of_class(current,unreal.SkeletalMeshActor)
        new_traps=[c for a in actors for c in a.get_components_by_class(unreal.QuasicomboLarvaeTrapComponent)]
        assert len(new_traps)==1
        assert new_traps[0].state==unreal.QuasicomboLarvaeTrapState.ARMED and not new_traps[0].has_delivered_lethal_hit()
        record('Normal defeat reload resets health, locks, and trap')
        finish(True)

def tick(delta):
    try:
        if time.monotonic()-started>65: raise RuntimeError('PIE validation timeout at '+stage)
        step()
    except Exception:
        finish(False,traceback.format_exc())

callback=unreal.register_slate_post_tick_callback(tick)
print('BACKGROUND_PIE_STARTED')
