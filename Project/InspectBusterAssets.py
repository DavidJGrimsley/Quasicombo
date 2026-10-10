"""Read-only Buster rig/animation audit inside Unreal."""
import unreal
lib = unreal.EditorAssetLibrary
bp = lib.load_asset('/Game/Quasicombo/Mike/ABP_Mike_SideScroller')
for cls in (unreal.AnimGraphNode_LayeredBoneBlend, unreal.AnimGraphNode_Slot):
    for node in unreal.AnimationLibrary.get_nodes_of_class(bp, cls):
        print('BUSTER_NODE', node.get_name(), [(str(unreal.BlueprintGraphPinLibrary.get_pin_name(p)), str(p)) for p in unreal.BlueprintEditorLibrary.list_all_pins(node)])
mesh = lib.load_asset('/Game/RadicalMike/Mesh/SM_RadicalBuster')
print('BUSTER_BOUNDS', mesh.get_bounding_box())
skel = lib.load_asset('/Game/RadicalMike/Mesh/SK_MegaMikeZ')
print('BUSTER_SKELETON_API', [n for n in dir(skel) if 'socket' in n])
seq = lib.load_asset('/Game/RadicalMike/Animations/Anim_ZMIKE_BusterEquip_R')
for t in (0.,0.25,0.5,0.75,1.,1.25,1.5):
    print('BUSTER_EQUIP_POSE',t,unreal.AnimationLibrary.get_bone_pose_for_time(seq,'Buster_R',t,False))
player_class=lib.load_blueprint_class('/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter')
cdo=unreal.get_default_object(player_class)
comp=cdo.get_editor_property('mesh')
print('BUSTER_SOCKET', comp.get_socket_transform('Buster_RSocket',unreal.RelativeTransformSpace.RTS_COMPONENT))
print('BUSTER_INSPECT_SUCCESS')
