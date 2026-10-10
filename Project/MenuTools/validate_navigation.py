"""Exercise real Slate mouse, keyboard and gamepad events through Unreal MCP."""
import base64
import json
import re
import time
from mcp_client import UnrealMCP, ref, REPORT, ROOT

m = UnrealMCP()
S = 'SlateInspectorToolset.SlateInspectorToolset.'
d = ref('/Game/UI/Menu/UEDPIE_0_Lvl_QuasicomboIntro.Lvl_QuasicomboIntro:PersistentLevel.BP_QuasicomboFrontend_C_0')
checks = []

def state():
    return m.get(d, 'menuScreen', 'storyPage', 'menuStarting')

def snapshot():
    text = m.call(S+'Snapshot', ref='', maxDepth=45)
    return text[text.find('window "Quasicombo Preview'):]

def click(label):
    text = snapshot()
    match = re.search(r'button "'+re.escape(label)+r'"[^\n]*\[ref=(\w+)\]', text)
    assert match, 'Missing button '+label
    assert m.call(S+'Click', ref=match[1])

def check(label, screen, page=None):
    value = state()
    assert value['menuScreen'] == screen, (label, value)
    if page is not None: assert value['storyPage'] == page, (label, value)
    checks.append(dict(check=label, state=value))
    print(label, value, flush=True)

def key(name):
    m.call(S+'PressKey', key=name)
    time.sleep(.2)  # Allow Slate to focus the newly visible page before the next input.

def capture(name):
    match = re.search(r'\[ref=(w\d+)\]', snapshot())
    result = m.call(S+'Screenshot', ref=match[1])
    (REPORT/(name+'.png')).write_bytes(base64.b64decode(result['data']))

m.call(S+'Observe', ref='', maxDepth=45)
check('Launch story', 1, 0)
capture('story-720p')
def story_offset():
    all_text=m.call(S+'Snapshot',ref='',maxDepth=45)
    console=re.findall(r'textbox[^\n]*\[ref=(\w+)\]',all_text)[2]
    m.call(S+'Click',ref=console)
    m.call(S+'Type',ref=console,text='py "'+str(ROOT/'Project/MenuTools/read_story_scroll.py').replace('\\','/')+'"',submit=True)
    time.sleep(.15)
    return float((REPORT/'story-offset.txt').read_text())
key('Down');key('Gamepad_DPad_Up')
check('Story reading navigation keeps its page',1,0)
key('Enter'); check('Keyboard Continue', 1, 1)
key('Gamepad_FaceButton_Bottom'); check('Gamepad Continue', 1, 2)
key('Enter'); check('Final question has its own page', 1, 3)
capture('story-final-720p')
key('Enter');time.sleep(2)
d=ref('/Game/Maps/UEDPIE_0_Lvl_Main.Lvl_Main:PersistentLevel.BP_QuasicomboFrontend_C_0')
check('Initial story loads jungle Home', 0)
capture('home-720p')
click('Customize'); check('Mouse customization', 2)
capture('customize-720p')
key('Gamepad_FaceButton_Right'); check('Gamepad Back', 0)
key('Gamepad_DPad_Down'); key('Gamepad_DPad_Down'); key('Gamepad_FaceButton_Bottom')
check('Gamepad opens Settings', 3)
click('Replay story'); check('Replay story opens', 1, 0)
key('Escape'); check('Replay Skip returns to Settings', 3)
click('Replay story');key('Gamepad_FaceButton_Right');check('Gamepad Skip returns to Settings',3)
click('Replay story')
for _ in range(4): key('Enter')
check('Replay completion returns to Settings',3)
click('Game info'); check('Game info opens', 4)
capture('info-720p')
key('Escape'); check('Info Back returns to Settings', 3)
capture('settings-720p')
key('Escape'); check('Settings Back returns to Home', 0)
(REPORT/'navigation-tests.json').write_text(json.dumps(checks,indent=2))
