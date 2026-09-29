"""Repeatable editor wiring for the existing Lvl_Main blockout.

All generated actors have QC_ labels and Quasicombo folders. Enemy placement and
each route entry's Enemies array belong to the level author and are never changed.
"""
import unreal

LEVEL = '/Game/Maps/Lvl_Main'
Y = 1000.0
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL)
existing = {a.get_actor_label(): a for a in actor_sub.get_all_level_actors()}

def native(name):
    result = unreal.load_class(None, '/Script/Braided_Quanta2026.' + name)
    if not result:
        raise RuntimeError('Native class missing: ' + name)
    return result

def place(label, klass, x, z, folder, scale=None, preserve_existing_transform=False,
          preserve_existing_actor=False):
    actor = existing.get(label)
    is_new = actor is None
    if actor is not None and preserve_existing_actor:
        return actor
    if actor is None:
        actor = actor_sub.spawn_actor_from_class(klass, unreal.Vector(x, Y, z))
        if actor is None:
            raise RuntimeError('Could not spawn ' + label)
        actor.set_actor_label(label)
        existing[label] = actor
    else:
        actor.modify()
    if is_new or not preserve_existing_transform:
        actor.set_actor_location(unreal.Vector(x, Y, z), False, False)
    actor.set_folder_path(folder)
    actor.set_editor_property('is_spatially_loaded', False)
    if scale and (is_new or not preserve_existing_transform):
        actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor

def authored_rear_gate(section_no):
    label = 'RearGateSection%d' % section_no
    matches = [a for a in actor_sub.get_all_level_actors() if a.get_actor_label() == label]
    if len(matches) != 1 or not isinstance(matches[0], unreal.StaticMeshActor):
        raise RuntimeError('Expected one authored StaticMeshActor named ' + label)
    return matches[0]

lane_enum = {'A': unreal.QuasicomboLane.A, 'B': unreal.QuasicomboLane.B, 'C': unreal.QuasicomboLane.C}
leg_class = native('QuasicomboRouteLeg')
section_class = native('QuasicomboRouteSection')
barrier_class = native('QuasicomboBarrier')
kill_class = native('QuasicomboKillVolume')
entry_class = native('QuasicomboArenaEntry')
hud_class = native('QuasicomboHUDActor')
boss_class = unreal.EditorAssetLibrary.load_blueprint_class('/Game/Quasicombo/Boss/BP_QC_Boss')
if not boss_class:
    raise RuntimeError('Run CreateQuasicomboAssets.py first')

# x and floor z at route entry, then x and floor z at forward barrier.
layout = {
    1: {
        'A': (3400, 2290, 10000, 2800),
        'B': (3400, 700, 10000, 1300),
        'C': (2800, -300, 10000, -900),
    },
    2: {
        'A': (10300, 2800, 18900, 3000),
        'B': (10300, 1300, 18900, 1200),
        'C': (10300, -900, 18900, -1310),
    },
    3: {
        'A': (19300, 3000, 25300, 3000),
        'B': (19300, 1200, 25300, 1400),
        'C': (19300, -1310, 26600, -1000),
    },
}
legs = {}
for section_no, lanes in layout.items():
    section = place('QC_Section_%d' % section_no, section_class, 0, 0,
                    'Quasicombo/Routes/Section%d' % section_no)
    section.set_editor_property('section_number', section_no)
    section.set_editor_property('rear_gate', authored_rear_gate(section_no))
    section_legs = []
    for lane, (entry_x, entry_floor, exit_x, exit_floor) in lanes.items():
        folder = 'Quasicombo/Routes/Section%d/%s' % (section_no, lane)
        leg_label = 'QC_%s%d_Entry' % (lane, section_no)
        leg_is_new = leg_label not in existing
        leg = place(leg_label, leg_class, entry_x, entry_floor + 90, folder,
                    preserve_existing_actor=True)
        forward = place('QC_%s%d_ExitGate' % (lane, section_no), barrier_class,
                        exit_x, exit_floor + 510, folder, (1, 1, 3),
                        preserve_existing_transform=True)
        if leg_is_new:
            leg.set_editor_property('lane', lane_enum[lane])
            leg.set_editor_property('section', section)
            leg.set_editor_property('forward_barrier', forward)
        section_legs.append(leg)
        legs[(section_no, lane)] = leg
    section.set_editor_property('legs', section_legs)

