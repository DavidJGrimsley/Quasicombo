"""Run from the editor Python console, then start a fresh rendered PIE session.

Checks route seeks using audio-device playheads, returning routes, a loop boundary,
pause/time dilation, stair silence, and the absence of the removed debug overlay. Saves no assets.
"""
import json
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
out = Path(unreal.Paths.project_saved_dir())/'BossApproachValidation'/'RouteTimestamp'/'Rendered'
out.mkdir(parents=True, exist_ok=True)
world = mode = music = run = player = boss = None
stage, began, position, seek, starts = 'setup', time.monotonic(), 0.0, 0.0, 0
records = []
handle = None
deadline = time.monotonic() + 90


def current_audio():
    sound = music.get_editor_property('current_music')
    return next(a for a in mode.get_components_by_class(unreal.AudioComponent) if a.get_editor_property('sound') == sound)


def record(label, **values):
    sound = music.get_editor_property('current_music')
    widgets = [o for o in unreal.ObjectIterator(unreal.TextBlock) if o.get_name() == 'MusicDebugText']
    assert not widgets, 'Removed music debug overlay is still present'
    records.append(dict(check=label, track=sound.get_name(), timestamp=music.get_playback_seconds(),
        last_start=music.get_editor_property('last_track_start_seconds'), **values))
    print('ROUTE_TIMESTAMP_CHECK', label, records[-1])


def capture(label):
    # SHOWUI includes UMG; HighResShot captures only the 3D viewport.
    unreal.SystemLibrary.execute_console_command(world, 'Shot showui filename="%s" -nosuffix' % (out/(label+'.png')), unreal.GameplayStatics.get_player_controller(world, 0))


def next_stage(name):
    global stage, began
    stage, began = name, time.monotonic()


def commit(section, lane):
    global seek
    seek = music.get_playback_seconds()
    assert run.try_commit_route(section, lane), 'Route commitment rejected'
    actual = music.get_editor_property('last_track_start_seconds')
    assert abs(actual-seek) < .05, (seek, actual)
    assert abs(music.get_playback_seconds()-seek) < .05, 'Incoming song reset'
    record('seek_'+str(lane), outgoing_timestamp=seek)


def finish(success, error=None):
    unreal.GameplayStatics.set_game_paused(world, False)
    unreal.GameplayStatics.set_global_time_dilation(world, 1.0)
    (out/'report.json').write_text(json.dumps(dict(success=success, error=error, checks=records), indent=2))
    unreal.unregister_slate_post_tick_callback(handle)
    print('ROUTE_TIMESTAMP_RESULT', success, len(records))
    levels.editor_request_end_play()


def tick(_):
    global position, starts, world, mode, music, run, player, boss
    try:
        assert time.monotonic() < deadline, 'Timeout at '+stage
        age = time.monotonic()-began
        if stage == 'setup':
            world = editor.get_game_world()
            if not world:
                return
            mode = unreal.GameplayStatics.get_game_mode(world)
            player = unreal.GameplayStatics.get_player_pawn(world, 0)
            boss = unreal.GameplayStatics.get_actor_of_class(world, unreal.QuasicomboBoss)
            if not mode or not player or not boss:
                return
            music = mode.get_editor_property('music_controller')
            run = next(obj for obj in unreal.ObjectIterator(unreal.QuasicomboRunSubsystem) if obj.get_outer() == world)
            player.set_editor_property('can_be_damaged', False)
            player.character_movement.disable_movement()
            boss.set_actor_tick_enabled(False)
            boss.character_movement.disable_movement()
            next_stage('wait')
        elif stage == 'wait' and music.get_playback_seconds() > 5:
            record('default_audio_position')
            commit(1, unreal.QuasicomboLane.A)
            next_stage('a_fade')
        elif stage == 'a_fade' and age > .4:
            assert len([a for a in mode.get_components_by_class(unreal.AudioComponent) if a.is_playing() and a.get_editor_property('volume_multiplier') > 0]) >= 2
            record('a_crossfade')
            capture('a_crossfade')
            next_stage('a')
        elif stage == 'a' and age > 1.75:
            assert music.get_playback_seconds() > seek+1, 'Audio seek did not take effect'
            record('route_a_audio_position')
            commit(2, unreal.QuasicomboLane.B)
            next_stage('b')
        elif stage == 'b' and age > 1.75:
            assert music.get_playback_seconds() > seek+1
            record('return_to_b_audio_position')
            capture('return_to_b')
            next_stage('near_loop')
        elif stage == 'near_loop' and age > 1.75:
            audio = current_audio()
            audio.play(audio.get_editor_property('sound').duration-.65)
            next_stage('loop')
        elif stage == 'loop' and age > 1.4:
            assert music.get_playback_seconds() < 4, 'Loop playhead did not wrap'
            record('natural_loop_boundary')
            commit(3, unreal.QuasicomboLane.C)
            next_stage('c')
        elif stage == 'c' and age > 1.75:
            assert music.get_playback_seconds() > seek+1
            record('route_c_audio_position')
            capture('route_c')
            next_stage('dilation_setup')
        elif stage == 'dilation_setup' and age > 1.75:
            unreal.GameplayStatics.set_global_time_dilation(world, .4)
            position = music.get_playback_seconds()
            next_stage('dilation')
        elif stage == 'dilation' and age > 2.0:
            advance = music.get_playback_seconds()-position
            assert abs(advance-age) < .3, (advance, age)
            assert current_audio().get_editor_property('pitch_multiplier') == 1.0
            record('undilated_audio_time', real_seconds=age, playback_advance=advance)
            unreal.GameplayStatics.set_game_paused(world, True)
            next_stage('pause_settle')
        elif stage == 'pause_settle' and age > .3:
            position = music.get_playback_seconds()
            next_stage('pause')
        elif stage == 'pause' and age > 1.25:
            assert abs(music.get_playback_seconds()-position) < .15, 'Timestamp advanced while audio paused'
            record('paused_audio_position')
            unreal.GameplayStatics.set_game_paused(world, False)
            music.set_approach_progress(1.0)
            position, starts = music.get_playback_seconds(), music.get_editor_property('track_starts')
            next_stage('silence')
        elif stage == 'silence' and age > 1.75:
            advance = music.get_playback_seconds()-position
            record('silent_audio_position', real_seconds=age, playback_advance=advance)
            # Unpause and audio callbacks arrive on different frames. Check continued source
            # playback here; the dilation stage separately verifies its real-time rate.
            assert advance > .75, 'Muted route source stopped advancing'
            assert music.get_editor_property('track_starts') == starts, 'Silence restarted the source'
            music.set_approach_progress(0.0)
            record('silent_route_keeps_time_and_backtracks')
            assert current_audio().get_editor_property('volume_multiplier') > 0
            next_stage('final')
        elif stage == 'final' and age > .3:
            record('debug_overlay_removed')
            capture('clean_gameplay_hud')
            next_stage('exit')
        elif stage == 'exit' and age > 1.75:
            finish(True)
    except Exception:
        finish(False, traceback.format_exc())
        raise


handle = unreal.register_slate_post_tick_callback(tick)
