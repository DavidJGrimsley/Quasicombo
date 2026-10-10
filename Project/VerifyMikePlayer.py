"""Read-only checks for the saved Mike player animation assets."""

import unreal


assets = unreal.EditorAssetLibrary
root = "/Game/Quasicombo/Mike"
sound_class = assets.load_asset(root + "/Sounds/SC_Mike_Punches")
assert isinstance(sound_class, unreal.SoundClass)
assert 0.0 <= sound_class.get_editor_property("properties").get_editor_property("volume") <= 1.0
mesh = assets.load_asset("/Game/RadicalMike/Mesh/SKM_MegaMikeZ")
skeleton = assets.load_asset("/Game/RadicalMike/Mesh/SK_MegaMikeZ")
physics = mesh.get_editor_property("physics_asset") if mesh else None
assert mesh and skeleton and physics, "Mike mesh, skeleton, or physics asset is missing"
assert mesh.get_editor_property("skeleton") == skeleton
print("MIKE_VERIFY_MESH", mesh.get_path_name(), physics.get_path_name())

blend = assets.load_asset(root + "/BS_Mike_Locomotion")
assert blend and blend.get_editor_property("skeleton") == skeleton
samples = blend.get_editor_property("sample_data")
assert len(samples) >= 9
for sample in samples:
    assert sample.get_editor_property("animation").get_editor_property("skeleton") == skeleton
print("MIKE_VERIFY_BLEND", len(samples))

abp = assets.load_asset(root + "/ABP_Mike_SideScroller")
assert abp and abp.get_editor_property("target_skeleton") == skeleton
unreal.BlueprintEditorLibrary.compile_blueprint(abp)
print("MIKE_VERIFY_ABP_STATUS", abp.get_editor_property("status"))
assert "BS_UP_TO_DATE" in str(abp.get_editor_property("status"))
sequence_nodes = unreal.AnimationLibrary.get_nodes_of_class(abp, unreal.AnimGraphNode_SequencePlayer)
assert len(sequence_nodes) >= 5
for node in sequence_nodes:
    sequence = node.get_editor_property("node").get_editor_property("sequence")
    assert sequence and sequence.get_path_name().startswith("/Game/RadicalMike/"), node.get_path_name()
blend_nodes = unreal.AnimationLibrary.get_nodes_of_class(abp, unreal.AnimGraphNode_BlendSpacePlayer)
assert len(blend_nodes) == 1
assert blend_nodes[0].get_editor_property("node").get_editor_property("blend_space") == blend
slot_nodes = unreal.AnimationLibrary.get_nodes_of_class(abp, unreal.AnimGraphNode_Slot)
slots_by_name = {str(node.get_editor_property("node").get_editor_property("slot_name")): node
                 for node in slot_nodes}
assert set(slots_by_name) == {"DefaultSlot", "BusterArm"}
arm_source = next(pin for pin in unreal.BlueprintEditorLibrary.list_all_pins(slots_by_name["BusterArm"])
                  if str(unreal.BlueprintGraphPinLibrary.get_pin_name(pin)) == "Source")
arm_sources = unreal.BlueprintGraphPinLibrary.list_connected_pins(arm_source)
cache_saves = unreal.AnimationLibrary.get_nodes_of_class(abp, unreal.AnimGraphNode_SaveCachedPose)
cache_uses = unreal.AnimationLibrary.get_nodes_of_class(abp, unreal.AnimGraphNode_UseCachedPose)
assert any(node.get_editor_property("cache_name") == "MikeLocomotionForBuster" for node in cache_saves)
assert len(cache_uses) >= 2 and len(arm_sources) == 1
assert any(unreal.BlueprintGraphPinLibrary.is_same_native_pin(arm_sources[0], pose)
           for node in cache_uses
           for pose in unreal.BlueprintEditorLibrary.list_all_pins(node)
           if str(unreal.BlueprintGraphPinLibrary.get_pin_name(pose)) == "Pose"), "Buster arm must follow locomotion"
root_nodes = unreal.AnimationLibrary.get_nodes_of_class(abp, unreal.AnimGraphNode_Root)
assert len(root_nodes) == 1
root_result = next(pin for pin in unreal.BlueprintEditorLibrary.list_all_pins(root_nodes[0])
                   if str(unreal.BlueprintGraphPinLibrary.get_pin_name(pin)) == "Result")
slot_pose = next(pin for pin in unreal.BlueprintEditorLibrary.list_all_pins(slots_by_name["DefaultSlot"])
                 if str(unreal.BlueprintGraphPinLibrary.get_pin_name(pin)) == "Pose")
assert any(unreal.BlueprintGraphPinLibrary.is_same_native_pin(link, slot_pose)
           for link in unreal.BlueprintGraphPinLibrary.list_connected_pins(root_result))
layer = unreal.AnimationLibrary.get_nodes_of_class(abp, unreal.AnimGraphNode_LayeredBoneBlend)
assert len(layer) == 1
base_pin = next(pin for pin in unreal.BlueprintEditorLibrary.list_all_pins(layer[0])
                if str(unreal.BlueprintGraphPinLibrary.get_pin_name(pin)) == "BasePose")
base_sources = unreal.BlueprintGraphPinLibrary.list_connected_pins(base_pin)
assert len(base_sources) == 1
assert any(unreal.BlueprintGraphPinLibrary.is_same_native_pin(base_sources[0], pose)
           for node in cache_uses
           for pose in unreal.BlueprintEditorLibrary.list_all_pins(node)
           if str(unreal.BlueprintGraphPinLibrary.get_pin_name(pose)) == "Pose"), "Full body must follow locomotion"
