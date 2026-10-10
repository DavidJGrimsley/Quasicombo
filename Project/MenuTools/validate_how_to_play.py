"""MCP PIE navigation and layout validation. Run after author_how_to_play.py.

Assets are never modified. Normal/short screenshots and runtime evidence stay
under Saved/HowToPlayValidation. Original PIE dimensions are restored.
"""
import base64
import json
import re
import time
from mcp_client import UnrealMCP, ROOT, ref
from author_how_to_play import PARAGRAPH, CONTROLS, FOOTNOTE

OUT = ROOT / 'Saved/HowToPlayValidation'
S = 'SlateInspectorToolset.SlateInspectorToolset.'
m = UnrealMCP()
records = []


def snapshot():
    tree = m.call(S + 'Snapshot', ref='', maxDepth=45)
    start = tree.find('window "Quasicombo Preview')
    assert start >= 0, 'PIE preview missing'
    return tree[start:]


def button(label):
    match = re.search(r'button "' + re.escape(label) + r'"[^\n]*\[ref=(\w+)\]', snapshot())
    assert match, 'Missing button ' + label
    assert m.call(S + 'Click', ref=match[1])
    time.sleep(.3)


def key(name, count=1):
    for _ in range(count):
        assert m.call(S + 'PressKey', key=name)
    time.sleep(.2)


def observe(label):
    request_id = str(time.time_ns())
    (OUT / 'request.json').write_text(json.dumps({'id': request_id}))
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        try:
            result = json.loads((OUT / 'response.json').read_text())
            if result['id'] == request_id:
                assert 'error' not in result, result.get('error')
                records.append(dict(check=label, **result))
                (OUT / 'navigation-tests.json').write_text(json.dumps(records, indent=2))
                print(label, 'screen', result['screen'], 'offset', result['scroll'], flush=True)
                return result
        except (FileNotFoundError, json.JSONDecodeError):
            pass
        time.sleep(.1)
    raise TimeoutError('PIE observer: ' + label)


def capture(label):
    win = re.search(r'\[ref=(w\d+)\]', snapshot())[1]
    result = m.call(S + 'Screenshot', ref=win)
    (OUT / (label + '.png')).write_bytes(base64.b64decode(result['data']))


def install_probe():
    (OUT / 'request.json').unlink(missing_ok=True)
    m.call(S + 'Observe', ref='', maxDepth=45)
    tree = m.call(S + 'Snapshot', ref='', maxDepth=45)
    cmd = re.search(r'text "Cmd"[\s\S]*?textbox[^\n]*\[ref=(\w+)\]', tree)[1]
    m.call(S + 'Click', ref=cmd)
    m.call(S + 'PressKey', key='Ctrl+A')
    path = str(ROOT / 'Project/MenuTools/probe_how_to_play.py').replace('\\', '/')
    assert m.call(S + 'Type', ref=cmd, submit=True,
                  text='py exec(compile(open(' + repr(path) + ').read(),' + repr(path) + ',"exec"))')


def stop():
    if m.call('EditorToolset.EditorAppToolset.IsPIERunning'):
        m.call('EditorToolset.EditorAppToolset.StopPIE')
        deadline = time.monotonic() + 10
        while m.call('EditorToolset.EditorAppToolset.IsPIERunning') and time.monotonic() < deadline:
            time.sleep(.2)


def run(width, height, label):
    stop()
    m.set(settings, newWindowWidth=width, newWindowHeight=height, newWindowPosition={'x': 50, 'y': 50})
    m.call('EditorToolset.EditorAppToolset.StartPIE', options={
        'bSimulate': False, 'playMode': 'PlayMode_InEditorFloating', 'warmupSeconds': 2})
    m.call(S + 'Observe', ref='', maxDepth=45)
    if 'button "Skip"' in snapshot():
        button('Skip')
        time.sleep(3)
    button('How to play')
    top = observe(label + ': mouse opens help and focuses Back')
    assert top['screen'] == top['pageIndex'] == 6 and top['pageCount'] == 7
    assert top['backFocus'] and top['scroll'] == 0
    assert top['paragraph'] == PARAGRAPH and top['footnote'] == FOOTNOTE
    assert top['rows'] == [list(r) for r in [('Action', 'Keyboard / mouse', 'Xbox controller'), *CONTROLS]]
    assert top['bodySize'] == 22 and top['bodyTypeface'] == 'Regular'
    assert all(abs(a-b) < .0001 for a,b in zip(top['anchors'], [.08,.06,.92,.94]))
    capture(label + '-top')
    key('Down', 3)
    down = observe(label + ': keyboard Down scrolls')
    assert down['scroll'] > top['scroll']
    key('Up')
    up = observe(label + ': keyboard Up scrolls')
    assert up['scroll'] < down['scroll']
    key('Gamepad_DPad_Down', 12)
    bottom = observe(label + ': D-pad reaches remaining controls and notes')
    assert bottom['scroll'] > up['scroll']
    assert bottom['scroll'] <= bottom['scrollEnd'] + .01
    assert bottom['backFocus']
    capture(label + '-bottom')
    key('Gamepad_DPad_Up')
    moved_up = observe(label + ': D-pad Up scrolls')
    assert moved_up['scroll'] < bottom['scroll']
    assert abs(moved_up['scroll'] - max(0, bottom['scroll'] - 80)) < .01
    capture(label + '-scrolled-up')
    key('Gamepad_FaceButton_Right')
    assert observe(label + ': controller B returns Home')['screen'] == 0
    # Return-to-Home focuses Start. How to Play is the third button.
    key('Down', 2)
    focused = observe(label + ': keyboard navigation highlights How to Play')
    assert focused['homeHelpFocus']
    assert all(abs(a-b) < .001 for a,b in zip(focused['helpColor'], [.18,.58,.65,1]))
    key('Enter')
    reopened = observe(label + ': keyboard Enter opens help and resets scroll')
    assert reopened['screen'] == 6 and reopened['scroll'] == 0 and reopened['backFocus']
    key('Escape')
    assert observe(label + ': Escape returns Home')['screen'] == 0
    key('Gamepad_DPad_Down', 2)
    assert observe(label + ': controller navigation focuses How to Play')['homeHelpFocus']
    key('Gamepad_FaceButton_Bottom')
    assert observe(label + ': controller A opens help')['screen'] == 6
    button('Back')
    assert observe(label + ': mouse Back returns Home')['screen'] == 0
    for entry, index in [('Customize', 2), ('Settings', 3)]:
        button(entry)
        state = observe(label + ': existing ' + entry + ' page')
        assert state['screen'] == index and abs(state['anchors'][0] - .055) < .0001
        if index == 3:
            button('Game info')
            assert observe(label + ': existing Game info')['screen'] == 4
            button('Back')
            button('Replay story')
            assert observe(label + ': existing Story')['screen'] == 1
            button('Skip')
        else:
            button('Back')
    capture(label + '-home')
    print(label, 'navigation and layout passed', flush=True)


if __name__ == '__main__':
    OUT.mkdir(parents=True, exist_ok=True)
    settings = ref('/Script/UnrealEd.Default__LevelEditorPlaySettings')
    original = m.get(settings, 'newWindowWidth', 'newWindowHeight', 'newWindowPosition')
    (OUT / 'play-settings-before.json').write_text(json.dumps(original))
    install_probe()
    try:
        run(1280, 720, 'normal')
        run(1200, 550, 'short')
    finally:
        stop()
        m.set(settings, **original)
        (OUT / 'request.json').write_text(json.dumps({'id': str(time.time_ns()), 'stop': True}))
