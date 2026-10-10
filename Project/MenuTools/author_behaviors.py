"""Editable menu state, save and transition graphs authored using Unreal MCP.

Run after author_layouts.py. Generated DSL snapshots are kept under Saved for
review; runtime behavior resides entirely in the Blueprint assets.
"""
import json
import re
import sys
from mcp_client import UnrealMCP, ROOT, REPORT, ref
from blueprint_author import Blueprint, Graph, B, A, quote as q
from author_layouts import ASSETS, SWATCHES, linear

sys.stdout.reconfigure(encoding='utf-8')
DNAME = 'BP_QuasicomboFrontend'
PNAME = 'BP_QuasicomboPreferences'
DCLASS = f'/Game/UI/Menu/{DNAME}.{DNAME}_C'
PCLASS = f'/Game/UI/Menu/{PNAME}.{PNAME}_C'

FUNCTIONS = {
    DNAME: {
        'FrontendInitialize': {}, 'SetFrontendScreen': {'ScreenIndex': 'int'},
        'StoryNext': {}, 'StorySkip': {}, 'FrontendBack': {},
        'LoadPreferences': {}, 'SavePreferences': {}, 'ResetColors': {},
        'StartFrontendRun': {}, 'FinishFrontendRun': {}, 'CancelWave': {},
        'BeginWave': {}, 'UpdateFrontend': {'FrameSeconds': 'float'},
        'UpdatePreviewCamera': {}, 'InitializeMenuAnimation': {}, 'RestoreGameplayPose': {},
        'PlayMenuIdle': {}, 'BeginBlink': {}, 'UpdateMenuAnimation': {'FrameSeconds': 'float'},
        'UpdateIntroVolume': {}, 'FaceMenuCamera': {},
    },
    ASSETS[0]: {}, ASSETS[1]: {'RefreshStory': {}},
    ASSETS[2]: {'RefreshAppearance': {}, 'ApplyBodySliders': {}, 'ApplyLightSliders': {}},
    ASSETS[3]: {'RefreshAudio': {}, 'ApplyFrontendAudio': {}},
    ASSETS[4]: {}, ASSETS[5]: {'ShowFrontendPage': {'PageIndex': 'int'}},
}


def setup(m):
    prefs, director = Blueprint(m, PNAME), Blueprint(m, DNAME)
    prefs.create('/Script/Engine.SaveGame'); director.create('/Script/Engine.Actor')
    prefs.variables({'PreferenceVersion': 'int', 'SavedBody': 'LinearColor',
                     'SavedLights': 'LinearColor', 'SavedMusic': 'float', 'SavedAmbience': 'float'})
    prefs.compile()
    prefs.defaults({'PreferenceVersion': 1, 'SavedBody': {'r': 1, 'g': .08, 'b': .5, 'a': 1},
                    'SavedLights': {'r': 1, 'g': .82, 'b': .05, 'a': 1},
                    'SavedMusic': 1, 'SavedAmbience': 1})
    director.variables({
        'MenuShell': f'/Game/UI/Menu/{ASSETS[5]}.{ASSETS[5]}_C',
        'MenuPawn': '/Script/Braided_Quanta2026.SideScrollingCharacter',
        'MenuController': '/Script/Engine.PlayerController',
        'MenuCamera': '/Script/Engine.CameraActor',
        'MenuMesh': '/Script/Engine.SkeletalMeshComponent',
        'MenuAnimation': '/Script/Engine.AnimInstance',
        'IntroComponent': '/Script/Engine.AudioComponent',
        'IntroSound': '/Script/Engine.SoundBase',
        'BlinkClip': '/Script/Engine.AnimSequence',
        'RegularIdleClip': '/Script/Engine.AnimSequence', 'AggroIdleClip': '/Script/Engine.AnimSequence',
        'MenuActiveMontage': '/Script/Engine.AnimMontage',
        'WaveCompleteClip': '/Script/Engine.AnimSequence',
        'MenuPreferences': PCLASS, 'MenuInitialized': 'bool', 'MenuStarting': 'bool',
        'MenuFinished': 'bool', 'MenuScreen': 'int', 'StoryPage': 'int', 'StoryReplay': 'bool',
        'InitializationTime': 'float', 'TransitionTime': 'float',
        'FadeSeconds': 'float', 'CameraBlendSeconds': 'float',
        'WaveWait': 'float', 'WaveTime': 'float', 'WavePlaybackSeconds': 'float', 'WavePhase': 'int',
        'EmoteIntervalSeconds': 'float', 'BlinkOffsetSeconds': 'float',
        'IdleCycleSeconds': 'float', 'IdleAggroShare': 'float',
        'MenuPreviewTime': 'float', 'MenuIdleCycleTime': 'float',
        'NextWaveAt': 'float', 'NextBlinkAt': 'float', 'MenuIdleAggro': 'bool',
        'CameraOffset': 'Vector', 'CameraAimOffset': 'Vector', 'CameraFOV': 'float',
        'GameplayMeshRotation': 'Rotator', 'SaveSlot': 'string',
    })
    widgets = {name: Blueprint(m, name) for name in ASSETS}
    for widget in widgets.values():
        widget.variables({'MenuDirector': DCLASS, 'RefreshingControls': 'bool'})
    story = widgets[ASSETS[1]]
    # Text is embedded verbatim, preserving paragraph contents and internal newlines.
    paragraphs = re.split(r'\n\s*\n', (ROOT / 'Project/Narrative.txt').read_text(encoding='utf-8').rstrip('\n'))
    if len(paragraphs) != 4:
        raise ValueError('The narrative must contain exactly four paragraph-based pages')
    story.variables({f'NarrativePage{i}': 'text' for i in range(4)})
    for name, functions in FUNCTIONS.items():
        bp = director if name == DNAME else widgets[name]
        for function, inputs in functions.items():
            bp.function(function, inputs)
        bp.compile()
    story.defaults({f'NarrativePage{i}': text for i, text in enumerate(paragraphs)})
    director.defaults({'FadeSeconds': .4, 'CameraBlendSeconds': 1.2,
                       'EmoteIntervalSeconds': 10, 'BlinkOffsetSeconds': 5,
                       'IdleCycleSeconds': 50, 'IdleAggroShare': .4,
                       'CameraOffset': {'x': -180, 'y': 700, 'z': 110},
                       'CameraAimOffset': {'x': -180, 'y': 0, 'z': 10},
                       'CameraFOV': 45, 'SaveSlot': 'QuasicomboPreferences',
                       'IntroSound': ref('/Game/UI/Menu/SW_Intro.SW_Intro'),
                       **{key: ref(f'/Game/RadicalMike/Animations/Anim_ZMIKE_{clip}.Anim_ZMIKE_{clip}')
                          for key, clip in [('WaveCompleteClip','WaveComplete'), ('BlinkClip','Blinking'),
                                            ('RegularIdleClip','Idle'), ('AggroIdleClip','IdleAggro')]}})
    return director, prefs, widgets


