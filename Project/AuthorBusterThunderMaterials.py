"""Author project-owned Thunder materials, then rebuild the three Cascade copies."""
import unreal

assets = unreal.EditorAssetLibrary
editing = unreal.MaterialEditingLibrary
folder = '/Game/Quasicombo/Buster/VFX/ThunderMaterials'
assets.make_directory(folder)

def save(asset):
    if not assets.save_loaded_asset(asset):
        raise RuntimeError('Could not save ' + asset.get_path_name())

def copy_material(source):
    path = folder + '/M_BusterThunder_' + source.get_name()
    material = unreal.load_asset(path)
    if not material:
        material = assets.duplicate_asset(source.get_path_name(), path)
    if not material:
        raise RuntimeError('Could not copy ' + source.get_path_name())
    if isinstance(source, unreal.MaterialInstanceConstant):
        parent = copy_material(source.get_editor_property('parent'))
        editing.set_material_instance_parent(material, parent)
        overrides = material.get_editor_property('base_property_overrides')
        overrides.set_editor_property('override_blend_mode', True)
        overrides.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
        overrides.set_editor_property('override_shading_model', False)
        material.set_editor_property('base_property_overrides', overrides)
    else:
        material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
        material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        material.set_editor_property('two_sided', True)
        gain_nodes = [n for n in editing.get_material_expressions(material)
                      if isinstance(n, unreal.MaterialExpressionScalarParameter)
                      and str(n.get_editor_property('parameter_name')) == 'BusterEmissiveGain']
        if not gain_nodes:
            original = editing.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            original_pin = editing.get_material_property_input_node_output_name(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            if not original:
                original = editing.create_material_expression(material, unreal.MaterialExpressionConstant, 1500, 0)
                original.set_editor_property('r', 0.0)
            nonnegative = editing.create_material_expression(material, unreal.MaterialExpressionMax, 1750, 0)
            nonnegative.set_editor_property('const_b', 0.0)
            gain = editing.create_material_expression(material, unreal.MaterialExpressionScalarParameter, 1750, 200)
            gain.set_editor_property('parameter_name', 'BusterEmissiveGain')
            gain.set_editor_property('default_value', 0.1 if source.get_name() == 'M_ky_smoke01' else 1.0)
            product = editing.create_material_expression(material, unreal.MaterialExpressionMultiply, 1950, 0)
            connections = [
                editing.connect_material_expressions(original, original_pin, nonnegative, 'A'),
                editing.connect_material_expressions(nonnegative, '', product, 'A'),
                editing.connect_material_expressions(gain, '', product, 'B'),
                editing.connect_material_property(product, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR),
            ]
            if not all(connections):
                raise RuntimeError('Emissive connection failed for ' + path)
        opacity = editing.get_material_property_input_node(material, unreal.MaterialProperty.MP_OPACITY)
        if opacity and not isinstance(opacity, unreal.MaterialExpressionSaturate):
            opacity_pin = editing.get_material_property_input_node_output_name(material, unreal.MaterialProperty.MP_OPACITY)
            bounded_opacity = editing.create_material_expression(material, unreal.MaterialExpressionSaturate, 1950, 400)
            if not editing.connect_material_expressions(opacity, opacity_pin, bounded_opacity, ''):
                raise RuntimeError('Opacity clamp input failed for ' + path)
            if not editing.connect_material_property(bounded_opacity, '', unreal.MaterialProperty.MP_OPACITY):
                raise RuntimeError('Opacity clamp output failed for ' + path)
        editing.recompile_material(material)
    save(material)
    print('THUNDER_MATERIAL_AUTHORED', material.get_path_name())
    return material

for name in ['MI_ky_smoke01', 'MI_ky_smoke01b', 'MI_ky_lightningDust01_4x4_nonD',
             'MI_ky_lightning02_4x4', 'MI_ky_polar_two', 'MI_ky_flare_flash_trs', 'M_ky_empty']:
    source = unreal.load_asset('/Game/FXVarietyPack/Materials/' + name)
    if not source:
        raise RuntimeError('Missing Thunder source material ' + name)
    copy_material(source)

if '-BusterMaterialsOnly' not in unreal.SystemLibrary.get_command_line():
    unreal.SystemLibrary.execute_console_command(None, 'Quasicombo.AuthorBusterThunderVFX')
print('THUNDER_MATERIAL_AUTHORING_COMPLETE')
