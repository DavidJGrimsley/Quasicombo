"""Author the single configurable Quasicombo enemy and its golem attack assets."""
import unreal

LIB = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ROOT = "/Game/Quasicombo/Enemies"
GOLEM = ROOT + "/StoneGolem"


def asset(path):
    result = unreal.load_asset(path)
    if not result:
        raise RuntimeError("Missing asset: " + path)
    return result


def create(name, folder, cls, factory):
    path = folder + "/" + name
    if LIB.does_asset_exist(path):
        return asset(path)
    result = TOOLS.create_asset(name, folder, cls, factory)
    if not result:
        raise RuntimeError("Could not create " + path)
    return result


def save(value):
    if not LIB.save_loaded_asset(value):
        raise RuntimeError("Could not save " + value.get_path_name())


source_mesh = asset("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")
golem_mesh = asset("/Game/Stone_Golem/mesh/SKM_Stone_Golem")
rigs = []
for name, mesh in [("IK_EnemyManny", source_mesh), ("IK_StoneGolem", golem_mesh)]:
    is_new = not LIB.does_asset_exist(GOLEM + "/" + name)
    rig = create(name, GOLEM, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    if is_new:
        controller = unreal.IKRigController.get_controller(rig)
        if not controller.set_skeletal_mesh(mesh):
            raise RuntimeError("Could not set mesh on " + name)
        if not controller.apply_auto_generated_retarget_definition():
            raise RuntimeError("Could not generate retarget chains for " + name)
        save(rig)
    rigs.append(rig)

retarget_is_new = not LIB.does_asset_exist(GOLEM + "/RTG_Manny_StoneGolem")
retarget = create("RTG_Manny_StoneGolem", GOLEM, unreal.IKRetargeter, unreal.IKRetargetFactory())
if retarget_is_new:
    controller = unreal.IKRetargeterController.get_controller(retarget)
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, rigs[0])
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, rigs[1])
    controller.add_default_ops()
    controller.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    controller.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
    save(retarget)

golem_montage_path = GOLEM + "/Golem_WM_AM_ComboAttack"
if not LIB.does_asset_exist(golem_montage_path):
    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.set_editor_property("assets_to_retarget", [
        LIB.find_asset_data(ROOT + "/WoodMonster/Anims/WM_AM_ComboAttack")])
    inputs.set_editor_property("source_mesh", source_mesh)
    inputs.set_editor_property("target_mesh", golem_mesh)
    inputs.set_editor_property("ik_retarget_asset", retarget)
    inputs.set_editor_property("target_path", GOLEM)
    inputs.set_editor_property("prefix", "Golem_")
    inputs.set_editor_property("include_referenced_assets", True)
    inputs.set_editor_property("overwrite_existing_files", True)
    created = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    print("QC_GOLEM_RETARGETED", [str(x.package_name) for x in created])
    for data in created:
        save(data.get_asset())
golem_montage = asset(golem_montage_path)
if golem_montage.get_skeleton() != golem_mesh.get_editor_property("skeleton"):
    raise RuntimeError("Retargeted golem montage has the wrong skeleton")
wood_montage = asset(ROOT + "/WoodMonster/Anims/WM_AM_ComboAttack")
for montage in (wood_montage, golem_montage):
    montage.set_editor_property("rate_scale", 1.2)
    save(montage)

parent = unreal.load_class(None, "/Script/Braided_Quanta2026.QuasicomboEnemy")
if not parent:
    raise RuntimeError("QuasicomboEnemy C++ class is unavailable")
path = ROOT + "/BP_QC_Enemy"
if LIB.does_asset_exist(path):
    bp = asset(path)
else:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    bp = create("BP_QC_Enemy", ROOT, unreal.Blueprint, factory)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
klass = LIB.load_blueprint_class(path)
if not klass:
    raise RuntimeError("BP_QC_Enemy did not generate a class")
