"""Run from the Unreal editor Python console to jump into the lizard boss PIE fight.

This changes only the PIE copy of Lvl_Main. Edit the four preview values below to
exercise different states without changing the placed boss or using the API.
"""
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)

PREVIEW_PRE_EVOLUTION_TAU = 0.2  # look and attack rate before hit three
PREVIEW_TAU = 0.8          # look and attack rate after evolution
PREVIEW_ARMOR_BARS = 2     # 0 bare, 1 partial, 2 full armor after hit three
PREVIEW_QTE_ROUNDS = 1     # 1..3 rounds, three prompts each
PREVIEW_QTE_OUTCOME = 1   # 0 Vacuum, 1 Tau

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
callback = None

def begin_boss_preview(_delta):
    world = editor.get_game_world()
    if not world:
        return
    player = unreal.GameplayStatics.get_player_pawn(world, 0)
    boss = unreal.GameplayStatics.get_actor_of_class(world, unreal.QuasicomboBoss)
    if not player or not boss:
        return
    if callback is not None:
        unreal.unregister_slate_post_tick_callback(callback)
    boss.quantum.set_editor_property('use_live_quantum_api', False)
    boss.set_editor_property('preview_tau', PREVIEW_TAU)
    boss.set_editor_property('preview_pre_evolution_tau', PREVIEW_PRE_EVOLUTION_TAU)
    boss.set_editor_property('preview_armor_bars', PREVIEW_ARMOR_BARS)
    boss.set_editor_property('preview_qte_rounds', PREVIEW_QTE_ROUNDS)
    boss.set_editor_property('preview_qte_outcome', PREVIEW_QTE_OUTCOME)
    player.set_actor_location(boss.get_actor_location() + unreal.Vector(-420, 0, 0), False, True)
    player.character_movement.stop_movement_immediately()
    boss.start_encounter(player)
    phase = boss.get_boss_phase()
    if phase != unreal.QuasicomboBossPhase.COMBAT:
        raise RuntimeError('Lizard boss preview did not start combat: {}'.format(phase))
    print('Lizard boss PIE preview ready: combat active, approach and attack.')

if editor.get_game_world():
    begin_boss_preview(0.0)
else:
    levels.load_level('/Game/Maps/Lvl_Main')
    callback = unreal.register_slate_post_tick_callback(begin_boss_preview)
    levels.editor_request_begin_play()
