"""Read-only PIE state report, executed in the editor through Unreal MCP."""
import json
from pathlib import Path
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if world:
    mode = unreal.GameplayStatics.get_game_mode(world)
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    run = next(o for o in unreal.ObjectIterator(unreal.QuasicomboRunSubsystem) if o.get_outer() == world)
    loadout = unreal.QuasicomboLoadoutSubsystem.get_loadout(world)
    cls = unreal.load_class(None, '/Game/UI/Menu/BP_QuasicomboFrontend.BP_QuasicomboFrontend_C')
    director = unreal.GameplayStatics.get_actor_of_class(world, cls)
    music = mode.get_editor_property('music_controller')
    report = dict(world=world.get_path_name(), elapsed=run.get_elapsed_seconds(),
                  started=run.has_run_started(), damage=run.get_damage_taken(),
                  routes=[str(x) for x in run.get_route_history()],
                  session_started=loadout.has_started_session(),
                  music=str(music.get_editor_property('current_music')),
                  track_starts=music.get_editor_property('track_starts'),
                  hp=pawn.get_current_hp(), cannon=loadout.has_replay_cannon(),
                  body=str(loadout.get_mike_body_region_color(unreal.MikeBodyRegion.COLOR_G)), lights=str(loadout.get_mike_light_color()),
                  music_volume=loadout.get_user_music_volume(), ambience_volume=loadout.get_user_ambience_volume())
    report['frontend_class'] = str(mode.get_editor_property('frontend_director_class'))
    report['game_mode_class'] = mode.get_class().get_path_name()
    report['should_show_frontend'] = mode.should_show_frontend()
    if director:
        report['director'] = director.get_path_name()
        for name in ['MenuScreen', 'StoryPage', 'MenuInitialized', 'MenuStarting', 'MenuFinished', 'WavePhase', 'WaveWait', 'TransitionTime']:
            report[name] = director.get_editor_property(name)
        shell = director.get_editor_property('MenuShell')
        report['shell_visible'] = bool(shell and shell.is_in_viewport())
    else:
        report['director'] = None
    path = Path(unreal.Paths.project_saved_dir())/'MenuValidation/runtime-state.json'
    path.write_text(json.dumps(report, indent=2))
    print('QUASICOMBO_MENU_STATE', json.dumps(report))
