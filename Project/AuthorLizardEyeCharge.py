"""Add runtime emissive scaling to project copies of the three evolution eye materials."""
import gc
import unreal

def main():
    library=unreal.MaterialEditingLibrary
    folder='/Game/Quasicombo/Boss/Lizard/Materials'
    unreal.EditorAssetLibrary.make_directory(folder)
    for name in ['M_Eyes_03','M_Eyes_02','M_GlowEyes']:
        target=folder+'/'+name+'_Evolution'
        if unreal.EditorAssetLibrary.does_asset_exist(target):
            material=unreal.load_asset(target)
        else:
            material=unreal.EditorAssetLibrary.duplicate_asset('/Game/Lizardman_Berserker/Materials/'+name,target)
        assert material, target
        if 'EvolutionEmissiveScale' not in [str(p) for p in library.get_scalar_parameter_names(material)]:
            source=library.get_material_property_input_node(material,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            output=library.get_material_property_input_node_output_name(material,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            assert source, 'Missing emissive input: '+target
            scale=library.create_material_expression(material,unreal.MaterialExpressionScalarParameter,200,400)
            scale.set_editor_property('parameter_name','EvolutionEmissiveScale')
            scale.set_editor_property('default_value',1.0)
            multiply=library.create_material_expression(material,unreal.MaterialExpressionMultiply,400,300)
            assert library.connect_material_expressions(source,output,multiply,'A')
            assert library.connect_material_expressions(scale,'',multiply,'B')
            assert library.connect_material_property(multiply,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            library.recompile_material(material)
        assert unreal.EditorAssetLibrary.save_loaded_asset(material,False)
        print('LIZARD_EYE_CHARGE_AUTHORED',target)

main()
gc.collect()
