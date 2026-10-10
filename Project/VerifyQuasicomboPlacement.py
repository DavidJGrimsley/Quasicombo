"""Report every generated route actor's nearest blockmesh support surface."""
import unreal

unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/Maps/Lvl_Main')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
meshes = [a for a in actors if a.get_class().get_name() == 'StaticMeshActor' and 'Sky' not in a.get_actor_label()]

for actor in sorted(actors, key=lambda a: a.get_actor_label()):
    label = actor.get_actor_label()
    if not label.startswith('QC_') or not any(word in label for word in ['Entry', 'Enemy', 'Gate']):
        continue
    if 'Arena' in label or 'Boss' in label:
        continue
    loc = actor.get_actor_location()
    expected = loc.z - (510 if 'Gate' in label else 90)
    surfaces = []
    for mesh in meshes:
        center, extent = mesh.get_actor_bounds(False)
        if center.x - extent.x <= loc.x <= center.x + extent.x and center.y - extent.y <= loc.y <= center.y + extent.y:
            top = center.z + extent.z
            if abs(top - expected) < 135:
                surfaces.append((abs(top-expected), mesh.get_actor_label(), round(top)))
    surfaces.sort()
    print('BQ_SUPPORT', label, 'x', round(loc.x), 'floor', round(expected), surfaces[:3])
