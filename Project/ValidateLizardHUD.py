"""Inspect boss HUD, encounter lighting, crossed flames, and paused retry in PIE.

Run in a dedicated editor with -RenderOffScreen -ExecutePythonScript=<this file>.
Runtime placement is test setup only; this script never saves the map.
"""
import json
import time
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
out = Path(unreal.Paths.project_saved_dir()) / 'EvolutionHUDValidation'
out.mkdir(parents=True, exist_ok=True)
scenarios = [(0,1920,1080),(1,1920,1080),(2,1920,1080),(2,1860,550),(2,960,1080)]
index, phase, between, began, deadline = 0, 'setup', 0.0, 0.0, 0.0
records, seen = [], set()
pause_start, paused_time, paused_position = 0.0, 0.0, None
pause_checked, stages = False, set()
callback = None

def finish(success):
    (out/'report.json').write_text(json.dumps(dict(success=success,scenarios=index,
        authored_sunlight=authored_intensity,captures=records),indent=2))
    unreal.unregister_slate_post_tick_callback(callback)
    print('BOSS_HUD_VALIDATION_COMPLETE',success,index,len(records))
    unreal.SystemLibrary.quit_editor()

def sunlight(world):
    sun = unreal.GameplayStatics.get_actor_of_class(world,unreal.DirectionalLight)
    assert sun, 'Map directional light is missing'
    return sun.get_component_by_class(unreal.DirectionalLightComponent).get_editor_property('intensity')

def capture(world,boss,label):
    level,width,height = scenarios[index]
    filename = '%d_level%d_%dx%d_%s.png' % (index,level,width,height,label)
    intensity = sunlight(world)
    expected = 4.0 if label == 'before_evolution' else .2 if label in ('HOVER','ARMOR_ASSEMBLY','ROAR') else 1.0
    assert abs(intensity-expected)<0.001, 'Unexpected sunlight at %s: %s' % (label,intensity)
    unreal.SystemLibrary.execute_console_command(world,'Quasicombo.EvolutionValidationShot "%s"' % (out/filename))
    records.append(dict(file=filename,level=level,stage=label,shields=boss.get_armor_bars(),
        status=boss.get_encounter_status(),buffs=boss.get_evolution_buffs(),sunlight=intensity,
        paused=unreal.GameplayStatics.is_game_paused(world)))

def end_session():
    global index,phase,between
    levels.editor_request_end_play()
    index += 1
    phase,between = 'setup',time.monotonic()+2

