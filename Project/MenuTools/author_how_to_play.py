"""Replace the duplicated How to Play page and integrate it through Unreal MCP.

Run against the level-design editor. Only the four menu assets below are saved.
Original graph snapshots and authoring diagnostics live under Saved.
"""
import json
import re
import sys
from mcp_client import UnrealMCP, ROOT, ref
import blueprint_author
from blueprint_author import Blueprint, Graph, B, A, quote as q
from author_behaviors import resolve_widget
from author_controls import tick_focus
from author_layouts import Layout, BATCH_PRELUDE, PALE, CYAN

sys.stdout.reconfigure(encoding='utf-8')
REPORT = ROOT / 'Saved/HowToPlayValidation'
blueprint_author.REPORT = REPORT
PAGE = 6
NAMES = ['WBP_HowToPlay', 'WBP_QuasicomboHome', 'WBP_QuasicomboMenu', 'BP_QuasicomboFrontend']
PARAGRAPH = (
    'Guide NYSQ through the jungle, choose your routes, and defeat the boss to complete your run. '
    'Earn a higher score by finishing quickly, building long hit combos, and making route crossings '
    'that do not cancel each other out. Landing hits builds your combo; taking damage or going seven '
    'seconds without another hit resets it. Your route choices also shape the boss encounter, and a '
    'Tau result awards bonus points. When injured, go three seconds without landing or receiving '
    'damage to begin recovering 10% of your maximum health per second. Recovery pauses during QTEs '
    'and fatal-trap capture. Follow the boss’s timed prompts and prepare for additional rounds: the '
    'boss refills its health between rounds, and its counterattacks halve your current health. '
    'Avoid pits and larvae traps—they can end your run.'
)
CONTROLS = [
    ('Move', 'A / D', 'Left stick'),
    ('Jump / double jump', 'Space; press again in midair', 'A; press again in midair'),
    ('Drop through platforms', 'S', 'Left stick down'),
    ('Interact', 'E', 'Y'),
    ('Attack / combo', 'Left mouse button', 'B'),
    ('Charged attack', 'Hold right mouse, then release', 'Hold RT, then release'),
    ('Dash', 'Left Shift', 'RB'),
    ('Buster Cannon', 'F or middle mouse button', 'X'),
    ('Pause / resume', 'Backspace', 'Menu / Start'),
]
FOOTNOTE = ('The Buster Cannon is earned after defeating the boss and remains available on replays '
            'during the same game session. During QTEs, follow the displayed input and hold it when instructed.')


def end_form(code, start):
    depth = 0
    quoted = escaped = False
    for i in range(start, len(code)):
        char = code[i]
        if escaped:
            escaped = False
        elif char == '\\' and quoted:
            escaped = True
        elif char == '"':
            quoted = not quoted
        elif not quoted:
            depth += (char == '(') - (char == ')')
            if depth == 0:
                return i + 1
    raise ValueError('Unbalanced graph DSL')


def forms(code):
    result = []
    offset = 0
    while True:
        start = code.find('(', offset)
        if start < 0:
            return result
        offset = end_form(code, start)
        result.append(code[start:offset])


def body(code):
    match = re.match(r'\(fn\s+\S+\s+\([^)]*\)\s*', code)
    assert match, code
    return code[match.end():].rstrip()[:-1].rstrip()


def insert_case(code, after, new_case):
    start = code.index('(:' + str(after))
    end = end_form(code, start)
    return code[:end] + '\n' + new_case + code[end:]


def widgets(m, bp):
    return {w['widgetName']: w for w in m.call('UMGToolSet.UMGToolSet.GetWidgets', widgetBlueprint=bp.asset)['widgets']}


