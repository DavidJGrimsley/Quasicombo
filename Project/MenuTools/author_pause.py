"""Wire the existing pause Designer widget through Unreal MCP.

Preserves existing menu layouts, saved preferences, and gameplay state. The
frontend actor is reused during the first run; a lightweight subclass supplies
the same menu functions on retries that bypass the frontend.
"""
import json
import re
import sys

from mcp_client import UnrealMCP, ref, REPORT
from blueprint_author import Blueprint, B, A, quote as q
from author_behaviors import DCLASS, PCLASS
from author_director import function, shell, hud, loadout

sys.stdout.reconfigure(encoding='utf-8')
PAUSE = 'WBP_QuasicomboPause'
SESSION = 'BP_QuasicomboPauseSession'
CONTROLLER = '/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingPlayerController.BP_SideScrollingPlayerController'


class CachedMCP(UnrealMCP):
    """Node catalogs are identical across functions in a Blueprint."""
    def __init__(self):
        super().__init__()
        self.catalog_cache = {}

    def call(self, tool_name, **args):
        if tool_name.endswith(('.find_node_types', '.get_node_type_pins')):
            key = (tool_name, args['graph']['refPath'].split(':')[0],
                   args.get('type_id_filter', args.get('type_id')))
            if key not in self.catalog_cache:
                self.catalog_cache[key] = super().call(tool_name, **args)
            return self.catalog_cache[key]
        return super().call(tool_name, **args)


def refresh_catalogs(m):
    """Invalidate MCP's per-graph catalogs after changing another Blueprint."""
    script = REPORT / 'refresh-pause-catalogs.py'
    REPORT.mkdir(parents=True, exist_ok=True)
    script.write_text('from editor_toolset.toolsets.blueprint import _BlueprintCache\n'
                      '_BlueprintCache._graph_node_types.clear()\n'
                      'print("QUASICOMBO_PAUSE_CATALOGS_REFRESHED")\n', encoding='utf-8')
    slate = 'SlateInspectorToolset.SlateInspectorToolset.'
    m.call(slate + 'Observe', ref='w1', maxDepth=45)
    tree = m.call(slate + 'Snapshot', ref='w1', maxDepth=45)
    console = re.search(r'text "Cmd"[\s\S]*?textbox[^\n]*\[ref=(\w+)\]', tree)
    assert console, 'Open the Output Log console for the MCP catalog refresh'
    assert m.call(slate + 'Type', ref=console.group(1), text='py "' + script.as_posix() + '"', submit=True)
    if hasattr(m, 'catalog_cache'): m.catalog_cache.clear()


