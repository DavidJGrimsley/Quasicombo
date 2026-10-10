"""Read the actual PIE scroll offset and restore the Story button's focus."""
import unreal
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
cls=unreal.load_class(None,'/Game/UI/Menu/BP_QuasicomboFrontend.BP_QuasicomboFrontend_C')
d=unreal.GameplayStatics.get_actor_of_class(world,cls)
story=d.get_editor_property('MenuShell').get_editor_property('Story')
offset=story.get_editor_property('StoryScroll').get_scroll_offset()
(Path(unreal.Paths.project_saved_dir())/'MenuValidation/story-offset.txt').write_text(str(offset))
story.get_editor_property('Continue').set_user_focus(unreal.GameplayStatics.get_player_controller(world,0))
