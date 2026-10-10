"""Author Buster presentation assets; run with Unreal Editor -ExecutePythonScript.

Idempotent. Existing marketplace animation sequences are read only.
"""
import unreal

library = unreal.EditorAssetLibrary
root = "/Game/Quasicombo/Mike"
tools = unreal.AssetToolsHelpers.get_asset_tools()
path = root + "/M_BusterEnergy"
material = library.load_asset(path) if library.does_asset_exist(path) else None
if not material:
    material = tools.create_asset("M_BusterEnergy", root, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    editing = unreal.MaterialEditingLibrary
    color = editing.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -500, 0)
    color.set_editor_property("parameter_name", "LightColor")
    color.set_editor_property("default_value", unreal.LinearColor(0.175, 0.367, 1, 1))
    strength = editing.create_material_expression(material, unreal.MaterialExpressionConstant, -500, 150)
    strength.set_editor_property("r", 0.8)
    multiply = editing.create_material_expression(material, unreal.MaterialExpressionMultiply, -220, 0)
    editing.connect_material_expressions(color, "", multiply, "A")
    editing.connect_material_expressions(strength, "", multiply, "B")
    editing.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity = editing.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -200, 250)
    opacity.set_editor_property("parameter_name", "Opacity")
    opacity.set_editor_property("default_value", 1.0)
    editing.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    editing.recompile_material(material)
    if not library.save_loaded_asset(material):
        raise RuntimeError("Could not save Buster energy material")

# Additive blending made the bolt look transparent against the outdoor level.
# Full-opacity translucent blending preserves the chosen color, while the
# shared Opacity parameter still lets impact pulses fade away.
material.modify()
material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
strength_nodes = [node for node in unreal.MaterialEditingLibrary.get_material_expressions(material)
                  if isinstance(node, unreal.MaterialExpressionConstant)]
if len(strength_nodes) != 1:
    raise RuntimeError("Expected one Buster energy brightness constant")
strength_nodes[0].set_editor_property("r", 1.0)
unreal.MaterialEditingLibrary.recompile_material(material)
if not library.save_loaded_asset(material, only_if_is_dirty=False):
    raise RuntimeError("Could not save Buster energy brightness")

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(world, "Quasicombo.AuthorBusterLayer")
bp = library.load_asset(root + "/ABP_Mike_SideScroller")
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
if "ERROR" in str(bp.get_editor_property("status")).upper():
    raise RuntimeError("Buster Animation Blueprint failed compilation")
slots = unreal.AnimationLibrary.get_nodes_of_class(bp, unreal.AnimGraphNode_Slot)
assert any(str(n.get_editor_property("node").get_editor_property("slot_name")) == "BusterArm" for n in slots)
assert library.save_loaded_asset(bp)

for label in ("BusterEquip_R", "BusterIdle_R", "FireR", "JumpEnd"):
    sequence = library.load_asset("/Game/RadicalMike/Animations/Anim_ZMIKE_" + label)
    assert sequence
    print("BUSTER_SEQUENCE", label, sequence.get_play_length(), sequence.get_editor_property("enable_root_motion"))

for path in ("/Game/RadicalMike/Materials/Body/MI_ZMike_default", "/Game/RadicalMike/Materials/Buster/MI_RBuster_Default"):
    instance = library.load_asset(path)
    names = unreal.MaterialEditingLibrary.get_vector_parameter_names(instance)
    assert "EmissiveColorPicker" in [str(n) for n in names], (path, names)
    print("BUSTER_MATERIAL", path, "EmissiveColorPicker verified")

mesh = library.load_asset("/Game/RadicalMike/Mesh/SM_RadicalBuster")
print("BUSTER_MESH_API", [x for x in dir(mesh) if any(t in x for t in ("bound", "socket"))])
print("BUSTER_POSE_API", unreal.AnimationLibrary.get_bone_pose_for_time.__doc__)
print("BUSTER_AUTHOR_SUCCESS")

exec(open(unreal.Paths.project_dir() + "Project/InspectBusterAssets.py", encoding="utf-8").read())
