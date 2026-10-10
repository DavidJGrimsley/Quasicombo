"""Check actual Buster pickup lighting in a dedicated offscreen PIE session.

Run with -RenderOffScreen -ExecutePythonScript=<this file>. Actor placement and
automatic QTE inputs are runtime validation only; this script never saves assets.
"""
import json
import time
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
out = Path(unreal.Paths.project_saved_dir()) / 'EvolutionPickupLightingValidation'
out.mkdir(parents=True, exist_ok=True)
phase, began, deadline = 'setup', 0.0, time.monotonic()+90
records, seen = [], set()
callback = None

def sunlight(world):
    sun = unreal.GameplayStatics.get_actor_of_class(world,unreal.DirectionalLight)
    assert sun, 'Directional light is missing'
    return sun.get_component_by_class(unreal.DirectionalLightComponent).get_editor_property('intensity')

def finish(success):
    (out/'report.json').write_text(json.dumps(dict(success=success,
        authored_sunlight=authored_intensity,captures=records),indent=2))
    unreal.unregister_slate_post_tick_callback(callback)
    print('PICKUP_LIGHTING_VALIDATION_COMPLETE',success,len(records))
    unreal.SystemLibrary.quit_editor()

def capture(world,label):
    assert abs(sunlight(world)-1.0)<0.001, 'Pickup restored sunlight: '+label
    filename = label+'.png'
    unreal.SystemLibrary.execute_console_command(world,
        'Quasicombo.EvolutionValidationShot "%s"' % (out/filename))
    records.append(dict(stage=label,sunlight=sunlight(world),file=filename))

def tick_impl(_delta):
    global phase,began
    real = time.monotonic()
    if real > deadline: raise RuntimeError('Pickup validation timed out: '+phase)
    world = editor.get_game_world()
    if not world:
        assert sunlight(editor.get_editor_world())==authored_intensity, 'Authored sunlight changed'
        if phase == 'exit':
            finish(True)
        elif phase == 'setup':
            unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),
                'Quasicombo.EvolutionValidationPIE 1920 1080')
            phase = 'waiting'
        return
    boss = unreal.GameplayStatics.get_actor_of_class(world,unreal.QuasicomboBoss)
    player = unreal.GameplayStatics.get_player_pawn(world,0)
    if not boss or not player: return
    if phase == 'waiting':
        if unreal.GameplayStatics.get_time_seconds(world)<5: return
        player.set_actor_location(boss.get_actor_location()+unreal.Vector(-420,0,0),False,True)
        player.character_movement.stop_movement_immediately()
        boss.set_actor_rotation(unreal.Rotator(yaw=180),False)
        boss.quantum.initialize_preview(.2)
        boss.quantum.evolve_preview(.2)
        boss.set_editor_property('preview_pre_evolution_tau',.2)
        boss.set_editor_property('preview_tau',.2)
        boss.set_editor_property('preview_armor_bars',0)
        boss.set_editor_property('preview_qte_rounds',1)
        boss.qte.set_editor_property('success_beat_seconds',0.0)
        boss.start_encounter(player)
        phase,began = 'combat',real
        return
    if phase == 'combat':
        if real-began<1.1: return
        assert abs(sunlight(world)-4.0)<0.001, 'Combat fade failed'
        for _ in range(boss.base_health_hits - boss.evolution_trigger_health): boss.apply_damage(1.0,None,boss.get_actor_location(),unreal.Vector())
        phase = 'evolving'
        return
    if phase == 'evolving':
        if 'COMBAT' not in str(boss.get_boss_phase()): return
        if abs(sunlight(world)-1.0) >= .001: return
        capture(world,'after_evolution')
        for _ in range(boss.base_health_hits): boss.apply_damage(1.0,None,boss.get_actor_location(),unreal.Vector())
        phase = 'qte'
        return
    if phase == 'qte':
        if boss.qte.is_qte_active() and not boss.qte.is_finishing_beat():
            boss.submit_qte_input(boss.qte.get_expected_input())
        if not boss.qte.is_qte_active():
            print('PICKUP_QTE_FINISHED',str(boss.get_boss_phase()))
            phase = 'reward'
        return
    if phase == 'reward':
        acquisition = str(player.get_buster_cannon().get_editor_property('acquisition_phase'))
        if 'NONE' not in acquisition:
            print('PICKUP_COLLECTION_VERIFIED',acquisition,sunlight(world))
            phase,began = 'pickup',real
            return
        pickup = unreal.GameplayStatics.get_actor_of_class(world,unreal.QuasicomboVictoryPickup)
        if not pickup: return
        player.set_actor_location(pickup.get_actor_location(),False,True)
        player.character_movement.stop_movement_immediately()
        if 'LIFT' in str(player.get_buster_cannon().get_editor_property('acquisition_phase')):
            phase,began = 'pickup',real
        return
    if phase == 'pickup':
        assert abs(sunlight(world)-1.0)<0.001, 'Sunlight brightened during pickup'
        stage = str(player.get_buster_cannon().get_editor_property('acquisition_phase')).split('.')[-1].split(':')[0]
        if stage not in seen:
            seen.add(stage)
            began = real
            print('PICKUP_STAGE_VERIFIED',stage,sunlight(world))
        if stage not in {item['stage'] for item in records} and real-began>0.2:
            capture(world,stage)
        if stage == 'COMPLETE' and real-began>0.5:
            capture(world,'victory_screen')
            levels.editor_request_end_play()
            phase = 'exit'

def tick(delta):
    try: tick_impl(delta)
    except Exception:
        import traceback
        unreal.log_error(traceback.format_exc())
        levels.editor_request_end_play()
        finish(False)

levels.load_level('/Game/Maps/Lvl_Main')
authored_intensity = sunlight(editor.get_editor_world())
callback = unreal.register_slate_post_tick_callback(tick)
