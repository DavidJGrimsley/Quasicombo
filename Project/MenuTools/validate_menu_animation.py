"""Observe real PIE preview cycles, page changes, and Start during a wave.

Execute through editor_console.py in a fresh PIE. No saved settings are edited.
"""
import json
import math
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
cls = unreal.load_class(None, '/Game/UI/Menu/BP_QuasicomboFrontend.BP_QuasicomboFrontend_C')
out = Path(unreal.Paths.project_saved_dir()) / 'MenuValidation/menu-animation-tests.json'
state = dict(stage='intro', deadline=time.monotonic()+130, events=[], idles=[], checks=[],
             previous_phase=0, previous_idle=False, montage=None, position=0,
             visited=set(), replay=False, samples=0, regular_time=0.0, aggro_time=0.0,
             last_clock=0.0, idle_clips=set(), finished=False)
callback = None


def call(obj, name, *args):
    return obj.call_method(name, args=tuple(args))


def record(label, **values):
    state['checks'].append(dict(check=label, **values))
    print('QUASICOMBO_MENU_ANIMATION_CHECK', label, json.dumps(values))


def finish(success, error=None):
    state['finished'] = True
    unreal.unregister_slate_post_tick_callback(callback)
    report = {k: sorted(v) if isinstance(v,set) else v for k,v in state.items()
              if k not in ['montage', 'deadline']}
    report.update(success=success, error=error)
    out.write_text(json.dumps(report, indent=2))
    print('QUASICOMBO_MENU_ANIMATION_COMPLETE', success, error)


def face_check(director, pawn):
    camera = director.get_editor_property('MenuCamera')
    delta = camera.get_actor_location()-pawn.get_actor_location()
    yaw = math.degrees(math.atan2(delta.y, delta.x))
    original = director.get_editor_property('GameplayMeshRotation')
    expected = yaw-pawn.get_actor_rotation().yaw+original.yaw
    actual = director.get_editor_property('MenuMesh').get_editor_property('relative_rotation')
    error = (actual.yaw-expected+180) % 360-180
    assert abs(error)<.1, ('Facing camera', error, actual, expected)


