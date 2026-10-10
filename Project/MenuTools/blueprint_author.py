"""Schema-aware helpers for visual Blueprint graph authoring over Unreal MCP."""
import json
import re
from mcp_client import ref, REPORT

B = 'editor_toolset.toolsets.blueprint.BlueprintTools.'
A = 'editor_toolset.toolsets.asset.AssetTools.'


def quote(value):
    return json.dumps(value, ensure_ascii=False)


def normalize(value):
    return re.sub('[^a-z0-9]', '', value.lower())


class Graph:
    """Resolve registered node types and their exact pins before writing DSL."""
    def __init__(self, mcp, graph):
        self.m, self.graph, self.catalog = mcp, graph, {}

    def node(self, search, prefer=None):
        key = (search, prefer)
        if key not in self.catalog:
            choices = self.m.call(B + 'find_node_types', graph=self.graph,
                                  type_id_filter=search, context_pins=[])
            if not choices and '_' in search:
                choices = self.m.call(B + 'find_node_types', graph=self.graph,
                                      type_id_filter=search.replace('_', ''), context_pins=[])
            choices = list(dict.fromkeys(choices))
            exact = [c for c in choices if normalize(c.split('|')[-1]) == normalize(search)]
            if exact:
                choices = exact
            if prefer:
                choices = [c for c in choices if prefer.lower() in c.lower()]
            if len(choices) > 1:
                current_class = normalize(self.graph['refPath'].split('.')[-1].split(':')[0])
                local = [c for c in choices if any(normalize(part) == current_class for part in c.split('|')[:-1])]
                if local:
                    choices = local
                else:
                    defaults = [c for c in choices if c.startswith('Variables|Default|')]
                    if defaults:
                        choices = defaults
            if len(choices) > 1:
                functions = [c for c in choices if not c.startswith(('Class|', 'Variables|'))]
                if len(functions) == 1:
                    choices = functions
            if len(choices) != 1:
                raise ValueError(f'{self.graph}: {search!r} ambiguous/missing: {choices}')
            node_id = choices[0]
            pins = self.m.call(B + 'get_node_type_pins', graph=self.graph, type_id=node_id)
            self.catalog[key] = (node_id, pins)
        return self.catalog[key]

    def call(self, search, prefer=None, **inputs):
        node_id, pins = self.node(search, prefer)
        names = [p['name'] for p in pins['input_pins']]
        values = []
        for supplied, expression in inputs.items():
            supplied = 'self' if supplied == 'target' else supplied
            matches = [n for n in names if normalize(n) == normalize(supplied)]
            if not matches and supplied.startswith('b'):
                matches = [n for n in names if normalize(n) == normalize(supplied[1:])]
            if not matches:
                for prefix in ['New', 'In']:
                    if supplied.startswith(prefix):
                        matches = [n for n in names if normalize(n) == normalize(supplied[len(prefix):])]
                        if matches:
                            break
            if not matches and supplied == 'WorldContextObject':
                continue  # This pin is hidden and supplied automatically by K2.
            if len(matches) != 1:
                raise ValueError(f'{node_id}: unknown pin {supplied}; actual {names}')
            pin = matches[0]
            values += [':' + (quote(pin) if any(c.isspace() for c in pin) else pin), str(expression)]
        return '(' + ' '.join([node_id] + values) + ')'

    def get(self, name, target=None, prefer=None):
        return self.call('Get' + name, prefer=prefer, **({'target': target} if target else {}))

    def set(self, name, expression, target=None):
        args = {name: expression}
        if target:
            args['target'] = target
        return self.call('Set' + name, **args)

    def write(self, code, label):
        REPORT.mkdir(parents=True, exist_ok=True)
        (REPORT / (label + '.dsl')).write_text(code, encoding='utf-8')
        result = self.m.call(B + 'write_graph_dsl', graph=self.graph, code=code)
        print('Graph', label, result, flush=True)
        return result


class Blueprint:
    def __init__(self, mcp, name):
        self.m, self.name = mcp, name
        self.asset = ref(f'/Game/UI/Menu/{name}.{name}')
        self.cls = ref(self.asset['refPath'] + '_C')

    def create(self, parent):
        if not self.m.call(A + 'exists', path=self.asset['refPath']):
            self.m.call(B + 'create', folder_path='/Game/UI/Menu', asset_name=self.name,
                        asset_type=ref(parent))

    def variables(self, definitions):
        existing = self.m.call(B + 'list_variables', blueprint=self.asset)
        names = {str(v['name']) if isinstance(v, dict) else str(v) for v in existing}
        for name, kind in definitions.items():
            if name in names:
                continue
            args = {'blueprint': self.asset, 'name': name}
            if kind.startswith('/'):
                self.m.call(B + 'add_object_variable', **args, object_class=ref(kind))
            else:
                self.m.call(B + 'add_variable', **args, type_name=kind)

    def function(self, name, inputs=None, outputs=None):
        graph = self.m.call(B + 'add_function_graph', blueprint=self.asset, graph_name=name)
        current_dsl = self.m.call(B + 'read_graph_dsl', graph=graph)
        parameters = re.search(r'\(fn\s+\S+\s+\(([^)]*)\)', current_dsl)
        existing_inputs = set(parameters.group(1).split()) if parameters else set()
        # Parameter creation is idempotent only through this file's graph registry.
        for is_input, params in [(True, inputs or {}), (False, outputs or {})]:
            for param, kind in params.items():
                if is_input and param in existing_inputs:
                    continue
                try:
                    self.m.call(B + 'add_function_param', graph=graph, param_name=param,
                                param_type=kind, input_param=is_input)
                except RuntimeError as error:
                    if 'already' not in str(error).lower() and 'exists' not in str(error).lower():
                        raise
        return Graph(self.m, graph)

    def events(self):
        graph = ref(self.asset['refPath'] + ':EventGraph')
        return Graph(self.m, graph)

    def bind(self, widget, event, kind):
        try:
            self.m.call('UMGToolSet.UMGToolSet.BindToEventProperty', widgetBlueprint=self.asset,
                        eventName=event, propertyName=widget, propertyClass=ref('/Script/UMG.' + kind))
        except RuntimeError as error:
            if 'already bound' not in str(error):
                raise
        code = self.m.call(B + 'read_graph_dsl', graph=self.events().graph)
        # The compiler gives component-bound events stable generated names.
        expected = event + '(' + widget + ')'
        if not re.search(r'\(event\s+' + re.escape(expected) + r'(?=[\s)])', code):
            raise ValueError(f'Cannot identify {widget}.{event}: {code}')
        return quote(expected)

    def defaults(self, values):
        obj = self.m.call(B + 'get_default_object', blueprint=self.asset)
        schema = self.m.props(obj)
        resolved = {}
        for name, value in values.items():
            matches = [key for key in schema if normalize(key) == normalize(name)]
            if len(matches) != 1:
                raise ValueError(f'{self.name}: missing default property {name}: {list(schema)}')
            resolved[matches[0]] = value
        result = self.m.set(obj, **resolved)
        # Refresh the generated class's optimized property-copy list before spawn.
        self.compile()
        return result

    def compile(self):
        result = self.m.call(B + 'compile_blueprint', blueprint=self.asset)
        print('Compile', self.name, result, flush=True)
        if isinstance(result, dict) and result.get('errors'):
            raise RuntimeError(result)
        return result

    def save(self):
        return self.m.call(A + 'save_assets', asset_paths=[self.asset['refPath']])
