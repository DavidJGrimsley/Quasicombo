"""Replace generated lane rear gates with the three authored section meshes.

Run this focused migration with Unreal Editor closed. It updates section and
leg references and removes only the nine obsolete generated actors in
/Game/Maps/Lvl_Main.
"""
import unreal

LEVEL = '/Game/Maps/Lvl_Main'
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors_by_label = {}
for actor in actor_subsystem.get_all_level_actors():
    actors_by_label.setdefault(actor.get_actor_label(), []).append(actor)


def require_one(label):
    matches = actors_by_label.get(label, [])
    if len(matches) != 1:
        raise RuntimeError('Expected exactly one ' + label + ', found ' + str(len(matches)))
    return matches[0]


def transform_values(actor):
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    scale = actor.get_actor_scale3d()
    return (location.x, location.y, location.z,
            rotation.pitch, rotation.yaw, rotation.roll,
            scale.x, scale.y, scale.z)


obsolete = []
exit_transforms = {}
rear_transforms = {}
for section_number in (1, 2, 3):
    section = require_one('QC_Section_%d' % section_number)
    rear_gate = require_one('RearGateSection%d' % section_number)
    if not isinstance(rear_gate, unreal.StaticMeshActor):
        raise RuntimeError(rear_gate.get_actor_label() + ' must be a StaticMeshActor')
    rear_transforms[rear_gate.get_actor_label()] = transform_values(rear_gate)
    if section.get_class().get_name() != 'QuasicomboRouteSection':
        raise RuntimeError(section.get_actor_label() + ' has the wrong class')
    print('BQ_SECTION_PATH', section_number, section.get_path_name())
    for lane in 'ABC':
        leg = require_one('QC_%s%d_Entry' % (lane, section_number))
        exit_gate = require_one('QC_%s%d_ExitGate' % (lane, section_number))
        if leg.get_editor_property('section') != section:
            raise RuntimeError(leg.get_actor_label() + ' has the wrong section')
        if leg.get_editor_property('forward_barrier') != exit_gate:
            raise RuntimeError(leg.get_actor_label() + ' has the wrong exit gate')
        # Reserialize the leg after removing its old RearBarrier UPROPERTY.
        # Otherwise its external actor package still imports the deleted gate.
        leg.modify()
        exit_transforms[exit_gate.get_actor_label()] = transform_values(exit_gate)
        print('BQ_EXIT_BEFORE', exit_gate.get_actor_label(), exit_transforms[exit_gate.get_actor_label()])
        old_label = 'QC_%s%d_RearGate' % (lane, section_number)
        matches = actors_by_label.get(old_label, [])
        if len(matches) > 1:
            raise RuntimeError('Duplicate generated gate ' + old_label)
        for gate in matches:
            if gate.get_class().get_name() != 'QuasicomboBarrier':
                raise RuntimeError('Refusing to remove non-generated gate ' + old_label)
            print('BQ_OBSOLETE_GATE_PATH', old_label, gate.get_path_name())
        obsolete.extend(matches)
    section.modify()
    section.set_editor_property('rear_gate', rear_gate)

for gate in obsolete:
    if not actor_subsystem.destroy_actor(gate):
        raise RuntimeError('Could not remove ' + gate.get_actor_label())

for label, original_transform in exit_transforms.items():
    if transform_values(require_one(label)) != original_transform:
        raise RuntimeError('Exit gate moved: ' + label)
for label, original_transform in rear_transforms.items():
    if transform_values(require_one(label)) != original_transform:
        raise RuntimeError('Authored rear gate moved: ' + label)

if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError('Could not save ' + LEVEL)
print('BQ_SHARED_REAR_GATES', 3, 'removed_generated', len(obsolete),
      'exit_transforms_unchanged', len(exit_transforms))
