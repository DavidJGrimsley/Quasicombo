"""Tools > Quasicombo > Publish QTE Choreography invokes this editor script."""
import importlib
import pathlib
import sys
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
import QTEPublish
importlib.reload(QTEPublish)
QTEPublish.publish()
