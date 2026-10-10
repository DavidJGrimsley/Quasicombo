"""Expose and organize the frontend's Designer-friendly tuning properties."""
from mcp_client import UnrealMCP
from blueprint_author import Blueprint, B

m=UnrealMCP()
m.describe(B.rstrip('.'))
director=Blueprint(m,'BP_QuasicomboFrontend')
groups={'Menu|Camera':['CameraOffset','CameraAimOffset','CameraFOV'],
        'Menu|Timing':['FadeSeconds','CameraBlendSeconds','EmoteIntervalSeconds','BlinkOffsetSeconds','IdleCycleSeconds','IdleAggroShare'],
        'Menu|Audio':['IntroSound'], 'Menu|Animation':['WaveCompleteClip','BlinkClip','RegularIdleClip','AggroIdleClip'],
        'Menu|Saving':['SaveSlot']}
for category,names in groups.items():
    for name in names:
        m.call(B+'set_variable_category',blueprint=director.asset,variable_name=name,category=category)
        m.call(B+'set_variable_instance_editable',blueprint=director.asset,variable_name=name,instance_editable=True)
director.compile();director.save()
prefs=Blueprint(m,'BP_QuasicomboPreferences')
for name in ['PreferenceVersion','SavedBody','SavedLights','SavedMusic','SavedAmbience']:
    m.call(B+'set_variable_instance_editable',blueprint=prefs.asset,variable_name=name,instance_editable=True)
prefs.compile();prefs.save()
