"""Idempotent editor commandlet for the Quasicombo boss Blueprint.

Run SetupQuasicomboEnemy.py to create the configurable enemy Blueprint.
"""
import unreal

library = unreal.EditorAssetLibrary
source = '/Game/Variant_Combat/Blueprints/AI/BP_CombatEnemy'
specs = [
    ('/Game/Quasicombo/Boss/BP_QC_Boss', 'QuasicomboBoss', 6),
]

for path, parent_name, hits in specs:
    if not library.does_asset_exist(path):
        if not library.duplicate_asset(source, path):
            raise RuntimeError('Could not duplicate ' + source + ' to ' + path)
        blueprint = library.load_asset(path)
        parent = unreal.load_class(None, '/Script/Braided_Quanta2026.' + parent_name)
        if not parent:
            raise RuntimeError('Missing parent class ' + parent_name)
        unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, parent)
    else:
        blueprint = library.load_asset(path)
    klass = library.load_blueprint_class(path)
    cdo = unreal.get_default_object(klass)
    print('BQ_BEFORE', path, cdo.get_class().get_name(), cdo.get_editor_property('required_hits'), cdo.get_editor_property('ai_controller_class'))
    cdo.set_editor_property('ai_controller_class', unreal.AIController.static_class())
    cdo.set_editor_property('required_hits', hits)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    if not library.save_loaded_asset(blueprint):
        raise RuntimeError('Could not save ' + path)
    print('BQ_CREATED', path, 'class', cdo.get_class().get_name(), 'hits', cdo.get_editor_property('required_hits'),
          'controller', cdo.get_editor_property('ai_controller_class'))
