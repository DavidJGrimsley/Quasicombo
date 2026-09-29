"""Wire the Mike mesh, AnimBP, and full-body montages into the player BP."""

import unreal


library = unreal.EditorAssetLibrary
root = "/Game/Quasicombo/Mike"
blueprint_path = "/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter"
blueprint = library.load_asset(blueprint_path)
klass = library.load_blueprint_class(blueprint_path)
anim_class = library.load_blueprint_class(root + "/ABP_Mike_SideScroller")
mesh = library.load_asset("/Game/RadicalMike/Mesh/SKM_MegaMikeZ")
if not all((blueprint, klass, anim_class, mesh)):
    raise RuntimeError("Mike player Blueprint dependencies are missing")


def montage(name):
    path = root + "/AM_Mike_" + name
    value = library.load_asset(path)
    if not value:
        raise RuntimeError("Missing montage: " + path)
    if value.get_editor_property("skeleton") != mesh.get_editor_property("skeleton"):
        raise RuntimeError("Incompatible montage: " + path)
    return value


cdo = unreal.get_default_object(klass)
cdo.set_editor_property("character_visual_mesh_override", mesh)
cdo.set_editor_property("character_visual_anim_class_override", anim_class)
cdo.set_editor_property("mike_combo_montages", [montage(f"Combo{number}") for number in range(1, 6)])
for property_name, asset_name in (
    ("mike_charged_hold_montage", "ChargeHold"),
    ("mike_charged_release_montage", "ChargeRelease"),
    ("mike_dash_montage", "Dash"),
    ("mike_hit_front_montage", "HitFront"),
    ("mike_hit_back_montage", "HitBack"),
    ("mike_hit_left_montage", "HitLeft"),
    ("mike_hit_right_montage", "HitRight"),
    ("mike_death_montage", "Death"),
):
    cdo.set_editor_property(property_name, montage(asset_name))
cdo.set_editor_property("mike_combo_play_rate", 2.4)
cdo.set_editor_property("mike_uppercut_play_rate", 2.0)
cdo.set_editor_property("mike_combo_damage_step", 0.25)
cdo.set_editor_property("mike_charged_release_play_rate", 1.5)
cdo.set_editor_property("mike_dash_play_rate", 1.0)
cdo.set_editor_property("dash_fallback_duration", 0.97)

# Show the active mesh and AnimBP in the Blueprint viewport as well as at
# runtime; retain the component's existing capsule-relative placement.
component = cdo.get_editor_property("mesh")
component.set_editor_property("skeletal_mesh_asset", mesh)
component.set_editor_property("anim_class", anim_class)

unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
status = str(blueprint.get_editor_property("status"))
print("MIKE_PLAYER_BP_STATUS", status)
if "ERROR" in status.upper():
    raise RuntimeError("Mike player Blueprint did not compile")
if not library.save_loaded_asset(blueprint):
    raise RuntimeError("Could not save Mike player Blueprint")
print("MIKE_PLAYER_WIRED", component.get_editor_property("relative_location"), component.get_editor_property("relative_rotation"))
