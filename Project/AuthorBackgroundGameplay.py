"""Author scenery prefabs, preserve placed actor identities, and bake Mike's wall pose.

Execute through the project's Unreal MCP console after rebuilding the editor.
Original marketplace assets are read-only. Reruns reuse components and assets.
"""
import json
import math
from pathlib import Path
import unreal
from editor_toolset.toolsets.actor import ActorTools

LIB = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ROOT = '/Game/Quasicombo/Background'
REPORT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / 'BackgroundGameplayValidation'
REPORT.mkdir(parents=True, exist_ok=True)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert unreal.Paths.project_dir().replace('\\', '/').rstrip('/').endswith('/Quasicombo-HUD'), unreal.Paths.project_dir()
assert world.get_path_name().startswith('/Game/Maps/Lvl_Main'), world.get_path_name()


def load(path):
    value = unreal.load_asset(path)
    assert value, path
    return value


def save(asset):
    assert LIB.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


def transform(actor):
    t = actor.get_actor_transform()
    return {'location': [t.translation.x, t.translation.y, t.translation.z],
            'rotation': [t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w],
            'scale': [t.scale3d.x, t.scale3d.y, t.scale3d.z]}


SPECS = {
    'StoneGolem': dict(mesh='/Game/Stone_Golem/mesh/SKM_Stone_Golem',
                      idle='/Game/Stone_Golem/demo/animations/ThirdPersonIdle',
                      walk='/Game/Stone_Golem/demo/animations/ThirdPersonWalk', yaw=-90),
    'WoodMonster': dict(mesh='/Game/Wood_Monster/CharacterParts/Meshes/SK_wood_giant_01_a',
                       idle='/Game/Wood_Monster/DemoContent/Mannequins/Animations/ThirdPerson_retarget/MM_Idle',
                       walk='/Game/Wood_Monster/DemoContent/Mannequins/Animations/ThirdPerson_retarget/MM_Walk_InPlace', yaw=-90),
    'Larvae': dict(mesh='/Game/Megaspikan/MegaspikanLarvae/Meshes/SK_MegaspikanLarvae',
                  idle='/Game/Megaspikan/MegaspikanLarvae/Animations/ANIM_MegaspikanLarvae_Idle',
                  walk='/Game/Megaspikan/MegaspikanLarvae/Animations/ANIM_MegaspikanLarvae_Walk', yaw=-90),
    'Megaspikan': dict(mesh='/Game/Megaspikan/Meshes/SK_Megaspikan_Body',
                      idle='/Game/Megaspikan/Animations/ANIM_Megaspikan_Idle',
                      walk='/Game/Megaspikan/Animations/ANIM_Megaspikan_WalkForward', yaw=-90,
                      turn_left='/Game/Megaspikan/Animations/ANIM_Megaspikan_Turn45Left',
                      turn_right='/Game/Megaspikan/Animations/ANIM_Megaspikan_Turn45Right'),
}


def configure(component, spec, size=1):
    component.set_editor_property('idle_animation', load(spec['idle']))
    component.set_editor_property('walk_animation', load(spec['walk']))
    if 'turn_left' in spec:
        component.set_editor_property('turn_left_animation',load(spec['turn_left']))
        component.set_editor_property('turn_right_animation',load(spec['turn_right']))
    component.set_editor_property('idle_only', False)
    factor = max(.5, min(6, size))
    component.set_editor_property('roaming_radius', 600 * factor)
    component.set_editor_property('walk_speed', 90 * factor)
    component.set_editor_property('animation_walk_speed', 150 * size)
    component.set_editor_property('maximum_ground_step', 100 * factor)
    component.set_editor_property('obstacle_radius', 40 * factor)
    component.set_editor_property('mesh_forward_yaw_offset', spec['yaw'])


