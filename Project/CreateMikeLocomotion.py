"""Create Mike's locomotion blend space and adapt the side-scroller AnimBP.

Run in the Unreal Editor Python environment after building the editor target.
The original Manny animation assets are only read, never modified.
"""

import unreal


ROOT = "/Game/Quasicombo/Mike"
SOURCE_ABP = "/Game/Variant_SideScrolling/Anims/ABP_Manny_SideScroller"
BLEND_PATH = ROOT + "/BS_Mike_Locomotion"
ABP_PATH = ROOT + "/ABP_Mike_SideScroller"

library = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
skeleton = library.load_asset("/Game/RadicalMike/Mesh/SK_MegaMikeZ")
mesh = library.load_asset("/Game/RadicalMike/Mesh/SKM_MegaMikeZ")
if not skeleton or not mesh:
    raise RuntimeError("Mike mesh or skeleton is missing")


def sequence(name):
    path = f"/Game/RadicalMike/Animations/Anim_ZMIKE_{name}"
    value = library.load_asset(path)
    if not value or value.get_editor_property("skeleton") != skeleton:
        raise RuntimeError(f"Missing or incompatible animation: {path}")
    return value


animations = {name: sequence(name) for name in (
    "Idle", "Walk", "WalkLeft", "WalkRight", "Walk_Back", "Run",
    "JumpStart", "Jump", "JumpApex", "JumpEnd",
)}

if library.does_asset_exist(BLEND_PATH):
    blend = library.load_asset(BLEND_PATH)
else:
    factory = unreal.BlendSpaceFactoryNew()
    factory.set_editor_property("target_skeleton", skeleton)
    factory.set_editor_property("preview_skeletal_mesh", mesh)
    blend = tools.create_asset("BS_Mike_Locomotion", ROOT, unreal.BlendSpace, factory)
if not blend:
    raise RuntimeError("Could not create Mike locomotion blend space")

parameters = list(blend.get_editor_property("blend_parameters"))
parameters[0].set_editor_property("display_name", "Direction")
parameters[0].set_editor_property("min", -180.0)
parameters[0].set_editor_property("max", 180.0)
parameters[1].set_editor_property("display_name", "Speed")
parameters[1].set_editor_property("min", 0.0)
parameters[1].set_editor_property("max", 500.0)
blend.set_editor_property("blend_parameters", parameters)

samples = []
for direction in (-180.0, -90.0, 0.0, 90.0, 180.0):
    for speed, name in (
        (0.0, "Idle"),
        (220.0, "WalkLeft" if direction == -90.0 else "WalkRight" if direction == 90.0 else "Walk_Back" if abs(direction) == 180.0 else "Walk"),
        (500.0, "Run"),
    ):
        sample = unreal.BlendSample()
        sample.set_editor_property("animation", animations[name])
        sample.set_editor_property("sample_value", unreal.Vector(direction, speed, 0.0))
        samples.append(sample)
blend.set_editor_property("sample_data", samples)
blend.set_editor_property("target_weight_interpolation_speed_per_sec", 8.0)
if not library.save_loaded_asset(blend):
    raise RuntimeError("Could not save Mike blend space")
# Directly setting sample_data does not build BlendSpaceData's runtime
# triangles. The editor command recalculates and saves them in this session.
editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(editor_world, "Quasicombo.RebuildMikeBlendSpace")
print("MIKE_CREATED_BLEND", blend.get_path_name(), len(blend.get_editor_property("sample_data")))

if library.does_asset_exist(ABP_PATH):
    abp = library.load_asset(ABP_PATH)
else:
    if not library.duplicate_asset(SOURCE_ABP, ABP_PATH):
        raise RuntimeError("Could not duplicate side-scroller AnimBP")
    abp = library.load_asset(ABP_PATH)
if not abp:
    raise RuntimeError("Could not load Mike AnimBP")

abp.set_editor_property("target_skeleton", skeleton)
for node in unreal.AnimationLibrary.get_nodes_of_class(abp, unreal.AnimGraphNode_SequencePlayer):
    path = node.get_path_name()
    if ".Idle." in path:
        animation = animations["Idle"]
    elif ".Wall Jump." in path or ".Double Jump." in path:
        animation = animations["Jump"]
    elif ".Jump." in path:
        animation = animations["JumpStart"]
    elif ".Fall Loop." in path:
        animation = animations["JumpApex"]
    elif ".Land." in path:
        animation = animations["JumpEnd"]
    else:
        raise RuntimeError(f"Unexpected sequence player: {path}")
    data = node.get_editor_property("node")
    data.set_editor_property("sequence", animation)
    node.set_editor_property("node", data)
    print("MIKE_ABP_NODE", path, animation.get_name())

blend_nodes = unreal.AnimationLibrary.get_nodes_of_class(abp, unreal.AnimGraphNode_BlendSpacePlayer)
if len(blend_nodes) != 1:
    raise RuntimeError(f"Expected one locomotion blend node, found {len(blend_nodes)}")
data = blend_nodes[0].get_editor_property("node")
data.set_editor_property("blend_space", blend)
blend_nodes[0].set_editor_property("node", data)

# The template routes its final pose through a Manny-specific foot IK Control
# Rig. Mike's hierarchy differs, so route the full-body slot directly to the
# result instead of evaluating that incompatible rig.
root_nodes = unreal.AnimationLibrary.get_nodes_of_class(abp, unreal.AnimGraphNode_Root)
slot_nodes = unreal.AnimationLibrary.get_nodes_of_class(abp, unreal.AnimGraphNode_Slot)
rig_nodes = unreal.AnimationLibrary.get_nodes_of_class(abp, unreal.AnimGraphNode_ControlRig)
if len(root_nodes) != 1 or len(slot_nodes) != 1 or len(rig_nodes) > 1:
    raise RuntimeError("Unexpected root, slot, or Manny Control Rig layout")


def pin(node, name):
    for candidate in unreal.BlueprintEditorLibrary.list_all_pins(node):
        if str(unreal.BlueprintGraphPinLibrary.get_pin_name(candidate)) == name:
            return candidate
    raise RuntimeError(f"Missing {name} pin on {node.get_name()}")


pins = unreal.BlueprintGraphPinLibrary
if rig_nodes:
    pins.break_pin_links(pin(root_nodes[0], "Result"))
    pins.break_pin_links(pin(rig_nodes[0], "Source"))
    if not pins.try_create_connection(pin(slot_nodes[0], "Pose"), pin(root_nodes[0], "Result")):
        raise RuntimeError("Could not bypass Manny Control Rig")
    unreal.BlueprintEditorLibrary.remove_unused_nodes(abp)

unreal.BlueprintEditorLibrary.compile_blueprint(abp)
status = str(abp.get_editor_property("status"))
print("MIKE_ABP_STATUS", status)
if "ERROR" in status.upper():
    raise RuntimeError("Mike AnimBP did not compile")
if not library.save_loaded_asset(abp):
    raise RuntimeError("Could not save Mike AnimBP")
print("MIKE_CREATED_ABP", abp.get_path_name(), abp.get_editor_property("target_skeleton").get_path_name())
