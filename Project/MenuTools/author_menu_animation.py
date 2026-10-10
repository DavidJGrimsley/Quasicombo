"""Update only the frontend actor; preserve subsequent Designer menu edits."""
from mcp_client import UnrealMCP, ref
from blueprint_author import Blueprint, B, A
from author_director import author_wave, author_screens, author_transition, author_initialize, author_tick
import sys


def main():
    m = UnrealMCP()
    for toolset in [B.rstrip('.'), A.rstrip('.'), 'editor_toolset.toolsets.object.ObjectTools']:
        m.describe(toolset)
    if m.call('EditorToolset.EditorAppToolset.IsPIERunning'):
        m.call('EditorToolset.EditorAppToolset.StopPIE')
    bp = Blueprint(m, 'BP_QuasicomboFrontend')
    if '--finish' not in sys.argv:
        author_wave(bp)
        author_screens(bp)
        author_transition(bp)
        author_initialize(bp)
    author_tick(bp)
    bp.compile()
    bp.defaults({
        'EmoteIntervalSeconds': 10, 'BlinkOffsetSeconds': 5,
        'IdleCycleSeconds': 50, 'IdleAggroShare': .4,
        **{key: ref(f'/Game/RadicalMike/Animations/Anim_ZMIKE_{clip}.Anim_ZMIKE_{clip}')
           for key, clip in [('BlinkClip', 'Blinking'), ('RegularIdleClip', 'Idle'),
                             ('AggroIdleClip', 'IdleAggro')]},
    })
    groups = {'Menu|Timing': ['EmoteIntervalSeconds', 'BlinkOffsetSeconds',
                             'IdleCycleSeconds', 'IdleAggroShare'],
              'Menu|Animation': ['WaveCompleteClip', 'BlinkClip', 'RegularIdleClip', 'AggroIdleClip']}
    for category, names in groups.items():
        for name in names:
            m.call(B+'set_variable_category', blueprint=bp.asset, variable_name=name, category=category)
            m.call(B+'set_variable_instance_editable', blueprint=bp.asset, variable_name=name, instance_editable=True)
    variables = m.call(B+'list_variables', blueprint=bp.asset)
    existing = {v['name'] if isinstance(v,dict) else str(v) for v in variables}
    for name in ['WaveMinSeconds', 'WaveMaxSeconds', 'WaveStartClip', 'WaveLoopClip']:
        if name not in existing:
            continue
        m.call(B+'set_variable_category', blueprint=bp.asset, variable_name=name, category='Legacy|Unused')
        m.call(B+'set_variable_instance_editable', blueprint=bp.asset, variable_name=name, instance_editable=False)
    bp.compile()
    bp.save()


if __name__ == '__main__':
    main()