def shortcut_function(m):
    """Use FKey equality directly; the generic promoted operator rejects FKey."""
    bp = Blueprint(m, 'WBP_QuasicomboMenu')
    g = bp.function('IsPauseShortcut', outputs={'IsShortcut': 'bool'})
    if 'InKeyEvent' not in m.call(B + 'read_graph_dsl', graph=g.graph):
        m.call(B + 'add_struct_function_param', graph=g.graph, param_name='InKeyEvent',
               struct_type=ref('/Script/SlateCore.KeyEvent'), input_param=True)
    g.write('(fn IsPauseShortcut (InKeyEvent) (return (or false false)))', 'Pause_IsPauseShortcut')
    script = REPORT / 'create-pause-key-nodes.py'
    result = REPORT / 'pause-key-nodes.json'
    script.write_text('import unreal,json\nfrom pathlib import Path\n'
        'g=unreal.load_object(None,' + repr(g.graph['refPath']) + ')\n'
        'e=unreal.BlueprintGraphEditor.get_graph_editor(g)\n'
        'nodes=[e.add_call_function_node("/Script/Engine.KismetInputLibrary:EqualEqual_KeyKey") for _ in range(2)]\n'
        'assert all(nodes), "Could not create native FKey equality nodes"\n'
        'Path(' + repr(result.as_posix()) + ').write_text(json.dumps([n.get_path_name() for n in nodes]))\n', encoding='utf-8')
    slate = 'SlateInspectorToolset.SlateInspectorToolset.'
    tree = m.call(slate + 'Snapshot', ref='w1', maxDepth=45)
    console = re.search(r'text "Cmd"[\s\S]*?textbox[^\n]*\[ref=(\w+)\]', tree)
    assert console and m.call(slate + 'Type', ref=console.group(1), text='py "' + script.as_posix() + '"', submit=True)
    native = [ref(p) for p in json.loads(result.read_text())]
    get_key = m.call(B + 'create_node', graph=g.graph, type_id=g.node('GetKey', prefer='Input|KeyEvent|')[0], pos={'x':200,'y':100})
    nodes = m.call(B + 'find_nodes', graph=g.graph, title='', node_class=None, entry_points_only=False)
    infos = m.call(B + 'get_node_infos', nodes=nodes)
    entry = next(i for i in infos if any(p['name']=='InKeyEvent' for p in i['output_pins']))
    boolean_or = next(i for i in infos if any(p['name']=='A' and p['type_id']=='Boolean' for p in i['input_pins']))
    by_path = {i['node']['refPath']:i for i in infos}
    key_info = by_path[get_key['refPath']]
    def pin(info, name, output=False):
        return next(p['pin_id'] for p in info['output_pins' if output else 'input_pins'] if p['name']==name)
    def wire(a, out, b, inp):
        m.call(B + 'connect_pins', output_pin=pin(a,out,True), input_pin=pin(b,inp))
    wire(entry,'InKeyEvent',key_info,'Input')
    for node, name, dest in zip(native, ['BackSpace','Gamepad_Special_Right'], ['A','B']):
        info = by_path[node['refPath']]
        wire(key_info,'ReturnValue',info,'A')
        m.call(B + 'set_pin_value', pin=pin(info,'B'), value=name)
        wire(info,'ReturnValue',boolean_or,dest)
    bp.compile()


def controller(m):
    bp = Blueprint(m, 'BP_SideScrollingPlayerController')
    bp.asset = ref(CONTROLLER)
    bp.cls = ref(CONTROLLER + '_C')
    return bp


def write(bp, name, build, inputs=None):
    function(bp, name, build, inputs)


def can_pause(g):
    return g.call('CanPauseRun')


def status_function(bp):
    g = bp.function('CanPauseRun', outputs={'CanPause': 'bool'})
    run = g.call('GetRun', prefer='Quasicombo|Run|')
    switch, _ = g.node('SwitchonEQuasicomboRunOutcome')
    started = f'(and {g.call("HasRunStarted", target=run)} {g.call("HasStartedSession", target=g.call("GetLoadout"))})'
    g.write(f'''(fn CanPauseRun () (switch {switch} {g.call('GetOutcome', target=run)}
      (:Playing (return {started})) (:Victory (return false)) (:Defeat (return false))))''', bp.name + '_CanPauseRun')
    bp.compile()


def setup(m):
    bp = Blueprint(m, 'BP_QuasicomboFrontend')
    bp.variables({'PauseMenuOpen': 'bool'})
    for name in ['OpenPauseMenu', 'ResumePauseRun', 'TogglePauseMenu', 'InitializePauseSession']:
        bp.function(name)
    bp.compile()
    menu = Blueprint(m, 'WBP_QuasicomboMenu')
    widgets = {w['widgetName']: w for w in m.call('UMGToolSet.UMGToolSet.GetWidgets', widgetBlueprint=menu.asset)['widgets']}
    if 'PauseMenu' not in widgets:
        info = m.call('UMGToolSet.UMGToolSet.AddWidget', widgetBlueprint=menu.asset,
                      widgetClass=Blueprint(m, PAUSE).cls, widgetDisplayName='PauseMenu',
                      parentWidget=widgets['Pages']['widget'])
        m.call('UMGToolSet.UMGToolSet.ToggleWidgetAsVariable', widgetBlueprint=menu.asset,
               widget=info['widget'], bIsVariable=True)
    menu.compile()
    assert m.call('UMGToolSet.UMGToolSet.CompileWidgetBlueprint', widgetBlueprint=menu.asset)


