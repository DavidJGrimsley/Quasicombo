"""Focused PIE pause lifecycle checks; execute through Unreal MCP's console.

Send BackSpace/Enter through Slate MCP at the requested stages.
The test reads existing preferences and never changes their audio/color values.
"""
import json
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
frontend_class = unreal.load_class(None, '/Game/UI/Menu/BP_QuasicomboFrontend.BP_QuasicomboFrontend_C')
report = Path(unreal.Paths.project_saved_dir()) / 'MenuValidation' / 'pause-tests.json'
checks = []
stage = 'startup'
began = time.monotonic()
callback = None
finished = False
snapshot = None
previous_world = None
volumes = None
game_fov = None
resume_check = None


def publish(success=None, error=None):
    pending = report.with_suffix('.tmp')
    pending.write_text(json.dumps(dict(stage=stage, success=success, error=error, checks=checks), indent=2))
    pending.replace(report)


def record(label, **values):
    checks.append(dict(check=label, **values))
    print('QUASICOMBO_PAUSE_CHECK', label, json.dumps(values))


def advance(name):
    global stage, began
    stage, began = name, time.monotonic()
    publish()


def call(obj, name, *args):
    return obj.call_method(name, args=tuple(args))


def capture(world, pawn, controller, run):
    mesh = pawn.get_editor_property('mesh')
    return dict(clock=run.get_elapsed_seconds(), location=pawn.get_actor_location(),
                health=pawn.get_current_hp(), routes=list(run.get_route_history()),
                camera=controller.get_view_target(),
                mesh_rotation=mesh.get_editor_property('relative_rotation'))


def assert_frozen(world, pawn, controller, run):
    assert unreal.GameplayStatics.is_game_paused(world)
    assert abs(run.get_elapsed_seconds() - snapshot['clock']) < .0001
    assert (pawn.get_actor_location() - snapshot['location']).length() < .001
    assert pawn.get_current_hp() == snapshot['health']
    assert list(run.get_route_history()) == snapshot['routes']
    mesh = pawn.get_editor_property('mesh')
    assert mesh.get_editor_property('relative_rotation') == snapshot['mesh_rotation'], 'Pause changed Mike facing'
    if snapshot.get('attack_montage'):
        animation = mesh.get_anim_instance()
        assert animation.get_current_active_montage() == snapshot['attack_montage'], 'Pause replaced Mike active attack'
        assert animation.montage_is_playing(snapshot['attack_montage']), 'Pause stopped Mike attack'
        assert abs(animation.montage_get_position(snapshot['attack_montage']) - snapshot['attack_position']) < .0001


def assert_menu_camera(world, pawn, controller, director):
    camera = director.get_editor_property('MenuCamera')
    expected = pawn.get_actor_location() + director.get_editor_property('CameraOffset')
    manager = unreal.GameplayStatics.get_player_camera_manager(world, 0)
    assert controller.get_view_target() == camera
    assert (manager.get_camera_location() - expected).length() < 1
    assert abs(manager.get_fov_angle() - director.get_editor_property('CameraFOV')) < .1


def begin_resume_check(world, director, next_stage):
    global resume_check
    assert unreal.GameplayStatics.is_game_paused(world)
    assert director.get_editor_property('PauseResuming')
    assert director.get_editor_property('MenuShell').is_in_viewport()
    assert not director.get_editor_property('MenuShell').get_is_enabled()
    manager = unreal.GameplayStatics.get_player_camera_manager(world, 0)
    resume_check = dict(next_stage=next_stage, start_fov=manager.get_fov_angle(),
                        partial=False, hidden_at=None,
                        last_time=unreal.GameplayStatics.get_real_time_seconds(world), max_step=0.0)
    elapsed = director.get_editor_property('ResumeElapsed')
    for _ in range(3): call(director, 'ResumePauseRun')
    assert director.get_editor_property('ResumeElapsed') == elapsed
    advance('resume_transition_check')


def monitor_resume(world, pawn, controller, run, director):
    fov = unreal.GameplayStatics.get_player_camera_manager(world, 0).get_fov_angle()
    if min(resume_check['start_fov'], game_fov) + .05 < fov < max(resume_check['start_fov'], game_fov) - .05:
        resume_check['partial'] = True
    menu_visible = director.get_editor_property('MenuShell').is_in_viewport()
    now = unreal.GameplayStatics.get_real_time_seconds(world)
    resume_check['max_step'] = max(resume_check['max_step'], now - resume_check['last_time'])
    resume_check['last_time'] = now
    if unreal.GameplayStatics.is_game_paused(world):
        assert_frozen(world, pawn, controller, run)
        if not menu_visible and resume_check['hidden_at'] is None:
            assert abs(fov - game_fov) < .1, 'Menu hid before the camera reached gameplay FOV'
            if abs(resume_check['start_fov'] - game_fov) > 1:
                assert resume_check['partial'], 'Camera cut instead of panning through intermediate views'
            resume_check['hidden_at'] = now
            record('Camera pans through intermediate views before the menu disappears')
        return
    assert not menu_visible
    assert not director.get_editor_property('PauseResuming')
    assert not director.get_editor_property('PauseMenuOpen')
    assert resume_check['hidden_at'] is not None, 'Gameplay resumed in the same frame the menu disappeared'
    delay = now - resume_check['hidden_at']
    assert delay >= .24, 'Gameplay resumed before the quarter-second hold completed'
    assert delay <= .25 + resume_check['max_step'] + .025, 'The quarter-second hold was stretched by game time dilation'
    record('Gameplay stays frozen for at least 0.25 seconds after the menu disappears', delay=delay)
    advance(resume_check['next_stage'])


