"""Run scenery/traversal and existing gameplay regressions through Unreal MCP."""
import json
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent / 'MenuTools'))
from mcp_client import UnrealMCP, ROOT
sys.stdout.reconfigure(encoding='utf-8')
m = UnrealMCP()
t = 'AutomationTestToolset.AutomationTestToolset.'
report_dir = ROOT / 'Saved' / 'BackgroundGameplayValidation'
report_dir.mkdir(parents=True, exist_ok=True)
print('Discover:', m.call(t + 'DiscoverTests', bForceRediscover=False), flush=True)
available = json.loads(m.call(t + 'ListTests', nameFilter='Quasicombo', tagFilter='', limit=0))['tests']
focused = [n for n in available if n.startswith(('Quasicombo.Background.', 'Quasicombo.Trap.'))
           or n in ['Quasicombo.Player.WallGripSlideJump', 'Quasicombo.Combat.PlayerHealthRegeneration']]
tests = focused if '--focused' in sys.argv else [n for n in available if n.startswith(
    ('Quasicombo.Background.', 'Quasicombo.Trap.', 'Quasicombo.Player.', 'Quasicombo.Combat.',
     'Quasicombo.Buster.', 'Quasicombo.QTE.', 'Quasicombo.Route.', 'Quasicombo.Frontend.'))]
print('Running', len(tests), 'tests', tests, flush=True)
summary = m.call(t + 'RunTests', testNames=tests)
results = m.call(t + 'GetTestResults')
(report_dir / ('focused-tests.json' if '--focused' in sys.argv else 'regressions.json')).write_text(results, encoding='utf-8')
data = json.loads(results)
print({k: data[k] for k in ['passed', 'failed', 'skipped', 'total']}, flush=True)
for test in data['tests']:
    if test['state'] != 'Success': print(json.dumps(test), flush=True)
if data['failed'] or data['skipped']: sys.exit(1)
