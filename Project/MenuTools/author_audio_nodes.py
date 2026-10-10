"""Disambiguate AudioComponent actions with MCP's declaring_class parameter."""
from blueprint_author import B, ref, normalize


def audio_function(bp, name, action, constants, level_from_settings=False):
    m = bp.m
    g = bp.function(name)
    nodes = m.call(B + 'find_nodes', graph=g.graph, title='', node_class=None, entry_points_only=False)
    infos = m.call(B + 'get_node_infos', nodes=nodes)
    entry = next(info for info in infos if info['type_id'].lower() == '|' + name.lower())
    for info in infos:
        if info['node'] != entry['node']:
            m.call(B + 'delete_node', node=info['node'])
    sequence = 0

    def create(search, declaring=None):
        nonlocal sequence
        node_id, _ = g.node(search,prefer='Audio|Components|Audio|' if search=='Stop' else None)
        args = {'graph': g.graph, 'type_id': node_id, 'pos': {'x': 280 * (sequence + 1), 'y': 120 * sequence}}
        if declaring:
            args['declaring_class'] = ref(declaring)
        node = m.call(B + 'create_node', **args)
        sequence += 1
        return m.call(B + 'get_node_infos', nodes=[node])[0]

    def pin(info, name, output=False):
        candidates = info['output_pins' if output else 'input_pins']
        matches = [p for p in candidates if normalize(p['name']) == normalize(name)]
        if len(matches) != 1:
            raise ValueError(f'{info["type_id"]}: {name}: {[p["name"] for p in candidates]}')
        return matches[0]['pin_id']

    def wire(source, output, dest, input_name):
        m.call(B + 'connect_pins', output_pin=pin(source, output, True), input_pin=pin(dest, input_name))

    component = create('GetIntroComponent')
    audio = create(action, '/Script/Engine.AudioComponent')
    wire(entry, 'then', audio, 'execute')
    wire(component, 'IntroComponent', audio, 'self')
    for name, value in constants.items():
        m.call(B + 'set_pin_value', pin=pin(audio, name), value=str(value))
    if level_from_settings:
        loadout = create('GetLoadout')
        volume = create('GetUserMusicVolume')
        wire(loadout, 'ReturnValue', volume, 'self')
        level_pin = next(p['name'] for p in audio['input_pins'] if 'volume' in p['name'].lower() and p['name'] not in constants)
        wire(volume, 'ReturnValue', audio, level_pin)
    bp.compile()


def author(bp):
    audio_function(bp, 'UpdateIntroVolume', 'SetVolumeMultiplier', {}, True)
    audio_function(bp, 'StopIntro', 'FadeOut', {'FadeOutDuration': .4, 'FadeVolumeLevel': 0.0})
    audio_function(bp, 'ReplayIntro', 'FadeIn', {'FadeInDuration': .2, 'FadeVolumeLevel': 1.0, 'StartTime': 0.0})