def finish(success, error=None):
    global finished, stage
    finished = True
    unreal.unregister_slate_post_tick_callback(callback)
    world = editor.get_game_world()
    if world: unreal.GameplayStatics.set_global_time_dilation(world, 1.0)
    if world and unreal.GameplayStatics.is_game_paused(world):
        unreal.GameplayStatics.set_game_paused(world, False)
    stage = 'complete'
    publish(success, error)
    print('QUASICOMBO_PAUSE_COMPLETE', success, error)


def tick_impl(dt):
    global snapshot, previous_world, volumes, game_fov
    assert time.monotonic() - began < 50, 'Timeout at ' + stage
    world = editor.get_game_world()
    if not world: return
    d = unreal.GameplayStatics.get_actor_of_class(world, frontend_class)
    if stage == 'startup':
        if not d or not d.get_editor_property('MenuInitialized'): return
        if d.get_editor_property('IntroOnly'):
            if not d.get_editor_property('MenuStarting'): call(d, 'StorySkip')
            return
        if not d.get_editor_property('MenuShell'): return
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        call(pc, 'ToggleQuasicomboPause')
        assert not unreal.GameplayStatics.is_game_paused(world)
        assert not d.get_editor_property('PauseMenuOpen')
        record('Pause is unavailable before Start')
        l = unreal.QuasicomboLoadoutSubsystem.get_loadout(world)
        volumes = [l.get_user_music_volume(), l.get_user_ambience_volume()]
        call(d, 'StartFrontendRun')
        advance('start_transition')
        return
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    if not pc or not pawn: return
    run = unreal.QuasicomboRunSubsystem.get_run(world)
    age = time.monotonic() - began
    if stage == 'start_transition':
        if not d or not d.get_editor_property('MenuFinished'): return
        assert run.has_run_started()
        assert not unreal.EditorAssetLibrary.does_asset_exist('/Game/UI/Menu/WBP_QuasicomboPauseButton')
        assert not pc.get_editor_property('bShowMouseCursor')
        game_fov = unreal.GameplayStatics.get_player_camera_manager(world, 0).get_fov_angle()
        record('Gameplay has no Pause button and keeps the cursor hidden')
        snapshot = capture(world, pawn, pc, run)
        advance('ready_keyboard_pause')
    elif stage == 'ready_keyboard_pause':
        if not d.get_editor_property('PauseMenuOpen'): return
        assert d.get_editor_property('MenuScreen') == 5
        assert d.get_editor_property('MenuShell').is_in_viewport()
        assert pawn.get_editor_property('mesh').get_editor_property('relative_rotation') == snapshot['mesh_rotation']
        snapshot = capture(world, pawn, pc, run)
        snapshot['camera'] = d.get_editor_property('GameplayViewTarget')
        record('Backspace opens the pause page and freezes the world')
        advance('frozen_world')
    elif stage == 'frozen_world':
        if age < d.get_editor_property('CameraBlendSeconds') + .4: return
        assert_frozen(world, pawn, pc, run)
        assert_menu_camera(world, pawn, pc, d)
        record('Pause freezes gameplay and blends to the Home camera offset and FOV')
        call(d, 'SetFrontendScreen', 2)
        assert d.get_editor_property('MenuScreen') == 2
        call(d, 'FrontendBack')
        assert d.get_editor_property('MenuScreen') == 5
        assert_frozen(world, pawn, pc, run)
        assert_menu_camera(world, pawn, pc, d)
        record('Customization Back returns to the paused menu')
        call(d, 'SetFrontendScreen', 3)
        settings = d.get_editor_property('MenuShell').get_editor_property('Settings')
        assert abs(settings.get_editor_property('Music').get_value() - volumes[0]) < .0001
        assert abs(settings.get_editor_property('Ambience').get_value() - volumes[1]) < .0001
        record('Pause Settings reflects existing audio preferences', music=volumes[0], ambience=volumes[1])
        call(d, 'SetFrontendScreen', 4)
        call(d, 'FrontendBack')
        assert d.get_editor_property('MenuScreen') == 3
        record('Game info Back returns to Settings')
        call(d.get_editor_property('MenuShell'), 'FocusActivePage')
        advance('ready_replay_click')
    elif stage == 'ready_replay_click':
        if d.get_editor_property('MenuScreen') != 1: return
        call(d, 'StoryNext')
        assert d.get_editor_property('StoryPage') == 1
        call(d, 'StorySkip')
        assert d.get_editor_property('MenuScreen') == 3
        assert_frozen(world, pawn, pc, run)
        record('Replay story remains navigable while paused and returns to Settings')
        call(d.get_editor_property('MenuShell'), 'FocusActivePage')
        advance('ready_keyboard_resume')
    elif stage == 'ready_keyboard_resume':
        if not d.get_editor_property('PauseResuming'): return
        record('Backspace starts the staged Resume transition from a submenu')
        begin_resume_check(world, d, 'resumed_world')
    elif stage == 'resume_transition_check':
        monitor_resume(world, pawn, pc, run, d)
    elif stage == 'resumed_world':
        if age < d.get_editor_property('CameraBlendSeconds') + .4: return
        assert run.get_elapsed_seconds() > snapshot['clock']
        assert pc.get_view_target() == snapshot['camera']
        assert pawn.get_editor_property('mesh').get_editor_property('relative_rotation') == snapshot['mesh_rotation']
        assert not pc.get_editor_property('bShowMouseCursor')
        l = unreal.QuasicomboLoadoutSubsystem.get_loadout(world)
        assert [l.get_user_music_volume(), l.get_user_ambience_volume()] == volumes
        record('Resume restores the gameplay camera and cursor while retaining Mike facing and audio settings')
        pawn.do_charged_attack_start()
        animation = pawn.get_editor_property('mesh').get_anim_instance()
        attack = animation.get_current_active_montage()
        assert attack and animation.montage_is_playing(attack), 'Mike charged attack did not start'
        snapshot = capture(world, pawn, pc, run)
        snapshot['attack_montage'] = attack
        snapshot['attack_position'] = animation.montage_get_position(attack)
        call(pc, 'ToggleQuasicomboPause')
        assert d.get_editor_property('PauseMenuOpen')
        assert_frozen(world, pawn, pc, run)
        record('Pause preserves Mike active charged attack and facing', montage=attack.get_name())
        advance('ready_resume_click')
    elif stage == 'ready_resume_click':
        if not d.get_editor_property('PauseResuming'): return
        begin_resume_check(world, d, 'button_resumed')
    elif stage == 'button_resumed':
        animation = pawn.get_editor_property('mesh').get_anim_instance()
        attack = snapshot['attack_montage']
        assert animation.get_current_active_montage() == attack
        assert animation.montage_is_playing(attack)
        assert animation.montage_get_position(attack) > snapshot['attack_position']
        record('Mike continues the same charged attack after Resume')
        pawn.do_charged_attack_end()
        record('Resume button and repeated Resume requests complete one transition safely')
        previous_world = world
        unreal.GameplayStatics.open_level(world, 'Lvl_Main')
        advance('retry_world')
    elif stage == 'retry_world':
        if world == previous_world or not run.has_run_started(): return
        assert not d, 'A retry should bypass the initial frontend'
        game_fov = unreal.GameplayStatics.get_player_camera_manager(world, 0).get_fov_angle()
        call(pc, 'ToggleQuasicomboPause')
        d = unreal.GameplayStatics.get_actor_of_class(world, frontend_class)
        assert d and 'BP_QuasicomboPauseSession' in d.get_class().get_name()
        assert d.get_editor_property('PauseMenuOpen')
        assert d.get_editor_property('MenuScreen') == 5
        snapshot = capture(world, pawn, pc, run)
        snapshot['camera'] = d.get_editor_property('GameplayViewTarget')
        record('Pause works in a fresh retry world without restarting the frontend')
        advance('retry_frozen')
    elif stage == 'retry_frozen':
        if age < d.get_editor_property('CameraBlendSeconds') + .4: return
        assert_frozen(world, pawn, pc, run)
        assert_menu_camera(world, pawn, pc, d)
        unreal.GameplayStatics.set_global_time_dilation(world, .2)
        call(d, 'ResumePauseRun')
        begin_resume_check(world, d, 'retry_resumed')
    elif stage == 'retry_resumed':
        if age < d.get_editor_property('CameraBlendSeconds') + .4: return
        assert pc.get_view_target() == snapshot['camera']
        assert not pc.get_editor_property('bShowMouseCursor')
        l = unreal.QuasicomboLoadoutSubsystem.get_loadout(world)
        assert [l.get_user_music_volume(), l.get_user_ambience_volume()] == volumes
        assert resume_check['hidden_at'] is not None
        record('The final 0.25-second hold uses real time during slow motion')
        record('Retry pause freezes the world and resumes with the same preferences')
        finish(True)


def tick(dt):
    if finished: return
    try: tick_impl(dt)
    except Exception: finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
publish()