def director(m):
    bp = Blueprint(m, 'BP_QuasicomboFrontend')
    status_function(bp)
    def init(g):
        cast, _ = g.node('CastToSideScrollingCharacter')
        return f'''(bind pawn ({cast} :Object {g.call('GetPlayerCharacter', PlayerIndex=0)})
          (:then {g.set('MenuPawn', 'pawn')}
            {g.set('MenuController', g.call('GetPlayerController', prefer='Game|', PlayerIndex=0))}
            {g.set('MenuPreferences', g.call('CreateSaveGameObject', SaveGameClass=q(PCLASS)))}
            {g.set('MenuShell', g.call('CreateWidget', Class=q('/Game/UI/Menu/WBP_QuasicomboMenu.WBP_QuasicomboMenu_C'), OwningPlayer=g.get('MenuController')))}
            {g.set('IntroComponent', g.call('SpawnSound2D', Sound=g.get('IntroSound'), VolumeMultiplier=g.call('GetUserMusicVolume', target=loadout(g)), bAutoDestroy='false'))}
            {g.call('SilenceIntro')}
            {g.set('MenuInitialized', 'true')} {g.set('MenuFinished', 'true')}
            {g.set('MenuStarting', 'true')}
            {g.call('SetActorTickEnabled', target='self', bEnabled='false')})
          (:CastFailed))'''
    write(bp, 'InitializePauseSession', init)
    pause_camera(bp)
    def open_menu(g):
        return f'''(if (and {can_pause(g)} (and {g.get('MenuFinished')} (not {g.get('PauseMenuOpen')})))
          (if (not {g.call('IsGamePaused')})
            (if {g.call('SetGamePaused', bPaused='true')}
              {g.set('PauseMenuOpen', 'true')}
              {g.call('PreparePauseCamera')}
              {g.call('SetRenderOpacity', target=shell(g), InOpacity=1.0)}
              {g.call('SetIsEnabled', target=shell(g), bInIsEnabled='true')}
              {g.call('AddToViewport', target=shell(g), ZOrder=30)}
              {hud(g, 'Collapsed')}
              {g.call('SetShowMouseCursor', target=g.get('MenuController'), bShowMouseCursor='true')}
              {g.call('SetInputModeUIOnly', PlayerController=g.get('MenuController'), InWidgetToFocus=shell(g), bFlushInput='true')}
              {g.call('SetFrontendScreen', ScreenIndex=5)})))'''
    write(bp, 'OpenPauseMenu', open_menu)
    resume_transition(bp)
    write(bp, 'TogglePauseMenu', lambda g: f'''(if {g.get('PauseMenuOpen')}
      {g.call('ResumePauseRun')} (else {g.call('OpenPauseMenu')}))''')
    def show(g):
        return f'''(if (or {g.get('PauseMenuOpen')} (not {g.get('MenuStarting')}))
          {g.set('MenuScreen', f'(select (and {g.get("PauseMenuOpen")} (== ScreenIndex 0)) 5 ScreenIndex)')}
          {g.call('ShowFrontendPage', target=shell(g), PageIndex=g.get('MenuScreen'))}
          (if (and (not {g.get('PauseMenuOpen')}) (not {g.get('IntroOnly')}))
            {g.call('FaceMenuCamera')}
            (if (== ScreenIndex 1) {g.call('CancelWave')}
              (elif (not {g.call('MontageIsPlaying', target=g.get('MenuAnimation'), Montage=g.get('MenuActiveMontage'))})
                {g.call('PlayMenuIdle')}))))'''
    write(bp, 'SetFrontendScreen', show, {'ScreenIndex': 'int'})
    def back(g):
        home = f'(select {g.get("PauseMenuOpen")} 5 0)'
        return f'''(if (or {g.get('PauseMenuOpen')} (not {g.get('MenuStarting')}))
          (if (== {g.get('MenuScreen')} 5) {g.call('ResumePauseRun')}
          (else (switch int {g.get('MenuScreen')}
            (:0) (:1 {g.call('StorySkip')})
            (:2 {g.call('SavePreferences')} {g.call('SetFrontendScreen', ScreenIndex=home)})
            (:3 {g.call('SavePreferences')} {g.call('SetFrontendScreen', ScreenIndex=home)})
            (:Default {g.call('SetFrontendScreen', ScreenIndex=3)})))))'''
    write(bp, 'FrontendBack', back)
    # Replay story must remain navigable while the game is paused after Start.
    story_navigation(bp)
    bp.compile(); bp.save()
    session = Blueprint(m, SESSION)
    session.create(DCLASS)
    g = session.events()
    g.write('(event EventBeginPlay ' + g.call('InitializePauseSession') + ')', SESSION + '_Events')
    session.compile(); session.save()