prefabs = []
for label, spec in SPECS.items():
    path = ROOT + '/BP_QC_Background_' + label
    if LIB.does_asset_exist(path):
        bp = load(path)
    elif label == 'Megaspikan':
        bp = LIB.duplicate_asset('/Game/Megaspikan/Blueprint/BP_Megaspikan_Blueprint', path)
        assert bp
        unreal.BlueprintEditorLibrary.reparent_blueprint(bp, unreal.QuasicomboBackgroundCreature)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.QuasicomboBackgroundCreature)
        bp = TOOLS.create_asset(path.rsplit('/', 1)[-1], ROOT, unreal.Blueprint, factory)
    assert bp, path
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    body = cdo.get_editor_property('skeletal_mesh_component')
    body.set_skeletal_mesh_asset(load(spec['mesh']))
    body.set_editor_property('animation_mode', unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    body.set_editor_property('animation_data', unreal.SingleAnimationPlayData(
        anim_to_play=load(spec['idle']), saved_looping=True, saved_playing=True, saved_play_rate=1))
    cdo.set_editor_property('idle_only', False)
    configure(cdo.get_editor_property('roaming'), spec)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert 'ERROR' not in str(bp.get_editor_property('status')).upper(), path
    save(bp)
    prefabs.append(path)

trap_path = ROOT + '/BP_QC_LarvaeTrap'
if LIB.does_asset_exist(trap_path):
    trap_bp = load(trap_path)
else:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.QuasicomboLarvaeTrap)
    trap_bp = TOOLS.create_asset('BP_QC_LarvaeTrap', ROOT, unreal.Blueprint, factory)
assert trap_bp
unreal.BlueprintEditorLibrary.compile_blueprint(trap_bp)
trap_cdo = unreal.get_default_object(trap_bp.generated_class())
trap_cdo.get_editor_property('skeletal_mesh_component').set_skeletal_mesh_asset(load(SPECS['Larvae']['mesh']))
trap_cdo.get_editor_property('trap').set_editor_property('mesh_forward_yaw_offset', SPECS['Larvae']['yaw'])
unreal.BlueprintEditorLibrary.compile_blueprint(trap_bp)
save(trap_bp)

placements = []
for actor in ACTORS.get_all_level_actors():
    if not isinstance(actor, unreal.SkeletalMeshActor):
        continue
    body = actor.get_editor_property('skeletal_mesh_component')
    mesh = body.get_editor_property('skeletal_mesh_asset')
    if not mesh:
        continue
    species = next((name for name, spec in SPECS.items() if mesh.get_path_name().split('.')[0] == spec['mesh']), None)
    if not species:
        continue
    before = transform(actor)
    materials = [m.get_path_name() if m else None for m in body.get_materials()]
    label = actor.get_actor_label()
    is_trap = label == 'ANIM_MegaspikanLarvae_Roar'
    if actor.get_components_by_class(unreal.QuasicomboAmbientRoarComponent):
        # Preserve a user-converted stationary placement instead of adding roaming.
        placements.append({'path': actor.get_path_name(), 'label': label, 'species': species,
                           'trap': False, 'ambient': True, 'transform': before, 'materials': materials,
                           'components': len(actor.get_components_by_class(unreal.SkeletalMeshComponent))})
        continue
    cls = unreal.QuasicomboLarvaeTrapComponent if is_trap else unreal.QuasicomboBackgroundRoamingComponent
    existing = actor.get_components_by_class(cls)
    component = existing[0] if existing else ActorTools.add_component(actor, cls.static_class(), 'LarvaeTrap' if is_trap else 'BackgroundRoaming')
    actor.modify()
    component.modify()
    if is_trap:
        actor.tags = list(dict.fromkeys(list(actor.tags) + ['QC_LarvaeTrap']))
        component.set_editor_property('mesh_forward_yaw_offset', SPECS['Larvae']['yaw'])
    else:
        actor.tags = list(dict.fromkeys(list(actor.tags) + ['QC_Background']))
        configure(component, SPECS[species], actor.get_actor_scale3d().x)
    body.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    assert transform(actor) == before, label
    assert [m.get_path_name() if m else None for m in body.get_materials()] == materials, label
    placements.append({'path': actor.get_path_name(), 'label': label, 'species': species,
                       'trap': is_trap, 'transform': before, 'materials': materials,
                       'components': len(actor.get_components_by_class(unreal.SkeletalMeshComponent))})

