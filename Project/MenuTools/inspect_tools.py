import base64
import json
import sys
from mcp_client import UnrealMCP, REPORT, ref

sys.stdout.reconfigure(encoding='utf-8')
m = UnrealMCP()
REPORT.mkdir(parents=True, exist_ok=True)
toolsets = ['editor_toolset.toolsets.blueprint.BlueprintTools',
            'editor_toolset.toolsets.object.ObjectTools',
            'editor_toolset.toolsets.asset.AssetTools', 'UMGToolSet.UMGToolSet',
            'SlateInspectorToolset.SlateInspectorToolset',
            'EditorToolset.EditorAppToolset',
            'ConfigSettingsToolset.ConfigSettingsToolset',
            'editor_toolset.toolsets.programmatic.ProgrammaticToolset']
for toolset in toolsets:
    m.describe(toolset)
(REPORT / 'tool_schemas.json').write_text(json.dumps(m.schemas, indent=2), encoding='utf-8')
print('Cached', len(m.schemas), 'registered MCP tool schemas')
for name, schema in m.schemas.items():
    if any(name.endswith('.' + n) for n in ['Type', 'PressKey', 'create', 'GetSections', 'GetSettings']):
        print(json.dumps(schema, indent=2))
for path in ['/Game/Maps/Lvl_Main', '/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingGameMode']:
    print(path, 'dirty:', m.call('editor_toolset.toolsets.asset.AssetTools.is_dirty', asset_path=path))
print('Open assets:', m.call('EditorToolset.EditorAppToolset.GetOpenAssets'))
print('BP GameMode graph:', m.call('editor_toolset.toolsets.blueprint.BlueprintTools.read_graph_dsl', graph=ref('/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingGameMode.BP_SideScrollingGameMode:EventGraph')))
capture = m.call('EditorToolset.EditorAppToolset.CaptureViewport', captureTransform=None, annotations=None)
(REPORT / 'before.png').write_bytes(base64.b64decode(capture['image']['data']))
print('Saved viewport capture', REPORT / 'before.png')
print('Observer:', m.call('SlateInspectorToolset.SlateInspectorToolset.Observe', ref='w1', maxDepth=40))
