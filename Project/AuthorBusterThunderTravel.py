"""Regenerate ONLY Thunder travel and its three dedicated materials.

Run with UnrealEditor-Cmd <project> -run=pythonscript -script=<this file>.
Add -BusterTravelMaterialOnly to update only the existing travel master.
Never saves imported assets or the approved muzzle/impact material copies.
"""
import unreal

assets = unreal.EditorAssetLibrary
editing = unreal.MaterialEditingLibrary
folder = '/Game/Quasicombo/Buster/VFX/ThunderMaterials'
material_only = '-BusterTravelMaterialOnly' in unreal.SystemLibrary.get_command_line()

def connect(source, pin, destination, input_pin):
    if not editing.connect_material_expressions(source, pin, destination, input_pin):
        raise RuntimeError('Failed travel material link: ' + source.get_name() + ' -> ' + destination.get_name())

def output(source, prop):
    if not editing.connect_material_property(source, '', prop):
        raise RuntimeError('Failed travel material output: ' + str(prop))

def duplicate(source, name):
    path = folder + '/' + name
    result = unreal.load_asset(path) or assets.duplicate_asset(source, path)
    if not result:
        raise RuntimeError('Could not create ' + path)
    return result

master = duplicate('/Game/FXVarietyPack/Materials/M_ky_smoke01', 'M_BusterThunderTravel')
master.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
master.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
markers = [n for n in editing.get_material_expressions(master)
           if isinstance(n, unreal.MaterialExpressionStaticSwitchParameter)
           and str(n.get_editor_property('parameter_name')) == 'TravelPremultiply']
