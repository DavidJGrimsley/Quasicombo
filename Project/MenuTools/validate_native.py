"""Focused frontend, score, music and session-loadout regressions through MCP."""
import json
import sys
from mcp_client import UnrealMCP, REPORT

sys.stdout.reconfigure(encoding='utf-8')
T = 'AutomationTestToolset.AutomationTestToolset.'
TESTS = ['Quasicombo.Frontend.RunStart', 'Quasicombo.Frontend.AudioPreferences',
         'Quasicombo.Frontend.MusicMultiplier',
         'Quasicombo.Score.Breakdown', 'Quasicombo.Music.RouteApproachAndEncounter',
         'Quasicombo.Music.RoutePlaybackContinuity',
         'Quasicombo.Music.QTEDuckingPreservesHitsAndPlayback',
         'Quasicombo.Buster.Acquisition', 'Quasicombo.Buster.FreshSessionLoadout',
         'Quasicombo.Buster.ReplayLoadout']

if __name__ == '__main__':
    m = UnrealMCP()
    m.describe(T.rstrip('.'))
    print('Discover:', m.call(T + 'DiscoverTests', bForceRediscover=False), flush=True)
    available = json.loads(m.call(T + 'ListTests', nameFilter='Quasicombo', tagFilter='', limit=0))
    missing = set(TESTS) - set(available['tests'])
    if missing:
        raise RuntimeError('Missing tests: ' + str(missing))
    print('Running', len(TESTS), 'focused tests', flush=True)
    summary = m.call(T + 'RunTests', testNames=TESTS)
    results = m.call(T + 'GetTestResults')
    (REPORT / 'native-tests.json').write_text(results, encoding='utf-8')
    report=json.loads(results)
    print({key:report[key] for key in ['passed','failed','skipped','total']})
    for test in report['tests']:
        if test['state']!='Success': print(test)
    if report['failed'] or report['skipped']:
        raise RuntimeError('Focused native regressions did not all pass')