def pause_camera(bp):
    bp.variables({'GameplayViewTarget': '/Script/Engine.Actor', 'PausePreviousFullTick': 'bool',
                  'ResumeBlendSeconds': 'float'})
    bp.compile()
    def ensure(g):
        position = f'(+ {g.call("GetActorLocation", prefer="Transformation|", target=g.get("MenuPawn"))} {g.get("CameraOffset")})'
        aim = f'(+ {g.call("GetActorLocation", prefer="Transformation|", target=g.get("MenuPawn"))} {g.get("CameraAimOffset")})'
        transform = g.call('MakeTransform', Location=position,
                           Rotation=g.call('FindLookAtRotation', Start=position, Target=aim))
        valid = g.call('IsValid', prefer='Utilities|IsValid', InputObject=g.get('MenuCamera'))
        spawn = g.set('MenuCamera', g.call('SpawnActorfromClass', Class=q('/Script/Engine.CameraActor'),
                                          SpawnTransform=transform, CollisionHandlingOverride=q('AlwaysSpawn')))
        return valid[:-1] + f' (:"Is Valid") (:"Is Not Valid" {spawn}))'
    write(bp, 'EnsurePauseCamera', ensure)
    bp.compile()
    def prepare(g):
        pc = g.get('MenuController')
        camera = g.get('MenuCamera')
        position = f'(+ {g.call("GetActorLocation", prefer="Transformation|", target=g.get("MenuPawn"))} {g.get("CameraOffset")})'
        aim = f'(+ {g.call("GetActorLocation", prefer="Transformation|", target=g.get("MenuPawn"))} {g.get("CameraAimOffset")})'
        transform = g.call('MakeTransform', Location=position,
                           Rotation=g.call('FindLookAtRotation', Start=position, Target=aim))
        component = g.call('GetCameraComponent', prefer='Class|CameraActor|', target=camera)
        view = g.call('GetViewTarget', prefer='Pawn|', target=pc)
        return f'''(if (not (== {view} {camera})) {g.set('GameplayViewTarget', view)})
          {g.set('PausePreviousFullTick', g.call('GetShouldPerformFullTickwhenPaused', target=pc))}
          {g.call('EnsurePauseCamera')}
          {g.call('UpdatePreviewCamera')}
          {g.call('SetFieldOfView', target=component, InFieldOfView=g.get('CameraFOV'))}
          {g.call('SetConstraintAspectRatio', target=component, bInConstrainAspectRatio='false')}
          {g.call('SetShouldPerformFullTickwhenPaused', target=pc, bShouldPerformFullTickWhenPaused='true')}
          {g.call('SetViewTargetWithBlend', target=pc, NewViewTarget=camera,
                  BlendTime=g.get('CameraBlendSeconds'), BlendFunc=q('VTBlend_Cubic'), BlendExp=2.0, bLockOutgoing='true')}'''
    write(bp, 'PreparePauseCamera', prepare)
    def restore(g):
        pc = g.get('MenuController')
        manager = g.call('GetPlayerCameraManager', PlayerIndex=0)
        camera = g.get('MenuCamera')
        component = g.call('GetCameraComponent', prefer='Class|CameraActor|', target=camera)
        # The paused world does not advance LastFrameCameraCache. Use a camera
        # actor at the visible POV, and let the outgoing target update each frame.
        return f'''(bind visible_location {g.call('GetCameraLocation', target=manager)})
          (bind visible_rotation {g.call('GetCameraRotation', target=manager)})
          (bind visible_fov {g.call('GetFOVAngle', target=manager)})
          {g.call('SetActorLocationAndRotation', prefer='Transformation|', target=camera,
                  NewLocation='visible_location', NewRotation='visible_rotation', bSweep='false', bTeleport='true')}
          {g.call('SetFieldOfView', target=component, InFieldOfView='visible_fov')}
          {g.call('SetViewTargetWithBlend', target=pc, NewViewTarget=camera, BlendTime=0.0)}
          {g.call('SetViewTargetWithBlend', target=pc, NewViewTarget=g.get('GameplayViewTarget'),
                  BlendTime=g.get('ResumeBlendSeconds'), BlendFunc=q('VTBlend_Cubic'), BlendExp=2.0, bLockOutgoing='false')}'''
    write(bp, 'RestorePauseCamera', restore)