if not markers:
    emission = editing.get_material_property_input_node(master, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    emission_pin = editing.get_material_property_input_node_output_name(master, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity = editing.get_material_property_input_node(master, unreal.MaterialProperty.MP_OPACITY)
    opacity_pin = editing.get_material_property_input_node_output_name(master, unreal.MaterialProperty.MP_OPACITY)
    if not emission or not opacity:
        raise RuntimeError('Source smoke material is missing emission/opacity')

    bounded = editing.create_material_expression(master, unreal.MaterialExpressionSaturate, 1750, 350)
    connect(opacity, opacity_pin, bounded, '')
    output(bounded, unreal.MaterialProperty.MP_OPACITY)

    positive = editing.create_material_expression(master, unreal.MaterialExpressionMax, 1750, 0)
    positive.set_editor_property('const_b', 0.0)
    connect(emission, emission_pin, positive, 'A')
    gain = editing.create_material_expression(master, unreal.MaterialExpressionScalarParameter, 1750, 180)
    gain.set_editor_property('parameter_name', 'TravelEmissiveGain')
    gain.set_editor_property('default_value', 1.0)
    amplified = editing.create_material_expression(master, unreal.MaterialExpressionMultiply, 1950, 0)
    connect(positive, '', amplified, 'A')
    connect(gain, '', amplified, 'B')

    premultiplied = editing.create_material_expression(master, unreal.MaterialExpressionMultiply, 2150, 200)
    connect(amplified, '', premultiplied, 'A')
    connect(bounded, '', premultiplied, 'B')
    switch = editing.create_material_expression(master, unreal.MaterialExpressionStaticSwitchParameter, 2350, 0)
    switch.set_editor_property('parameter_name', 'TravelPremultiply')
    switch.set_editor_property('default_value', False)
    connect(premultiplied, '', switch, 'True')
    connect(amplified, '', switch, 'False')
    output(switch, unreal.MaterialProperty.MP_EMISSIVE_COLOR)

# Additive source alpha can carry authored intensity above one. Only the
# AlphaComposite core needs bounded coverage; retain source alpha for sparks.
opacity_node = editing.get_material_property_input_node(master, unreal.MaterialProperty.MP_OPACITY)
if not isinstance(opacity_node, unreal.MaterialExpressionStaticSwitchParameter):
    if not isinstance(opacity_node, unreal.MaterialExpressionSaturate):
        raise RuntimeError('Expected final travel opacity clamp')
    inputs = editing.get_inputs_for_material_expression(master, opacity_node)
    if len(inputs) != 1:
        raise RuntimeError('Could not find original masked opacity')
    opacity_switch = editing.create_material_expression(master, unreal.MaterialExpressionStaticSwitchParameter, 2350, 400)
    opacity_switch.set_editor_property('parameter_name', 'TravelPremultiply')
    opacity_switch.set_editor_property('default_value', False)
    connect(opacity_node, '', opacity_switch, 'True')
    connect(inputs[0], '', opacity_switch, 'False')
    output(opacity_switch, unreal.MaterialProperty.MP_OPACITY)

# Align core coverage with the inherited glow mask. Keep emitted light identical:
# old: particle RGB * glow * gain * bounded opacity
# new: particle RGB * gain * (bounded opacity * glow)
# The glow appears once in both expressions. Only background occlusion decreases.
coverage_nodes = [n for n in editing.get_material_expressions(master)
                  if isinstance(n, unreal.MaterialExpressionSaturate)
                  and n.get_editor_property('desc') == 'ThunderTravelCoreCoverage']
if not coverage_nodes:
    opacity_switch = editing.get_material_property_input_node(master, unreal.MaterialProperty.MP_OPACITY)
    emission_switch = editing.get_material_property_input_node(master, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if not isinstance(opacity_switch, unreal.MaterialExpressionStaticSwitchParameter):
        raise RuntimeError('Expected travel opacity switch')
    opacity_inputs = editing.get_inputs_for_material_expression(master, opacity_switch)
    bounded = next(n for n in opacity_inputs if isinstance(n, unreal.MaterialExpressionSaturate))
    emission_inputs = editing.get_inputs_for_material_expression(master, emission_switch)
    premultiplied = next(n for n in emission_inputs
                        if isinstance(n, unreal.MaterialExpressionMultiply)
                        and bounded in editing.get_inputs_for_material_expression(master, n))
    expressions = editing.get_material_expressions(master)
    particle = next(n for n in expressions if isinstance(n, unreal.MaterialExpressionParticleColor))
    glow = next(n for n in expressions if isinstance(n, unreal.MaterialExpressionPower))
    gain = next(n for n in expressions if isinstance(n, unreal.MaterialExpressionScalarParameter)
                and str(n.get_editor_property('parameter_name')) == 'TravelEmissiveGain')

    bounded_glow = editing.create_material_expression(master, unreal.MaterialExpressionSaturate, 1750, 650)
    connect(glow, '', bounded_glow, '')
    coverage_product = editing.create_material_expression(master, unreal.MaterialExpressionMultiply, 1950, 450)
    connect(bounded, '', coverage_product, 'A')
    connect(bounded_glow, '', coverage_product, 'B')
    coverage = editing.create_material_expression(master, unreal.MaterialExpressionSaturate, 2150, 450)
    coverage.set_editor_property('desc', 'ThunderTravelCoreCoverage')
    connect(coverage_product, '', coverage, '')
    connect(coverage, '', opacity_switch, 'True')

    core_positive = editing.create_material_expression(master, unreal.MaterialExpressionMax, 1750, -250)
    core_positive.set_editor_property('const_b', 0.0)
    connect(particle, 'RGB', core_positive, 'A')
    core_gain = editing.create_material_expression(master, unreal.MaterialExpressionMultiply, 1950, -250)
    connect(core_positive, '', core_gain, 'A')
    connect(gain, '', core_gain, 'B')
    connect(core_gain, '', premultiplied, 'A')
    connect(coverage, '', premultiplied, 'B')
editing.recompile_material(master)

materials = [master]
instance_specs = [
    ('MI_BusterThunderTravelCore', '/Game/FXVarietyPack/Materials/MI_ky_smoke01b', True),
    ('MI_BusterThunderTravelSparks', '/Game/FXVarietyPack/Materials/MI_ky_smoke01', False),
]
for name, source, core in instance_specs:
    if material_only:
        material = unreal.load_asset(folder + '/' + name)
        if not material or material.get_editor_property('parent') != master:
            raise RuntimeError('Material-only update requires existing travel instances')
        continue
    material = duplicate(source, name)
    editing.set_material_instance_parent(material, master)
    overrides = material.get_editor_property('base_property_overrides')
    overrides.set_editor_property('override_blend_mode', True)
    overrides.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ALPHA_COMPOSITE if core else unreal.BlendMode.BLEND_ADDITIVE)
    overrides.set_editor_property('override_shading_model', False)
    material.set_editor_property('base_property_overrides', overrides)
    # UE 5.8's setter returns false even after applying the value; verify by reading it.
    editing.set_material_instance_static_switch_parameter_value(material, 'TravelPremultiply', core)
    editing.update_material_instance(material)
    if editing.get_material_instance_static_switch_parameter_value(material, 'TravelPremultiply') != core:
        raise RuntimeError('Could not verify premultiplication for ' + name)
    materials.append(material)

for material in materials:
    if not assets.save_loaded_asset(material):
        raise RuntimeError('Could not save ' + material.get_path_name())
    unreal.log('THUNDER_TRAVEL saved ' + material.get_path_name())

if not material_only:
    unreal.SystemLibrary.execute_console_command(None, 'Quasicombo.AuthorBusterThunderTravel')
unreal.log('THUNDER_TRAVEL authoring complete (' + ('travel master only' if material_only else 'travel system + three materials only') + ')')
