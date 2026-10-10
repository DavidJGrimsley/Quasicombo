"""Rendered PIE regression for approach lighting, music, retries and pickup.

Import this module in the editor Python console, then start PIE via MCP.
After the first run ends, start PIE once more for the interruption scenario.
Changes are confined to PIE; no assets save.
Reports and viewport captures go to Saved/BossApproachValidation/Rendered.
"""
import json
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
out = Path(unreal.Paths.project_saved_dir()) / 'BossApproachValidation' / 'Rendered'
out.mkdir(parents=True, exist_ok=True)
stage, began, scenario = 'setup', 0.0, 'complete'
deadline = time.monotonic() + 260
records, pickup_seen = [], set()
callback = None
starts = 0


def sunlight(world):
    sun = unreal.GameplayStatics.get_actor_of_class(world, unreal.DirectionalLight)
    return sun.get_component_by_class(unreal.DirectionalLightComponent).get_editor_property('intensity')


authored_intensity = sunlight(editor.get_editor_world())


def music_name(music):
    sound = music.get_editor_property('current_music')
    return sound.get_name() if sound else None


def voices(music):
    return [a for a in music.get_owner().get_components_by_class(unreal.AudioComponent)
            if a.get_editor_property('sound')]


def record(world, label, capture=True):
    music = unreal.GameplayStatics.get_game_mode(world).get_editor_property('music_controller')
    items = [dict(sound=a.get_editor_property('sound').get_name(),
                  volume=a.get_editor_property('volume_multiplier'),
                  playing=a.is_playing(), pitch=a.get_editor_property('pitch_multiplier'))
             for a in voices(music)]
    boss = unreal.GameplayStatics.get_actor_of_class(world, unreal.QuasicomboBoss)
    volume = unreal.GameplayStatics.get_actor_of_class(world, unreal.QuasicomboBossApproachVolume)
    records.append(dict(stage=label, sunlight=sunlight(world), track=music_name(music),
                        gain=music.get_editor_property('approach_gain'), voices=items,
                        boss_phase=str(boss.get_boss_phase()), progress=volume.get_editor_property('progress'),
                        player_inside=volume.get_editor_property('player_inside'),
                        encounter_owns_effects=volume.get_editor_property('encounter_owns_effects')))
    if capture:
        unreal.SystemLibrary.execute_console_command(world,
            'Shot showui filename="%s" -nosuffix' % (out/(label+'.png')), unreal.GameplayStatics.get_player_controller(world, 0))
    print('BQ_RENDERED_CHECK', label, sunlight(world), music_name(music))


def next_stage(name):
    global stage, began
    stage, began = name, time.monotonic()


def move(player, position):
    player.character_movement.stop_movement_immediately()
    player.set_actor_location(position, False, True)


def damage(boss, count):
    for _ in range(count):
        boss.apply_damage(1.0, None, boss.get_actor_location(), unreal.Vector())


def finish(success, error=None):
    (out/'report.json').write_text(json.dumps(dict(success=success, error=error,
        authored_sunlight=authored_intensity, checks=records), indent=2))
    unreal.unregister_slate_post_tick_callback(callback)
    print('BQ_RENDERED_VALIDATION_COMPLETE', success, len(records))