def resolve_widget(g):
    actor = g.call('GetActorOfClass', ActorClass=q(DCLASS), WorldContextObject='self')
    cast, _ = g.node('CastTo' + DNAME)
    return f'''(bind director ({cast} :Object {actor})
      (:then {g.set('MenuDirector', 'director')}) (:CastFailed))'''


def bind_buttons(bp, handlers):
    g = bp.events()
    forms = ['(event UserInterface|EventConstruct ' + resolve_widget(g) + ')']
    for widget, body in handlers.items():
        event = bp.bind(widget, 'OnClicked', 'Button')
        forms.append(f'(event {event} {body(g)})')
    return g, forms


def screen(index):
    return lambda g: g.call('SetFrontendScreen', target=g.get('MenuDirector'), ScreenIndex=index)


def simple_widgets(d, widgets):
    home = widgets[ASSETS[0]]
    g, forms = bind_buttons(home, {
        'Start': lambda g: g.call('StartFrontendRun', target=g.get('MenuDirector')),
        'Customize': screen(2), 'Settings': screen(3)})
    g.write('\n'.join(forms), home.name + '_Events')
    story = widgets[ASSETS[1]]
    g, forms = bind_buttons(story, {
        'Continue': lambda g: g.call('StoryNext', target=g.get('MenuDirector')),
        'Skip': lambda g: g.call('StorySkip', target=g.get('MenuDirector'))})
    g.write('\n'.join(forms), story.name + '_Events')
    g = story.function('RefreshStory')
    page = g.get('StoryPage', g.get('MenuDirector'))
    # Explicit values avoid localization/format-string conversion ambiguities.
    cases = ' '.join(f'(:{i} {g.call("SetText(Text)", target=g.get("StoryText"), InText=g.get(f"NarrativePage{i}"))} {g.call("SetText(Text)", target=g.get("PageNumber"), InText=q(f"{i+1} / 4"))})' for i in range(4))
    g.write(f'(fn RefreshStory () (switch int {page} {cases}))', story.name + '_Refresh')
    info = widgets[ASSETS[4]]
    g, forms = bind_buttons(info, {'Back': screen(3)})
    g.write('\n'.join(forms), info.name + '_Events')


def main():
    m = UnrealMCP()
    for toolset in [B.rstrip('.'), A.rstrip('.'), 'UMGToolSet.UMGToolSet',
                    'editor_toolset.toolsets.object.ObjectTools']:
        m.describe(toolset)
    if '--graphs' in sys.argv:
        d, p = Blueprint(m, DNAME), Blueprint(m, PNAME)
        widgets = {name: Blueprint(m, name) for name in ASSETS}
    else:
        d, p, widgets = setup(m)
    simple_widgets(d, widgets)
    for bp in [p, d, *widgets.values()]:
        bp.compile(); bp.save()


if __name__ == '__main__':
    main()