# Old generated per-lane rear gates have been superseded by one authored gate
# per section. Only remove the nine exact generated labels.
for section_no in layout:
    for lane in 'ABC':
        label = 'QC_%s%d_RearGate' % (lane, section_no)
        old_gate = existing.get(label)
        if old_gate:
            if old_gate.get_class().get_name() != 'QuasicomboBarrier':
                raise RuntimeError('Refusing to remove non-generated gate ' + label)
            if not actor_sub.destroy_actor(old_gate):
                raise RuntimeError('Could not remove ' + label)
            existing.pop(label)

# Each final route ends at a marked transfer into one common floor. The lower
# exit uses the existing moving-platform approach.
for lane, x, floor in [('A', 25700, 3000), ('B', 25700, 1400), ('C', 27000, -1000)]:
    entry = place('QC_%s3_ArenaTransfer' % lane, entry_class, x, floor + 90,
                  'Quasicombo/Arena/Transfers')
    entry.set_editor_property('lane', lane_enum[lane])
    entry.set_editor_property('final_leg', legs[(3, lane)])
    entry.set_editor_property('arrival_location', unreal.Vector(28200, Y, 1090))

floor_actor = place('QC_ArenaFloor', unreal.StaticMeshActor, 30000, 500,
                    'Quasicombo/Arena', (45, 6, 10))
cube = unreal.load_asset('/Engine/BasicShapes/Cube')
floor_actor.get_component_by_class(unreal.StaticMeshComponent).set_static_mesh(cube)
floor_actor.get_component_by_class(unreal.StaticMeshComponent).set_collision_profile_name('BlockAll')
place('QC_ArenaRearWall', barrier_class, 27800, 1510, 'Quasicombo/Arena', (1, 1, 3)).set_editor_property('initially_closed', True)
place('QC_ArenaFrontWall', barrier_class, 32100, 1510, 'Quasicombo/Arena', (1, 1, 3)).set_editor_property('initially_closed', True)

boss = place('QC_Boss', boss_class, 30000, 1122, 'Quasicombo/Arena', (1.35, 1.35, 1.35))
close = place('QC_BossCloseCamera', unreal.CameraActor, 29880, 1280, 'Quasicombo/Arena/Cameras')
close.set_actor_location(unreal.Vector(29880, 1500, 1280), False, False)
close.set_actor_rotation(unreal.Rotator(-4, -75, 0), False)
close.get_component_by_class(unreal.CameraComponent).set_editor_property('field_of_view', 45.0)
impact = place('QC_BossImpactCamera', unreal.CameraActor, 30200, 1290, 'Quasicombo/Arena/Cameras')
impact.set_actor_location(unreal.Vector(30200, 1450, 1290), False, False)
impact.set_actor_rotation(unreal.Rotator(-6, -115, 0), False)
impact.get_component_by_class(unreal.CameraComponent).set_editor_property('field_of_view', 48.0)
boss.set_editor_property('closeup_camera', close)
boss.set_editor_property('impact_camera', impact)

place('QC_HUD', hud_class, -500, 1000, 'Quasicombo/UI')

for label, x, z, sx, sy in [
    ('QC_Pit_FirstGap', 7600, -1900, 2.0, 4.0),
    ('QC_Pit_SecondGap', 16700, -2300, 2.0, 4.0),
    ('QC_Pit_FinalGap', 25800, -1900, 1.5, 4.0),
    ('QC_BelowLevelSafety', 15000, -4300, 34.0, 10.0),
]:
    place(label, kill_class, x, z, 'Quasicombo/Pits', (sx, sy, 2))

saved_level = unreal.EditorLevelLibrary.save_current_level()
print('BQ_WIRED', len(legs), 'legs, 3 shared rear gates, boss, HUD, pits', 'level_saved', saved_level)
