"""Submit a Python authoring or validation script through Unreal MCP's console."""
import re,sys
from pathlib import Path
from mcp_client import UnrealMCP,ROOT
m=UnrealMCP();s='SlateInspectorToolset.SlateInspectorToolset.'
m.call(s+'Observe',ref='',maxDepth=45)
tree=m.call(s+'Snapshot',ref='',maxDepth=45)
window=re.search(r'window "Braided_Quanta2026 - Unreal Editor"[^\n]*\[ref=(\w+)\]',tree)
assert window, 'The project editor window is not available'
m.call(s+'Observe',ref=window.group(1),maxDepth=45)
tree=m.call(s+'Snapshot',ref=window.group(1),maxDepth=45)
main=tree[tree.find('window "Braided_Quanta2026 - Unreal Editor"'):]
main=main.split('\nwindow ',1)[0]
command=re.search(r'text "Cmd"[\s\S]*?textbox[^\n]*\[ref=(\w+)\]',main)
assert command, 'Open the Output Log console before executing scripts'
console=command.group(1)
path=Path(sys.argv[1]);path=path if path.is_absolute() else ROOT/path
assert path.is_file(),path
m.call(s+'Click',ref=console)
m.call(s+'PressKey',key='Ctrl+A')
print(m.call(s+'Type',ref=console,text='py "'+path.as_posix()+'"',submit=True))
