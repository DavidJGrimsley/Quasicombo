"""UMG navigation, color controls and audio preferences: visual Blueprint graphs."""
import sys
from mcp_client import UnrealMCP, ref
from blueprint_author import Blueprint, B, A, quote as q
from author_behaviors import ASSETS, SWATCHES, linear, resolve_widget, bind_buttons, screen

sys.stdout.reconfigure(encoding='utf-8')


def director(g):
    return g.get('MenuDirector')


def session(g):
    return g.call('GetLoadout')


def save(g):
    return g.call('SavePreferences', target=director(g))


def value(g, name):
    return g.call('GetValue', prefer='Class|Slider|', target=g.get(name))


def set_value(g, name, expression):
    return g.call('SetValue', prefer='Class|Slider|', target=g.get(name), Value=expression)


def number(g, expression):
    return g.call('ToText(Integer)', Value=g.call('Round', prefer='Math|Float|', A=expression), bUseGrouping='false')


def label(g, name, expression):
    return g.call('SetText(Text)', target=g.get(name), InText=expression)


def srgb(g, c):
    return f'(select (<= {c} 0.0031308) (* {c} 12.92) (- (* 1.055 {g.call("Power", Base=c, Exp=1/2.4)}) 0.055))'


def to_linear(g, c):
    return f'(select (<= {c} 0.04045) (/ {c} 12.92) {g.call("Power", Base=f"(/ (+ {c} 0.055) 1.055)", Exp=2.4)})'


def tick_focus(g, buttons, swatches=()):
    stmts = []
    color = lambda r, green, b, a: g.call('MakeColor', prefer='Math|Color|', R=r, G=green, B=b, A=a)
    for name in buttons:
        button = g.get(name)
        focused = f'(or {g.call("HasKeyboardFocus", target=button)} {g.call("IsHovered", target=button)})'
        if name in swatches:
            tint = f'(select {focused} {color(.24,.76,.83,1.0)} {color(.87,.94,.89,1.0)})'
            stmts.append(g.call('SetColorAndOpacity', prefer='Button|', target=button, InColorAndOpacity=tint))
        else:
            tint = f'(select {focused} {color(.18,.58,.65,1.0)} {color(.12,.25,.28,.92)})'
            stmts.append(g.call('SetBackgroundColor', prefer='Button|Appearance|', target=button, InBackgroundColor=tint))
    return '(event UserInterface|EventTick (MyGeometry InDeltaTime) ' + ' '.join(stmts) + ')'


