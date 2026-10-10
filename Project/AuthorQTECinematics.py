"""One-time airborne migration inside the connected editor. Reruns retain source edits."""
import importlib
import pathlib
import sys
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
import QTEPoseAuthoring
importlib.reload(QTEPoseAuthoring)
import QTEChoreography
importlib.reload(QTEChoreography)
QTEChoreography.author_if_needed()
import QTEPublish
importlib.reload(QTEPublish)
import QTEEvolutionIntegration
importlib.reload(QTEEvolutionIntegration)
QTEEvolutionIntegration.integrate_if_needed()
import QTEFlightRecoil
importlib.reload(QTEFlightRecoil)
QTEFlightRecoil.author_if_needed()
QTEPublish.publish()
