"""Reset story scroll per page and expose its keyboard/gamepad reading hint."""
from mcp_client import UnrealMCP
from blueprint_author import Blueprint,B,A,quote as q

m=UnrealMCP()
for t in [B.rstrip('.'),A.rstrip('.')]: m.describe(t)
bp=Blueprint(m,'WBP_QuasicomboStory')
g=bp.function('RefreshStory')
page=g.get('StoryPage',g.get('MenuDirector'))
cases=' '.join(f'(:{i} {g.call("SetText(Text)",target=g.get("StoryText"),InText=g.get(f"NarrativePage{i}"))} '+
              g.call('SetText(Text)',target=g.get('PageNumber'),InText=q(f'{i+1} / 4 · Up/Down to scroll'))+')' for i in range(4))
g.write(f'(fn RefreshStory () (switch int {page} {cases}))',bp.name+'_Refresh')
bp.compile();bp.save()
