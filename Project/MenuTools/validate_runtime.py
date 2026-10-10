"""PIE regression for frontend timing, settings and session replay.

Execute through the editor console using Unreal MCP with a fresh PIE running.
Only PIE objects and the menu preferences SaveGame are changed.
"""
import json
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
out=Path(unreal.Paths.project_saved_dir())/'MenuValidation'
cls=unreal.load_class(None,'/Game/UI/Menu/BP_QuasicomboFrontend.BP_QuasicomboFrontend_C')
pcls=unreal.load_class(None,'/Game/UI/Menu/BP_QuasicomboPreferences.BP_QuasicomboPreferences_C')
stage='setup'
began=time.monotonic()
deadline=began+90
checks=[]
callback=None
previous_world=None
expected_body=None
expected_lights=None
first_wave_at=None
wave_phases=set()

def record(label, **values):
    checks.append(dict(check=label,**values))
    print('QUASICOMBO_FRONTEND_CHECK',label,json.dumps(values))

def advance(name):
    global stage,began
    stage,began=name,time.monotonic()

def color(c):
    return [c.r,c.g,c.b,c.a]

def close(a,b):
    return all(abs(x-y)<.0001 for x,y in zip(a,b))

def call(obj,name,*args):
    return obj.call_method(name,args=tuple(args))

def finish(success,error=None):
    (out/'runtime-tests.json').write_text(json.dumps(dict(success=success,error=error,checks=checks),indent=2))
    unreal.unregister_slate_post_tick_callback(callback)
    print('QUASICOMBO_FRONTEND_VALIDATION_COMPLETE',success,len(checks),error)

