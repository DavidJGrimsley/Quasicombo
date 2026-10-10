"""Import only the four gameplay tracks and create the reusable approach Blueprint.

Run via the Unreal MCP editor console: py <absolute path to this script>.
No existing level actor, map, or Blueprint is saved by this script.
"""
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()) / 'Project' / 'AudioSource' / 'Music'
destination = '/Game/Audio/Music'
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
for name in ['SW_RouteA', 'SW_RouteC', 'SW_Boss', 'SW_Evolution']:
    path = destination + '/' + name
    if not assets.does_asset_exist(path):
        task = unreal.AssetImportTask()
        task.filename = str(root / (name + '.wav'))
        task.destination_path = destination
        task.destination_name = name
        task.automated = True
        task.replace_existing = False
        task.save = False
        tools.import_asset_tasks([task])
        assert task.imported_object_paths, 'Audio import failed: ' + name
    sound = assets.load_asset(path)
    assert isinstance(sound, unreal.SoundWave), path
    sound.set_editor_property('looping', True)
    sound.set_editor_property('volume', 1.0)
    sound.set_editor_property('pitch', 1.0)
    assert assets.save_loaded_asset(sound), 'Could not save ' + path
    print('BQ_MUSIC_IMPORTED', path, sound.duration)

path = '/Game/Quasicombo/Boss/BP_BossApproachVolume'
if not assets.does_asset_exist(path):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.load_class(None, '/Script/Braided_Quanta2026.QuasicomboBossApproachVolume'))
    blueprint = tools.create_asset('BP_BossApproachVolume', '/Game/Quasicombo/Boss', unreal.Blueprint, factory)
else:
    blueprint = assets.load_asset(path)
unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
assert 'ERROR' not in str(blueprint.get_editor_property('status')).upper()
assert assets.save_loaded_asset(blueprint)
print('BQ_APPROACH_ASSETS_READY')