def refresh_catalogs(m):
    slate = 'SlateInspectorToolset.SlateInspectorToolset.'
    m.call(slate + 'Observe', ref='', maxDepth=45)
    tree = m.call(slate + 'Snapshot', ref='', maxDepth=45)
    textbox = re.search(r'text "Cmd"[\s\S]*?textbox[^\n]*\[ref=(\w+)\]', tree).group(1)
    m.call(slate + 'Click', ref=textbox)
    m.call(slate + 'PressKey', key='Ctrl+A')
    assert m.call(slate + 'Type', ref=textbox, submit=True,
                  text='py from editor_toolset.toolsets.blueprint import _BlueprintCache; _BlueprintCache._graph_node_types.clear()')


def snapshot(m):
    REPORT.mkdir(parents=True, exist_ok=True)
    before = REPORT / 'Before'
    before.mkdir(exist_ok=True)
    for name in NAMES:
        bp = Blueprint(m, name)
        for graph in m.call(B + 'list_graphs', blueprint=bp.asset):
            fn = graph['refPath'].rsplit(':', 1)[-1]
            path = before / (name + '_' + fn + '.dsl')
            if not path.exists():
                path.write_text(m.call(B + 'read_graph_dsl', graph=graph), encoding='utf-8')
    path = before / 'Panel.json'
    if not path.exists():
        panel = widgets(m, Blueprint(m, 'WBP_QuasicomboMenu'))['Panel']
        path.write_text(json.dumps(m.get(panel['slot'], 'layoutData')), encoding='utf-8')


def original(name, fn):
    return (REPORT / 'Before' / (name + '_' + fn + '.dsl')).read_text(encoding='utf-8')


def layout(m):
    bp = Blueprint(m, 'WBP_HowToPlay')
    # Disconnect the copied behavior before removing its controls and functions.
    clear_events(m, bp)
    g = bp.events()
    event = bp.bind('Back', 'OnClicked', 'Button')
    g.write('(event UserInterface|EventConstruct ' + resolve_widget(g) + ')\n' +
            tick_focus(g, ['Back']) + '\n' +
            f'(event {event} {g.call("FrontendBack", target=g.get("MenuDirector"))})', 'HowToPlay_Events')
    for graph in m.call(B + 'list_graphs', blueprint=bp.asset):
        name = graph['refPath'].rsplit(':', 1)[-1]
        if name != 'EventGraph':
            m.call(B + 'remove_function_graph', blueprint=bp.asset, graph_name=name)
    if 'RefreshingControls' in m.call(B + 'list_variables', blueprint=bp.asset):
        m.call(B + 'remove_variable', blueprint=bp.asset, name='RefreshingControls')
    old = widgets(m, bp)
    for name, widget in old.items():
        if widget['parent'] == old['Root']['widget'] and name not in ['Heading', 'Back']:
            assert m.call('UMGToolSet.UMGToolSet.RemoveWidget', widgetBlueprint=bp.asset, widget=widget['widget'])
    page = Layout(bp.name)
    scroll = page.add('HowToScroll', 'ScrollBox', 'Root',
                      slot={'size': {'sizeRule': 'Fill', 'value': 1}, 'padding': {'top': 12, 'bottom': 18}})
    content = page.add('HowToContent', 'VerticalBox', scroll)
    page.text('GameplayInstructions', PARAGRAPH, content, 22)
    page.spacer(content, 24)
    page.text('ControlsHeading', 'Controls', content, 28, color=CYAN)
    for i, row in enumerate([('Action', 'Keyboard / mouse', 'Xbox controller'), *CONTROLS]):
        parent = page.add('ControlsRow' + str(i), 'HorizontalBox', content,
                          slot={'padding': {'bottom': 14}})
        for column, (text, weight) in enumerate(zip(row, [.28, .42, .30])):
            page.add(f'ControlsCell{i}_{column}', 'TextBlock', parent,
                     {'text': text, 'autoWrapText': True,
                      'font': {'size': 22, 'typefaceFontName': 'Bold' if i == 0 else 'Regular'},
                      'colorAndOpacity': {'specifiedColor': CYAN if i == 0 else PALE}},
                     {'size': {'sizeRule': 'Fill', 'value': weight},
                      'padding': {'right': 18}, 'verticalAlignment': 'VAlign_Top'})
    page.spacer(content, 16)
    page.text('BusterAndQTEHelp', FOOTNOTE, content, 22)
    page.spacer(content, 12)
    page.text('ScrollingHelp', 'Scroll with the mouse wheel, Up / Down, or the D-pad. Escape / B to return.', content, 18, color=CYAN)
    for item in page.items:
        if item['name'] in ['GameplayInstructions', 'BusterAndQTEHelp', 'ScrollingHelp']:
            item['props']['font']['typefaceFontName'] = 'Regular'
    script = BATCH_PRELUDE + '\nBP=' + repr(bp.asset) + '\nITEMS=json.loads(' + repr(json.dumps(page.items)) + ')\n'
    script += '''
def run():
    data=call('UMGToolSet.UMGToolSet.GetWidgets',{'widgetBlueprint':BP})
    widgets={w['widgetName']:w for w in data['widgets']}
    for item in ITEMS:
        args={'widgetBlueprint':BP,'widgetClass':item['cls'],'widgetDisplayName':item['name'],
              'parentWidget':widgets[item['parent']]['widget']}
        if item['name']=='HowToScroll': args['childIndex']=1
        info=call('UMGToolSet.UMGToolSet.AddWidget',args)
        widgets[item['name']]=info
        call('UMGToolSet.UMGToolSet.ToggleWidgetAsVariable',{'widgetBlueprint':BP,'widget':info['widget'],'bIsVariable':True})
        if item['props']: set_props(info['widget'],item['props'])
        if item['slot']: set_props(info['slot'],item['slot'])
    set_props(widgets['Heading']['widget'],{'text':'How to Play'})
    set_props(widgets['Back']['slot'],{'padding':{'top':0,'bottom':0}})
    return {'created':len(ITEMS),'preserved':['Heading','Back']}
'''
    print('Layout', m.batch(script), flush=True)
    bp.compile()
    ensure_shell(m, bp)


