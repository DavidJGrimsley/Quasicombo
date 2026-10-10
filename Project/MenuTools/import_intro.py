"""Run in the editor Python console through MCP's Slate tools.

There is no registered sound import tool. AssetImportTask keeps the source WAV
path in the SoundWave's import metadata; the original MP3 remains in Project.
"""
import unreal
from pathlib import Path

root = Path(unreal.Paths.project_dir())
destination = '/Game/UI/Menu/SW_Intro'
if not unreal.EditorAssetLibrary.does_asset_exist(destination):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(root / 'Project/AudioSource/Menu/SW_Intro.wav'))
    task.set_editor_property('destination_path', '/Game/UI/Menu')
    task.set_editor_property('destination_name', 'SW_Intro')
    task.set_editor_property('automated', True)
    task.set_editor_property('save', True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
sound = unreal.load_asset(destination)
if not sound:
    raise RuntimeError('Intro import failed')
sound.set_editor_property('looping', True)
unreal.EditorAssetLibrary.save_asset(destination)
unreal.log('QUASICOMBO_MENU_INTRO_IMPORTED ' + sound.get_path_name())