def color_controls(m):
    bp = Blueprint(m, ASSETS[2])
    def apply_body(g, color):
        return g.call('SetMikeBodyColor', target=session(g), Color=color)
    def apply_lights(g, color):
        pawn = g.get('MenuPawn', director(g))
        cannon = g.call('GetBusterCannon', prefer='BusterCannon|', target=pawn)
        return g.call('SetMikeLightColor', target=cannon, Color=color)
    methods = {'Body': apply_body, 'Light': apply_lights}
    handlers = {'Reset': lambda g: g.call('ResetColors', target=director(g)),
                'Back': lambda g: g.call('FrontendBack', target=director(g))}
    swatches = []
    for slot, method in methods.items():
        for i, (_, hex_value) in enumerate(SWATCHES):
            name = slot + 'Swatch' + str(i); swatches.append(name)
            color = linear(hex_value)
            color_literal = q('(R={r},G={g},B={b},A=1)'.format(**color))
            handlers[name] = lambda g, method=method, color=color_literal: f'{method(g, color)} {g.call("RefreshAppearance")} {save(g)}'
    g, forms = bind_buttons(bp, handlers)
    for slot in methods:
        for channel in 'RGB':
            name = slot + channel
            changed = bp.bind(name, 'OnValueChanged', 'Slider')
            forms.append(f'(event {changed} (Value) (if (not {g.get("RefreshingControls")}) {g.call("Apply" + slot + "Sliders")}))')
            for event in ['OnMouseCaptureEnd', 'OnControllerCaptureEnd']:
                bound = bp.bind(name, event, 'Slider')
                forms.append(f'(event {bound} {save(g)})')
    forms.append(tick_focus(g, ['Reset', 'Back'] + swatches, swatches))
    g.write('\n'.join(forms), bp.name + '_Events')
    g = bp.function('RefreshAppearance')
    body = [g.set('RefreshingControls', 'true')]
    for slot in methods:
        color = (g.call('GetMikeBodyRegionColor', target=session(g), Region=q('ColorG')) if slot == 'Body'
                 else g.call('GetMikeLightColor', target=session(g)))
        components = [slot + channel for channel in 'RGBA']
        body.append(f'(bind ({" ".join(components)}) {g.call("BreakColor", prefer="Math|Color|", InColor=color)})')
        for channel in 'RGB':
            component = srgb(g, slot + channel)
            body.append(set_value(g, slot + channel, component))
            body.append(label(g, slot + channel + 'Value', number(g, f'(* {component} 255.0)')))
    body.append(g.set('RefreshingControls', 'false'))
    g.write('(fn RefreshAppearance () ' + ' '.join(body) + ')', bp.name + '_Refresh')
    for slot, method in methods.items():
        function_name = 'Apply' + slot + 'Sliders'
        g = bp.function(function_name)
        components = {}
        for channel in 'RGB':
            quantized = f'(/ {g.call("Round", prefer="Math|Float|", A=f"(* {value(g, slot + channel)} 255.0)")} 255.0)'
            components[channel] = to_linear(g, quantized)
        color = g.call('MakeColor', prefer='Math|Color|', R=components['R'], G=components['G'], B=components['B'], A=1.0)
        labels = ' '.join(label(g, slot + channel + 'Value', number(g, f'(* {value(g, slot + channel)} 255.0)')) for channel in 'RGB')
        g.write(f'(fn {function_name} () {method(g, color)} {labels})', bp.name + '_' + function_name)
    bp.compile(); bp.save()


def audio_controls(m):
    bp = Blueprint(m, ASSETS[3])
    bp.function('RefreshAudioLabels')
    def replay(g):
        d = director(g)
        intro = g.get('IntroComponent', d)
        return f'''{g.set('StoryReplay', 'true', d)} {g.set('StoryPage', 0, d)}
          {g.call('SetFrontendScreen', target=d, ScreenIndex=1)}
          {g.call('ReplayIntro', target=d)}'''
    g, forms = bind_buttons(bp, {'ReplayStory': replay, 'GameInfo': screen(4),
                               'Back': lambda g: g.call('FrontendBack', target=director(g))})
    for name in ['Music', 'Ambience']:
        event = bp.bind(name, 'OnValueChanged', 'Slider')
        forms.append(f'(event {event} (Value) (if (not {g.get("RefreshingControls")}) {g.call("ApplyFrontendAudio")}))')
        for event in ['OnMouseCaptureEnd', 'OnControllerCaptureEnd']:
            bound = bp.bind(name, event, 'Slider')
            forms.append(f'(event {bound} {save(g)})')
    forms.append(tick_focus(g, ['ReplayStory', 'GameInfo', 'Back']))
    g.write('\n'.join(forms), bp.name + '_Events')
    g = bp.function('RefreshAudio')
    values = [('Music', g.call('GetUserMusicVolume', target=session(g))),
              ('Ambience', g.call('GetUserAmbienceVolume', target=session(g)))]
    setters = ' '.join(set_value(g, name, expression) for name, expression in values)
    g.write(f'(fn RefreshAudio () {g.set("RefreshingControls", "true")} {setters} {g.set("RefreshingControls", "false")} {g.call("RefreshAudioLabels")})', bp.name + '_Refresh')
    g = bp.function('RefreshAudioLabels')
    labels = ' '.join(label(g, name + 'Value', g.call('Append', prefer='Utilities|String|', A=number(g, f'(* {value(g, name)} 100.0)'), B=q('%'))) for name in ['Music', 'Ambience'])
    g.write(f'(fn RefreshAudioLabels () {labels})', bp.name + '_Labels')
    g = bp.function('ApplyFrontendAudio')
    g.write(f'''(fn ApplyFrontendAudio ()
      {g.call('SetUserAudioVolumes', target=session(g), Music=value(g, 'Music'), Ambience=value(g, 'Ambience'))}
      {g.call('UpdateIntroVolume', target=director(g))} {g.call('RefreshAudioLabels')})''', bp.name + '_Apply')
    bp.compile(); bp.save()


