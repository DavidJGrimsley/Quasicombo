"""Capture every editable menu page at 720p, 1080p and ultrawide through MCP."""
import base64
import json
import re
import time
from mcp_client import UnrealMCP,ref,REPORT

m=UnrealMCP()
S='SlateInspectorToolset.SlateInspectorToolset.'
settings=ref('/Script/UnrealEd.Default__LevelEditorPlaySettings')
original=m.get(settings,'newWindowWidth','newWindowHeight','newWindowPosition')
records=[]

def view():
    snap=m.call(S+'Snapshot',ref='',maxDepth=45)
    return snap[snap.find('window "Quasicombo Preview'):]

def button(label,occurrence=0):
    matches=re.findall(r'button "'+re.escape(label)+r'"[^\n]*\[ref=(\w+)\]',view())
    assert matches, 'Missing '+label
    assert m.call(S+'Click',ref=matches[occurrence])
    time.sleep(.2)

for width,height,label in [(1280,720,'720p'),(1920,1080,'1080p'),(2560,1080,'ultrawide')]:
    if m.call('EditorToolset.EditorAppToolset.IsPIERunning'):
        m.call('EditorToolset.EditorAppToolset.StopPIE')
    m.set(settings,newWindowWidth=width,newWindowHeight=height,newWindowPosition={'x':50,'y':50})
    m.call('EditorToolset.EditorAppToolset.StartPIE',options={'bSimulate':False,'playMode':'PlayMode_InEditorFloating','warmupSeconds':2})
    m.call(S+'Observe',ref='',maxDepth=45)
    time.sleep(.2)
    def capture(page):
        text=view()
        win=re.search(r'\[ref=(w\d+)\]',text)[1]
        result=m.call(S+'Screenshot',ref=win)
        file=REPORT/(page+'-'+label+'.png')
        file.write_bytes(base64.b64decode(result['data']))
        records.append(dict(width=width,height=height,page=page,image=str(file)))
        print(page,label,flush=True)
    capture('story')
    for _ in range(3): button('Continue')
    capture('story-final')
    button('Skip');time.sleep(2);capture('home')
    button('Customize');capture('customize')
    button('Cyan',0);button('Blue',1);capture('customize-swatches')
    # Verify real mouse swatch handlers updated the displayed RGB controls.
    director=ref('/Game/Maps/UEDPIE_0_Lvl_Main.Lvl_Main:PersistentLevel.BP_QuasicomboFrontend_C_0')
    shell=m.get(director,'menuShell')['menuShell']
    custom=m.get(shell,'customization')['customization']
    for slot,rgb in [('body',(85,215,230)),('light',(80,123,255))]:
        for channel,expected in zip('rgb',rgb):
            slider=m.get(custom,slot+channel.upper())[slot+channel.upper()]
            value=m.get(slider,'value')['value']
            assert abs(value-expected/255)<.0001, (slot,channel,value,expected)
    if label!='ultrawide': button('Reset colors')
    button('Back');button('Settings');capture('settings')
    button('Game info');capture('info')
    button('Back');button('Back')
    m.call('EditorToolset.EditorAppToolset.StopPIE')
m.set(settings,**original)
(REPORT/'layout-tests.json').write_text(json.dumps(records,indent=2))
