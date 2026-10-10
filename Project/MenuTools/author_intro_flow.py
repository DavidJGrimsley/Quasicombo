"""Black story world and Blueprint launch state, authored through Unreal MCP."""
import json
from mcp_client import UnrealMCP,ref
from blueprint_author import Blueprint,B,A,quote as q,normalize
from author_director import function,loadout,shell,author_save,author_screens,author_initialize,author_tick
from author_audio_nodes import audio_function
from author_controls import shell_controls,focus_controls
from author_layouts import asset,BATCH_PRELUDE,INK

SESSION='/Game/UI/Menu/BP_QuasicomboSession.BP_QuasicomboSession_C'
INTRO='/Game/UI/Menu/Lvl_QuasicomboIntro'
MAIN='/Game/Maps/Lvl_Main'
m=UnrealMCP()
for t in [B.rstrip('.'),A.rstrip('.'),'UMGToolSet.UMGToolSet','editor_toolset.toolsets.object.ObjectTools']:
    m.describe(t)

session=Blueprint(m,'BP_QuasicomboSession')
session.create('/Script/Engine.GameInstance')
session.variables({'LaunchStorySeen':'bool'})
session.compile();session.save()
bp=Blueprint(m,'BP_QuasicomboFrontend')
bp.variables({'IntroOnly':'bool'})
for name in ['ApplyPreferenceLights','FrontendStartup','InitializeIntro','InitializeJungleMenu',
             'LeaveIntroLevel','SilenceIntro']:
    bp.function(name)
bp.compile()

# Both existing light setters have the same action ID. Specify the subsystem
# declaring class so this also works in the intro world, where there is no pawn.
g=bp.function('ApplyPreferenceLights')
nodes=m.call(B+'find_nodes',graph=g.graph,title='',node_class=None,entry_points_only=False)
infos=m.call(B+'get_node_infos',nodes=nodes)
entry=next(i for i in infos if i['type_id'].lower()=='|applypreferencelights')
for i in infos:
    if i['node']!=entry['node']: m.call(B+'delete_node',node=i['node'])
def create(search,declaring=None):
    node_id,_=g.node(search)
    args=dict(graph=g.graph,type_id=node_id,pos={'x':400,'y':200})
    if declaring: args['declaring_class']=ref(declaring)
    n=m.call(B+'create_node',**args)
    return m.call(B+'get_node_infos',nodes=[n])[0]
def pin(n,name,out=False):
    return next(p['pin_id'] for p in n['output_pins' if out else 'input_pins'] if normalize(p['name'])==normalize(name))
def wire(a,ap,b,bp): m.call(B+'connect_pins',output_pin=pin(a,ap,True),input_pin=pin(b,bp))
l=create('GetLoadout');prefs=create('GetMenuPreferences');c=create('GetSavedLights')
setter=create('SetMikeLightColor','/Script/Braided_Quanta2026.QuasicomboLoadoutSubsystem')
wire(entry,'then',setter,'execute');wire(l,'ReturnValue',setter,'self')
wire(prefs,'MenuPreferences',c,'self');wire(c,'SavedLights',setter,'Color')
bp.compile()
audio_function(bp,'SilenceIntro','Stop',{})

def launch_session(g,body):
    cast,_=g.node('CastToBP_QuasicomboSession')
    return f'(bind launch ({cast} :Object {g.call("GetGameInstance",prefer="Game|")}) (:then {body}) (:CastFailed))'

def startup(g):
    manager=g.call('GetPlayerCameraManager',PlayerIndex=0)
    black=g.call('SetManualCameraFade',target=manager,InFadeAmount=1.0,
                 Color=q('(R=0,G=0,B=0,A=1)'),bInFadeAudio='false')
    current=g.call('GetCurrentLevelName',bRemovePrefixString='true')
    intro=g.call('EqualExactly(String)',A=current,B=q('Lvl_QuasicomboIntro'))
    return f'''{black} {g.set('IntroOnly',intro)}
      (if {g.get('IntroOnly')} {g.call('InitializeIntro')}
        (else {launch_session(g,f'(if (not {g.get("LaunchStorySeen","launch")}) '+g.call('OpenLevel(byName)',LevelName=q(INTRO),bAbsolute='true')+')')}))'''