cdo = unreal.get_default_object(klass)
cdo.set_editor_property("aggro_range", 800.0)
cdo.set_editor_property("attack_range", 150.0)
cdo.set_editor_property("patrol_half_width", 700.0)
cdo.set_editor_property("attack_cooldown", 1.0)
cdo.set_editor_property("attack_windup", 0.2)
cdo.set_editor_property("recovery_duration", 0.25)
life_bar = unreal.load_class(None, "/Game/Variant_Combat/UI/UI_LifeBar.UI_LifeBar_C")
if not life_bar:
    raise RuntimeError("Enemy life bar widget is unavailable")
cdo.set_editor_property("enemy_life_bar_class", life_bar)
cdo.set_editor_property("damage_effect", asset("/Game/Variant_Combat/VFX/NS_Damage"))
body = cdo.get_component_by_class(unreal.SkeletalMeshComponent)
body.set_collision_profile_name("Ragdoll")
body.set_collision_response_to_channel(unreal.CollisionChannel.ECC_CAMERA, unreal.CollisionResponseType.ECR_IGNORE)
cdo.get_component_by_class(unreal.CapsuleComponent).set_collision_response_to_channel(
    unreal.CollisionChannel.ECC_CAMERA, unreal.CollisionResponseType.ECR_IGNORE)

wood_root = "/Game/Wood_Monster/CharacterParts"
wood_material = wood_root + "/Materials/"
cdo.set_editor_property("wood_mesh", asset(wood_root + "/Meshes/SK_wood_giant_01_b"))
wood_anim = unreal.load_class(None, "/Game/Wood_Monster/DemoContent/Mannequins/Animations/ThirdPerson_retarget/ABP_Manny.ABP_Manny_C")
if not wood_anim:
    raise RuntimeError("Wood animation class is unavailable")
cdo.set_editor_property("wood_anim_class", wood_anim)
cdo.set_editor_property("wood_combo_montage", wood_montage)
cdo.set_editor_property("wood_charged_montage", asset(ROOT + "/WoodMonster/Anims/WM_AM_ChargedAttack"))
for field, names in [
    ("wood_light_materials", ["MI_Wood_monster_base", "MI_leaves", "MI_monster_orb_glow"]),
    ("wood_medium_materials", ["MI_monster_b_base", "MI_leaves", "MI_monster_orb_glow"]),
    ("wood_heavy_materials", ["MI_monster_b_dark_red", "MI_leaves_red", "MI_monster_orb_red_brown"]),
]:
    cdo.set_editor_property(field, [asset(wood_material + name) for name in names])

cdo.set_editor_property("golem_mesh", golem_mesh)
golem_anim = unreal.load_class(None, "/Script/Braided_Quanta2026.QuasicomboGolemAnimInstance")
if not golem_anim:
    raise RuntimeError("Golem animation class is unavailable")
cdo.set_editor_property("golem_anim_class", golem_anim)
cdo.set_editor_property("golem_idle_animation", asset("/Game/Stone_Golem/demo/animations/ThirdPersonIdle"))
cdo.set_editor_property("golem_walk_animation", asset("/Game/Stone_Golem/demo/animations/ThirdPersonWalk"))
cdo.set_editor_property("golem_combo_montage", golem_montage)
for field, name in [
    ("golem_light_material", "MI_Stone_Golem_Inst"),
    ("golem_medium_material", "MI_Stone_Golem_Inst1"),
    ("golem_heavy_material", "MI_Stone_Golem_Inst2"),
]:
    cdo.set_editor_property(field, asset("/Game/Stone_Golem/materials/" + name))

unreal.BlueprintEditorLibrary.compile_blueprint(bp)
status = str(bp.get_editor_property("status"))
if "ERROR" in status.upper():
    raise RuntimeError("BP_QC_Enemy compilation failed: " + status)
save(bp)
print("QC_ENEMY_READY", path, "status", status)
