"""Read-only PIE observations for the MCP menu validation driver.

Execute in the editor console. Requests and results stay under Saved.
The callback never changes widgets, gameplay, preferences or assets.
"""
import json
import traceback
from pathlib import Path
import unreal

_htp_out = Path(unreal.Paths.project_saved_dir()) / 'HowToPlayValidation'
_htp_out.mkdir(parents=True, exist_ok=True)
_htp_request = _htp_out / 'request.json'
_htp_response = _htp_out / 'response.json'
_htp_last = None
_htp_class = unreal.load_class(None, '/Game/UI/Menu/BP_QuasicomboFrontend.BP_QuasicomboFrontend_C')


def _htp_prop(obj, name):
    return obj.get_editor_property(name)


def _htp_tick(_delta):
    global _htp_last
    if not _htp_request.exists():
        return
    request = json.loads(_htp_request.read_text())
    if request['id'] == _htp_last:
        return
    _htp_last = request['id']
    if request.get('stop'):
        unreal.unregister_slate_post_tick_callback(_htp_callback)
        return
    result = {'id': request['id']}
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        director = unreal.GameplayStatics.get_actor_of_class(world, _htp_class)
        shell = _htp_prop(director, 'MenuShell')
        page = _htp_prop(shell, 'HowToPlay')
        scroll = _htp_prop(page, 'HowToScroll')
        back = _htp_prop(page, 'Back')
        panel = _htp_prop(shell, 'Panel')
        home = _htp_prop(shell, 'Home')
        help_button = _htp_prop(home, 'HowToPlay')
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        anchors = panel.slot.get_anchors()
        alignment = panel.slot.get_alignment()
        color = _htp_prop(help_button, 'BackgroundColor')
        result.update(
            screen=_htp_prop(director, 'MenuScreen'),
            pageIndex=_htp_prop(shell, 'Pages').get_active_widget_index(),
            pageCount=_htp_prop(shell, 'Pages').get_children_count(),
            scroll=scroll.get_scroll_offset(), scrollEnd=scroll.get_scroll_offset_of_end(),
            backFocus=back.has_user_focus(pc), homeHelpFocus=help_button.has_user_focus(pc),
            homeStartFocus=_htp_prop(home, 'Start').has_user_focus(pc),
            anchors=[anchors.minimum.x, anchors.minimum.y, anchors.maximum.x, anchors.maximum.y],
            alignment=[alignment.x, alignment.y],
            paragraph=str(_htp_prop(page, 'GameplayInstructions').get_text()),
            footnote=str(_htp_prop(page, 'BusterAndQTEHelp').get_text()),
            helpColor=[color.r, color.g, color.b, color.a],
            rows=[[str(_htp_prop(page, 'ControlsCell%d_%d' % (i,j)).get_text())
                   for j in range(3)] for i in range(10)],
            bodySize=_htp_prop(_htp_prop(page, 'GameplayInstructions'), 'Font').size,
            bodyTypeface=str(_htp_prop(_htp_prop(page, 'GameplayInstructions'), 'Font').typeface_font_name),
        )
    except Exception:
        result['error'] = traceback.format_exc()
    _htp_response.write_text(json.dumps(result, indent=2), encoding='utf-8')


_htp_callback = unreal.register_slate_post_tick_callback(_htp_tick)
print('How to Play observation callback installed')
