"""Create the frontend actor's editable Blueprint state machine via MCP."""
import sys
from mcp_client import UnrealMCP, ref
from blueprint_author import Blueprint, B, A, quote as q
from author_behaviors import DNAME, PNAME, DCLASS, PCLASS, ASSETS

sys.stdout.reconfigure(encoding='utf-8')


def function(bp, name, build, inputs=None):
    g = bp.function(name, inputs)
    body = build(g)
    g.write(f'(fn {name} ({" ".join((inputs or {}).keys())}) {body})', DNAME + '_' + name)


def loadout(g):
    return g.call('GetLoadout')


def shell(g):
    return g.get('MenuShell')


def light_target(g):
    # This component's existing Blueprint API forwards to the session loadout.
    # It also avoids the two same-named MCP actions for SetMikeLightColor.
    return g.call('GetBusterCannon', prefer='BusterCannon|', target=g.get('MenuPawn'))


def child(g, name):
    return g.get(name, shell(g), prefer='WBPQuasicomboMenu|')


def hud(g, visibility):
    widgets = g.call('GetAllWidgetsOfClass', WidgetClass=q('/Game/UI/UBP_GameHUD.UBP_GameHUD_C'), TopLevelOnly='true')
    return f'(bind huds {widgets}) (for hud huds {g.call("SetVisibility", prefer="Widget|", target="hud", InVisibility=q(visibility))})'


def start_gameplay(g):
    cast, _ = g.node('CastToSideScrollingGameMode')
    return f'''(bind gm ({cast} :Object {g.call('GetGameMode')})
      (:then {g.call('StartGameplay', target='gm')}) (:CastFailed))'''


def montage(g, clip, loops=1):
    return g.call('PlaySlotAnimationAsDynamicMontage', target=g.get('MenuAnimation'),
                  Asset=g.get(clip), SlotNodeName=q('DefaultSlot'), BlendInTime=.2,
                  BlendOutTime=.2, InPlayRate=1.0, LoopCount=loops,
                  BlendOutTriggerTime=-1.0, InTimeToStartMontageAt=0.0)


def author_save(bp):
    def apply(g):
        prefs = g.get('MenuPreferences'); session = loadout(g)
        return ' '.join([
            g.call('SetMikeBodyColor', target=session, Color=g.get('SavedBody', prefs)),
            g.call('ApplyPreferenceLights'),
            g.call('SetUserAudioVolumes', target=session, Music=g.get('SavedMusic', prefs),
                   Ambience=g.get('SavedAmbience', prefs))])
    function(bp, 'ApplyPreferences', apply)
    def load(g):
        saved = g.get('MenuPreferences')
        new = g.call('CreateSaveGameObject', SaveGameClass=q(PCLASS))
        cast, _ = g.node('CastTo' + PNAME)
        # NaNs fail comparisons; all components must be inside the supported range.
        valid = f'(== {g.get("PreferenceVersion", "loaded")} 1)'
        split_colors = []
        for color in ['SavedBody', 'SavedLights']:
            components = [color + component.upper() for component in 'rgba']
            split_colors.append(f'(bind ({" ".join(components)}) {g.call("BreakColor", prefer="Math|Color|", InColor=g.get(color, "loaded"))})')
            for component in 'rgba':
                value = color + component.upper()
                valid = f'(and {valid} (and (>= {value} 0.0) (<= {value} 1.0)))'
        for name in ['SavedMusic', 'SavedAmbience']:
            value = g.get(name, 'loaded')
            valid = f'(and {valid} (and (>= {value} 0.0) (<= {value} 1.0)))'
        apply_call = g.call('ApplyPreferences')
        return f'''{g.set('MenuPreferences', new)}
          (if {g.call('DoesSaveGameExist', SlotName=g.get('SaveSlot'), UserIndex=0)}
            (bind loaded ({cast} :Object {g.call('LoadGameFromSlot', SlotName=g.get('SaveSlot'), UserIndex=0)})
              (:then {' '.join(split_colors)} (if {valid}
                {g.set('MenuPreferences', 'loaded')} {apply_call}
                (else {apply_call})))
              (:CastFailed {apply_call}))
            (else {apply_call}))'''
    function(bp, 'LoadPreferences', load)
    def save(g):
        prefs = g.get('MenuPreferences'); session = loadout(g)
        notice = g.get('SaveNotice', shell(g))
        return f'''{g.set('PreferenceVersion', 1, prefs)}
          {g.set('SavedBody', g.call('GetMikeBodyRegionColor', target=session, Region=q('ColorG')), prefs)}
          {g.set('SavedLights', g.call('GetMikeLightColor', target=session), prefs)}
          {g.set('SavedMusic', g.call('GetUserMusicVolume', target=session), prefs)}
          {g.set('SavedAmbience', g.call('GetUserAmbienceVolume', target=session), prefs)}
          (if {g.call('SaveGameToSlot', SaveGameObject=prefs, SlotName=g.get('SaveSlot'), UserIndex=0)}
            {g.call('SetText(Text)', target=notice, InText=q(''))}
            (else {g.call('SetText(Text)', target=notice, InText=q('Settings are active for this session. Could not save to disk.'))}))'''
    function(bp, 'SavePreferences', save)
    def reset(g):
        return f'''{g.call('SetMikeBodyColor', target=loadout(g), Color=q('(R=1.0,G=0.08,B=0.5,A=1.0)'))}
          {g.call('SetMikeLightColor', target=light_target(g), Color=q('(R=1.0,G=0.82,B=0.05,A=1.0)'))}
          {g.call('RefreshAppearance', target=child(g, 'Customization'))}
          {g.call('SavePreferences')}'''
    function(bp, 'ResetColors', reset)


