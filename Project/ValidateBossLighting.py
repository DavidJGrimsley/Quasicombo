"""Run from the editor Python console, then start rendered PIE through Unreal MCP. No assets are saved."""
import json
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
volume_class = unreal.EditorAssetLibrary.load_blueprint_class('/Game/Quasicombo/Boss/BP_BossApproachVolume')
volume_defaults = unreal.get_default_object(volume_class)
out = Path(unreal.Paths.project_saved_dir())/'Section3Validation'/'Lighting'
out.mkdir(parents=True, exist_ok=True)
stage, began = 'setup', time.monotonic()
deadline = began+100
world = mode = player = boss = volume = music = sun = start = end = None
records, handle = [], None

def record(label, expected_sun, expected_track=None, expected_volume=None):
    assert not any(w.get_name() == 'MusicDebugText' for w in unreal.ObjectIterator(unreal.TextBlock)), 'Removed music clock is still present'
    assert not boss.get_evolution_cue(), 'Evolution diagnostic cue is still visible'
    light = sun.get_editor_property('intensity')
    assert abs(light-expected_sun) < .01, (label, light, expected_sun)
    sound = music.get_editor_property('current_music')
    track = sound.get_name() if sound else None
    gain = None
    if expected_track:
        assert track == expected_track, (label, track)
        audio = next(a for a in mode.get_components_by_class(unreal.AudioComponent) if a.get_editor_property('sound') == sound)
        gain = audio.get_editor_property('volume_multiplier')
        assert abs(gain-expected_volume) < .001, (label, gain, expected_volume)
    records.append(dict(check=label, sunlight=light, track=track, gain=gain))
    print('LIGHTING_MIX_CHECK', records[-1])

def next_stage(value):
    global stage, began
    stage, began = value, time.monotonic()

def move(location):
    player.set_actor_location(unreal.Vector(*location), False, True)

def capture(label):
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    unreal.SystemLibrary.execute_console_command(world, 'Shot showui filename="%s" -nosuffix' % (out/(label+'.png')), controller)

def finish(success, error=None):
    (out/'report.json').write_text(json.dumps(dict(success=success, error=error, checks=records), indent=2))
    unreal.unregister_slate_post_tick_callback(handle)
    print('LIGHTING_MIX_RESULT', success, len(records))
    levels.editor_request_end_play()

def tick(_):
    global world, mode, player, boss, volume, music, sun, start, end
    try:
        assert time.monotonic() < deadline, 'Timeout at '+stage
        age = time.monotonic()-began
        if stage == 'setup':
            world = editor.get_game_world()
            if not world: return
            player = unreal.GameplayStatics.get_player_pawn(world, 0)
            boss = unreal.GameplayStatics.get_actor_of_class(world, unreal.QuasicomboBoss)
            mode = unreal.GameplayStatics.get_game_mode(world)
            if not player or not boss or not mode: return
            player.set_editor_property('can_be_damaged', False)
            player.character_movement.disable_movement()
            boss.set_actor_tick_enabled(False)
            boss.character_movement.disable_movement()
            volume = unreal.GameplayStatics.get_actor_of_class(world, unreal.QuasicomboBossApproachVolume)
            assert not hasattr(boss, 'select_preview_level'), 'Evolution preview control is still exposed'
            music = mode.get_editor_property('music_controller')
            sun = unreal.GameplayStatics.get_actor_of_class(world, unreal.DirectionalLight).get_component_by_class(unreal.DirectionalLightComponent)
            if volume:
                start = volume.get_actor_transform().transform_location(volume.start_point)
                end = volume.get_actor_transform().transform_location(volume.end_point)
                assert volume.get_editor_property('end_intensity') == 4
            else:
                assert volume_defaults.end_intensity == 4
            boss.quantum.set_editor_property('use_live_quantum_api', False)
            assert boss.get_editor_property('encounter_lighting').get_editor_property('combat_intensity') == 4
            assert music.get_editor_property('evolution_volume_multiplier') == .5
            next_stage('wait')
        elif stage == 'wait' and age > 4:
            if volume:
                player.set_actor_location(start, False, True); next_stage('start')
            else:
                player.set_actor_location(boss.get_actor_location()+unreal.Vector(-420,0,0),False,True)
                boss.start_encounter(player); next_stage('combat')
        elif stage == 'start' and age > .6:
            record('approach_start',20)
            player.set_actor_location((start+end)*.5, False, True); next_stage('mid')
        elif stage == 'mid' and age > .6:
            record('approach_midpoint',12)
            player.set_actor_location(end, False, True); next_stage('end')
        elif stage == 'end' and age > .6:
            record('approach_end',4)
            player.set_actor_location(boss.get_actor_location()+unreal.Vector(-420,0,0),False,True)
            boss.start_encounter(player); next_stage('combat')
        elif stage == 'combat' and age > 1.5:
            record('combat',4,'SW_Boss',.7)
            capture('combat_four')
            next_stage('start_evolution')
        elif stage == 'start_evolution' and age > .6:
            for _ in range(boss.base_health_hits-boss.evolution_trigger_health):
                boss.apply_damage(1,None,boss.get_actor_location(),unreal.Vector())
            next_stage('evolution')
        elif stage == 'evolution' and age > 1.5 and abs(sun.get_editor_property('intensity')-.2) < .01:
            record('evolution_half_volume',.2,'SW_Evolution',.35)
            capture('evolution_half_volume')
            next_stage('finish')
        elif stage == 'finish' and 'COMBAT' in str(boss.get_boss_phase()):
            next_stage('boss_return')
        elif stage == 'boss_return' and age > 1.5:
            record('boss_volume_restored',1,'SW_Boss',.7)
            boss.destroy_actor(); next_stage('removed')
        elif stage == 'removed' and age > 1.25:
            record('encounter_removed_restores_sun',20)
            finish(True)
    except Exception:
        finish(False,traceback.format_exc())
        raise

handle = unreal.register_slate_post_tick_callback(tick)