assert len(placements) == 17, placements
assert sum(p['trap'] for p in placements) == 1
assert unreal.EditorLevelLibrary.save_current_level()
# World Partition stores existing actors in separate packages; save those too.
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
(REPORT / 'placements.json').write_text(json.dumps(placements, indent=2), encoding='utf-8')

# Bake an isolated, looping braced pose. Mike's mesh faces +Y; component-axis X
# adjustments raise the arms/knees toward the wall without changing the source.
source = load('/Game/RadicalMike/Animations/Anim_ZMIKE_JumpApex')
slide_path = '/Game/Quasicombo/Mike/AN_Mike_WallSlide'
slide = load(slide_path) if LIB.does_asset_exist(slide_path) else LIB.duplicate_asset(source.get_path_name(), slide_path)
assert slide
names = list(map(str, unreal.AnimationLibrary.get_animation_track_names(source)))
preview = ACTORS.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0, 0, -100000))
preview.skeletal_mesh_component.set_skeletal_mesh_asset(load('/Game/RadicalMike/Mesh/SKM_MegaMikeZ'))
parent = {name: str(preview.skeletal_mesh_component.get_parent_bone(name)) for name in names}
ACTORS.destroy_actor(preview)
adjustments = {'spine_01': -10, 'spine_02': -8, 'upperarm_l': -45, 'upperarm_r': -45,
               'lowerarm_l': -25, 'lowerarm_r': -25, 'thigh_l': 25, 'thigh_r': 12,
               'calf_l': -25, 'calf_r': -12}
keys = {name: [] for name in names}
frames = 30
for i in range(frames + 1):
    phase = math.sin(2 * math.pi * i / frames)
    poses = {name: unreal.AnimationLibrary.get_bone_pose_for_time(source, name, 0, False) for name in names}
    global_rot = {}
    def rotate(name):
        if name not in global_rot:
            p = parent.get(name)
            pq = rotate(p) if p in poses else unreal.Quat()
            q = poses[name].rotation
            if name in adjustments:
                angle = adjustments[name] + (1.5 * phase if name.startswith('spine') else .8 * phase)
                axis = pq.unrotate_vector(unreal.Vector(1, 0, 0))
                half = math.radians(angle) / 2
                q = unreal.Quat(axis.x * math.sin(half), axis.y * math.sin(half), axis.z * math.sin(half), math.cos(half)) * q
                poses[name].rotation = q
            global_rot[name] = pq * q
        return global_rot[name]
    for name in names:
        rotate(name)
        keys[name].append(poses[name])
controller = slide.get_editor_property('controller')
controller.open_bracket('Author looping Mike wall slide', False)
controller.set_frame_rate(unreal.FrameRate(30, 1), False)
controller.set_number_of_frames(unreal.FrameNumber(frames), False)
for name, poses in keys.items():
    assert controller.set_bone_track_keys(name, [p.translation for p in poses], [p.rotation for p in poses], [p.scale3d for p in poses], False), name
controller.close_bracket(False)
unreal.AnimationLibrary.remove_all_animation_notify_tracks(slide)
slide.set_editor_property('enable_root_motion', False)
LIB.set_metadata_tag(slide, 'QuasicomboWallSlideVersion', '1')
save(slide)
unreal.SystemLibrary.execute_console_command(world, 'Quasicombo.AuthorMikeWallSlide')
player_bp = load('/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter')
player_cdo = unreal.get_default_object(player_bp.generated_class())
player_cdo.set_editor_property('native_wall_slide_animation', slide)
player_cdo.set_editor_property('delay_between_wall_jumps', .2)
unreal.BlueprintEditorLibrary.compile_blueprint(player_bp)
save(player_bp)
assert 'ERROR' not in str(load('/Game/Quasicombo/Mike/ABP_Mike_SideScroller').get_editor_property('status')).upper()
assert unreal.EditorLevelLibrary.save_current_level()
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
result = {'roamers': sum(not p['trap'] and not p.get('ambient') for p in placements),
          'traps': 1, 'prefabs': prefabs + [trap_path], 'slide': slide_path}
(REPORT / 'authoring.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print('QUASICOMBO_BACKGROUND_AUTHOR_SUCCESS', json.dumps(result))
