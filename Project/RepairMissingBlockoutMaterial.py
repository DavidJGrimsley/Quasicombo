"""Restore the neutral blockout material referenced by old template actors."""
import unreal

destination = '/Game/ThirdPerson/MI_ThirdPersonColWay'
library = unreal.EditorAssetLibrary
if not library.does_asset_exist(destination):
    parent = unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial')
    if not parent:
        raise RuntimeError('Engine BasicShapeMaterial is unavailable')
    factory = unreal.MaterialInstanceConstantFactoryNew()
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'MI_ThirdPersonColWay', '/Game/ThirdPerson', unreal.MaterialInstanceConstant, factory)
    if not asset:
        raise RuntimeError('Could not create the missing blockout material')
    asset.set_editor_property('parent', parent)
    if not library.save_loaded_asset(asset):
        raise RuntimeError('Could not save the blockout material')
print('BQ_MATERIAL', destination, library.does_asset_exist(destination))