def resume_transition(bp):
    bp.variables({'PauseResuming': 'bool', 'PauseMenuHidden': 'bool',
                  'ResumeElapsed': 'float', 'ResumeBlendSeconds': 'float', 'ResumeHoldElapsed': 'float',
                  'ResumeMenuHiddenRealTime': 'float'})
    for name, args in [('ResumePauseRun', None), ('UpdatePauseResume', {'FrameSeconds': 'float'}),
                       ('FinishPauseResume', None)]:
        bp.function(name, args)
    bp.compile()
    def resume(g):
        return f'''(if (and {g.get('PauseMenuOpen')} (not {g.get('PauseResuming')}))
          {g.call('SavePreferences')} {g.call('StopIntro')}
          {g.set('PauseResuming', 'true')} {g.set('PauseMenuHidden', 'false')}
          {g.set('ResumeElapsed', 0.0)} {g.set('ResumeHoldElapsed', 0.0)}
          {g.set('ResumeBlendSeconds', g.get('CameraBlendSeconds'))}
          {g.call('SetIsEnabled', target=shell(g), bInIsEnabled='false')}
          {g.call('RestorePauseCamera')})'''
    write(bp, 'ResumePauseRun', resume)
    def update(g):
        # PC Tick runs before the camera update. Wait until the preceding frame
        # completed the blend before removing the menu, then count a fresh hold.
        return f'''(if {g.get('PauseResuming')}
          (if {g.get('PauseMenuHidden')}
            {g.set('ResumeHoldElapsed', f'(- {g.call("GetRealTimeSeconds")} {g.get("ResumeMenuHiddenRealTime")})')}
            (if (>= {g.get('ResumeHoldElapsed')} 0.25) {g.call('FinishPauseResume')})
            (else {g.set('ResumeElapsed', f'(+ {g.get("ResumeElapsed")} FrameSeconds)')}
              (if (>= (- {g.get('ResumeElapsed')} FrameSeconds) {g.get('ResumeBlendSeconds')})
                {g.call('RemoveFromParent', target=shell(g))}
                {g.set('PauseMenuHidden', 'true')} {g.set('ResumeHoldElapsed', 0.0)}
                {g.set('ResumeMenuHiddenRealTime', g.call('GetRealTimeSeconds'))}))))'''
    write(bp, 'UpdatePauseResume', update, {'FrameSeconds': 'float'})
    def finish(g):
        pc = g.get('MenuController')
        return f'''(if {g.get('PauseResuming')}
          {g.set('PauseMenuOpen', 'false')} {g.set('PauseResuming', 'false')}
          {g.call('SetShouldPerformFullTickwhenPaused', target=pc, bShouldPerformFullTickWhenPaused=g.get('PausePreviousFullTick'))}
          {g.call('SetGamePaused', bPaused='false')}
          {hud(g, 'SelfHitTestInvisible')}
          {g.call('SetShowMouseCursor', target=pc, bShowMouseCursor='false')}
          {g.call('SetInputModeGameOnly', PlayerController=pc, bFlushInput='true')})'''
    write(bp, 'FinishPauseResume', finish)


