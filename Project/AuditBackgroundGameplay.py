"""Read saved scenery assets and audit transforms/animation wiring via MCP console."""
import json
from pathlib import Path
import unreal
report = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / 'BackgroundGameplayValidation'
placements = json.loads((report / 'placements.json').read_text())
subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = {a.get_path_name(): a for a in subsystem.get_all_level_actors()}
results = []
for p in placements:
    a = actors[p['path']]
    # A user-converted stationary prefab has its own authored placement.
    ambient = a.get_components_by_class(unreal.QuasicomboAmbientRoarComponent)
    if ambient:
        assert len(ambient) == 1 and not a.get_components_by_class(unreal.QuasicomboBackgroundRoamingComponent)
        c = ambient[0]
        assert c.minimum_roar_interval == 5 and c.maximum_roar_interval == 20
        assert c.readjust_interval == 20 and c.readjust_angle == 45
        assert c.turn_left_animation and c.turn_right_animation
        assert len(a.get_components_by_class(unreal.SkeletalMeshComponent)) == 14
        results.append({'label': p['label'], 'special': 'ambient roarer', 'readjust_interval': c.readjust_interval})
        continue
    t = a.get_actor_transform()
    actual = {'location': [t.translation.x,t.translation.y,t.translation.z],
              'rotation': [t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w],
              'scale': [t.scale3d.x,t.scale3d.y,t.scale3d.z]}
    assert actual == p['transform'], p['label']
    body = a.skeletal_mesh_component
    assert [m.get_path_name() if m else None for m in body.get_materials()] == p['materials']
    assert len(a.get_components_by_class(unreal.SkeletalMeshComponent)) == p['components']
    cls = unreal.QuasicomboLarvaeTrapComponent if p['trap'] else unreal.QuasicomboBackgroundRoamingComponent
    components = a.get_components_by_class(cls)
    assert len(components) == 1
    c = components[0]
    assert c.mesh_forward_yaw_offset == -90, p['label']
    if p['trap']:
        assert not a.get_components_by_class(unreal.QuasicomboBackgroundRoamingComponent)
        starts = unreal.GameplayStatics.get_all_actors_of_class(a.get_world(), unreal.PlayerStart)
        distances = [(a.get_actor_location() - start.get_actor_location()).length() for start in starts]
        assert distances and min(distances) > c.trigger_radius
        settings = {'trigger_radius': c.trigger_radius, 'start_distances': distances}
    else:
        assert not c.idle_only
        assert not c.idle_animation.get_editor_property('enable_root_motion') and not c.walk_animation.get_editor_property('enable_root_motion')
        if p['species'] == 'Megaspikan':
            assert c.turn_left_animation and c.turn_right_animation
            assert not c.turn_left_animation.get_editor_property('enable_root_motion') and not c.turn_right_animation.get_editor_property('enable_root_motion')
        settings = {'radius': c.roaming_radius, 'speed': c.walk_speed}
    results.append({'label': p['label'], **settings})
bp_paths = json.loads((report/'authoring.json').read_text())['prefabs']
prefab_defaults = []
for path in bp_paths:
    cdo = unreal.get_default_object(unreal.load_asset(path).generated_class())
    assert cdo.skeletal_mesh_component.get_editor_property('skeletal_mesh_asset'), path
    component = cdo.get_editor_property('trap' if path.endswith('LarvaeTrap') else 'roaming')
    assert component.mesh_forward_yaw_offset == -90, path
    if path.endswith('LarvaeTrap'):
        assert component.emergence_animation and component.spit_animation, path
    else:
        assert not cdo.idle_only and not component.idle_only, path
        assert component.idle_animation and component.walk_animation, path
        if path.endswith('_Megaspikan'): assert component.turn_left_animation and component.turn_right_animation, path
    prefab_defaults.append(path)
roar_path = '/Game/Quasicombo/Background/BP_QC_MegaspikanRoarer'
roar_bp = unreal.load_asset(roar_path)
assert roar_bp
roar_cdo = unreal.get_default_object(roar_bp.generated_class())
roar_component = roar_cdo.get_editor_property('ambient_roar')
assert roar_component.idle_animation and roar_component.roar_animation
assert roar_component.minimum_roar_interval == 5 and roar_component.maximum_roar_interval == 20
assert roar_component.turn_left_animation and roar_component.turn_right_animation
assert roar_component.readjust_interval == 20 and roar_component.readjust_angle == 45
assert not roar_cdo.get_components_by_class(unreal.QuasicomboBackgroundRoamingComponent)
bp_paths.append(roar_path)
prefab_defaults.append(roar_path)
bp_paths += ['/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter','/Game/Quasicombo/Mike/ABP_Mike_SideScroller']
for path in bp_paths:
    bp = unreal.load_asset(path)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert 'ERROR' not in str(bp.get_editor_property('status')).upper(), path
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
slide = unreal.load_asset('/Game/Quasicombo/Mike/AN_Mike_WallSlide')
assert not slide.get_editor_property('enable_root_motion')
assert unreal.AnimationLibrary.get_num_frames(slide) == 30
abp = unreal.load_asset('/Game/Quasicombo/Mike/ABP_Mike_SideScroller')
nodes = unreal.AnimationLibrary.get_nodes_of_class(abp, unreal.AnimGraphNode_BlendListByBool)
assert len(nodes) == 1
wall_sequence = [n for n in unreal.AnimationLibrary.get_nodes_of_class(abp, unreal.AnimGraphNode_SequencePlayer)
                 if n.get_editor_property('node').get_editor_property('sequence') == slide]
assert len(wall_sequence) == 1
result = {'success': True, 'placements': results, 'prefab_defaults': prefab_defaults, 'compiled_blueprints': bp_paths,
          'wall_animation': slide.get_path_name(), 'wall_graph_nodes': len(nodes)}
(report/'asset-audit.json').write_text(json.dumps(result,indent=2))
print('BACKGROUND_ASSET_AUDIT_PASS',result)
