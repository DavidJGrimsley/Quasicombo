"""Import the three supplied Mike punch effects as Unreal SoundWave assets."""

from pathlib import Path

import unreal


source_dir = Path(unreal.Paths.project_dir()) / "Project" / "AudioSource" / "Mike"
destination = "/Game/Quasicombo/Mike/Sounds"
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
sound_class_path = f"{destination}/SC_Mike_Punches"
sound_class = assets.load_asset(sound_class_path)
if not sound_class:
    sound_class = tools.create_asset("SC_Mike_Punches", destination,
                                     unreal.SoundClass, unreal.SoundClassFactory())
    if sound_class:
        properties = sound_class.get_editor_property("properties")
        properties.set_editor_property("volume", 0.8)
        sound_class.set_editor_property("properties", properties)
if not isinstance(sound_class, unreal.SoundClass):
    raise RuntimeError("Could not create Mike punch Sound Class")
if not assets.save_loaded_asset(sound_class):
    raise RuntimeError("Could not save Mike punch Sound Class")
print("MIKE_PUNCH_VOLUME", sound_class.get_path_name(),
      sound_class.get_editor_property("properties").get_editor_property("volume"))

for number in range(1, 4):
    source = source_dir / f"punch_{number:02d}.wav"
    if not source.is_file():
        raise RuntimeError(f"Missing Mike punch source: {source}")

    name = f"SFX_Mike_Punch{number}"
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    tools.import_asset_tasks([task])

    sound = assets.load_asset(f"{destination}/{name}")
    if not isinstance(sound, unreal.SoundWave):
        raise RuntimeError(f"Failed to import Mike punch sound: {source}")
    sound.set_editor_property("sound_class_object", sound_class)
    if not assets.save_loaded_asset(sound):
        raise RuntimeError(f"Could not save Mike punch sound: {name}")
    print("MIKE_PUNCH_SOUND", number, sound.get_path_name())