def tick_impl(_delta):
    global scenario, starts
    real = time.monotonic()
    assert real < deadline, 'Validation timed out at '+stage
    world = editor.get_game_world()
    if not world:
        assert sunlight(editor.get_editor_world()) == authored_intensity, 'Authored light changed'
        if stage == 'exit_complete':
            scenario = 'interrupted'
            next_stage('setup')
        elif stage == 'exit_final':
            finish(True)
        elif stage == 'setup':
            next_stage('waiting')
        return
    boss = unreal.GameplayStatics.get_actor_of_class(world, unreal.QuasicomboBoss)
    player = unreal.GameplayStatics.get_player_pawn(world, 0)
    volume = unreal.GameplayStatics.get_actor_of_class(world, unreal.QuasicomboBossApproachVolume)
    if not boss or not player or not volume:
        return
    mode = unreal.GameplayStatics.get_game_mode(world)
    music = mode.get_editor_property('music_controller')
    run = next(obj for obj in unreal.ObjectIterator(unreal.QuasicomboRunSubsystem) if obj.get_outer() == world)
    age = real-began
    if stage == 'waiting':
        if unreal.GameplayStatics.get_time_seconds(world) < 5:
            return
        assert music_name(music) == 'Quantum_Ambience', 'Default piano missing'
        player.character_movement.disable_movement()
        player.set_editor_property('can_be_damaged', False)
        boss.set_actor_tick_enabled(False)
        boss.character_movement.disable_movement()
        boss.quantum.initialize_preview(.2)
        boss.quantum.evolve_preview(.2)
        boss.set_editor_property('preview_pre_evolution_tau',.2)
        boss.set_editor_property('preview_tau',.2)
        boss.set_editor_property('preview_armor_bars',0)
        boss.set_editor_property('preview_qte_rounds', 1)
        boss.qte.set_editor_property('success_beat_seconds', 0.0)
        assert len(voices(music)) == 1
        assert all(not a.is_playing() for a in boss.get_components_by_class(unreal.AudioComponent))
        if scenario == 'interrupted':
            move(player, boss.get_actor_location()+unreal.Vector(-420, 0, 0))
            boss.set_actor_tick_enabled(True)
            boss.start_encounter(player)
            next_stage('interrupt_combat')
            return
        record(world, 'default_piano')
        assert run.try_commit_route(1, unreal.QuasicomboLane.A)
        next_stage('route_a')
    elif stage == 'route_a' and age > 1.25:
        if len(voices(music)) > 1:
            assert age < 8, 'Route A crossfade did not finish'
            return
        assert music_name(music) == 'SW_RouteA' and len(voices(music)) == 1
        record(world, 'route_a')
        assert run.try_commit_route(2, unreal.QuasicomboLane.B)
        next_stage('route_b')
    elif stage == 'route_b' and age > 1.25:
        if len(voices(music)) > 1:
            assert age < 8, 'Route B crossfade did not finish'
            return
        assert music_name(music) == 'Quantum_Ambience'
        record(world, 'route_b')
        assert run.try_commit_route(3, unreal.QuasicomboLane.C)
        next_stage('route_c')
    elif stage == 'route_c' and age > 1.25:
        if len(voices(music)) > 1:
            assert age < 8, 'Route C crossfade did not finish'
            return
        assert music_name(music) == 'SW_RouteC' and len(voices(music)) == 1
        starts = music.get_editor_property('track_starts')
        assert not run.try_commit_route(3, unreal.QuasicomboLane.A)
        assert starts == music.get_editor_property('track_starts'), 'Rejected route restarted music'
        for name in ('SW_RouteA', 'SW_RouteC', 'SW_Boss', 'SW_Evolution'):
            assert unreal.load_asset('/Game/Audio/Music/'+name).get_editor_property('looping')
        record(world, 'route_c')
        next_stage('loop')
    elif stage == 'loop' and age > 82:
        assert music_name(music) == 'SW_RouteC' and voices(music)[0].is_playing()
        assert music.get_editor_property('track_starts') == starts, 'Loop restarted through controller'
        record(world, 'natural_loop')
        move(player, unreal.Vector(25500, 1000, -750))
        next_stage('approach_start')
    elif stage == 'approach_start' and age > 1.75:
        assert abs(sunlight(world)-20) < .01 and abs(music.get_editor_property('approach_gain')-1) < .01
        record(world, stage)
        move(player, unreal.Vector(27000, 1000, 225))
        next_stage('approach_midpoint')
    elif stage == 'approach_midpoint' and age > 1.75:
        record(world, 'midpoint_state', False)
        assert abs(sunlight(world)-12) < .01 and abs(music.get_editor_property('approach_gain')-.5) < .01
        record(world, stage)
        move(player, unreal.Vector(26250, 1000, -262.5))
        next_stage('approach_backtracking')
    elif stage == 'approach_backtracking' and age > 1.75:
        assert abs(sunlight(world)-16) < .01 and abs(music.get_editor_property('approach_gain')-.75) < .01
        record(world, stage)
        move(player, unreal.Vector(26250, 1800, -262.5))
        next_stage('approach_side_exit')
    elif stage == 'approach_side_exit' and age > 1.75:
        assert abs(sunlight(world)-16) < .01
        record(world, stage)
        move(player, unreal.Vector(25000, 1000, -1075))
        # Re-enter before testing the bottom exit; a second outside location does not re-evaluate.
        move(player, unreal.Vector(25500, 1000, -750))
        next_stage('bottom_entry')
    elif stage == 'bottom_entry' and age > 1.75:
        move(player, unreal.Vector(25000, 1000, -1075))
        next_stage('approach_bottom_exit')
    elif stage == 'approach_bottom_exit' and age > 1.75:
        assert abs(sunlight(world)-20) < .01 and abs(music.get_editor_property('approach_gain')-1) < .01
        record(world, stage)
        move(player, unreal.Vector(28500, 1000, 1200))
        next_stage('approach_top')
    elif stage == 'approach_top' and age > 1.75:
        assert abs(sunlight(world)-4) < .01 and music.get_editor_property('approach_gain') < .001
        assert all(a.get_editor_property('volume_multiplier') < .001 for a in voices(music))
        record(world, stage)
        move(player, boss.get_actor_location()+unreal.Vector(-420, 0, 0))
        boss.set_actor_tick_enabled(True)
        boss.start_encounter(player)
        next_stage('combat')
    elif stage in ('combat', 'interrupt_combat') and age > 1.75:
        assert music_name(music) == 'SW_Boss' and len(voices(music)) == 1
        assert abs(sunlight(world)-4) < .01
        record(world, scenario+'_combat')
        damage(boss, boss.base_health_hits-boss.evolution_trigger_health)
        starts = music.get_editor_property('track_starts')
        assert music_name(music) == 'SW_Evolution'
        next_stage('evolution')
    elif stage == 'evolution':
        if 'EVOLUTION' in str(boss.get_boss_phase()):
            assert music_name(music) == 'SW_Evolution'
            assert music.get_editor_property('track_starts') == starts
            if age > 1.75 and abs(sunlight(world)-.2) < .01 and len(voices(music)) == 1 and not any(r['stage'] == scenario+'_transformation' for r in records):
                assert abs(sunlight(world)-.2) < .01
                assert len(voices(music)) == 1
                assert all(a.get_editor_property('pitch_multiplier') == 1 for a in voices(music))
                assert all(abs(a.get_editor_property('volume_multiplier')-.35) < .001 for a in voices(music))
                record(world, scenario+'_transformation')
                boss.quantum.on_quantum_state_ready.broadcast(.8, .2, False)
                assert music_name(music) == 'SW_Evolution' and music.get_editor_property('track_starts') == starts
                if scenario == 'interrupted':
                    assert boss.handle_encounter_failure()
                    assert music_name(music) == 'SW_Boss'
                    next_stage('interrupt_retry')
            return
        assert 'COMBAT' in str(boss.get_boss_phase())
        assert music_name(music) == 'SW_Boss'
        next_stage('evolution_finished')
    elif stage == 'evolution_finished' and age > 1.25:
        assert abs(sunlight(world)-1) < .01 and len(voices(music)) == 1
        record(world, 'transformation_finished')
        damage(boss, boss.base_health_hits)
        next_stage('qte_retry')
    elif stage == 'qte_retry' and age > .4:
        assert music_name(music) == 'SW_Boss' and abs(sunlight(world)-1) < .01
        record(world, 'qte')
        assert boss.handle_encounter_failure()
        assert music_name(music) == 'SW_Boss'
        record(world, 'retry_announcement')
        next_stage('retry')
    elif stage in ('retry', 'interrupt_retry') and 'COMBAT' in str(boss.get_boss_phase()) and age > 1.5:
        assert abs(sunlight(world)-1) < .01 and music_name(music) == 'SW_Boss'
        record(world, scenario+'_retry_combat')
        if scenario == 'interrupted':
            assert boss.handle_encounter_failure()
            assert abs(sunlight(world)-20) < .01
            next_stage('defeat')
        else:
            damage(boss, boss.base_health_hits+boss.get_armor_bars()*boss.hits_per_armor_bar)
            next_stage('qte')
    elif stage == 'qte':
        assert music_name(music) in ('SW_Boss', None)
        if boss.qte.is_qte_active() and not boss.qte.is_finishing_beat():
            boss.submit_qte_input(boss.qte.get_expected_input())
        if not boss.qte.is_qte_active():
            next_stage('reward')
    elif stage == 'reward':
        pickup = unreal.GameplayStatics.get_actor_of_class(world, unreal.QuasicomboVictoryPickup)
        if pickup:
            move(player, pickup.get_actor_location())
        if 'NONE' not in str(player.get_buster_cannon().get_editor_property('acquisition_phase')):
            next_stage('pickup')
    elif stage == 'pickup':
        assert abs(sunlight(world)-1) < .01 and music_name(music) is None
        acquisition = str(player.get_buster_cannon().get_editor_property('acquisition_phase')).split('.')[-1].split(':')[0]
        if acquisition not in pickup_seen:
            pickup_seen.add(acquisition)
            next_stage('pickup')
        elif age > .2 and not any(r['stage'] == 'pickup_'+acquisition for r in records):
            record(world, 'pickup_'+acquisition)
        if acquisition == 'COMPLETE' and age > .5:
            assert len(voices(music)) == 0
            record(world, 'victory_screen')
            levels.editor_request_end_play()
            next_stage('exit_complete')
    elif stage == 'defeat' and age > 1.75:
        assert len(voices(music)) == 0 and abs(sunlight(world)-20) < .01
        record(world, 'defeat_restores_baseline')
        levels.editor_request_end_play()
        next_stage('exit_final')


def tick(delta):
    try:
        tick_impl(delta)
    except Exception:
        error = traceback.format_exc()
        unreal.log_error(error)
        levels.editor_request_end_play()
        finish(False, error)


callback = unreal.register_slate_post_tick_callback(tick)
