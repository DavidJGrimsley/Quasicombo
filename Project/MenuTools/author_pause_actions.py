"""Add pause Restart/Quit controls through the running Unreal Editor's MCP.

Preserve the existing Designer tree, menu functions and preference handling.
"""
import re

from mcp_client import UnrealMCP, ref, REPORT
from blueprint_author import Blueprint, B, quote as q

UMG = 'UMGToolSet.UMGToolSet.'


def refresh_catalogs(m):
    # The MCP graph catalogs retain other Blueprint classes until invalidated.
    REPORT.mkdir(parents=True, exist_ok=True)
    script = REPORT / 'refresh-pause-action-catalogs.py'
    script.write_text('from editor_toolset.toolsets.blueprint import _BlueprintCache\n'
                      '_BlueprintCache._graph_node_types.clear()\n', encoding='utf-8')
    slate = 'SlateInspectorToolset.SlateInspectorToolset.'
    m.call(slate + 'Observe', ref='', maxDepth=45)
    tree = m.call(slate + 'Snapshot', ref='', maxDepth=45)
    window = re.search(r'window "Braided_Quanta2026 - Unreal Editor"[^\n]*\[ref=(\w+)\]', tree)
    assert window, 'The project editor window is unavailable'
    tree = m.call(slate + 'Snapshot', ref=window.group(1), maxDepth=45)
    console = re.search(r'text "Cmd"[\s\S]*?textbox[^\n]*\[ref=(\w+)\]', tree)
    assert console, 'Open the Output Log console before refreshing node catalogs'
    assert m.call(slate + 'Type', ref=console.group(1), text='py "' + script.as_posix() + '"', submit=True)


def remove_action_events(code):
    # Replace our own button handlers on a repeated authoring run.
    for name in ['Restart', 'Quit']:
        match = re.search(r'\(event "OnClicked\(' + name + r'\)"', code)
        if not match:
            continue
        depth, quoted, escaped = 0, False, False
        for end in range(match.start(), len(code)):
            char = code[end]
            if escaped:
                escaped = False
                continue
            if char == '\\' and quoted:
                escaped = True
            elif char == '"':
                quoted = not quoted
            elif not quoted:
                depth += (char == '(') - (char == ')')
                if depth == 0:
                    code = code[:match.start()] + code[end + 1:]
                    break
    return code


def copy_properties(m, source, destination, names, **overrides):
    values = m.get(source, *names)
    values.update(overrides)
    m.set(destination, **values)


def add_button(m, bp, widgets, name, child_index):
    if name in widgets:
        return
    def add(kind, suffix, parent, index=-1):
        info = m.call(UMG + 'AddWidget', widgetBlueprint=bp.asset,
                      widgetClass=ref('/Script/UMG.' + kind),
                      widgetDisplayName=name + suffix, parentWidget=parent,
                      childIndex=index)
        m.call(UMG + 'ToggleWidgetAsVariable', widgetBlueprint=bp.asset,
               widget=info['widget'], bIsVariable=True)
        return info
    button = add('Button', '', widgets['Root']['widget'], child_index)
    copy_properties(m, widgets['Resume']['widget'], button['widget'],
                    ['isFocusable', 'backgroundColor', 'widgetStyle'])
    copy_properties(m, widgets['Resume']['slot'], button['slot'],
                    ['padding', 'horizontalAlignment', 'verticalAlignment', 'size'])
    box = add('SizeBox', 'Box', button['widget'])
    copy_properties(m, widgets['ResumeBox']['widget'], box['widget'],
                    ['heightOverride', 'bOverride_HeightOverride'])
    text = add('TextBlock', 'Text', box['widget'])
    copy_properties(m, widgets['ResumeText']['widget'], text['widget'],
                    ['font', 'colorAndOpacity', 'autoWrapText', 'justification'], text=name)
    copy_properties(m, widgets['ResumeText']['slot'], text['slot'],
                    ['padding', 'horizontalAlignment', 'verticalAlignment'])
    spacer = add('Spacer', 'Space', widgets['Root']['widget'], child_index + 1)
    copy_properties(m, widgets['Space7']['widget'], spacer['widget'], ['size'])