def resume_tick(m):
    bp = controller(m)
    g = bp.events()
    cast, _ = g.node('CastToBP_QuasicomboFrontend')
    body = f'''(if {g.call('IsGamePaused')}
      (bind director ({cast} :Object {g.call('GetActorOfClass', ActorClass=q(DCLASS))})
        (:then {g.call('UpdatePauseResume', target='director', FrameSeconds='DeltaSeconds')}) (:CastFailed)))'''
    g.write('(event EventTick (DeltaSeconds) ' + body + ')', 'Pause_ResumeTick')
    bp.compile(); bp.save()


def update_resume(m):
    bp = Blueprint(m, 'BP_QuasicomboFrontend')
    resume_transition(bp)
    pause_camera(bp)
    bp.compile(); bp.save()
    session = Blueprint(m, SESSION)
    session.compile(); session.save()
    refresh_catalogs(m)
    resume_tick(m)


def story_navigation(bp):
    def next_page(g):
        return f'''(if (and (or {g.get('PauseMenuOpen')} (not {g.get('MenuStarting')})) (== {g.get('MenuScreen')} 1))
          (if (< {g.get('StoryPage')} 3)
            {g.set('StoryPage', f'(+ {g.get("StoryPage")} 1)')}
            {g.call('ShowFrontendPage', target=shell(g), PageIndex=1)}
            (else {g.call('StorySkip')})))'''
    write(bp, 'StoryNext', next_page)
    def skip(g):
        return f'''(if (or {g.get('PauseMenuOpen')} (not {g.get('MenuStarting')}))
          {g.call('StopIntro')}
          (if {g.get('IntroOnly')} {g.call('LeaveIntroLevel')}
            (elif {g.get('StoryReplay')} {g.call('SetFrontendScreen', ScreenIndex=3)}
              (else {g.call('SetFrontendScreen', ScreenIndex=0)}))))'''
    write(bp, 'StorySkip', skip)


def pages(m):
    bp = Blueprint(m, 'WBP_QuasicomboMenu')
    definitions = [(0, 'Home', 'Start', 'WBP_QuasicomboHome', None),
                   (1, 'Story', 'Continue', 'WBP_QuasicomboStory', 'RefreshStory'),
                   (2, 'Customization', 'BodySwatch0', 'WBP_NYSQCustomization', 'RefreshAppearance'),
                   (3, 'Settings', 'ReplayStory', 'WBP_QuasicomboSettings', 'RefreshAudio'),
                   (4, 'GameInfo', 'Back', 'WBP_QuasicomboGameInfo', None),
                   (5, 'PauseMenu', 'Resume', PAUSE, None)]
    def build(g, focus_only=False):
        cases = []
        for index, name, first, cls, refresh in definitions:
            if index == 5:
                cast, _ = g.node('CastToWBP_QuasicomboPause')
                widget = 'pause_widget'
            else:
                widget = g.get(name)
            button = g.get(first, widget, prefer=cls.replace('_', '') + '|')
            body = ''
            if not focus_only:
                if index == 1:
                    scroll = g.get('StoryScroll', widget, prefer='WBPQuasicomboStory|')
                    body += g.call('SetScrollOffset', prefer='Widget|SetScrollOffset', target=scroll, NewScrollOffset=0.0) + ' '
                if refresh: body += g.call(refresh, target=widget) + ' '
            body += g.call('SetUserFocus', prefer='Widget|SetUserFocus', target=button,
                           PlayerController=g.call('GetOwningPlayer', target='self'))
            if index == 5:
                pause_body = f'(bind pause_widget ({cast} :Object {g.call("GetChildAt", target=g.get("Pages"), Index=5)}) (:then {body}) (:CastFailed))'
            else: cases.append(f'(:{index} {body})')
        page = g.call('GetActiveWidgetIndex', target=g.get('Pages')) if focus_only else 'PageIndex'
        return '(if (== ' + page + ' 5) ' + pause_body + ' (else (switch int ' + page + ' ' + ' '.join(cases) + ')))'
    g = bp.function('ShowFrontendPage', {'PageIndex': 'int'})
    body = ' '.join([g.call('ConfigureStoryLayout', PageIndex='PageIndex'),
                     g.set('PendingFocus', 'true'), g.set('FocusDelay', 0.0),
                     g.call('SetActiveWidgetIndex', target=g.get('Pages'), Index='PageIndex'), build(g)])
    g.write('(fn ShowFrontendPage (PageIndex) ' + body + ')', 'Pause_ShowFrontendPage')
    g = bp.function('FocusActivePage')
    g.write('(fn FocusActivePage () ' + build(g, True) + ')', 'Pause_FocusActivePage')
    key_and_buttons(m)


