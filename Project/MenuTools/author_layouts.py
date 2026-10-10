"""Author editable UMG Designer trees through registered Unreal MCP tools.

Initial creation only: existing menu widgets are preserved. Behavior graphs are
authored separately by author_behaviors.py. No binary asset is edited on disk.
"""
import json
import sys
from mcp_client import UnrealMCP, ROOT, REPORT, ref

sys.stdout.reconfigure(encoding='utf-8')
FOLDER = '/Game/UI/Menu'
ASSETS = ['WBP_QuasicomboHome', 'WBP_QuasicomboStory', 'WBP_NYSQCustomization',
          'WBP_QuasicomboSettings', 'WBP_QuasicomboGameInfo', 'WBP_QuasicomboMenu']
INK = {'r': .025, 'g': .075, 'b': .10, 'a': .82}
PALE = {'r': .87, 'g': .94, 'b': .89, 'a': 1}
CYAN = {'r': .24, 'g': .76, 'b': .83, 'a': 1}
SWATCHES = [('Pink', 'FF4C9F'), ('Amber', 'FFCE48'), ('Coral', 'FF725C'),
            ('Lime', 'AFE35C'), ('Cyan', '55D7E6'), ('Blue', '507BFF'),
            ('Violet', 'AE82EF'), ('Pearl', 'E3EBEF')]


def asset(name):
    return ref(f'{FOLDER}/{name}.{name}')


def cls(name):
    return ref(f'{FOLDER}/{name}.{name}_C')


