"""Remove swing-timed punch sounds while preserving Mike's trace/combo notifies.

The C++ melee impact path now plays the existing punch sounds only after contact.
Run once in Unreal Editor with -ExecutePythonScript; safe to rerun.
"""
import unreal

root = "/Game/Quasicombo/Mike/Sequences/AN_Mike_"
trace_class = unreal.load_class(None, "/Script/Braided_Quanta2026.AnimNotify_DoAttackTrace")
check_class = unreal.load_class(None, "/Script/Braided_Quanta2026.AnimNotify_CheckCombo")
if not trace_class or not check_class:
    raise RuntimeError("Mike combat notify classes are missing")

for name, trace_time, bone, check_time in (
    ("PunchR", 0.30, "hand_r", 0.52),
    ("PunchL", 0.30, "hand_l", 0.52),
    ("UppercutL", 0.48, "hand_l", 0.78),
    ("UppercutR", 0.48, "hand_r", 0.78),
):
    animation = unreal.EditorAssetLibrary.load_asset(root + name)
    if not animation:
        raise RuntimeError("Missing Mike sequence: " + name)
    animation.modify()
    unreal.AnimationLibrary.remove_all_animation_notify_tracks(animation)
    unreal.AnimationLibrary.add_animation_notify_track(animation, "MikeCombat")
    trace = unreal.AnimationLibrary.add_animation_notify_event(animation, "MikeCombat", trace_time, trace_class)
    if not trace:
        raise RuntimeError("Could not restore trace notify: " + name)
    trace.set_editor_property("attack_bone_name", bone)
    if not unreal.AnimationLibrary.add_animation_notify_event(animation, "MikeCombat", check_time, check_class):
        raise RuntimeError("Could not restore combo notify: " + name)
    if not unreal.EditorAssetLibrary.save_loaded_asset(animation, only_if_is_dirty=False):
        raise RuntimeError("Could not save Mike sequence: " + name)
    print("MIKE_IMPACT_ONLY", name)