def shell_controls(m):
    bp = Blueprint(m, ASSETS[5])
    bp.variables({'PendingFocus': 'bool', 'FocusDelay': 'float'})
    bp.function('FocusActivePage')
    bp.compile()
    bp.defaults({'bIsFocusable':True})
    g = bp.events()
    g.write('(event UserInterface|EventConstruct ' + resolve_widget(g) + ')\n' +
            f'''(event UserInterface|EventTick (MyGeometry InDeltaTime)
              (if {g.get('PendingFocus')}
                {g.set('FocusDelay', f'(+ {g.get("FocusDelay")} InDeltaTime)')}
                (if (> {g.get('FocusDelay')} 0.1)
                  {g.call('FocusActivePage')} {g.set('PendingFocus', 'false')})))''', bp.name + '_Events')
    g = bp.function('ShowFrontendPage', {'PageIndex': 'int'})
    forms = [g.call('ConfigureStoryLayout',PageIndex='PageIndex'),g.set('PendingFocus','true'), g.set('FocusDelay',0),
             g.call('SetActiveWidgetIndex', prefer='Switcher|', target=g.get('Pages'), Index='PageIndex')]
    cases = []
    for i, page, first, refresh in [(0, 'Home', 'Start', None), (1, 'Story', 'Continue', 'RefreshStory'),
            (2, 'Customization', 'BodySwatch0', 'RefreshAppearance'),
            (3, 'Settings', 'ReplayStory', 'RefreshAudio'), (4, 'GameInfo', 'Back', None)]:
        child = g.get(page)
        body = (g.call(refresh, target=child) if refresh else '')
        if i==1:
            body=g.call('SetScrollOffset',prefer='Widget|SetScrollOffset',
                        target=g.get('StoryScroll',child,prefer='WBPQuasicomboStory|'),NewScrollOffset=0.0)+' '+body
        preferred = ASSETS[i].replace('_', '') + '|'
        body += ' ' + g.call('SetKeyboardFocus', target=g.get(first, child, prefer=preferred))
        cases.append(f'(:{i if i < 4 else "Default"} {body})')
    forms.append('(switch int PageIndex ' + ' '.join(cases) + ')')
    g.write('(fn ShowFrontendPage (PageIndex) ' + ' '.join(forms) + ')', bp.name + '_Page')
    g = bp.function('FocusActivePage')
    cases=[]
    for i,page,first in [(0,'Home','Start'),(1,'Story','Continue'),(2,'Customization','BodySwatch0'),
                          (3,'Settings','ReplayStory'),(4,'GameInfo','Back')]:
        child=g.get(page)
        preferred=ASSETS[i].replace('_','')+'|'
        focus=g.call('SetUserFocus', prefer='Widget|SetUserFocus', target=g.get(first,child,prefer=preferred),
                     PlayerController=g.call('GetOwningPlayer',target='self'))
        cases.append(f'(:{i if i < 4 else "Default"} {focus})')
    g.write('(fn FocusActivePage () (switch int '+g.call('GetActiveWidgetIndex',target=g.get('Pages'))+
            ' '+' '.join(cases)+'))', bp.name+'_Focus')
    g = bp.function('OnPreviewKeyDown')
    action = g.call('GetKeyEventNavigationAction', InKeyEvent='InKeyEvent')
    switch_type, pins = g.node('SwitchonEUINavigationAction')
    branches = []
    for pin in pins['output_pins']:
        if pin['type_id'] != 'Exec':
            continue
        name = pin['name']
        short = name.split('::')[-1]
        body = f'(return {g.call("Unhandled")})'
        if short == 'Back':
            body = f'{g.call("FrontendBack", target=director(g))} (return {g.call("Handled")})'
        elif short == 'Accept':
            body = f'''(if (== {g.get('MenuScreen', director(g))} 1)
                {g.call('StoryNext', target=director(g))} (return {g.call('Handled')})
                (else (return {g.call('Unhandled')})))'''
        elif short == 'Invalid':
            scroll=g.get('StoryScroll',g.get('Story'),prefer='WBPQuasicomboStory|')
            direction=g.call('GetKeyEventNavigationDirection',InKeyEvent='InKeyEvent')
            nav_type,nav_pins=g.node('SwitchonEUINavigation')
            nav_cases=[]
            for nav in nav_pins['output_pins']:
                if nav['type_id']!='Exec': continue
                nav_name=nav['name']; nav_short=nav_name.split('::')[-1]
                reply=f'(return {g.call("Unhandled")})'
                if nav_short in ['Up','Down']:
                    offset=f'({"-" if nav_short=="Up" else "+"} {g.call("GetScrollOffset",prefer="Widget|GetScrollOffset",target=scroll)} 80.0)'
                    offset=g.call('Max(Float)',prefer='Math|Float|',A=offset,B=0.0)
                    reply=g.call('SetScrollOffset',prefer='Widget|SetScrollOffset',target=scroll,NewScrollOffset=offset)+f' (return {g.call("Handled")})'
                nav_cases.append(f'(:{nav_name} {reply})')
            body=f'''(if (== {g.get('MenuScreen',director(g))} 1)
                (switch {nav_type} {direction} {' '.join(nav_cases)})
                (else (return {g.call('Unhandled')})))'''
        branches.append(f'(:{name} {body})')
    g.write(f'(fn OnPreviewKeyDown (MyGeometry InKeyEvent) (switch {switch_type} {action} {" ".join(branches)}))', bp.name + '_Keys')
    bp.compile(); bp.save()