def linear(hex_value):
    components = [int(hex_value[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    components = [c / 12.92 if c <= .04045 else ((c + .055) / 1.055) ** 2.4 for c in components]
    return dict(zip(('r', 'g', 'b', 'a'), components + [1]))


class Layout:
    def __init__(self, name):
        self.name, self.items = name, []

    def add(self, name, kind, parent=None, props=None, slot=None):
        path = f'/Script/UMG.{kind}' if not kind.startswith('WBP_') else cls(kind)['refPath']
        props = dict(props or {})
        if kind == 'SizeBox':
            for dimension in ['width', 'height']:
                if dimension + 'Override' in props:
                    props['bOverride_' + dimension.capitalize() + 'Override'] = True
        self.items.append(dict(name=name, cls=ref(path), parent=parent, props=props, slot=slot or {}))
        return name

    def spacer(self, parent, height=18):
        self.add(f'Space{len(self.items)}', 'Spacer', parent, {'size': {'x': 1, 'y': height}})

    def text(self, name, text, parent, size=26, display=False, color=None):
        font = {'size': size}
        if display:
            font['fontObject'] = ref('/Game/Quasicombo/Fonts/Drwho42-og50_Font.Drwho42-og50_Font')
        self.add(name, 'TextBlock', parent,
                 {'text': text, 'font': font, 'colorAndOpacity': {'specifiedColor': color or PALE},
                  'autoWrapText': True}, {'padding': {'bottom': 8}})

    def button(self, name, label, parent, size=26):
        button = self.add(name, 'Button', parent,
                          {'isFocusable': True, 'backgroundColor': {'r': .12, 'g': .25, 'b': .28, 'a': .92}},
                          {'padding': {'bottom': 14}, 'horizontalAlignment': 'HAlign_Fill'})
        box = self.add(name + 'Box', 'SizeBox', button, {'heightOverride': 58})
        self.text(name + 'Text', label, box, size, color=PALE)
        return button

    def base(self, heading, title_size=42):
        root = self.add('Root', 'VerticalBox')
        self.text('Heading', heading, root, title_size, display=heading == 'QUASICOMBO')
        self.spacer(root, 20)
        return root

    def color_control(self, name, parent):
        self.text(name + 'Heading', 'Body' if name == 'Body' else 'Lights', parent, 25)
        row = self.add(name + 'Swatches', 'HorizontalBox', parent)
        for i, (label, hex_value) in enumerate(SWATCHES):
            b = self.add(f'{name}Swatch{i}', 'Button', row,
                         {'backgroundColor': linear(hex_value), 'isFocusable': True, 'toolTipText': label},
                         {'padding': {'right': 8, 'bottom': 12}})
            sz = self.add(f'{name}SwatchSize{i}', 'SizeBox', b,
                          {'widthOverride': 48, 'heightOverride': 42})
            self.text(f'{name}SwatchLabel{i}', label[:1], sz, 15)
        for channel in 'RGB':
            row = self.add(name + channel + 'Row', 'HorizontalBox', parent,
                           slot={'padding': {'bottom': 6}})
            label = self.add(name + channel + 'LabelWidth', 'SizeBox', row, {'widthOverride': 30})
            self.text(name + channel + 'Label', channel, label, 20)
            self.add(name + channel, 'Slider', row,
                     {'stepSize': 1 / 255, 'mouseUsesStep': True, 'isFocusable': True,
                      'sliderBarColor': CYAN, 'sliderHandleColor': PALE},
                     {'size': {'sizeRule': 'Fill', 'value': 1},
                      'verticalAlignment': 'VAlign_Center'})
            number = self.add(name + channel + 'ValueWidth', 'SizeBox', row, {'widthOverride': 52},
                              {'padding': {'left': 10}})
            self.text(name + channel + 'Value', '255', number, 20)
        self.spacer(parent, 18)


def layouts():
    result = []
    home = Layout(ASSETS[0]); root = home.base('QUASICOMBO', 68)
    home.spacer(root, 50)
    for name in ['Start', 'Customize', 'Settings']:
        home.button(name, name, root, 32); home.spacer(root, 24)
    home.spacer(root, 26)
    home.text('Help', 'Choose with arrows or the D-pad.\nEnter / A to select.', root, 19)
    result.append(home)
    story = Layout(ASSETS[1]); root = story.base('A way home')
    story.text('PageNumber', '1 / 4', root, 19, color=CYAN)
    story.spacer(root, 20)
    story.text('StoryText', '', root, 25)
    story.spacer(root, 24)
    story.button('Continue', 'Continue', root)
    story.button('Skip', 'Skip', root)
    result.append(story)
    custom = Layout(ASSETS[2]); root = custom.base('Customize NYSQ', 38)
    for name in ['Body', 'Light']:
        custom.color_control(name, root)
    custom.button('Reset', 'Reset colors', root, 24)
    custom.button('Back', 'Back', root, 24)
    result.append(custom)
    settings = Layout(ASSETS[3]); root = settings.base('Settings')
    settings.button('ReplayStory', 'Replay story', root)
    settings.button('GameInfo', 'Game info', root)
    settings.spacer(root)
    for name, label in [('Music', 'Music'), ('Ambience', 'Jungle ambience')]:
        settings.text(name + 'Label', label, root, 25)
        settings.add(name, 'Slider', root,
                     {'value': 1, 'stepSize': .02, 'mouseUsesStep': True, 'isFocusable': True,
                      'sliderBarColor': CYAN, 'sliderHandleColor': PALE})
        settings.text(name + 'Value', '100%', root, 20, color=CYAN)
        settings.spacer(root, 22)
    settings.button('Back', 'Back', root)
    result.append(settings)
    info = Layout(ASSETS[4]); root = info.base('Game info')
    info.text('Overview', 'Guide NYSQ through the jungle to find a way home.\n\nFight through your chosen route, build combos and cross between paths. Each crossing shapes the braid that influences the final boss.', root, 24)
    info.spacer(root, 20)
    info.text('Controls', 'Move     Left / Right · Left stick\nJump     Space · A\nAttack     Left mouse · B\nDash     Left Shift · Right shoulder\nBuster     F / Middle mouse · X\n\nThe Buster Cannon is earned after defeating the boss and stays with NYSQ on replays during this session.', root, 22)
    info.spacer(root, 22); info.button('Back', 'Back', root)
    result.append(info)
    shell = Layout(ASSETS[5])
    canvas = shell.add('Root', 'CanvasPanel')
    panel = shell.add('Panel', 'Border', canvas,
                      {'brushColor': INK, 'padding': {'left': 34, 'top': 30, 'right': 34, 'bottom': 24}},
                      {'layoutData': {'anchors': {'minimum': {'x': .055, 'y': .065},
                                                  'maximum': {'x': .055, 'y': .94}},
                                      'offsets': {'left': 0, 'top': 0, 'right': 640, 'bottom': 0}}})
    vb = shell.add('PanelContent', 'VerticalBox', panel)
    switch = shell.add('Pages', 'WidgetSwitcher', vb,
                       slot={'size': {'sizeRule': 'Fill', 'value': 1}})
    for name, child in zip(['Home', 'Story', 'Customization', 'Settings', 'GameInfo'], ASSETS[:-1]):
        shell.add(name, child, switch)
    shell.text('SaveNotice', '', vb, 18)
    result.append(shell)
    return result


BATCH_PRELUDE = '''import json,re,copy
def call(name,args):
    return execute_tool(name,json.dumps(args)).get('returnValue')
def key(mapping,wanted):
    normalize=lambda s: re.sub('[^a-z0-9]','',s.lower())
    matches=[k for k in mapping if normalize(k)==normalize(wanted)]
    if len(matches)!=1: raise ValueError('Unknown or ambiguous property '+wanted+' among '+str(list(mapping)))
    return matches[0]
def merge(current,patch):
    if isinstance(patch,dict) and set(patch)=={'refPath'}: return patch['refPath']
    if not isinstance(patch,dict): return patch
    result=copy.deepcopy(current) if isinstance(current,dict) else {}
    for k,v in patch.items():
        actual=key(result,k) if result else k
        result[actual]=merge(result.get(actual),v)
    return result
def set_props(obj,patch):
    schema=json.loads(call('editor_toolset.toolsets.object.ObjectTools.list_properties',{'instance':obj}))
    patch={key(schema,k):v for k,v in patch.items()}
    current=json.loads(call('editor_toolset.toolsets.object.ObjectTools.get_properties',{'instance':obj,'properties':list(patch)}))
    values={k:merge(current[k],v) for k,v in patch.items()}
    if not call('editor_toolset.toolsets.object.ObjectTools.set_properties',{'instance':obj,'values':json.dumps(values)}):
        raise ValueError('Property write rejected: '+str(obj)+' '+str(list(values)))
'''


def author(m, layout):
    bp = asset(layout.name)
    existing = m.call('editor_toolset.toolsets.asset.AssetTools.exists', path=bp['refPath'])
    if existing:
        print('Preserving existing', layout.name, flush=True)
        return
    m.call('UMGToolSet.UMGToolSet.CreateWidgetBlueprint', folderPath=FOLDER,
           assetName=layout.name, parentClass=ref('/Script/UMG.UserWidget'))
    # The schemas for every called tool are fetched by main before this batch.
    script = BATCH_PRELUDE + '\nBP=' + repr(bp) + '\nITEMS=json.loads(' + repr(json.dumps(layout.items)) + ')\n'
    script += '''
def run():
    widgets={}
    for item in ITEMS:
        args={'widgetBlueprint':BP,'widgetClass':item['cls'],'widgetDisplayName':item['name']}
        if item['parent']: args['parentWidget']=widgets[item['parent']]['widget']
        info=call('UMGToolSet.UMGToolSet.AddWidget',args)
        widgets[item['name']]=info
        call('UMGToolSet.UMGToolSet.ToggleWidgetAsVariable',{'widgetBlueprint':BP,'widget':info['widget'],'bIsVariable':True})
        if item['props']: set_props(info['widget'],item['props'])
        if item['slot']: set_props(info['slot'],item['slot'])
    call('UMGToolSet.UMGToolSet.CompileWidgetBlueprint',{'widgetBlueprint':BP})
    return {'name':BP['refPath'],'widget_count':len(widgets)}
'''
    print(m.batch(script), flush=True)
    m.call('editor_toolset.toolsets.asset.AssetTools.save_assets', asset_paths=[bp['refPath']])


if __name__ == '__main__':
    m = UnrealMCP()
    for toolset in ['UMGToolSet.UMGToolSet', 'editor_toolset.toolsets.object.ObjectTools',
                    'editor_toolset.toolsets.asset.AssetTools',
                    'editor_toolset.toolsets.programmatic.ProgrammaticToolset']:
        m.describe(toolset)
    m.call('editor_toolset.toolsets.programmatic.ProgrammaticToolset.get_execution_environment')
    for layout in layouts():
        author(m, layout)
