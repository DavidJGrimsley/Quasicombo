"""Apply turn clips without resetting placement overrides; create a stationary roar BP.

Execute through Unreal MCP's Python console after rebuilding native classes.
"""
import json
from pathlib import Path
import unreal

assets = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert unreal.Paths.project_dir().replace('\\','/').rstrip('/').endswith('/Quasicombo-HUD')
assert world.get_path_name().startswith('/Game/Maps/Lvl_Main')
report = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / 'BackgroundGameplayValidation'
left = unreal.load_asset('/Game/Megaspikan/Animations/ANIM_Megaspikan_Turn45Left')
right = unreal.load_asset('/Game/Megaspikan/Animations/ANIM_Megaspikan_Turn45Right')
assert left and right
assert not left.get_editor_property('enable_root_motion') and not right.get_editor_property('enable_root_motion')

def set_turns(component):
    component.set_editor_property('turn_left_animation',left)
    component.set_editor_property('turn_right_animation',right)

bp = unreal.load_asset('/Game/Quasicombo/Background/BP_QC_Background_Megaspikan')
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
set_turns(unreal.get_default_object(bp.generated_class()).get_editor_property('roaming'))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert assets.save_loaded_asset(bp,only_if_is_dirty=False)
updated = []
for actor in actors.get_all_level_actors():
    if not isinstance(actor,unreal.SkeletalMeshActor): continue
    mesh = actor.skeletal_mesh_component.get_editor_property('skeletal_mesh_asset')
    if not mesh or mesh.get_path_name().split('.')[0] != '/Game/Megaspikan/Meshes/SK_Megaspikan_Body': continue
    for component in actor.get_components_by_class(unreal.QuasicomboBackgroundRoamingComponent):
        before = actor.get_actor_transform()
        idle = component.idle_only
        actor.modify(); component.modify()
        set_turns(component)
        assert before == actor.get_actor_transform() and component.idle_only == idle
        updated.append(actor.get_actor_label())

path = '/Game/Quasicombo/Background/BP_QC_MegaspikanRoarer'
if assets.does_asset_exist(path):
    roar_bp = unreal.load_asset(path)
else:
    roar_bp = assets.duplicate_asset('/Game/Megaspikan/Blueprint/BP_Megaspikan_Blueprint',path)
    assert roar_bp
    unreal.BlueprintEditorLibrary.reparent_blueprint(roar_bp,unreal.QuasicomboAmbientRoarer)
unreal.BlueprintEditorLibrary.compile_blueprint(roar_bp)
cdo = unreal.get_default_object(roar_bp.generated_class())
body = cdo.skeletal_mesh_component
body.set_skeletal_mesh_asset(unreal.load_asset('/Game/Megaspikan/Meshes/SK_Megaspikan_Body'))
idle = unreal.load_asset('/Game/Megaspikan/Animations/ANIM_Megaspikan_Idle')
roar = unreal.load_asset('/Game/Megaspikan/Animations/ANIM_Megaspikan_Roar')
body.set_editor_property('animation_mode',unreal.AnimationMode.ANIMATION_SINGLE_NODE)
body.set_editor_property('animation_data',unreal.SingleAnimationPlayData(anim_to_play=idle,saved_looping=True,saved_playing=True,saved_play_rate=1))
component = cdo.get_editor_property('ambient_roar')
component.set_editor_property('idle_animation',idle)
component.set_editor_property('roar_animation',roar)
component.set_editor_property('minimum_roar_interval',5)
component.set_editor_property('maximum_roar_interval',20)
set_turns(component)
component.set_editor_property('readjust_interval',20)
component.set_editor_property('readjust_angle',45)
unreal.BlueprintEditorLibrary.compile_blueprint(roar_bp)
assert 'ERROR' not in str(roar_bp.get_editor_property('status')).upper()
assert assets.save_loaded_asset(roar_bp,only_if_is_dirty=False)
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
result = dict(success=True,updated_adults=updated,roar_blueprint=path,interval=[5,20],roar_length=roar.get_play_length())
(report/'turns-roar-authoring.json').write_text(json.dumps(result,indent=2))
print('BACKGROUND_TURNS_ROAR_AUTHORED',result)
