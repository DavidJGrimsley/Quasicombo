"""Attach the new director without replacing the GameMode's existing graph."""
import sys
from mcp_client import UnrealMCP, REPORT, ref
from blueprint_author import Blueprint, Graph, B, A
from author_behaviors import DCLASS, DNAME
from author_audio_nodes import author as audio_nodes
from author_director import author_initialize

sys.stdout.reconfigure(encoding='utf-8')
GM = ref('/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingGameMode.BP_SideScrollingGameMode')

if __name__ == '__main__':
    m = UnrealMCP()
    for toolset in [B.rstrip('.'), A.rstrip('.'), 'editor_toolset.toolsets.object.ObjectTools']:
        m.describe(toolset)
    bp = Blueprint(m, DNAME)
    audio_nodes(bp)
    author_initialize(bp)
    bp.compile(); bp.save()
    graph = Graph(m, ref(GM['refPath'] + ':EventGraph'))
    existing = m.call(B + 'read_graph_dsl', graph=graph.graph)
    (REPORT / 'GameMode_before_frontend.dsl').write_text(existing, encoding='utf-8')
    old = '(UserInterface|Viewport|AddToViewport _returnvalue)'
    if old in existing and 'Collapsed' not in existing:
        replacement = f'''(if {graph.call('ShouldShowFrontend')}
          {graph.call('SetVisibility', prefer='Widget|', target='_returnvalue', InVisibility='"Collapsed"')}
          {old}
          (else {old}))'''
        graph.write(existing.replace(old, replacement), 'GameMode_frontend')
    elif 'Collapsed' not in existing:
        raise RuntimeError('The GameMode graph changed; inspect it before wiring its HUD')
    m.call(B + 'compile_blueprint', blueprint=GM)
    obj = m.call(B + 'get_default_object', blueprint=GM)
    m.set(obj, frontendDirectorClass=ref(DCLASS))
    m.call(B + 'compile_blueprint', blueprint=GM)
    m.call(A + 'save_assets', asset_paths=[GM['refPath']])
    print('Frontend director assigned to the existing GameMode')