def key_and_buttons(m):
    shortcut_function(m)
    bp = Blueprint(m, 'WBP_QuasicomboMenu')
    g = bp.function('OnPreviewKeyDown')
    d = g.get('MenuDirector')
    handled, unhandled = g.call('Handled'), g.call('Unhandled')
    shortcut = f'''(if (and {g.get('PauseMenuOpen', d)} {g.call('IsPauseShortcut', InKeyEvent='InKeyEvent')})
      (if (not {g.call('IsRepeat', Input='InKeyEvent')}) {g.call('ResumePauseRun', target=d)})
      (return {handled}))'''
    action = g.call('GetKeyEventNavigationAction', InKeyEvent='InKeyEvent')
    switch, pins = g.node('SwitchonEUINavigationAction')
    cases = []
    for pin_info in pins['output_pins']:
        if pin_info['type_id'] != 'Exec': continue
        name = pin_info['name']; short = name.split('::')[-1]
        body = f'(return {unhandled})'
        if short == 'Back': body = g.call('FrontendBack', target=d) + f' (return {handled})'
        elif short == 'Accept':
            body = f'(if (== {g.get("MenuScreen",d)} 1) {g.call("StoryNext",target=d)} (return {handled}) (else (return {unhandled})))'
        elif short == 'Invalid':
            scroll = g.get('StoryScroll',g.get('Story'),prefer='WBPQuasicomboStory|')
            nav_type, nav_pins = g.node('SwitchonEUINavigation')
            nav_cases = []
            for p in nav_pins['output_pins']:
                if p['type_id'] != 'Exec': continue
                reply = f'(return {unhandled})'
                nav_name = p['name']; nav_short = nav_name.split('::')[-1]
                if nav_short in ['Up','Down']:
                    offset = f'({"-" if nav_short=="Up" else "+"} {g.call("GetScrollOffset",prefer="Widget|GetScrollOffset",target=scroll)} 80.0)'
                    reply = g.call('SetScrollOffset',prefer='Widget|SetScrollOffset',target=scroll,
                                   NewScrollOffset=g.call('Max(Float)',prefer='Math|Float|',A=offset,B=0.0)) + f' (return {handled})'
                nav_cases.append(f'(:{nav_name} {reply})')
            body = f'(if (== {g.get("MenuScreen",d)} 1) (switch {nav_type} {g.call("GetKeyEventNavigationDirection",InKeyEvent="InKeyEvent")} {" ".join(nav_cases)}) (else (return {unhandled})))'
        cases.append(f'(:{name} {body})')
    g.write(f'(fn OnPreviewKeyDown (MyGeometry InKeyEvent) {shortcut} (switch {switch} {action} {" ".join(cases)}))', 'Pause_OnPreviewKeyDown')
    bp.compile()
    assert m.call('UMGToolSet.UMGToolSet.CompileWidgetBlueprint', widgetBlueprint=bp.asset)
    bp.save()
    pause = Blueprint(m, PAUSE)
    g = pause.events()
    code = m.call(B + 'read_graph_dsl', graph=g.graph)
    old = 'Class|BPQuasicomboFrontend|StartFrontendRun'
    new, _ = g.node('ResumePauseRun', prefer='BPQuasicomboFrontend|')
    code = code.replace(old, new)
    code = re.sub(r'\(event (On\w+\([^)]*\))', lambda match:'(event '+q(match.group(1)),code)
    g.write(code, PAUSE + '_Events')
    pause.compile(); pause.save()
    # Read live settings even on initial construction, before selecting the page.
    settings = Blueprint(m, 'WBP_QuasicomboSettings')
    g = settings.events()
    code = m.call(B + 'read_graph_dsl', graph=g.graph)
    needle = '(Variables|Default|SetMenuDirector _asbp_quasicombo_frontend))'
    if needle in code:
        code = code.replace(needle, '(Variables|Default|SetMenuDirector _asbp_quasicombo_frontend) ' + g.call('RefreshAudio') + ')', 1)
        code = re.sub(r'\(event (On\w+\([^)]*\))', lambda match:'(event '+q(match.group(1)),code)
        g.write(code, 'Pause_SettingsConstruct')
    settings.compile(); settings.save()