def main():
    m = UnrealMCP()
    director = Blueprint(m, 'BP_QuasicomboFrontend')
    g = director.function('RestartPauseRun')
    controller = g.get('MenuController')
    shell = g.get('MenuShell')
    body = f'''(if (and {g.get('PauseMenuOpen')} (and {g.call('CanPauseRun')} (not {g.get('PauseResuming')})))
      {g.call('SetIsEnabled', target=shell, bInIsEnabled='false')}
      {g.call('SavePreferences')} {g.call('StopIntro')}
      {g.call('SetShouldPerformFullTickwhenPaused', target=controller, bShouldPerformFullTickWhenPaused=g.get('PausePreviousFullTick'))}
      {g.call('SetGamePaused', bPaused='false')}
      {g.call('SetShowMouseCursor', target=controller, bShowMouseCursor='false')}
      {g.call('SetInputModeGameOnly', PlayerController=controller, bFlushInput='true')}
      {g.call('OpenLevel(byName)', LevelName=g.call('GetCurrentLevelName', bRemovePrefixString='true'), bAbsolute='true')})'''
    g.write('(fn RestartPauseRun () ' + body + ')', 'Pause_RestartRun')
    g = director.function('QuitPauseGame')
    body = f'''(if (and {g.get('PauseMenuOpen')} (not {g.get('PauseResuming')}))
      {g.call('SetIsEnabled', target=g.get('MenuShell'), bInIsEnabled='false')}
      {g.call('SavePreferences')} {g.call('StopIntro')}
      {g.call('QuitGame', SpecificPlayer=g.get('MenuController'), QuitPreference=q('Quit'), bIgnorePlatformRestrictions='false')})'''
    g.write('(fn QuitPauseGame () ' + body + ')', 'Pause_QuitGame')
    director.compile(); director.save()

    bp = Blueprint(m, 'WBP_QuasicomboPause')
    widgets = {w['widgetName']: w for w in m.call(UMG + 'GetWidgets', widgetBlueprint=bp.asset)['widgets']}
    add_button(m, bp, widgets, 'Restart', 5)
    add_button(m, bp, widgets, 'Quit', 11)
    bp.compile()
    refresh_catalogs(m)
    g = bp.events()
    code = m.call(B + 'read_graph_dsl', graph=g.graph)
    code = re.sub(r'\(event (On\w+\([^)]*\))', lambda x: '(event ' + q(x.group(1)), code)
    code = remove_action_events(code)
    handlers = []
    for name, action in [('Restart', 'RestartPauseRun'), ('Quit', 'QuitPauseGame')]:
        event = bp.bind(name, 'OnClicked', 'Button')
        handlers.append(f'(event {event} {g.call(action, target=g.get("MenuDirector"))})')
    # Match the existing hover/focus highlight on the two new controls.
    highlights = []
    for name in ['Restart', 'Quit']:
        button = g.get(name)
        active = f'(or {g.call("HasKeyboardFocus", target=button)} {g.call("IsHovered", target=button)})'
        color = f'(select {active} {g.call("MakeColor", prefer="Math|Color|MakeColor", R=.18, G=.58, B=.65, A=1.0)} {g.call("MakeColor", prefer="Math|Color|MakeColor", R=.12, G=.25, B=.28, A=.92)})'
        highlights.append(g.call('SetBackgroundColor', prefer='Button|Appearance|', target=button, InBackgroundColor=color))
    if 'GetRestart)' not in code:
        code = code.replace('(event UserInterface|EventTick (MyGeometry InDeltaTime)',
                            '(event UserInterface|EventTick (MyGeometry InDeltaTime)\n  ' + '\n  '.join(highlights), 1)
    g.write(code + '\n' + '\n'.join(handlers), 'Pause_RestartQuitEvents')
    bp.compile()
    assert m.call(UMG + 'CompileWidgetBlueprint', widgetBlueprint=bp.asset)
    bp.save()

    character = Blueprint(m, 'BP_SideScrollingCharacter')
    character.asset = ref('/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter.BP_SideScrollingCharacter')
    character.cls = ref(character.asset['refPath'] + '_C')
    character.defaults({'HitComboResetDelay': 7.0})
    character.save()
    print('Pause Restart/Quit and seven-second character timeout saved.')


if __name__ == '__main__':
    main()