def author_screens(bp):
    from author_audio_nodes import author
    author(bp)
    def show(g):
        return f'''(if (not {g.get('MenuStarting')})
          {g.set('MenuScreen', 'ScreenIndex')}
          {g.call('ShowFrontendPage', target=shell(g), PageIndex='ScreenIndex')}
          (if (not {g.get('IntroOnly')})
            {g.call('FaceMenuCamera')}
            (if (== ScreenIndex 1) {g.call('CancelWave')}
              (elif (not {g.call('MontageIsPlaying', target=g.get('MenuAnimation'), Montage=g.get('MenuActiveMontage'))})
                {g.call('PlayMenuIdle')}))))'''
    function(bp, 'SetFrontendScreen', show, {'ScreenIndex': 'int'})
    def next_page(g):
        return f'''(if (and (not {g.get('MenuStarting')}) (== {g.get('MenuScreen')} 1))
          (if (< {g.get('StoryPage')} 3)
            {g.set('StoryPage', f'(+ {g.get("StoryPage")} 1)')}
            {g.call('ShowFrontendPage', target=shell(g), PageIndex=1)}
            (else {g.call('StorySkip')})))'''
    function(bp, 'StoryNext', next_page)
    def skip(g):
        return f'''(if (not {g.get('MenuStarting')})
          {g.call('StopIntro')}
          (if {g.get('IntroOnly')}
            {g.call('LeaveIntroLevel')}
            (elif {g.get('StoryReplay')}
            {g.call('SetFrontendScreen', ScreenIndex=3)}
            (else {g.call('SetFrontendScreen', ScreenIndex=0)}))))'''
    function(bp, 'StorySkip', skip)
    def back(g):
        return f'''(if (not {g.get('MenuStarting')})
          (switch int {g.get('MenuScreen')}
            (:0)
            (:1 {g.call('StorySkip')})
            (:2 {g.call('SavePreferences')} {g.call('SetFrontendScreen', ScreenIndex=0)})
            (:3 {g.call('SavePreferences')} {g.call('SetFrontendScreen', ScreenIndex=0)})
            (:Default {g.call('SetFrontendScreen', ScreenIndex=3)})))'''
    function(bp, 'FrontendBack', back)