function(bp,'FrontendStartup',startup)

def initialize_intro(g):
    return f'''{g.set('MenuController',g.call('GetPlayerController',prefer='Game|',PlayerIndex=0))}
      {g.call('LoadPreferences')}
      {g.set('MenuShell',g.call('CreateWidget',Class=q('/Game/UI/Menu/WBP_QuasicomboMenu.WBP_QuasicomboMenu_C'),OwningPlayer=g.get('MenuController')))}
      {g.call('AddToViewport',target=shell(g),ZOrder=30)}
      {g.call('SetShowMouseCursor',target=g.get('MenuController'),bShowMouseCursor='true')}
      {g.call('SetInputModeUIOnly',PlayerController=g.get('MenuController'),InWidgetToFocus=shell(g),bFlushInput='true')}
      {g.set('IntroComponent',g.call('SpawnSound2D',Sound=g.get('IntroSound'),VolumeMultiplier=g.call('GetUserMusicVolume',target=loadout(g)),bAutoDestroy='false'))}
      {g.set('MenuInitialized','true')} {g.set('StoryPage',0)} {g.set('StoryReplay','false')}
      {g.call('SetFrontendScreen',ScreenIndex=1)}'''
function(bp,'InitializeIntro',initialize_intro)
function(bp,'FrontendInitialize',lambda g:g.call('InitializeJungleMenu'))
def leave(g):
    body=f'''{g.set('LaunchStorySeen','true','launch')}
      {g.set('MenuStarting','true')} {g.set('TransitionTime',0.0)}
      {g.call('SetIsEnabled',target=shell(g),bInIsEnabled='false')}'''
    return launch_session(g,body)
function(bp,'LeaveIntroLevel',leave)
author_save(bp);author_screens(bp);author_initialize(bp);author_tick(bp)
bp.compile();bp.save()

mode=Blueprint(m,'BP_QuasicomboIntroMode')
mode.create('/Script/Engine.GameModeBase');mode.compile()
mode.defaults({'DefaultPawnClass':None,'HUDClass':None,'PlayerControllerClass':ref('/Script/Engine.PlayerController')})
g=mode.events()
transform=g.call('MakeTransform',Location=q('(X=0,Y=0,Z=0)'),Rotation=q('(Pitch=0,Yaw=0,Roll=0)'),Scale=q('(X=1,Y=1,Z=1)'))
g.write('(event EventBeginPlay '+g.call('SpawnActorfromClass',Class=q(bp.cls['refPath']),SpawnTransform=transform,
                                      CollisionHandlingOverride=q('AlwaysSpawn'))+')',mode.name+'_Events')
mode.compile();mode.save()

# Designer layout: completely opaque full-screen black, story centered above
# quiet text-only Continue/Skip controls. Home retains the left-hand panel.
wb=asset('WBP_QuasicomboMenu')
items={i['widgetName']:i for i in m.call('UMGToolSet.UMGToolSet.GetWidgets',widgetBlueprint=wb)['widgets']}
if 'StoryBlack' not in items:
    new=m.call('UMGToolSet.UMGToolSet.AddWidget',widgetBlueprint=wb,widgetClass=ref('/Script/UMG.Border'),
               widgetDisplayName='StoryBlack',parentWidget=items['Root']['widget'],childIndex=0)
    m.call('UMGToolSet.UMGToolSet.ToggleWidgetAsVariable',widgetBlueprint=wb,widget=new['widget'],bIsVariable=True)
    items['StoryBlack']=new
patches=[(items['StoryBlack']['widget'],{'brushColor':{'r':0,'g':0,'b':0,'a':1},'visibility':'Collapsed'}),
         (items['StoryBlack']['slot'],{'layoutData':{'anchors':{'minimum':{'x':0,'y':0},'maximum':{'x':1,'y':1}},
                     'offsets':{'left':0,'top':0,'right':0,'bottom':0}},'zOrder':0}),
         (items['Panel']['slot'],{'zOrder':1})]
