"""Readable named swatches; preserve all other Designer properties."""
import json
from mcp_client import UnrealMCP
from author_layouts import asset, SWATCHES, BATCH_PRELUDE, ASSETS

m = UnrealMCP()
for toolset in ['UMGToolSet.UMGToolSet', 'editor_toolset.toolsets.object.ObjectTools',
                'editor_toolset.toolsets.asset.AssetTools',
                'editor_toolset.toolsets.programmatic.ProgrammaticToolset']:
    m.describe(toolset)
m.call('editor_toolset.toolsets.programmatic.ProgrammaticToolset.get_execution_environment')
bp = asset('WBP_NYSQCustomization')
widgets = m.call('UMGToolSet.UMGToolSet.GetWidgets', widgetBlueprint=bp)
by_name = {item['widgetName']: item['widget'] for item in widgets['widgets']}
patches = []
for slot, index in [('Body',4), ('Light',11)]:
    second = slot+'SwatchesSecond'
    if second not in by_name:
        row=m.call('UMGToolSet.UMGToolSet.AddWidget',widgetBlueprint=bp,
                   widgetClass={'refPath':'/Script/UMG.HorizontalBox'},widgetDisplayName=second,
                   parentWidget=by_name['Root'],childIndex=index)
        by_name[second]=row['widget']
        for i in range(4,8):
            moved=m.call('UMGToolSet.UMGToolSet.MoveWidget',widgetBlueprint=bp,
                         widget=by_name[f'{slot}Swatch{i}'],newParent=row['widget'])
            patches.append((moved['slot'],{'padding':{'right':8,'bottom':12}}))
for slot in ['Body', 'Light']:
    for i, (name, _) in enumerate(SWATCHES):
        patches += [(by_name[f'{slot}SwatchLabel{i}'], {'text': name, 'font': {'size': 13}}),
                    (by_name[f'{slot}SwatchSize{i}'], {'widthOverride': 56})]
script = BATCH_PRELUDE + '\nPATCHES=json.loads(' + repr(json.dumps(patches)) + ')\n'
script += '''
def run():
    for obj,patch in PATCHES: set_props(obj,patch)
    return {'updated':len(PATCHES)}
'''
print(m.batch(script))
m.call('UMGToolSet.UMGToolSet.CompileWidgetBlueprint', widgetBlueprint=bp)
m.call('editor_toolset.toolsets.asset.AssetTools.save_assets', asset_paths=[bp['refPath']])

# Keep narrative controls visible even when a paragraph needs scrolling.
bp = asset('WBP_QuasicomboStory')
widgets = m.call('UMGToolSet.UMGToolSet.GetWidgets', widgetBlueprint=bp)['widgets']
indexed = {item['widgetName']: item for item in widgets}
if 'StoryScroll' not in indexed:
    scroll = m.call('UMGToolSet.UMGToolSet.AddWidget', widgetBlueprint=bp,
                   widgetClass={'refPath':'/Script/UMG.ScrollBox'}, widgetDisplayName='StoryScroll',
                   parentWidget=indexed['Root']['widget'], childIndex=4)
    m.call('UMGToolSet.UMGToolSet.ToggleWidgetAsVariable', widgetBlueprint=bp, widget=scroll['widget'], bIsVariable=True)
    m.call('UMGToolSet.UMGToolSet.MoveWidget', widgetBlueprint=bp, widget=indexed['StoryText']['widget'],
           newParent=scroll['widget'])
else:
    scroll = indexed['StoryScroll']
patches = [(scroll['slot'], {'size': {'sizeRule':'Fill','value':1}, 'padding':{'bottom':18}})]
for name in ASSETS[:-1]:
    for item in m.call('UMGToolSet.UMGToolSet.GetWidgets', widgetBlueprint=asset(name))['widgets']:
        if item['widgetClassPath']['refPath'] == '/Script/UMG.TextBlock' and item['widgetName'] in [
            'StoryText','Overview','Controls','Help','BodyHeading','LightHeading']:
            patches.append((item['widget'], {'font':{'typefaceFontName':'Regular',
                              **({'size':22} if item['widgetName']=='StoryText' else {})}}))
script = BATCH_PRELUDE + '\nPATCHES=json.loads(' + repr(json.dumps(patches)) + ')\n'
script += '''
def run():
    for obj,patch in PATCHES: set_props(obj,patch)
    return {'updated':len(PATCHES)}
'''
print(m.batch(script))
for name in ASSETS:
    b=asset(name)
    m.call('UMGToolSet.UMGToolSet.CompileWidgetBlueprint', widgetBlueprint=b)
    m.call('editor_toolset.toolsets.asset.AssetTools.save_assets', asset_paths=[b['refPath']])