def clear_events(m, bp):
    script = BATCH_PRELUDE + '\nGRAPH=' + repr(bp.events().graph) + '\n'
    script += '''
def run():
    nodes=call('editor_toolset.toolsets.blueprint.BlueprintTools.find_nodes',{'graph':GRAPH,'title':''})
    for node in nodes:
        call('editor_toolset.toolsets.blueprint.BlueprintTools.delete_node',{'node':node})
    return {'removed':len(nodes)}
'''
    print('Clear copied events', m.batch(script), flush=True)


def ensure_shell(m, bp):
    shell = Blueprint(m, 'WBP_QuasicomboMenu')
    tree = widgets(m, shell)
    if 'HowToPlay' not in tree:
        item = m.call('UMGToolSet.UMGToolSet.AddWidget', widgetBlueprint=shell.asset,
                      widgetClass=bp.cls, widgetDisplayName='HowToPlay', parentWidget=tree['Pages']['widget'])
        m.call('UMGToolSet.UMGToolSet.ToggleWidgetAsVariable', widgetBlueprint=shell.asset,
               widget=item['widget'], bIsVariable=True)
    shell.compile()


def graphs(m):
    refresh_catalogs(m)
    home = Blueprint(m, 'WBP_QuasicomboHome')
    g = home.events()
    event = home.bind('HowToPlay', 'OnClicked', 'Button')
    code = original(home.name, 'EventGraph')
    top = [f for f in forms(code) if not f.startswith('(event UserInterface|EventTick') and not re.match(r'\(event "?OnClicked\(HowToPlay\)', f)]
    top += [tick_focus(g, ['Start', 'Customize', 'HowToPlay', 'Settings']),
            f'(event {event} {g.call("SetFrontendScreen",target=g.get("MenuDirector"),ScreenIndex=PAGE)})']
    g.write('\n\n'.join(top), 'Home_HowToPlay')
    home.compile()
    shell = Blueprint(m, 'WBP_QuasicomboMenu')
    for fn in ['ShowFrontendPage', 'FocusActivePage']:
        g = Graph(m, ref(shell.asset['refPath'] + ':' + fn))
        page = g.get('HowToPlay')
        scroll = g.get('HowToScroll', page, prefer='WBPHowToPlay|')
        back = g.get('Back', page, prefer='WBPHowToPlay|')
        stmts = g.call('SetUserFocus', prefer='Widget|SetUserFocus', target=back,
                       PlayerController=g.call('GetOwningPlayer', target='self'))
        if fn == 'ShowFrontendPage':
            stmts = g.call('SetScrollOffset', prefer='Widget|', target=scroll, NewScrollOffset=0.0) + ' ' + stmts
        code = original(shell.name, fn).replace('(CallFunction|ConfigureStoryLayout PageIndex)',
                                               '(CallFunction|ConfigureStoryLayout :PageIndex PageIndex)')
        code = code.replace('Class|PCGPathfindingSettings|GetStart',
                            g.node('GetStart', prefer='WBPQuasicomboHome|')[0])
        # MCP allocates integer switch outputs by case count, including empty cases.
        g.write(insert_case(code, 4, f'(:5) (:6 {stmts})'), 'HowToPlay_' + fn)
    g = Graph(m, ref(shell.asset['refPath'] + ':ConfigureStoryLayout'))
    panel = g.get('Panel')
    slot = g.call('SlotAsCanvasSlot', Widget=panel)
    alignment = json.loads((REPORT / 'Before/Panel.json').read_text())['layoutData']['alignment']
    restore = g.call('SetAlignment', prefer='Layout|CanvasSlot|', target=slot,
                     InAlignment=q(f'(X={alignment["x"]},Y={alignment["y"]})'))
    help_layout = ' '.join([
        g.call('SetVisibility', prefer='Widget|SetVisibility', target=g.get('StoryBlack'), InVisibility=q('Collapsed')),
        g.call('SetAnchors', prefer='Layout|CanvasSlot|', target=slot, InAnchors=q('(Minimum=(X=0.08,Y=0.06),Maximum=(X=0.92,Y=0.94))')),
        g.call('SetOffsets', prefer='Layout|CanvasSlot|', target=slot, InOffset=q('(Left=0,Top=0,Right=0,Bottom=0)')),
        g.call('SetAlignment', prefer='Layout|CanvasSlot|', target=slot, InAlignment=q('(X=0,Y=0)')),
        g.call('SetBrushColor', prefer='Appearance|SetBrushColor', target=panel, InBrushColor=q('(R=0.025,G=0.075,B=0.10,A=0.82)')),
    ])
    g.write(f'(fn ConfigureStoryLayout (PageIndex) (if (== PageIndex 6) {help_layout} (else {restore} {body(original(shell.name,"ConfigureStoryLayout"))})))', 'HowToPlay_Layout')
    g = Graph(m, ref(shell.asset['refPath'] + ':OnPreviewKeyDown'))
    code = original(shell.name, 'OnPreviewKeyDown').replace('(CallFunction|IsPauseShortcut InKeyEvent)',
                                                         '(CallFunction|IsPauseShortcut :InKeyEvent InKeyEvent)')
    start = code.index('(:Invalid')
    end = end_form(code, start)
    invalid = code[start:end]
    cond_start = invalid.index('(if ') + len('(if ')
    cond_end = end_form(invalid, cond_start)
    page6 = f'(== {g.get("MenuScreen",g.get("MenuDirector"),prefer="BPQuasicomboFrontend|")} 6)'
    condition = invalid[cond_start:cond_end]
    invalid = invalid[:cond_start] + f'(or {condition} {page6})' + invalid[cond_end:]
    match = re.search(r'\(Class\|WBPQuasicomboStory\|GetStoryScroll\b', invalid)
    assert match, invalid
    old_scroll = invalid[match.start():end_form(invalid, match.start())]
    help_scroll = g.get('HowToScroll', g.get('HowToPlay'), prefer='WBPHowToPlay|')
    invalid = invalid.replace(old_scroll, f'(select {page6} {help_scroll} {old_scroll})')
    # Keep repeated keys at the content boundary, so Up reacts immediately.
    offset_end = g.call('GetScrollOffsetOfEnd', target=help_scroll)
    search = '(Math|Float|Max(Float) '
    cursor = 0
    while True:
        offset_start = invalid.find(search, cursor)
        if offset_start < 0:
            break
        end_offset = end_form(invalid, offset_start)
        offset = invalid[offset_start:end_offset]
        bounded = g.call('Clamp(Float)', Value=offset, Min=0.0, Max=offset_end)
        replacement = f'(select {page6} {bounded} {offset})'
        invalid = invalid[:offset_start] + replacement + invalid[end_offset:]
        cursor = offset_start + len(replacement)
    g.write(code[:start] + invalid + code[end:], 'HowToPlay_Keys')
    shell.compile()
    frontend = Blueprint(m, 'BP_QuasicomboFrontend')
    g = Graph(m, ref(frontend.asset['refPath'] + ':FrontendBack'))
    back_code = original(frontend.name, 'FrontendBack').replace('(CallFunction|SetFrontendScreen ',
                                                              '(CallFunction|SetFrontendScreen :ScreenIndex ')
    extra = f'(:0 {g.call("SetFrontendScreen",ScreenIndex=3)}) (:4 {g.call("SetFrontendScreen",ScreenIndex=3)}) (:5 {g.call("SetFrontendScreen",ScreenIndex=3)}) (:6 {g.call("SetFrontendScreen",ScreenIndex=0)})'
    g.write(insert_case(back_code, 3, extra), 'HowToPlay_Back')
    frontend.compile()
    page_asset = Blueprint(m, 'WBP_HowToPlay').asset
    script = BATCH_PRELUDE + '\nBP=' + repr(page_asset) + '\n'
    script += '''
def run():
    tree=call('UMGToolSet.UMGToolSet.GetWidgets',{'widgetBlueprint':BP})
    for widget in tree['widgets']:
        if widget['widgetName'] in ['GameplayInstructions','BusterAndQTEHelp','ScrollingHelp']:
            set_props(widget['widget'],{'font':{'typefaceFontName':'Regular'}})
    return {'bodyTypeface':'Regular'}
'''
    print(m.batch(script), flush=True)
    for name in NAMES:
        bp = Blueprint(m, name)
        bp.compile()
        bp.save()


def main():
    m = UnrealMCP()
    for toolset in [B.rstrip('.'), A.rstrip('.'), 'UMGToolSet.UMGToolSet',
                    'editor_toolset.toolsets.object.ObjectTools',
                    'editor_toolset.toolsets.programmatic.ProgrammaticToolset']:
        m.describe(toolset)
    m.call('editor_toolset.toolsets.programmatic.ProgrammaticToolset.get_execution_environment')
    assert not m.call('EditorToolset.EditorAppToolset.IsPIERunning'), 'Stop PIE before authoring menu assets'
    if not (REPORT / 'Before/WBP_QuasicomboMenu_ShowFrontendPage.dsl').exists():
        if 'HowToPlay' in widgets(m, Blueprint(m, 'WBP_QuasicomboMenu')):
            print('How to Play is already integrated; migration skipped. Edit the page in UMG Designer.')
            return
    snapshot(m)
    if '--graphs' not in sys.argv:
        layout(m)
    graphs(m)
    print('How to Play assets authored and saved', flush=True)


if __name__ == '__main__':
    main()