def author_wave(bp):
    # The preview clock runs across jungle submenus and pauses for Replay story.
    # Single complete clips prevent a duplicated start/loop/wave gesture.
    bp.variables({
        'BlinkClip': '/Script/Engine.AnimSequence',
        'RegularIdleClip': '/Script/Engine.AnimSequence',
        'AggroIdleClip': '/Script/Engine.AnimSequence',
        'MenuActiveMontage': '/Script/Engine.AnimMontage',
        'MenuPreviewTime': 'float', 'MenuIdleCycleTime': 'float',
        'NextWaveAt': 'float', 'NextBlinkAt': 'float', 'MenuIdleAggro': 'bool',
        'EmoteIntervalSeconds': 'float', 'BlinkOffsetSeconds': 'float',
        'IdleCycleSeconds': 'float', 'IdleAggroShare': 'float',
    })
    for name in ['PlayMenuIdle', 'BeginBlink', 'RestoreGameplayPose', 'InitializeMenuAnimation',
                 'UpdateMenuAnimation']:
        bp.function(name, {'FrameSeconds': 'float'} if name == 'UpdateMenuAnimation' else {})
    def face(g):
        # Rotate the mesh only; the actual pawn and gameplay heading stay fixed.
        relative = g.get('GameplayMeshRotation')
        camera_yaw = g.call('FindLookAtRotation', Start=g.call('GetActorLocation', prefer='Transformation|', target=g.get('MenuPawn')),
                           Target=g.call('GetActorLocation', prefer='Transformation|', target=g.get('MenuCamera')))
        actor_rotation = g.call('GetActorRotation', prefer='Transformation|', target=g.get('MenuPawn'))
        rotation = g.call('MakeRotator', Roll=f'(.roll {relative})', Pitch=f'(.pitch {relative})',
            Yaw=f'(+ (- (.yaw {camera_yaw}) (.yaw {actor_rotation})) (.yaw {relative}))')
        return g.call('SetRelativeRotation', prefer='Transformation|', target=g.get('MenuMesh'), NewRotation=rotation, bSweep='false', bTeleport='true')
    function(bp, 'FaceMenuCamera', face)
    def cancel(g):
        return f'''{g.call('MontageStop', prefer='Animation|Montage|', InBlendOutTime=.15, target=g.get('MenuAnimation'))}
          {g.set('WavePhase', 0)} {g.set('WaveTime', 0.0)}'''
    function(bp, 'CancelWave', cancel)
    function(bp, 'RestoreGameplayPose', lambda g: f'''{g.call('CancelWave')}
      {g.call('SetRelativeRotation', prefer='Transformation|', target=g.get('MenuMesh'),
        NewRotation=g.get('GameplayMeshRotation'), bSweep='false', bTeleport='true')}''')
    function(bp, 'PlayMenuIdle', lambda g: f'''{g.set('WavePhase', 0)}
      {g.set('WaveTime', 0.0)}
      (if {g.get('MenuIdleAggro')}
        {g.set('MenuActiveMontage', montage(g, 'AggroIdleClip', 100000))}
        (else {g.set('MenuActiveMontage', montage(g, 'RegularIdleClip', 100000))}))''')
    function(bp, 'BeginWave', lambda g: f'''{g.call('FaceMenuCamera')} {g.set('WavePhase', 1)}
      {g.set('WaveTime', 0.0)} {g.set('MenuActiveMontage', montage(g, 'WaveCompleteClip'))}''')
    function(bp, 'BeginBlink', lambda g: f'''{g.call('FaceMenuCamera')} {g.set('WavePhase', 2)}
      {g.set('WaveTime', 0.0)} {g.set('MenuActiveMontage', montage(g, 'BlinkClip'))}''')
    function(bp, 'InitializeMenuAnimation', lambda g: f'''{g.set('MenuPreviewTime', 0.0)}
      {g.set('MenuIdleCycleTime', 0.0)} {g.set('MenuIdleAggro', 'false')}
      {g.set('NextWaveAt', g.get('EmoteIntervalSeconds'))}
      {g.set('NextBlinkAt', g.get('BlinkOffsetSeconds'))}
      {g.call('PlayMenuIdle')}''')
    def update(g):
        time = g.get('MenuPreviewTime')
        cycle = g.get('MenuIdleCycleTime')
        interval = g.call('Max(Float)', prefer='Math|Float|', A=g.get('EmoteIntervalSeconds'), B=1.0)
        cycle_length = g.call('Max(Float)', prefer='Math|Float|', A=g.get('IdleCycleSeconds'), B=1.0)
        share = g.call('Clamp(Float)', Value=g.get('IdleAggroShare'), Min=0.0, Max=1.0)
        wave_length = g.call('GetPlayLength', prefer='Animation|', target=g.get('WaveCompleteClip'))
        blink_length = g.call('GetPlayLength', prefer='Animation|', target=g.get('BlinkClip'))
        return f'''{g.set('MenuPreviewTime', f'(+ {time} FrameSeconds)')}
          {g.set('MenuIdleCycleTime', f'(+ {cycle} FrameSeconds)')}
          (if (>= {cycle} {cycle_length})
            {g.set('MenuIdleCycleTime', f'(- {cycle} {cycle_length})')})
          (bind aggro (>= {cycle} (* {cycle_length} (- 1.0 {share}))))
          (if (not (== aggro {g.get('MenuIdleAggro')}))
            {g.set('MenuIdleAggro', 'aggro')}
            (if (== {g.get('WavePhase')} 0) {g.call('PlayMenuIdle')}))
          (if (not (== {g.get('WavePhase')} 0))
            {g.set('WaveTime', f'(+ {g.get("WaveTime")} FrameSeconds)')}
            (if (>= {g.get('WaveTime')} (select (== {g.get('WavePhase')} 1) {wave_length} {blink_length}))
              {g.call('PlayMenuIdle')}))
          (if (>= {time} {g.get('NextWaveAt')})
            {g.set('NextWaveAt', f'(+ {g.get("NextWaveAt")} {interval})')}
            (if (== {g.get('WavePhase')} 0) {g.call('BeginWave')}))
          (if (>= {time} {g.get('NextBlinkAt')})
            {g.set('NextBlinkAt', f'(+ {g.get("NextBlinkAt")} {interval})')}
            (if (== {g.get('WavePhase')} 0) {g.call('BeginBlink')}))
          {g.set('WaveWait', f'(- {g.get("NextWaveAt")} {time})')}'''
    function(bp, 'UpdateMenuAnimation', update, {'FrameSeconds': 'float'})


