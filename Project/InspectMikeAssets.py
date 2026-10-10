"""Inspect Mike asset compatibility and available editor authoring APIs."""

import unreal


def asset(path):
    value = unreal.EditorAssetLibrary.load_asset(path)
    if value is None:
        raise RuntimeError(f"Missing asset: {path}")
    print("MIKE_ASSET", path, value.get_class().get_name())
    return value


mesh = asset("/Game/RadicalMike/Mesh/SKM_MegaMikeZ")
skeleton = asset("/Game/RadicalMike/Mesh/SK_MegaMikeZ")
physics = asset("/Game/RadicalMike/Mesh/PA_MegaMikeZ")
print("MIKE_MESH_SKELETON", mesh.get_editor_property("skeleton").get_path_name())
print("MIKE_MESH_PHYSICS", mesh.get_editor_property("physics_asset").get_path_name())

for name in (
    "Idle", "Walk", "Run", "JumpStart", "Jump", "JumpApex", "JumpEnd",
    "Run_Faster", "PunchR", "PunchL", "UppercutR", "IdleAggro",
    "HitRegisterFront", "HitRegisterBack", "HitRegisterL", "HitRegisterR",
    "HitRegisterFront_Death", "DeathState",
):
    animation = asset(f"/Game/RadicalMike/Animations/Anim_ZMIKE_{name}")
    print(
        "MIKE_ANIM", name,
        "skeleton", animation.get_editor_property("skeleton").get_path_name(),
        "length", animation.get_play_length(),
        "root_motion", animation.get_editor_property("enable_root_motion"),
    )

for type_name in (
    "AnimationBlueprintFactory", "BlendSpaceFactory1D", "AnimMontageFactory",
    "AnimGraphNode_SequencePlayer", "AnimGraphNode_BlendSpacePlayer",
    "AnimGraphNode_Slot", "AnimGraphNode_StateMachine",
    "AnimationLibrary", "BlueprintEditorLibrary", "BlueprintGraphLibrary",
    "AnimMontageLibrary",
):
    cls = getattr(unreal, type_name, None)
    print("MIKE_API", type_name, bool(cls))
    if cls is not None and type_name in ("AnimationLibrary", "BlueprintEditorLibrary", "BlueprintGraphLibrary"):
        print("MIKE_API_METHODS", type_name, ",".join(x for x in dir(cls) if any(t in x for t in ("node", "graph", "notify", "montage", "pin", "compile"))))

blueprint = asset("/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter")
klass = unreal.EditorAssetLibrary.load_blueprint_class(blueprint.get_path_name())
cdo = unreal.get_default_object(klass)
mesh_component = cdo.get_editor_property("mesh")
print("MIKE_BP_MESH_COMPONENT", mesh_component.get_class().get_name())
print("MIKE_BP_MESH_RELATIVE", mesh_component.get_editor_property("relative_location"), mesh_component.get_editor_property("relative_rotation"), mesh_component.get_editor_property("relative_scale3d"))
print("MIKE_BP_VISUAL_OVERRIDE", cdo.get_editor_property("character_visual_mesh_override"))
print("MIKE_BP_ANIM_OVERRIDE", cdo.get_editor_property("character_visual_anim_class_override"))
