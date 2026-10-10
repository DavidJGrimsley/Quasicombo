"""PIE-only save observation and optional slider focus for real input checks."""
import unreal,json
from pathlib import Path
out=Path(unreal.Paths.project_saved_dir())/'MenuValidation'
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
cls=unreal.load_class(None,'/Game/UI/Menu/BP_QuasicomboFrontend.BP_QuasicomboFrontend_C')
director=unreal.GameplayStatics.get_actor_of_class(world,cls)
shell=director.get_editor_property('MenuShell')
custom=shell.get_editor_property('Customization');settings=shell.get_editor_property('Settings')
loadout=unreal.QuasicomboLoadoutSubsystem.get_loadout(world)
saved=unreal.GameplayStatics.load_game_from_slot('QuasicomboPreferences',0)
def color(c): return [c.r,c.g,c.b,c.a]
data={'body':color(loadout.get_mike_body_region_color(unreal.MikeBodyRegion.COLOR_G)),
      'saved_body':color(saved.get_editor_property('SavedBody')),
      'lights':color(loadout.get_mike_light_color()),'saved_lights':color(saved.get_editor_property('SavedLights')),
      'body_g':custom.get_editor_property('BodyG').get_value(),
      'music':loadout.get_user_music_volume(),'saved_music':saved.get_editor_property('SavedMusic')}
(out/'slider-probe.json').write_text(json.dumps(data,indent=2))
request=out/'slider-focus.txt'
if request.exists():
    name=request.read_text().strip()
    if name:
        widget=(settings if name=='Music' else custom).get_editor_property(name)
        widget.set_user_focus(unreal.GameplayStatics.get_player_controller(world,0))
print('QUASICOMBO_SLIDER_SAVE_PROBE',data)
