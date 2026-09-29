"""Build only the lizard boss assets. Run with the UE Python editor commandlet."""
import math
import unreal

LIB = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ROOT = "/Game/Quasicombo/Boss/Lizard"
ANIMS = ROOT + "/Animations"

def asset(path):
    value = unreal.load_asset(path)
    if not value:
        raise RuntimeError("Missing asset: " + path)
    return value

def create(name, folder, cls, factory):
    path = folder + "/" + name
    if LIB.does_asset_exist(path):
        return asset(path)
    value = TOOLS.create_asset(name, folder, cls, factory)
    if not value:
        raise RuntimeError("Could not create " + path)
    return value

def save(value):
    if not LIB.save_loaded_asset(value):
        raise RuntimeError("Could not save " + value.get_path_name())

source = asset("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")
target = asset("/Game/Lizardman_Berserker/Mesh/SeparatedMesh/SK_Body")
rigs = []
for name, mesh in [("IK_BossManny", source), ("IK_LizardBoss", target)]:
    rig = create(name, ROOT, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    ctl = unreal.IKRigController.get_controller(rig)
    assert ctl.set_skeletal_mesh(mesh)
    assert ctl.apply_auto_generated_retarget_definition(), "Could not characterize " + name
    save(rig)
    rigs.append(rig)
retarget = create("RTG_Manny_LizardBoss", ROOT, unreal.IKRetargeter, unreal.IKRetargetFactory())
ctl = unreal.IKRetargeterController.get_controller(retarget)
ctl.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, rigs[0])
ctl.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, rigs[1])
ctl.add_default_ops()
ctl.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
ctl.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
save(retarget)
inputs = unreal.IKRetargetBatchOperationInputs()
inputs.set_editor_property("assets_to_retarget", [
    LIB.find_asset_data("/Game/Variant_Combat/Anims/AM_ComboAttack"),
    LIB.find_asset_data("/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_01")])
inputs.set_editor_property("source_mesh", source)
inputs.set_editor_property("target_mesh", target)
inputs.set_editor_property("ik_retarget_asset", retarget)
inputs.set_editor_property("target_path", ANIMS)
inputs.set_editor_property("prefix", "Lizard_")
inputs.set_editor_property("include_referenced_assets", True)
inputs.set_editor_property("overwrite_existing_files", True)
expected = ["Lizard_AM_ComboAttack", "Lizard_MM_Attack_01", "Lizard_MM_Attack_02", "Lizard_MM_Attack_03", "Lizard_MM_Death_Front_01"]
created = [] if all(LIB.does_asset_exist(ANIMS + "/" + name) for name in expected) else unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
print("BQ_RETARGETED", [str(x.package_name) for x in created])
for data in created:
    save(data.get_asset())

# Bake a readable 1.8 second spin and tail whip into a dedicated sequence.
# Work exclusively on our generated copy; preserve every imported skeleton/bone.
attack = asset(ANIMS + "/Lizard_MM_Attack_03")
tail_path = ANIMS + "/AS_Lizard_TailSweep"
tail = asset(tail_path) if LIB.does_asset_exist(tail_path) else LIB.duplicate_asset(attack.get_path_name(), tail_path)
controller = tail.get_editor_property("controller")
print("BQ_CONTROLLER", controller)
duration = 1.8
frames = 54
source_length = attack.get_play_length()
track_names = list(unreal.AnimationLibrary.get_animation_track_names(attack))
for i in range(1, 8):
    name = "u_Tail_%02d" % i
    if name not in [str(x) for x in track_names]:
        track_names.append(name)
poses = {}
for name in track_names:
    poses[str(name)] = [unreal.AnimationLibrary.get_bone_pose_for_time(attack, name, i / frames * source_length, False) for i in range(frames + 1)]
controller.open_bracket("Bake boss tail sweep", False)
controller.set_frame_rate(unreal.FrameRate(30, 1), False)
controller.set_number_of_frames(unreal.FrameNumber(frames), False)

def smooth(x):
    x = max(0.0, min(1.0, x))
    return x*x*(3.0-2.0*x)