def focus_controls(m):
    definitions = [
        (ASSETS[0], {'Start': lambda g: g.call('StartFrontendRun', target=director(g)),
                     'Customize': screen(2), 'Settings': screen(3)}),
        (ASSETS[1], {'Continue': lambda g: g.call('StoryNext', target=director(g)),
                     'Skip': lambda g: g.call('StorySkip', target=director(g))}),
        (ASSETS[4], {'Back': screen(3)}),
    ]
    for name, handlers in definitions:
        bp = Blueprint(m, name)
        g, forms = bind_buttons(bp, handlers)
        if name==ASSETS[1]:
            colors=[]
            for button in handlers:
                focus=g.call('HasKeyboardFocus',target=g.get(button))
                hover=g.call('IsHovered',target=g.get(button))
                cyan=g.call('MakeColor',prefer='Math|Color|',R=.24,G=.76,B=.83,A=1)
                pale=g.call('MakeColor',prefer='Math|Color|',R=.7,G=.7,B=.7,A=1)
                tint=g.call('MakeSlateColor',SpecifiedColor=f'(select (or {focus} {hover}) {cyan} {pale})')
                colors.append(g.call('SetColorAndOpacity',prefer='Class|Text|',target=g.get(button+'Text'),InColorAndOpacity=tint))
            forms.append('(event UserInterface|EventTick (MyGeometry InDeltaTime) '+' '.join(colors)+')')
        else:
            forms.append(tick_focus(g, list(handlers)))
        g.write('\n'.join(forms), name + '_Events')
        bp.compile(); bp.save()
    # Keyboard slider adjustment saves when the key is released, too.
    for name in [ASSETS[2], ASSETS[3]]:
        bp = Blueprint(m, name)
        g = bp.function('OnKeyUp')
        g.write(f'(fn OnKeyUp (MyGeometry InKeyEvent) {save(g)} (return {g.call("Unhandled")}))', name + '_KeyUp')
        bp.compile(); bp.save()


def main():
    m = UnrealMCP()
    for toolset in [B.rstrip('.'), A.rstrip('.'), 'UMGToolSet.UMGToolSet', 'editor_toolset.toolsets.object.ObjectTools']:
        m.describe(toolset)
    stages = {'colors': color_controls, 'audio': audio_controls, 'shell': shell_controls, 'focus': focus_controls}
    for stage in sys.argv[1:] or stages:
        stages[stage](m)


if __name__ == '__main__':
    main()
