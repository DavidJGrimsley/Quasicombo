"""Real mouse drag, keyboard and gamepad slider-save checks in customization."""
import json,re,time
from mcp_client import UnrealMCP,ROOT,REPORT
m=UnrealMCP();S='SlateInspectorToolset.SlateInspectorToolset.'
checks=[]
def tree():
    s=m.call(S+'Snapshot',ref='',maxDepth=45)
    return s[s.find('window "Quasicombo Preview'):]
def click(label):
    r=re.search(r'button "'+re.escape(label)+r'"[^\n]*\[ref=(\w+)\]',tree())[1]
    m.call(S+'Click',ref=r);time.sleep(.2)
def probe(focus=''):
    (REPORT/'slider-focus.txt').write_text(focus)
    all_tree=m.call(S+'Snapshot',ref='',maxDepth=45)
    main=all_tree[all_tree.find('window "Braided_Quanta2026 - Unreal Editor"'):].split('\nwindow ',1)[0]
    console=re.findall(r'textbox[^\n]*\[ref=(\w+)\]',main)[2]
    m.call(S+'Click',ref=console)
    m.call(S+'Type',ref=console,text='py "'+(ROOT/'Project/MenuTools/probe_slider_save.py').as_posix()+'"',submit=True)
    time.sleep(.1)
    return json.loads((REPORT/'slider-probe.json').read_text())
def key(k): m.call(S+'PressKey',key=k);time.sleep(.1)
def saved(d):
    for a,b in [('body','saved_body'),('lights','saved_lights')]:
        assert all(abs(x-y)<.0001 for x,y in zip(d[a],d[b])),d
    assert abs(d['music']-d['saved_music'])<.0001,d
def record(label,d):
    saved(d);checks.append({'check':label,'state':d});print(label,flush=True)
t=tree()
sliders=re.findall(r'slider[^\n]*\[ref=(\w+)\]',t)
assert len(sliders)==6
# Drag the R control toward the numeric value label to exercise capture end.
target=re.search(r'slider[^\n]*\[ref='+sliders[0]+r'\][\s\S]*?text[^\n]*\[ref=(\w+)\]',t)[1]
m.call(S+'Drag',startRef=sliders[0],endRef=target)
record('Mouse RGB drag applies and saves on release',probe('BodyG'))
before=probe('BodyG');key('Enter');key('Left');key('Enter');after=probe()
assert after['body_g']<before['body_g'],(before,after)
record('Keyboard RGB adjustment applies and saves',after)
click('Back');click('Settings')
before=probe('Music');key('Gamepad_FaceButton_Bottom');key('Gamepad_DPad_Right');key('Gamepad_FaceButton_Bottom')
after=probe();assert after['music']>before['music'],(before,after)
record('Gamepad audio adjustment applies and saves',after)
(REPORT/'slider-tests.json').write_text(json.dumps(checks,indent=2))
