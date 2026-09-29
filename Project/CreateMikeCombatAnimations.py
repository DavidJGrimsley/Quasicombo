"""Create Mike-specific combat sequences, notifies, and full-body montages.

The marketplace animation sequences remain untouched. Safe to rerun.
"""

import unreal


ROOT = "/Game/Quasicombo/Mike"
SEQUENCES = ROOT + "/Sequences"
library = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
skeleton = library.load_asset("/Game/RadicalMike/Mesh/SK_MegaMikeZ")
if not skeleton:
    raise RuntimeError("Mike skeleton is missing")

trace_class = unreal.load_class(None, "/Script/Braided_Quanta2026.AnimNotify_DoAttackTrace")
check_class = unreal.load_class(None, "/Script/Braided_Quanta2026.AnimNotify_CheckCombo")
sound_class = unreal.load_class(None, "/Script/Engine.AnimNotify_PlaySound")
if not trace_class or not check_class or not sound_class:
    raise RuntimeError("Combat animation notify classes are missing")


def source(name):
    asset = library.load_asset(f"/Game/RadicalMike/Animations/Anim_ZMIKE_{name}")
    if not asset or asset.get_editor_property("skeleton") != skeleton:
        raise RuntimeError(f"Missing or incompatible Mike clip: {name}")
    return asset


def copy_with_notifies(label, source_name, trace_time, bone, check_time=None, sound_number=None):
    path = f"{SEQUENCES}/AN_Mike_{label}"
    if not library.does_asset_exist(path):
        if not library.duplicate_asset(source(source_name).get_path_name(), path):
            raise RuntimeError(f"Could not copy {source_name}")
    animation = library.load_asset(path)
    # Rebuild every notify track on the Mike-only copy. UE can move notifies to
    # numbered tracks when a sequence is saved and reopened, so clearing only
    # the named track leaves duplicate damage traces on later authoring runs.
    unreal.AnimationLibrary.remove_all_animation_notify_tracks(animation)
    unreal.AnimationLibrary.add_animation_notify_track(animation, "MikeCombat")
    trace = unreal.AnimationLibrary.add_animation_notify_event(animation, "MikeCombat", trace_time, trace_class)
    if not trace:
        raise RuntimeError(f"Could not add trace notify to {label}")
    trace.set_editor_property("attack_bone_name", bone)
    if sound_number is not None:
        sound = library.load_asset(f"{ROOT}/Sounds/SFX_Mike_Punch{sound_number}")
        if not isinstance(sound, unreal.SoundWave):
            raise RuntimeError(f"Missing Mike punch sound {sound_number}")
        play_sound = unreal.AnimationLibrary.add_animation_notify_event(
            animation, "MikeCombat", trace_time, sound_class)
        if not play_sound:
            raise RuntimeError(f"Could not add sound notify to {label}")
        play_sound.set_editor_property("sound", sound)
    if check_time is not None:
        if not unreal.AnimationLibrary.add_animation_notify_event(animation, "MikeCombat", check_time, check_class):
            raise RuntimeError(f"Could not add combo notify to {label}")
    if not library.save_loaded_asset(animation):
        raise RuntimeError(f"Could not save {label}")
    print("MIKE_SEQUENCE", label, trace_time, bone, check_time, sound_number)
    return animation


attack_r = copy_with_notifies("PunchR", "PunchR", 0.30, "hand_r", 0.52, 1)
attack_l = copy_with_notifies("PunchL", "PunchL", 0.30, "hand_l", 0.52, 2)
attack_upper_l = copy_with_notifies("UppercutL", "UppercutL", 0.48, "hand_l", 0.78, 3)
attack_u = copy_with_notifies("UppercutR", "UppercutR", 0.48, "hand_r", 0.78, 3)
# Use a distinct claw strike so the charged release cannot be mistaken for
# the combo's uppercut finisher.
charge_release = copy_with_notifies("ChargedClaw", "ClawR", 0.40, "hand_r")


def montage(name, animation, trim=None):
    path = f"{ROOT}/AM_Mike_{name}"
    if library.does_asset_exist(path):
        value = library.load_asset(path)
    else:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property("target_skeleton", skeleton)
        factory.set_editor_property("source_animation", animation)
        value = asset_tools.create_asset(f"AM_Mike_{name}", ROOT, unreal.AnimMontage, factory)
    if not value:
        raise RuntimeError(f"Could not create montage {name}")
    if value.get_editor_property("skeleton") != skeleton:
        raise RuntimeError(f"Montage {name} has the wrong skeleton")
    slot_tracks = list(value.get_editor_property("slot_anim_tracks"))
    if len(slot_tracks) != 1:
        raise RuntimeError(f"Montage {name} must have exactly one slot")
    slot = slot_tracks[0]
    track = slot.get_editor_property("anim_track")
    segments = list(track.get_editor_property("anim_segments"))
    if len(segments) != 1:
        raise RuntimeError(f"Montage {name} must have exactly one segment")
    segment = segments[0]
    segment.set_editor_property("anim_reference", animation)
    segment.set_editor_property("anim_start_time", 0.0)
    segment.set_editor_property("anim_end_time", trim if trim is not None else animation.get_play_length())
    track.set_editor_property("anim_segments", [segment])
    slot.set_editor_property("anim_track", track)
    slot.set_editor_property("slot_name", "DefaultSlot")
    value.set_editor_property("slot_anim_tracks", [slot])
    for property_name, seconds in (("blend_in", 0.04), ("blend_out", 0.06)):
        blend = value.get_editor_property(property_name)
        blend.set_editor_property("blend_time", seconds)
        value.set_editor_property(property_name, blend)
    if not library.save_loaded_asset(value):
        raise RuntimeError(f"Could not save montage {name}")
    print("MIKE_MONTAGE", name, value.get_play_length(), animation.get_name())
    return value


montage("Combo1", attack_r, 0.65)
montage("Combo2", attack_upper_l, 0.90)
montage("Combo3", attack_r, 0.65)
montage("Combo4", attack_l, 0.65)
montage("Combo5", attack_u, 0.90)
montage("ChargeHold", source("IdleAggro"))
montage("ChargeRelease", charge_release, 0.90)
montage("Dash", source("Run_Faster"))
for label, sequence_name in (
    ("HitFront", "HitRegisterFront"),
    ("HitBack", "HitRegisterBack"),
    ("HitLeft", "HitRegisterL"),
    ("HitRight", "HitRegisterR"),
    ("Death", "HitRegisterFront_Death"),
):
    montage(label, source(sequence_name))

# Segment edits do not shrink the montage data model's saved play length on
# their own. Recalculate it so the new combo endpoints determine recovery.
editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(editor_world, "Quasicombo.RefreshMikeMontageLengths")
