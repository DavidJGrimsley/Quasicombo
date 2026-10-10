"""Restore defaults after menu validation; never called during normal gameplay."""
import unreal
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
cls=unreal.load_class(None,'/Game/UI/Menu/BP_QuasicomboFrontend.BP_QuasicomboFrontend_C')
director=unreal.GameplayStatics.get_actor_of_class(world,cls)
assert director and not director.get_editor_property('IntroOnly')
def call(name,*args): return director.call_method(name,args=tuple(args))
call('SetFrontendScreen',2);call('ResetColors');call('SetFrontendScreen',3)
settings=director.get_editor_property('MenuShell').get_editor_property('Settings')
settings.get_editor_property('Music').set_value(1)
settings.get_editor_property('Ambience').set_value(1)
settings.call_method('ApplyFrontendAudio');call('SavePreferences');call('SetFrontendScreen',0)
saved=unreal.GameplayStatics.load_game_from_slot('QuasicomboPreferences',0)
assert saved.get_editor_property('SavedMusic')==1 and saved.get_editor_property('SavedAmbience')==1
body=saved.get_editor_property('SavedBody');lights=saved.get_editor_property('SavedLights')
assert abs(body.g-.08)<.0001 and abs(lights.g-.82)<.0001
print('QUASICOMBO_VALIDATION_PREFERENCES_RESTORED')