print("MIKE_VERIFY_ABP", len(sequence_nodes), abp.get_editor_property("status"))

montage_names = (
    "Combo1", "Combo2", "Combo3", "Combo4", "Combo5", "ChargeHold", "ChargeRelease", "Dash",
    "HitFront", "HitBack", "HitLeft", "HitRight", "Death",
)
combo_sequences = {
    "Combo1": "PunchR", "Combo2": "UppercutL", "Combo3": "PunchR",
    "Combo4": "PunchL", "Combo5": "UppercutR",
}
for name in montage_names:
    montage = assets.load_asset(root + "/AM_Mike_" + name)
    assert montage and montage.get_editor_property("skeleton") == skeleton, name
    assert montage.get_num_sections() >= 1 and str(montage.get_section_name(0)) == "Default", name
    slots = montage.get_editor_property("slot_anim_tracks")
    assert len(slots) == 1 and str(slots[0].get_editor_property("slot_name")) == "DefaultSlot", name
    segments = slots[0].get_editor_property("anim_track").get_editor_property("anim_segments")
    assert len(segments) == 1, name
    sequence = segments[0].get_editor_property("anim_reference")
    assert sequence and sequence.get_editor_property("skeleton") == skeleton, name
    if name in combo_sequences:
        assert sequence.get_name() == "AN_Mike_" + combo_sequences[name], name
    assert montage.get_play_length() > 0, name
    if name in combo_sequences:
        expected_length = 0.90 if name in ("Combo2", "Combo5") else 0.65
        assert abs(montage.get_play_length() - expected_length) < 0.02, name
    assert montage.get_editor_property("blend_in").get_editor_property("blend_time") <= 0.06, name
    assert montage.get_editor_property("blend_out").get_editor_property("blend_time") <= 0.08, name
    print("MIKE_VERIFY_MONTAGE", name, round(montage.get_play_length(), 3), sequence.get_name())

notify_spec = {
    "PunchR": ("hand_r", True),
    "PunchL": ("hand_l", True),
    "UppercutL": ("hand_l", True),
    "UppercutR": ("hand_r", True),
    "ChargedClaw": ("hand_r", False),
}
for name, (bone, has_combo_check) in notify_spec.items():
    sequence = assets.load_asset(root + "/Sequences/AN_Mike_" + name)
    assert sequence and sequence.get_editor_property("skeleton") == skeleton, name
    # The source clips can carry their own tracks, so inspect notify class directly.
    all_events = unreal.AnimationLibrary.get_animation_notify_events(sequence)
    traces = [event.get_editor_property("notify") for event in all_events
              if event.get_editor_property("notify") and
              event.get_editor_property("notify").get_class().get_name() == "AnimNotify_DoAttackTrace"]
    checks = [event for event in all_events if event.get_editor_property("notify") and
              event.get_editor_property("notify").get_class().get_name() == "AnimNotify_CheckCombo"]
    sounds = [event.get_editor_property("notify") for event in all_events
              if event.get_editor_property("notify") and
              event.get_editor_property("notify").get_class().get_name() == "AnimNotify_PlaySound"]
    assert len(traces) == 1 and str(traces[0].get_editor_property("attack_bone_name")) == bone, name
    assert len(checks) == (1 if has_combo_check else 0), name
    assert len(sounds) == 0, name
    print("MIKE_VERIFY_NOTIFIES", name, len(traces), len(checks), len(sounds), bone)

player_path = "/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter"
player = assets.load_asset(player_path)
player_class = assets.load_blueprint_class(player_path)
assert player and player_class
unreal.BlueprintEditorLibrary.compile_blueprint(player)
assert "BS_UP_TO_DATE" in str(player.get_editor_property("status"))
cdo = unreal.get_default_object(player_class)
for property_name, number in (("mike_punch_right_impact_sound", 1),
                              ("mike_punch_left_impact_sound", 2),
                              ("mike_uppercut_impact_sound", 3)):
    assert cdo.get_editor_property(property_name) == assets.load_asset(root + f"/Sounds/SFX_Mike_Punch{number}")
assert cdo.get_editor_property("character_visual_mesh_override") == mesh
assert cdo.get_editor_property("character_visual_anim_class_override") == assets.load_blueprint_class(root + "/ABP_Mike_SideScroller")
assert len(cdo.get_editor_property("mike_combo_montages")) == 5
assert abs(cdo.get_editor_property("mike_combo_play_rate") - 2.4) < 0.01
assert abs(cdo.get_editor_property("mike_uppercut_play_rate") - 2.0) < 0.01
assert abs(cdo.get_editor_property("mike_combo_damage_step") - 0.25) < 0.01
print("MIKE_VERIFY_DAMAGE", cdo.get_editor_property("melee_damage"),
      cdo.get_editor_property("mike_combo_damage_step"))
for name in ("charged_hold", "charged_release", "dash", "hit_front", "hit_back", "hit_left", "hit_right", "death"):
    assert cdo.get_editor_property("mike_" + name + "_montage"), name
component = cdo.get_editor_property("mesh")
assert component.get_editor_property("skeletal_mesh_asset") == mesh
print("MIKE_VERIFY_PLAYER", player.get_editor_property("status"), component.get_editor_property("relative_location"),
      "dash_duration", cdo.get_editor_property("dash_fallback_duration"))
print("MIKE_VERIFY_PASS")