def author_transition(bp):
    def start(g):
        return f'''(if (and {g.get('MenuInitialized')} (not {g.get('MenuStarting')}))
          {g.set('MenuStarting', 'true')} {g.set('TransitionTime', 0.0)}
          {g.call('SetIsEnabled', target=shell(g), bInIsEnabled='false')}
          {g.call('SavePreferences')} {g.call('RestoreGameplayPose')}
          {g.call('StopIntro')}
          {g.call('SetViewTargetWithBlend', target=g.get('MenuController'), NewViewTarget=g.get('MenuPawn'),
              BlendTime=g.get('CameraBlendSeconds'), BlendFunc=q('VTBlend_Cubic'), BlendExp=2.0, bLockOutgoing='true')})'''
    function(bp, 'StartFrontendRun', start)
    def finish(g):
        return f'''{g.set('MenuFinished', 'true')}
          {g.call('RemoveFromParent', target=shell(g))}
          {hud(g, 'SelfHitTestInvisible')}
          {g.call('SetShowMouseCursor', target=g.get('MenuController'), bShowMouseCursor='false')}
          {g.call('SetInputModeGameOnly', PlayerController=g.get('MenuController'), bFlushInput='true')}
          {g.call('ResetIgnoreMoveInput', target=g.get('MenuController'))}
          {g.call('ResetIgnoreLookInput', target=g.get('MenuController'))}
          {g.call('DestroyActor', target=g.get('MenuCamera'))}
          {g.call('SetActorTickEnabled', target='self', bEnabled='false')}
          {start_gameplay(g)}'''
    function(bp, 'FinishFrontendRun', finish)


