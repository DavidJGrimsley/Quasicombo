"""Move Interact from gamepad X / keyboard F to Y / E.

Run in Unreal Editor with -ExecutePythonScript. Keeps other IMC bindings intact.
"""
import unreal

path = "/Game/Variant_SideScrolling/Input/IMC_SideScroller"
context = unreal.EditorAssetLibrary.load_asset(path)
interact = unreal.EditorAssetLibrary.load_asset(
    "/Game/Variant_SideScrolling/Input/Actions/IA_Interact"
)
if not context or not interact:
    raise RuntimeError("Side-scroller input mapping or Interact action missing")

x = unreal.Key()
x.set_editor_property("key_name", "Gamepad_FaceButton_Left")
y = unreal.Key()
y.set_editor_property("key_name", "Gamepad_FaceButton_Top")
f = unreal.Key()
f.set_editor_property("key_name", "F")
e = unreal.Key()
e.set_editor_property("key_name", "E")
def mappings():
    data = context.get_editor_property("default_key_mappings")
    return [(m.get_editor_property("action"), m.get_editor_property("key"))
            for m in data.get_editor_property("mappings")]

before = mappings()
key_name = lambda key: str(key.get_editor_property("key_name"))
print("BUSTER_INTERACT_BEFORE", [(str(a), key_name(k)) for a, k in before if a == interact])
context.modify()
if not any(a == interact and key_name(k) == "Gamepad_FaceButton_Left" for a, k in before) and not any(a == interact and key_name(k) == "Gamepad_FaceButton_Top" for a, k in before):
    raise RuntimeError("Expected original X mapping was not found")
if any(a == interact and key_name(k) == "Gamepad_FaceButton_Left" for a, k in before):
    context.unmap_key(interact, x)
if not any(a == interact and key_name(k) == "Gamepad_FaceButton_Top" for a, k in before):
    context.map_key(interact, y)
if any(a == interact and key_name(k) == "F" for a, k in before):
    context.unmap_key(interact, f)
if not any(a == interact and key_name(k) == "E" for a, k in before):
    context.map_key(interact, e)
after = mappings()
assert not any(a == interact and key_name(k) == "Gamepad_FaceButton_Left" for a, k in after)
assert sum(a == interact and key_name(k) == "Gamepad_FaceButton_Top" for a, k in after) == 1
assert not any(a == interact and key_name(k) == "F" for a, k in after)
assert sum(a == interact and key_name(k) == "E" for a, k in after) == 1
assert unreal.EditorAssetLibrary.save_loaded_asset(context, only_if_is_dirty=False)
print("BUSTER_INTERACT_AFTER", [(str(a), key_name(k)) for a, k in after if a == interact])