def tick_impl(_delta):
    global previous_world,expected_body,expected_lights,first_wave_at
    assert time.monotonic()<deadline, 'Timeout at '+stage
    world=editor.get_game_world()
    if not world: return
    mode=unreal.GameplayStatics.get_game_mode(world)
    pawn=unreal.GameplayStatics.get_player_pawn(world,0)
    if not mode: return
    run=next(o for o in unreal.ObjectIterator(unreal.QuasicomboRunSubsystem) if o.get_outer()==world)
    loadout=unreal.QuasicomboLoadoutSubsystem.get_loadout(world)
    director=unreal.GameplayStatics.get_actor_of_class(world,cls)
    if stage=='setup' and unreal.GameplayStatics.get_current_level_name(world)=='Lvl_QuasicomboIntro':
        if not director or not director.get_editor_property('MenuInitialized'): return
        assert not pawn and not run.has_run_started() and not loadout.has_started_session()
        assert not unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SideScrollingCharacter)
        assert director.get_editor_property('MenuShell').get_editor_property('StoryBlack').get_visibility()==unreal.SlateVisibility.VISIBLE
        assert director.get_editor_property('IntroComponent').is_playing()
        assert abs(director.get_editor_property('IntroComponent').get_editor_property('volume_multiplier')-loadout.get_user_music_volume())<.001
        record('Black story launches in an empty world before jungle or gameplay, with saved intro gain')
        call(director,'StorySkip')
        advance('jungle_setup')
        return
    if not pawn: return
    music=mode.get_editor_property('music_controller')
    age=time.monotonic()-began
    if stage in ['setup','jungle_setup']:
        if not director or not director.get_editor_property('MenuInitialized'): return
        assert run.get_elapsed_seconds()==0 and not run.has_run_started()
        assert not music.get_editor_property('current_music')
        assert director.get_editor_property('MenuScreen')==0
        assert not director.get_editor_property('IntroComponent').is_playing()
        saved=unreal.GameplayStatics.load_game_from_slot('QuasicomboPreferences',0)
        if saved:
            assert close(color(saved.get_editor_property('SavedBody')),color(loadout.get_mike_body_region_color(unreal.MikeBodyRegion.COLOR_G)))
            assert close(color(saved.get_editor_property('SavedLights')),color(loadout.get_mike_light_color()))
            assert abs(saved.get_editor_property('SavedMusic')-loadout.get_user_music_volume())<.001
            record('Saved settings load before Home in a fresh session')
        assert not pawn.get_buster_cannon().is_equipped()
        unreal.GameplayStatics.apply_damage(pawn,100,None,None,None)
        assert pawn.get_current_hp()==10 and run.get_damage_taken()==0
        assert not run.try_commit_route(1,unreal.QuasicomboLane.B)
        record('Frontend excludes time, damage, routes and gameplay music',elapsed=0,hp=10,track_starts=0)
        shell=director.get_editor_property('MenuShell')
        story=shell.get_editor_property('Story')
        paragraphs=(Path(unreal.Paths.project_dir())/'Project/Narrative.txt').read_text().rstrip('\n').split('\n\n')
        for i,text in enumerate(paragraphs):
            assert str(story.get_editor_property('NarrativePage'+str(i)))==text
        record('Four narrative pages match source verbatim')
        call(director,'StorySkip')
        call(director,'InitializeMenuAnimation')
        wait=director.get_editor_property('NextWaveAt')
        assert wait==10
        record('Complete wave scheduled every 10 seconds, with blink halfway between',wait=wait)
        advance('wave')
    elif stage=='wave':
        assert run.get_elapsed_seconds()==0 and not run.get_route_history() and run.get_damage_taken()==0
        if director.get_editor_property('WavePhase')!=1: return
        anim=director.get_editor_property('MenuAnimation')
        assert anim.montage_is_playing(None)
        record('Home wave plays a montage while run remains held',elapsed=run.get_elapsed_seconds())
        first_wave_at=unreal.GameplayStatics.get_time_seconds(world)
        wave_phases.add(1)
        advance('wave_cycle')
    elif stage=='wave_cycle':
        assert run.get_elapsed_seconds()==0
        phase=director.get_editor_property('WavePhase')
        if phase==2: wave_phases.add(phase)
        if phase!=1 or unreal.GameplayStatics.get_time_seconds(world)-first_wave_at<7: return
        interval=unreal.GameplayStatics.get_time_seconds(world)-first_wave_at
        assert wave_phases=={1,2} and 9.9<=interval<=10.2, (wave_phases,interval)
        record('Complete wave repeats every 10 seconds with intervening blink',interval=interval)
        montage=director.get_editor_property('MenuActiveMontage')
        call(director,'SetFrontendScreen',2)
        assert director.get_editor_property('WavePhase')==1
        assert director.get_editor_property('MenuActiveMontage')==montage
        record('Customization retains the continuous camera-facing animated preview')
        custom=director.get_editor_property('MenuShell').get_editor_property('Customization')
        for slot,rgb in [('Body',(98,180,250)),('Light',(230,65,190))]:
            for channel,value in zip('RGB',rgb): custom.get_editor_property(slot+channel).set_value(value/255)
            call(custom,'Apply'+('Body' if slot=='Body' else 'Light')+'Sliders')
        expected_body=color(loadout.get_mike_body_region_color(unreal.MikeBodyRegion.COLOR_G))
        expected_lights=color(loadout.get_mike_light_color())
        def linear(v):
            c=v/255
            return c/12.92 if c<=.04045 else ((c+.055)/1.055)**2.4
        assert close(expected_body,[linear(x) for x in (98,180,250)]+[1])
        assert close(expected_lights,[linear(x) for x in (230,65,190)]+[1])
        record('RGB controls consistently convert sRGB to linear colors',body=expected_body,lights=expected_lights)
        call(director,'SetFrontendScreen',3)
        settings=director.get_editor_property('MenuShell').get_editor_property('Settings')
        settings.get_editor_property('Music').set_value(.35)
        settings.get_editor_property('Ambience').set_value(.6)
        call(settings,'ApplyFrontendAudio')
        assert abs(loadout.get_user_music_volume()-.35)<.001 and abs(loadout.get_user_ambience_volume()-.6)<.001
        forest=next(c for c in unreal.ObjectIterator(unreal.AudioComponent)
                    if c.get_editor_property('sound') and c.get_editor_property('sound').get_name()=='forest_loop' and c.is_playing())
        assert abs(forest.get_editor_property('volume_multiplier')-
                   mode.get_editor_property('forest_ambience_volume')*.6)<.001
        call(director,'ReplayIntro')
        assert abs(director.get_editor_property('IntroComponent').get_editor_property('volume_multiplier')-.35)<.001
        call(director,'StopIntro')
        record('Ambience and replay intro retain their base mix with user multipliers')
        call(director,'SavePreferences')
        saved=unreal.GameplayStatics.load_game_from_slot('QuasicomboPreferences',0)
        assert saved and close(color(saved.get_editor_property('SavedBody')),expected_body)
        assert close(color(saved.get_editor_property('SavedLights')),expected_lights)
        assert abs(saved.get_editor_property('SavedMusic')-.35)<.001
        record('Blueprint preferences save colors and audio')
        director.set_editor_property('SaveSlot','QuasicomboWriteCheck*')
        call(director,'SavePreferences')
        notice=director.get_editor_property('MenuShell').get_editor_property('SaveNotice')
        assert str(notice.get_text()) and close(color(loadout.get_mike_light_color()),expected_lights)
        record('Failed save retains working session settings and shows notice')
        invalid=unreal.GameplayStatics.create_save_game_object(pcls)
        invalid.set_editor_property('PreferenceVersion',99)
        assert unreal.GameplayStatics.save_game_to_slot(invalid,'QuasicomboInvalidPreferencesCheck',0)
        director.set_editor_property('SaveSlot','QuasicomboInvalidPreferencesCheck')
        call(director,'LoadPreferences')
        assert close(color(loadout.get_mike_light_color()),[1,.82,.05,1])
        assert loadout.get_user_music_volume()==1 and loadout.get_user_ambience_volume()==1
        unreal.GameplayStatics.delete_game_in_slot('QuasicomboInvalidPreferencesCheck',0)
        director.set_editor_property('SaveSlot','QuasicomboPreferences')
        call(director,'LoadPreferences')
        assert close(color(loadout.get_mike_light_color()),expected_lights)
        record('Invalid save uses defaults; valid saved preferences reload')
        call(director,'SetFrontendScreen',0)
        call(director,'BeginWave')
        call(director,'StartFrontendRun')
        call(director,'StartFrontendRun')
        assert director.get_editor_property('MenuStarting') and director.get_editor_property('WavePhase')==0
        assert not director.get_editor_property('MenuShell').get_is_enabled()
        assert not run.has_run_started() and not music.get_editor_property('current_music')
        record('Repeated Start disables UI and cancels wave immediately')
        advance('transition')
    elif stage=='transition':
        if age<.5: return
        if age<1.1:
            assert not run.has_run_started() and run.get_elapsed_seconds()==0
            assert not music.get_editor_property('current_music')
            assert director.get_editor_property('MenuShell').get_render_opacity()==0
            record('UI fades by 0.4s while camera handoff still holds gameplay')
            advance('finish_transition')
    elif stage=='finish_transition':
        if not run.has_run_started(): return
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        assert director.get_editor_property('TransitionTime')>=1.2
        assert director.get_editor_property('MenuFinished')
        assert not director.get_editor_property('MenuShell').is_in_viewport()
        assert pc.get_view_target()==pawn and not pc.get_editor_property('show_mouse_cursor')
        assert music.get_editor_property('current_music') and music.get_editor_property('track_starts')==1
        assert run.get_elapsed_seconds()<.15
        record('Gameplay, music and pawn camera release after 1.2s',elapsed=run.get_elapsed_seconds())
        previous_world=world
        run.end_run(unreal.QuasicomboRunOutcome.DEFEAT)
        advance('death_retry')
    elif stage=='death_retry':
        if world==previous_world or unreal.GameplayStatics.get_time_seconds(world)<.5: return
        assert not director and run.has_run_started() and loadout.has_started_session()
        assert not pawn.get_buster_cannon().is_equipped()
        assert close(color(loadout.get_mike_light_color()),expected_lights)
        record('Death retry bypasses frontend and preserves colors')
        pawn.get_buster_cannon().equip()
        run.end_run(unreal.QuasicomboRunOutcome.VICTORY)
        assert loadout.replay_with_buster_cannon(world)
        previous_world=world
        advance('victory_replay')
    elif stage=='victory_replay':
        if world==previous_world or unreal.GameplayStatics.get_time_seconds(world)<.5: return
        assert not director and run.has_run_started() and pawn.get_buster_cannon().is_equipped()
        assert close(color(loadout.get_mike_body_region_color(unreal.MikeBodyRegion.COLOR_G)),expected_body)
        record('Victory replay bypasses frontend with acquired cannon and colors')
        run.end_run(unreal.QuasicomboRunOutcome.DEFEAT)
        previous_world=world
        advance('cannon_retry')
    elif stage=='cannon_retry':
        if world==previous_world or unreal.GameplayStatics.get_time_seconds(world)<.5: return
        assert not director and run.has_run_started() and pawn.get_buster_cannon().is_equipped()
        record('Death retry retains acquired cannon')
        finish(True)

def tick(delta):
    try: tick_impl(delta)
    except Exception:
        finish(False,traceback.format_exc())

callback=unreal.register_slate_post_tick_callback(tick)
print('QUASICOMBO_FRONTEND_VALIDATION_STARTED')
