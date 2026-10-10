"""Keep the live menu preview framed as the spawned pawn settles to the floor."""
from mcp_client import UnrealMCP
from blueprint_author import Blueprint,B,A
from author_director import author_wave,author_tick

m=UnrealMCP()
for t in [B.rstrip('.'),A.rstrip('.'),'editor_toolset.toolsets.object.ObjectTools']:
    m.describe(t)
bp=Blueprint(m,'BP_QuasicomboFrontend')
bp.variables({'WavePlaybackSeconds':'float'})
g=bp.function('UpdatePreviewCamera')
position='(+ '+g.call('GetActorLocation',target=g.get('MenuPawn'))+' '+g.get('CameraOffset')+')'
aim='(+ '+g.call('GetActorLocation',target=g.get('MenuPawn'))+' '+g.get('CameraAimOffset')+')'
rotate=g.call('FindLookatRotation',Start=position,Target=aim)
g.write('(fn UpdatePreviewCamera () '+g.call('SetActorLocationAndRotation',target=g.get('MenuCamera'),
        NewLocation=position,NewRotation=rotate,bSweep='false',bTeleport='true')+')','BP_QuasicomboFrontend_PreviewCamera')
author_wave(bp)
author_tick(bp)
bp.defaults({'CameraOffset':{'x':-180,'y':700,'z':110},
             'CameraAimOffset':{'x':-180,'y':0,'z':10}})
bp.save()