story=asset('WBP_QuasicomboStory')
sitems={i['widgetName']:i for i in m.call('UMGToolSet.UMGToolSet.GetWidgets',widgetBlueprint=story)['widgets']}
for name in ['Heading','PageNumber']:
    patches.append((sitems[name]['widget'],{'visibility':'Collapsed'}))
for name,i in sitems.items():
    if i['widgetClassPath']['refPath']=='/Script/UMG.Spacer':
        patches.append((i['widget'],{'visibility':'Collapsed'}))
patches.append((sitems['StoryText']['widget'],{'font':{'size':28,'typefaceFontName':'Regular'},
               'colorAndOpacity':{'specifiedColor':{'r':.9,'g':.9,'b':.9,'a':1}}}))
patches.append((sitems['StoryScroll']['widget'],{'scrollBarVisibility':'Collapsed'}))
if 'StoryNavigation' not in sitems:
    nav=m.call('UMGToolSet.UMGToolSet.AddWidget',widgetBlueprint=story,widgetClass=ref('/Script/UMG.HorizontalBox'),
               widgetDisplayName='StoryNavigation',parentWidget=sitems['Root']['widget'])
    for name in ['Continue','Skip']:
        moved=m.call('UMGToolSet.UMGToolSet.MoveWidget',widgetBlueprint=story,widget=sitems[name]['widget'],newParent=nav['widget'])
        patches.append((moved['slot'],{'padding':{'right':24,'bottom':0},'size':{'sizeRule':'Fill','value':1}}))
for name in ['Continue','Skip']:
    # Remove the theme's painted button backgrounds; focus colors the text.
    style=m.get(sitems[name]['widget'],'widgetStyle')['widgetStyle']
    for state in ['normal','hovered','pressed']:
        style[state]['drawAs']='NoDrawType'
    patches.append((sitems[name]['widget'],{'widgetStyle':style,'backgroundColor':{'r':0,'g':0,'b':0,'a':0}}))
    patches.append((sitems[name+'Text']['widget'],{'font':{'size':20}}))
script=BATCH_PRELUDE+'\nPATCHES=json.loads('+repr(json.dumps(patches))+')\ndef run():\n    for o,p in PATCHES: set_props(o,p)\n    return {"updated":len(PATCHES)}\n'
print(m.batch(script))

menu=Blueprint(m,'WBP_QuasicomboMenu')
g=menu.function('ConfigureStoryLayout',{'PageIndex':'int'})
panel=g.get('Panel');slot=g.call('SlotAsCanvasSlot',Widget=panel)
def layout(anchors,offsets,brush,visibility):
    return ' '.join([g.call('SetVisibility',prefer='Widget|',target=g.get('StoryBlack'),InVisibility=q(visibility)),
        g.call('SetAnchors',target=slot,InAnchors=q(anchors)),g.call('SetOffsets',target=slot,InOffset=q(offsets)),
        g.call('SetBrushColor',target=panel,InBrushColor=q(brush))])
black=layout('(Minimum=(X=0.15,Y=0.16),Maximum=(X=0.85,Y=0.84))',
             '(Left=0,Top=0,Right=0,Bottom=0)','(R=0,G=0,B=0,A=1)','Visible')
home=layout('(Minimum=(X=0.055,Y=0.065),Maximum=(X=0.055,Y=0.94))',
            '(Left=0,Top=0,Right=640,Bottom=0)','(R=0.025,G=0.075,B=0.10,A=0.82)','Collapsed')
g.write(f'(fn ConfigureStoryLayout (PageIndex) (if (== PageIndex 1) {black} (else {home})))',menu.name+'_StoryLayout')
menu.compile();menu.save()
shell_controls(m)
focus_controls(m)
for name in ['WBP_QuasicomboStory','WBP_QuasicomboMenu']:
    w=Blueprint(m,name);w.compile();w.save()
print('Black story flow authored')