def input_only(m):
    bp = controller(m)
    status_function(bp)
    def toggle(g):
        cast, _ = g.node('CastToBP_QuasicomboFrontend')
        return f'''(if {can_pause(g)}
          (bind menu ({cast} :Object {g.call('GetActorOfClass', ActorClass=q(DCLASS))})
            (:then {g.call('TogglePauseMenu', target='menu')})
            (:CastFailed
              (bind menu2 ({cast} :Object {g.call('SpawnActorfromClass', Class=q('/Game/UI/Menu/'+SESSION+'.'+SESSION+'_C'), SpawnTransform=g.call('MakeTransform'), CollisionHandlingOverride=q('AlwaysSpawn'))})
                (:then {g.call('TogglePauseMenu', target='menu2')}) (:CastFailed)))))'''
    write(bp, 'ToggleQuasicomboPause', toggle)
    bp.compile()
    controller_events(m)


def controller_events(m):
    bp = controller(m)
    g = bp.events()
    for node in m.call(B + 'find_nodes', graph=g.graph, title='', node_class=None, entry_points_only=False):
        m.call(B + 'delete_node', node=node)
    g.write('(event EventBeginPlay)\n(event EventTick (DeltaSeconds))', 'Pause_ControllerEvents')
    # Create input event nodes explicitly so only Pressed (never Released) toggles.
    for event in ['Backspace', 'GamepadSpecialRight']:
        typeid, _ = g.node(event, prefer='Input|KeyboardEvents|' if event == 'Backspace' else 'Input|GamepadEvents|')
        nodes = m.call(B + 'find_nodes', graph=g.graph, title='', node_class=None, entry_points_only=False)
        infos = m.call(B + 'get_node_infos', nodes=nodes)
        if any(i['type_id'] == typeid for i in infos): continue
        key = m.call(B + 'create_node', graph=g.graph, type_id=typeid, pos={'x':0,'y':500 if event=='Backspace' else 750})
        callid, _ = g.node('ToggleQuasicomboPause')
        call = m.call(B + 'create_node', graph=g.graph, type_id=callid, pos={'x':300,'y':500 if event=='Backspace' else 750})
        keyinfo, callinfo = m.call(B + 'get_node_infos', nodes=[key, call])
        output = next(p['pin_id'] for p in keyinfo['output_pins'] if p['name'] == 'Pressed')
        input = next(p['pin_id'] for p in callinfo['input_pins'] if p['type_id'] == 'Exec')
        m.call(B + 'connect_pins', output_pin=output, input_pin=input)
    bp.compile(); bp.save()
    resume_tick(m)


def main():
    m = CachedMCP()
    for ts in [B.rstrip('.'), A.rstrip('.'), 'UMGToolSet.UMGToolSet',
               'editor_toolset.toolsets.object.ObjectTools', 'editor_toolset.toolsets.programmatic.ProgrammaticToolset']:
        m.describe(ts)
    m.call('editor_toolset.toolsets.programmatic.ProgrammaticToolset.get_execution_environment')
    for stage in sys.argv[1:] or ['setup', 'director', 'pages', 'input']:
        refresh_catalogs(m)
        {'setup': setup, 'director': director, 'pages': pages, 'keys': key_and_buttons,
         'input': input_only, 'input_events': controller_events, 'resume': update_resume}[stage](m)


if __name__ == '__main__':
    main()