def tick_impl(_delta):
    global phase,between,began,deadline,seen,pause_start,paused_time,paused_position
    global pause_checked,stages
    world = editor.get_game_world()
    real = time.monotonic()
    if not world:
        assert sunlight(editor.get_editor_world()) == authored_intensity, 'PIE changed authored sunlight'
        if index == len(scenarios):
            finish(True)
            return
        if real >= between:
            _,width,height = scenarios[index]
            unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),
                'Quasicombo.EvolutionValidationPIE %d %d' % (width,height))
            between = real+30
        return
    player = unreal.GameplayStatics.get_player_pawn(world,0)
    boss = unreal.GameplayStatics.get_actor_of_class(world,unreal.QuasicomboBoss)
    if not player or not boss: return
    now = unreal.GameplayStatics.get_time_seconds(world)
    level,_,_ = scenarios[index]
    if phase == 'setup':
        if now < (5 if index==0 else 2): return
        assert not unreal.GameplayStatics.is_game_paused(world), 'PIE started paused after cleanup'
        player.set_actor_location(boss.get_actor_location()+unreal.Vector(-420,0,0),False,True)
        player.character_movement.stop_movement_immediately()
        boss.set_actor_rotation(unreal.Rotator(yaw=180),False)
        boss.quantum.initialize_preview(.2)
        boss.quantum.evolve_preview((.2,.5,.8)[level])
        boss.set_editor_property('preview_pre_evolution_tau',.2)
        boss.set_editor_property('preview_tau',(.2,.5,.8)[level])
        boss.set_editor_property('preview_armor_bars',level)
        boss.start_encounter(player)
        phase,began,deadline,seen,stages = 'before',real,real+35,set(),set()
        pause_checked = False
        return
    if real > deadline: raise RuntimeError('HUD validation timed out: '+phase)
    if phase == 'before':
        if real-began < 1.1: return
        assert not boss.get_evolution_buffs(), 'Buffs should start empty'
        assert 'retry' not in boss.get_encounter_status().lower(), 'Persistent retry count remains'
        capture(world,boss,'before_evolution')
        for _ in range(boss.base_health_hits - boss.evolution_trigger_health): boss.apply_damage(1.0,None,boss.get_actor_location(),unreal.Vector())
        phase = 'evolving'
        return
    stage = str(boss.evolution.get_stage()).split('.')[-1].split(':')[0]
    if phase == 'evolving':
        if stage not in stages:
            stages.add(stage)
            began = real
        delay = 0.35 if 'HOVER' in stage else 1.1 if 'ARMOR' in stage else 0.3
        if stage not in seen and real-began > delay and stage in ('HOVER','ARMOR_ASSEMBLY','ROAR'):
            seen.add(stage)
            if stage != 'ROAR':
                lower,upper = boss.evolution.get_active_aura(),boss.evolution.get_stacked_aura()
                if level == 1:
                    assert lower and upper and lower.is_active() and upper.is_active(), 'Both flame rings must be active'
                    delta = upper.get_world_location()-lower.get_world_location()
                    assert delta.length()<.01, 'Fire rings must share the body center'
                    center,extent,_=unreal.SystemLibrary.get_component_bounds(boss.mesh)
                    assert (lower.get_world_location()-center).length()<max(1.,extent.z*.2), 'Fire rings must follow the body midpoint'
                    assert abs(abs(lower.get_world_rotation().roll)-boss.evolution.get_editor_property('fire_ring_tilt_degrees'))<.01, 'Missing first ring tilt'
                    assert abs(lower.get_world_rotation().roll+upper.get_world_rotation().roll)<.01, 'Fire rings must tilt in opposite directions'
                    assert lower.get_editor_property('asset')==upper.get_editor_property('asset'), 'Fire rings must use same system'
                    print('CROSSED_FLAMES_VERIFIED',lower.get_world_rotation(),upper.get_world_rotation())
                else: assert not upper, 'Only flame outcome should have a stacked aura'
            capture(world,boss,stage)
        if 'INACTIVE' in stage:
            assert not boss.evolution.get_active_aura() and not boss.evolution.get_stacked_aura(), 'Auras not cleaned up'
            phase,began = 'combat',real
        return
    if phase == 'combat':
        if real-began < 1.1: return
        capture(world,boss,'combat')
        if level:
            for _ in range(4): boss.apply_damage(1.0,None,boss.get_actor_location(),unreal.Vector())
            phase,began = 'shield_broken',real
        else:
            boss.handle_encounter_failure()
            phase,pause_start = 'retry',real
            paused_time,paused_position = now,player.get_actor_location()
        return
    if phase == 'shield_broken':
        if real-began < 0.3: return
        capture(world,boss,'shield_broken')
        if index == 1:
            boss.handle_encounter_failure()
            phase,began = 'exit_during_retry',real
            return
        end_session()
        return
    if phase == 'exit_during_retry':
        if real-began < 0.25: return
        assert unreal.GameplayStatics.is_game_paused(world), 'Exit test must start during the retry pause'
        capture(world,boss,'exit_during_retry')
        print('RETRY_LEVEL_EXIT_VERIFIED')
        end_session()
        return
    if phase == 'retry':
        elapsed = real-pause_start
        if 'RETRY' in str(boss.get_boss_phase()):
            assert unreal.GameplayStatics.is_game_paused(world), 'Retry must pause the whole game'
            assert abs(now-paused_time)<0.001, 'Game clock moved during retry'
            delta = player.get_actor_location()-paused_position
            assert abs(delta.x)+abs(delta.y)+abs(delta.z)<0.01, 'Player moved during retry'
            if elapsed > 0.25 and not pause_checked:
                capture(world,boss,'one_more_chance')
                pause_checked = True
        else:
            assert pause_checked and 0.99 <= elapsed <= 1.5, 'Retry announcement did not last one real-time second'
            assert not unreal.GameplayStatics.is_game_paused(world), 'Retry did not release pause'
            assert 'COMBAT' in str(boss.get_boss_phase()), 'Retry did not restore combat'
            print('RETRY_REAL_TIME_PAUSE_VERIFIED',elapsed)
            capture(world,boss,'retry_restored')
            end_session()

def tick(delta):
    try: tick_impl(delta)
    except Exception:
        import traceback
        unreal.log_error(traceback.format_exc())
        levels.editor_request_end_play()
        finish(False)

levels.load_level('/Game/Maps/Lvl_Main')
authored_intensity = sunlight(editor.get_editor_world())
print('AUTHORED_BOSS_SUNLIGHT',authored_intensity)
callback = unreal.register_slate_post_tick_callback(tick)