def author_initialize(bp):
    def init(g):
        cast, _ = g.node('CastToSideScrollingCharacter')
        pawn_location = g.call('GetActorLocation', prefer='Transformation|', target=g.get('MenuPawn'))
        camera_location = f'(+ {pawn_location} {g.get("CameraOffset")})'
        aim = f'(+ {pawn_location} {g.get("CameraAimOffset")})'
        transform = g.call('MakeTransform', Location=camera_location,
            Rotation=g.call('FindLookAtRotation', Start=camera_location, Target=aim),
            Scale=q('(X=1,Y=1,Z=1)'))
        return f'''(bind pawn ({cast} :Object {g.call('GetPlayerCharacter', PlayerIndex=0)})
          (:then
            {g.set('MenuPawn', 'pawn')}
            {g.set('MenuController', g.call('GetPlayerController', prefer='Game|', PlayerIndex=0))}
            {g.set('MenuMesh', g.call('GetMesh', prefer='Class|Character|', target='pawn'))}
            {g.set('MenuAnimation', g.call('GetAnimInstance', prefer='Components|SkeletalMesh|', target=g.get('MenuMesh')))}
            {g.set('GameplayMeshRotation', g.call('GetRelativeRotation', prefer='SceneComponent', target=g.get('MenuMesh')))}
            {g.call('LoadPreferences')}
            {g.set('MenuCamera', g.call('SpawnActorfromClass', Class=q('/Script/Engine.CameraActor'), SpawnTransform=transform, CollisionHandlingOverride=q('AlwaysSpawn')))}
            {g.call('SetFieldOfView', target=g.call('GetCameraComponent', prefer='Class|CameraActor|', target=g.get('MenuCamera')), InFieldOfView=g.get('CameraFOV'))}
            {g.call('SetConstraintAspectRatio', target=g.call('GetCameraComponent', prefer='Class|CameraActor|', target=g.get('MenuCamera')), bInConstrainAspectRatio='false')}
            {g.call('SetViewTargetWithBlend', target=g.get('MenuController'), NewViewTarget=g.get('MenuCamera'), BlendTime=0.0)}
            {g.set('MenuShell', g.call('CreateWidget', Class=q(f'/Game/UI/Menu/{ASSETS[5]}.{ASSETS[5]}_C'), OwningPlayer=g.get('MenuController')))}
            {g.call('AddToViewport', target=shell(g), ZOrder=30)}
            {hud(g, 'Collapsed')}
            {g.call('SetShowMouseCursor', target=g.get('MenuController'), bShowMouseCursor='true')}
            {g.call('SetInputModeUIOnly', PlayerController=g.get('MenuController'), InWidgetToFocus=shell(g), bFlushInput='true')}
            {g.set('IntroComponent', g.call('SpawnSound2D', Sound=g.get('IntroSound'), VolumeMultiplier=g.call('GetUserMusicVolume', target=loadout(g)), bAutoDestroy='false'))}
            {g.call('SilenceIntro')}
            {g.set('MenuInitialized', 'true')}
            {g.set('StoryPage', 0)} {g.set('StoryReplay', 'false')}
            {g.call('InitializeMenuAnimation')}
            {g.call('SetFrontendScreen', ScreenIndex=0)}
            {g.call('SetManualCameraFade', target=g.call('GetPlayerCameraManager',PlayerIndex=0),InFadeAmount=0.0,Color=q('(R=0,G=0,B=0,A=1)'),bInFadeAudio='false')})
          (:CastFailed))'''
    function(bp, 'InitializeJungleMenu', init)


def author_tick(bp):
    def update(g):
        transition = g.get('TransitionTime')
        opacity = f'(select (< {transition} {g.get("FadeSeconds")}) (- 1.0 (/ {transition} {g.get("FadeSeconds")})) 0.0)'
        return f'''(if (and {g.get('MenuInitialized')} (and (not {g.get('IntroOnly')}) (not {g.get('MenuStarting')})))
            {g.call('UpdatePreviewCamera')} {g.call('FaceMenuCamera')})
          (if (not {g.get('MenuInitialized')})
          {g.set('InitializationTime', f'(+ {g.get("InitializationTime")} FrameSeconds)')}
          (if (> {g.get('InitializationTime')} 0.1) {g.call('FrontendInitialize')})
          (elif {g.get('MenuStarting')}
            {g.set('TransitionTime', f'(+ {transition} FrameSeconds)')}
            (if {g.get('IntroOnly')}
              (if (>= {transition} {g.get('FadeSeconds')})
                {g.call('OpenLevel(byName)',LevelName=q('/Game/Maps/Lvl_Main'),bAbsolute='true')})
              (else {g.call('SetRenderOpacity', target=shell(g), InOpacity=opacity)}
                (if (>= {transition} {g.get('CameraBlendSeconds')}) {g.call('FinishFrontendRun')})))
            (elif (and (not {g.get('IntroOnly')}) (not (== {g.get('MenuScreen')} 1)))
              {g.call('UpdateMenuAnimation', FrameSeconds='FrameSeconds')})))'''
    function(bp, 'UpdateFrontend', update, {'FrameSeconds': 'float'})
    g = bp.events()
    g.write(f'(event EventBeginPlay {g.call("FrontendStartup")})\n(event EventTick (DeltaSeconds) {g.call("UpdateFrontend", FrameSeconds="DeltaSeconds")})', DNAME + '_Events')


def main():
    m = UnrealMCP()
    for toolset in [B.rstrip('.'), A.rstrip('.'), 'editor_toolset.toolsets.object.ObjectTools']:
        m.describe(toolset)
    bp = Blueprint(m, DNAME)
    # Select a stage while repairing; the default authors all stages in dependency order.
    stages = {'save': author_save, 'screens': author_screens, 'wave': author_wave,
              'transition': author_transition, 'initialize': author_initialize, 'tick': author_tick}
    selected = sys.argv[1:] or list(stages)
    for stage in selected:
        stages[stage](bp)
        bp.compile(); bp.save()


if __name__ == '__main__':
    main()