def tick_impl(_delta):
    assert time.monotonic()<state['deadline'], ('Timeout', state['stage'])
    world = editor.get_game_world()
    if not world:
        return
    director = unreal.GameplayStatics.get_actor_of_class(world, cls)
    if not director or not director.get_editor_property('MenuInitialized'):
        return
    if director.get_editor_property('IntroOnly'):
        assert not unreal.GameplayStatics.get_player_pawn(world, 0)
        call(director, 'StorySkip')
        state['stage'] = 'preview'
        return
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    if not pawn:
        return
    run = next(o for o in unreal.ObjectIterator(unreal.QuasicomboRunSubsystem) if o.get_outer()==world)
    mode = unreal.GameplayStatics.get_game_mode(world)
    music = mode.get_editor_property('music_controller')
    anim = director.get_editor_property('MenuAnimation')
    clock = director.get_editor_property('MenuPreviewTime')
    phase = director.get_editor_property('WavePhase')
    screen = director.get_editor_property('MenuScreen')

    if state['stage']=='start':
        assert phase==0 and not anim.montage_is_playing(None)
        original = director.get_editor_property('GameplayMeshRotation')
        actual = director.get_editor_property('MenuMesh').get_editor_property('relative_rotation')
        assert abs((actual.yaw-original.yaw+180)%360-180)<.1
        if not director.get_editor_property('MenuFinished'):
            assert not run.has_run_started() and not music.get_editor_property('current_music')
            return
        assert run.has_run_started() and music.get_editor_property('current_music')
        assert not director.get_editor_property('MenuShell').is_in_viewport()
        assert unreal.GameplayStatics.get_player_controller(world,0).get_view_target()==pawn
        record('Start interrupts wave once and restores gameplay pose, camera and delayed music')
        finish(True)
        return

    assert run.get_elapsed_seconds()==0 and run.get_damage_taken()==0 and not run.get_route_history()
    assert not music.get_editor_property('current_music')
    face_check(director, pawn)
    state['samples'] += 1
    if state['stage']=='replay':
        assert screen==1 and phase==0 and not anim.montage_is_playing(None)
        assert clock==state['replay_clock']
        if time.monotonic()-state['replay_began']>.6:
            call(director,'StorySkip')
            assert director.get_editor_property('MenuScreen')==3
            record('Replay story pauses preview clock and resumes camera-facing idle in Settings')
            state['stage']='preview'
            state['previous_phase']=0
        return

    if screen not in state['visited']:
        state['visited'].add(screen)
        record('Preview faces camera on jungle menu page', screen=screen)
    aggro = director.get_editor_property('MenuIdleAggro')
    assert aggro==(30<=clock%50<50), ('Idle split', clock, aggro)
    delta = min(clock,50)-min(state['last_clock'],50)
    if delta>0:
        # Measure assigned idle mood over one complete 50-second cycle.
        boundary=30
        before=state['last_clock']
        state['regular_time'] += max(0,min(clock,boundary)-min(before,boundary))
        state['aggro_time'] += max(0,min(clock,50)-max(before,boundary))
    state['last_clock']=clock
    if aggro!=state['previous_idle']:
        state['idles'].append(dict(at=clock, aggro=aggro))
        state['previous_idle']=aggro

    montage=director.get_editor_property('MenuActiveMontage')
    slot=montage.get_editor_property('SlotAnimTracks')[0]
    segment=slot.get_editor_property('AnimTrack').get_editor_property('AnimSegments')[0]
    clip=segment.get_editor_property('AnimReference').get_name()
    expected_clip={1:'WaveComplete', 2:'Blinking'}.get(phase,'IdleAggro' if aggro else 'Idle')
    assert clip=='Anim_ZMIKE_'+expected_clip, ('Actual clip',clip,expected_clip)
    assert segment.get_editor_property('LoopingCount')==(1 if phase else 100000)
    if not phase:
        state['idle_clips'].add(clip)
    position=anim.montage_get_position(montage)
    if phase:
        if phase!=state['previous_phase']:
            state['events'].append(dict(at=clock, phase=phase))
            state['montage']=montage
            state['position']=position
            label='WaveComplete' if phase==1 else 'Blinking'
            expected=(len([e for e in state['events'] if e['phase']==phase])-1)*10+(10 if phase==1 else 5)
            assert abs(clock-expected)<.13, ('Emote onset', label, clock, expected)
            record('One scheduled full clip begins', clip=label, at=clock)
        else:
            assert montage==state['montage'], 'Montage replaced/restarted inside an emote'
            if anim.montage_is_playing(montage):
                assert position+.04>=state['position'], ('Animation jumped back', phase, position, state['position'])
                state['position']=position
    elif state['previous_phase']:
        assert state['position']>(3.5 if state['previous_phase']==1 else 2.2)
        assert anim.montage_is_playing(montage)
        record('Gesture completes and returns to looping idle', phase=state['previous_phase'], position=state['position'])
    state['previous_phase']=phase

    # Exercise transitions mid-gesture without reinitializing its montage/clock.
    if 11<clock<12 and screen==0:
        call(director,'SetFrontendScreen',2)
        assert director.get_editor_property('MenuActiveMontage')==montage
    elif 16<clock<17 and screen==2:
        call(director,'SetFrontendScreen',3)
        assert director.get_editor_property('MenuActiveMontage')==montage
    elif 21<clock<22 and screen==3:
        call(director,'SetFrontendScreen',4)
        assert director.get_editor_property('MenuActiveMontage')==montage
    elif clock>26 and not state['replay']:
        state['replay']=True
        state['replay_clock']=clock
        state['replay_began']=time.monotonic()
        settings = director.get_editor_property('MenuShell').get_editor_property('Settings')
        # Invoke the existing Blueprint button event, including its replay flag.
        call(settings, 'BndEvt__WBP_QuasicomboSettings_ReplayStory_K2Node_ComponentBoundEvent_0_OnButtonClickedEvent__DelegateSignature')
        state['stage']='replay'
    elif clock>50.7 and phase==1:
        assert state['visited']=={0,2,3,4}
        assert state['idle_clips']=={'Anim_ZMIKE_Idle','Anim_ZMIKE_IdleAggro'}
        assert abs(state['regular_time']-30)<.01 and abs(state['aggro_time']-20)<.01
        record('One 50-second cycle assigns 60% regular and 40% aggro',
               regular=state['regular_time'], aggro=state['aggro_time'])
        call(director,'StartFrontendRun')
        call(director,'StartFrontendRun')
        state['stage']='start'


def tick(delta):
    try:
        tick_impl(delta)
    except Exception:
        finish(False,traceback.format_exc())


callback=unreal.register_slate_post_tick_callback(tick)
print('QUASICOMBO_MENU_ANIMATION_VALIDATION_STARTED')
