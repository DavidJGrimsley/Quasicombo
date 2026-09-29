"""Read-only structural checks on the saved playable blockout wiring."""
import unreal

unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/Maps/Lvl_Main')
actors = {a.get_actor_label(): a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}

def check(condition, message):
    if not condition:
        raise RuntimeError(message)

for number in [1, 2, 3]:
    section = actors.get('QC_Section_%d' % number)
    check(section is not None, 'Missing section %d' % number)
    rear_gate = actors.get('RearGateSection%d' % number)
    check(isinstance(rear_gate, unreal.StaticMeshActor), 'Missing authored rear gate for section %d' % number)
    check(section.get_editor_property('rear_gate') == rear_gate, 'Wrong shared rear gate for section %d' % number)
    rear_center, rear_extent = rear_gate.get_actor_bounds(False)
    print('BQ_REAR_GATE_BOUNDS', number, rear_center, rear_extent)
    leg_list = section.get_editor_property('legs')
    check(len(leg_list) == 3, 'Section %d needs 3 legs' % number)
    for lane in 'ABC':
        label = 'QC_%s%d_Entry' % (lane, number)
        leg = actors.get(label)
        check(leg is not None and leg in leg_list, 'Missing/unlinked ' + label)
        check(leg.get_editor_property('section') == section, 'Wrong section on ' + label)
        check('QC_%s%d_RearGate' % (lane, number) not in actors, 'Obsolete per-lane rear gate ' + label)
        check(leg.get_editor_property('forward_barrier') == actors.get('QC_%s%d_ExitGate' % (lane, number)), 'Exit gate ' + label)
        exit_gate = actors.get('QC_%s%d_ExitGate' % (lane, number))
        print('BQ_EXIT_AFTER', exit_gate.get_actor_label(), exit_gate.get_actor_location(),
              exit_gate.get_actor_rotation(), exit_gate.get_actor_scale3d())
        enemies = leg.get_editor_property('enemies')
        for enemy in enemies:
            if not enemy:
                print('BQ_EMPTY_ENEMY_SLOT', label, 'Remove this slot when assigning placed enemies')
            else:
                check(enemy.get_class().get_name().startswith('BP_QC_Enemy'),
                      'Entry references a non-Quasicombo enemy: ' + label)
        print('BQ_ENTRY_X', label, leg.get_actor_location().x)
        print('BQ_VERIFIED_LEG', label, 'assigned_enemies', sum(bool(enemy) for enemy in enemies))

boss = actors.get('QC_Boss')
check(boss is not None, 'Missing boss')
check(boss.get_editor_property('closeup_camera') == actors.get('QC_BossCloseCamera'), 'Boss close camera')
check(boss.get_editor_property('impact_camera') == actors.get('QC_BossImpactCamera'), 'Boss impact camera')
for lane in 'ABC':
    entry = actors.get('QC_%s3_ArenaTransfer' % lane)
    check(entry is not None, 'Missing transfer ' + lane)
    check(entry.get_editor_property('final_leg') == actors.get('QC_%s3_Entry' % lane), 'Wrong transfer leg ' + lane)
for label in ['QC_HUD', 'QC_ArenaFloor', 'QC_BelowLevelSafety']:
    check(label in actors, 'Missing ' + label)
for label in ['QC_Pit_FirstGap', 'QC_Pit_SecondGap', 'QC_Pit_FinalGap']:
    if label not in actors:
        print('BQ_MISSING_OPTIONAL_PIT', label)
for label, actor in actors.items():
    if label.startswith('QC_'):
        check(not actor.get_editor_property('is_spatially_loaded'), 'Spatially loaded gameplay actor ' + label)
print('BQ_VERIFIED_TOTAL', len(actors), 'actors')
for label, actor in actors.items():
    if actor.get_class().get_name() == 'PlayerStart':
        print('BQ_PLAYER_START', label, actor.get_actor_location(), actor.get_editor_property('tags'))
mode = unreal.EditorAssetLibrary.load_blueprint_class('/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingGameMode')
print('BQ_GAME_MODE_PLAYERS', unreal.get_default_object(mode).get_editor_property('number_of_local_players'))