for name, samples in poses.items():
    if name not in [str(x) for x in unreal.AnimationLibrary.get_animation_track_names(tail)]:
        controller.add_bone_curve(name, False)
    positions, rotations, scales = [], [], []
    for i, pose in enumerate(samples):
        t = i/30.0
        pos, rot, scale = pose.translation, pose.rotation, pose.scale3d
        if name == "root":
            # Wind up away, sweep through the player's side, then finish the turn.
            angle = -35.0*smooth(t/0.65) if t < 0.65 else (-35.0+300.0*smooth((t-0.65)/0.6) if t < 1.25 else 265.0+95.0*smooth((t-1.25)/0.55))
            rot = unreal.Rotator(yaw=angle).quaternion() * samples[0].rotation
            pos = samples[0].translation
        elif name.startswith("u_Tail_"):
            n = int(name[-2:])
            # Distributed delayed curvature gives a whip instead of a rigid rod.
            phase = max(0.0, min(1.0, (t-0.4-(n-1)*0.025)/0.95))
            bend = math.sin(phase*math.pi*2.0)*13.0*math.sin(min(t/0.25,1.0)*math.pi/2.0)
            rot = samples[0].rotation * unreal.Rotator(yaw=bend).quaternion()
            pos = samples[0].translation
        positions.append(pos)
        rotations.append(rot)
        scales.append(scale)
    assert controller.set_bone_track_keys(name, positions, rotations, scales, False), name
controller.close_bracket(False)
tail.set_editor_property("enable_root_motion", False)
tail.set_editor_property("force_root_lock", False)
# Retargeted attack notifies would fire punch traces; the boss owns the tail's contact window.
unreal.AnimationLibrary.remove_all_animation_notify_tracks(tail)
save(tail)

bp = asset("/Game/Quasicombo/Boss/BP_QC_Boss")
klass = LIB.load_blueprint_class("/Game/Quasicombo/Boss/BP_QC_Boss")
cdo = unreal.get_default_object(klass)
cdo.set_editor_property("required_hits", 4)
cdo.set_editor_property("base_health_hits", 4)
cdo.set_editor_property("combo_attack_montage", asset(ANIMS + "/Lizard_AM_ComboAttack"))
cdo.set_editor_property("charged_attack_montage", None)
cdo.set_editor_property("tail_attack_animation", tail)
for property_name, clip_name in [("hit_reaction_animation", "Lizard_MM_HitReact_Front_Med_01"),
                                 ("dazed_animation", "AS_Lizard_DazedIdle")]:
    if LIB.does_asset_exist(ANIMS + "/" + clip_name):
        cdo.set_editor_property(property_name, asset(ANIMS + "/" + clip_name))
cdo.set_editor_property("defeat_animation", asset(ANIMS + ("/Lizard_MM_Death_Front_02" if LIB.does_asset_exist(ANIMS + "/Lizard_MM_Death_Front_02") else "/Lizard_MM_Death_Front_01")))
mesh = cdo.get_component_by_class(unreal.SkeletalMeshComponent)
# get_mesh is unambiguous even with the armor subcomponents.
mesh = cdo.mesh
mesh.set_skeletal_mesh_asset(target)
mesh.set_anim_instance_class(unreal.load_class(None, "/Script/Braided_Quanta2026.QuasicomboBossAnimInstance"))
mesh.set_relative_location(unreal.Vector(0,0,-87), False, False)
mesh.set_relative_rotation(unreal.Rotator(yaw=-90), False, False)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
if "ERROR" in str(bp.get_editor_property("status")).upper():
    raise RuntimeError("Boss Blueprint failed to compile")
save(bp)

editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
levels.load_level("/Game/Maps/Lvl_Main")
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
boss = next(a for a in actors if a.get_actor_label()=="QC_Boss")
boss.set_editor_property("required_hits",4)
boss.mesh.set_skeletal_mesh_asset(target)
boss.mesh.set_anim_instance_class(unreal.load_class(None, "/Script/Braided_Quanta2026.QuasicomboBossAnimInstance"))
boss.mesh.set_relative_rotation(unreal.Rotator(yaw=-90), False, False)
boss.set_editor_property("combo_attack_montage", asset(ANIMS + "/Lizard_AM_ComboAttack"))
boss.set_editor_property("charged_attack_montage",None)
assert levels.save_current_level()
print("BQ_LIZARD_READY", boss.get_path_name(), len(boss.get_components_by_class(unreal.SkeletalMeshComponent)))

