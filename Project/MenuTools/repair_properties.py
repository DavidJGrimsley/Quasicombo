"""Reapply the menu's intended properties after correcting JSON serialization.

Only for the initial menu implementation; do not run over later Designer edits.
"""
import json
from mcp_client import UnrealMCP, ref
from author_layouts import layouts, asset, BATCH_PRELUDE
from author_behaviors import setup, DCLASS

m = UnrealMCP()
for name in ['UMGToolSet.UMGToolSet', 'editor_toolset.toolsets.object.ObjectTools',
             'editor_toolset.toolsets.asset.AssetTools', 'editor_toolset.toolsets.blueprint.BlueprintTools',
             'editor_toolset.toolsets.programmatic.ProgrammaticToolset']:
    m.describe(name)
m.call('editor_toolset.toolsets.programmatic.ProgrammaticToolset.get_execution_environment')
for layout in layouts():
    bp = asset(layout.name)
    widgets = m.call('UMGToolSet.UMGToolSet.GetWidgets', widgetBlueprint=bp)['widgets']
    indexed = {v['widgetName']: v for v in widgets}
    patches = []
    for item in layout.items:
        info = indexed[item['name']]
        if item['props']: patches.append((info['widget'], item['props']))
        if item['slot']: patches.append((info['slot'], item['slot']))
    script = BATCH_PRELUDE + '\nPATCHES=json.loads(' + repr(json.dumps(patches)) + ')\n'
    script += '''
def run():
    for obj,patch in PATCHES: set_props(obj,patch)
    return {'updated':len(PATCHES)}
'''
    print(layout.name, m.batch(script), flush=True)
    m.call('UMGToolSet.UMGToolSet.CompileWidgetBlueprint', widgetBlueprint=bp)
    m.call('editor_toolset.toolsets.asset.AssetTools.save_assets', asset_paths=[bp['refPath']])
director, prefs, widgets = setup(m)
for bp in [director, prefs, *widgets.values()]:
    bp.save()
story = widgets['WBP_QuasicomboStory']
pages = m.get(m.call('editor_toolset.toolsets.blueprint.BlueprintTools.get_default_object', blueprint=story.asset),
              *[f'narrativePage{i}' for i in range(4)])
m.set(ref('/Game/UI/Menu/WBP_QuasicomboMenu.WBP_QuasicomboMenu:WidgetTree.Story'), **pages)
widgets['WBP_QuasicomboMenu'].compile(); widgets['WBP_QuasicomboMenu'].save()
gm = ref('/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingGameMode.BP_SideScrollingGameMode')
obj = m.call('editor_toolset.toolsets.blueprint.BlueprintTools.get_default_object', blueprint=gm)
m.set(obj, frontendDirectorClass=ref(DCLASS))
m.call('editor_toolset.toolsets.blueprint.BlueprintTools.compile_blueprint', blueprint=gm)
print('Director class', m.get(obj, 'frontendDirectorClass'), flush=True)
m.call('editor_toolset.toolsets.asset.AssetTools.save_assets', asset_paths=[gm['refPath']])
