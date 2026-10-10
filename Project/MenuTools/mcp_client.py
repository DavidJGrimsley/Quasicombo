"""Small client for the running Unreal Editor's local MCP server.

All asset authoring goes through registered Unreal MCP tools. This client never
opens or rewrites .uasset files. Run with the bundled Codex Python runtime.
"""
import json
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REPORT = ROOT / 'Saved' / 'MenuValidation'


def ref(path):
    return {'refPath': path}


class UnrealMCP:
    def __init__(self):
        self.url = 'http://127.0.0.1:8000/mcp'
        self.session = None
        self.sequence = 0
        self.schemas = {}
        self.rpc('initialize', {
            'protocolVersion': '2024-11-05', 'capabilities': {},
            'clientInfo': {'name': 'quasicombo-menu-authoring', 'version': '1.0'},
        })
        self.rpc('notifications/initialized', notification=True)

    def rpc(self, method, params=None, notification=False):
        self.sequence += 1
        payload = {'jsonrpc': '2.0', 'method': method}
        if not notification:
            payload['id'] = self.sequence
        if params is not None:
            payload['params'] = params
        headers = {'Accept': 'application/json, text/event-stream',
                   'Content-Type': 'application/json',
                   'MCP-Protocol-Version': '2024-11-05'}
        if self.session:
            headers['Mcp-Session-Id'] = self.session
        request = urllib.request.Request(self.url, json.dumps(payload).encode('utf-8'), headers)
        with urllib.request.urlopen(request, timeout=15 if method == 'initialize' else 900) as response:
            self.session = response.headers.get('Mcp-Session-Id', self.session)
            raw = response.read().decode('utf-8')
        if not raw:
            return {}
        result = json.loads(raw)
        if 'error' in result:
            raise RuntimeError(result['error'])
        return result.get('result', {})

    def top(self, name, args=None):
        result = self.rpc('tools/call', {'name': name, 'arguments': args or {}})
        if result.get('isError'):
            raise RuntimeError(result.get('content'))
        texts = [x['text'] for x in result.get('content', []) if x['type'] == 'text']
        if len(texts) == 1:
            try:
                return json.loads(texts[0])
            except json.JSONDecodeError:
                return texts[0]
        return result

    def describe(self, toolset):
        value = self.top('describe_toolset', {'toolset_name': toolset})
        for schema in value['tools']:
            self.schemas[schema['name']] = schema
        return value

    def call(self, tool_name, **args):
        toolset, tool = tool_name.rsplit('.', 1)
        value = self.top('call_tool', {
            'toolset_name': toolset, 'tool_name': tool, 'arguments': args,
        })
        return value.get('returnValue', value) if isinstance(value, dict) else value

    def batch(self, script):
        value = self.call('editor_toolset.toolsets.programmatic.ProgrammaticToolset.execute_tool_script', script=script)
        return json.loads(value)

    def props(self, instance):
        return json.loads(self.call('editor_toolset.toolsets.object.ObjectTools.list_properties', instance=instance))

    def get(self, instance, *names):
        schema = self.props(instance)
        absent = set(names) - set(schema)
        if absent:
            raise ValueError(f'Unknown properties on {instance}: {absent}')
        return json.loads(self.call('editor_toolset.toolsets.object.ObjectTools.get_properties', instance=instance, properties=list(names)))

    def set(self, instance, **values):
        self.get(instance, *values)
        def property_value(value):
            if isinstance(value, dict):
                if set(value) == {'refPath'}:
                    return value['refPath']
                return {k: property_value(v) for k, v in value.items()}
            if isinstance(value, list):
                return [property_value(v) for v in value]
            return value
        result = self.call('editor_toolset.toolsets.object.ObjectTools.set_properties',
                           instance=instance, values=json.dumps(property_value(values)))
        if not result:
            raise RuntimeError(f'Property write rejected: {instance}: {list(values)}')
        return result


if __name__ == '__main__':
    import sys
    sys.stdout.reconfigure(encoding='utf-8')
    mcp = UnrealMCP()
    if sys.argv[1] == 'describe':
        data = mcp.describe(sys.argv[2])
    else:
        data = mcp.call(sys.argv[1], **json.loads(sys.argv[2] if len(sys.argv) > 2 else '{}'))
    print(json.dumps(data, ensure_ascii=False, indent=2))
