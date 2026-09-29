"""Compile the side-scroller pawn and current Quasicombo combat assets."""

import unreal


ASSETS = (
    "/Game/Variant_SideScrolling/Blueprints/BP_SideScrollingCharacter",
    "/Game/Quasicombo/Enemies/BP_QC_Enemy",
    "/Game/Quasicombo/Boss/BP_QC_Boss",
)

for path in ASSETS:
    blueprint = unreal.EditorAssetLibrary.load_asset(path)
    if not blueprint:
        raise RuntimeError("Missing Blueprint: " + path)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    status = str(blueprint.get_editor_property("status"))
    print("BQ_BLUEPRINT", path, status)
    if "ERROR" in status.upper():
        raise RuntimeError("Blueprint compile failed: " + path)
