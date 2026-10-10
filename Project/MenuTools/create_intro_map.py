"""Executed by the Unreal MCP console: create only the empty intro world."""
import unreal,json
from pathlib import Path
path='/Game/UI/Menu/Lvl_QuasicomboIntro'
assets=unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
if assets.does_asset_exist(path):
    world=assets.load_asset(path)
else:
    world=unreal.AssetToolsHelpers.get_asset_tools().create_asset('Lvl_QuasicomboIntro','/Game/UI/Menu',unreal.World,unreal.WorldFactory())
assert world, 'Intro map creation failed'
settings=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.WorldSettings)
assert len(settings)==1,settings
settings[0].set_editor_property('default_game_mode',unreal.load_class(None,'/Game/UI/Menu/BP_QuasicomboIntroMode.BP_QuasicomboIntroMode_C'))
assert unreal.EditorLoadingAndSavingUtils.save_map(world,path), 'Intro map save failed'
result={'world':world.get_path_name(),'actors':[a.get_class().get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)],'game_mode':settings[0].get_editor_property('default_game_mode').get_path_name()}
(Path(unreal.Paths.project_saved_dir())/'MenuValidation/intro-map.json').write_text(json.dumps(result,indent=2))
print('QUASICOMBO_INTRO_MAP_CREATED',result)
