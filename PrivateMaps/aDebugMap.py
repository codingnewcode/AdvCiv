#
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#

from CvPythonExtensions import *
import CvUtil
import CvMapGeneratorUtil
from CvMapGeneratorUtil import FractalWorld
from CvMapGeneratorUtil import TerrainGenerator
from CvMapGeneratorUtil import FeatureGenerator
import sys
from SAS_WorldSizeUtils import *

def getDescription():
	"Description shown in the main menu"
	return "Smallest possible map used for debugging"

def isAdvancedMap():
	"This map should show up in simple mode, but only in debug builds"
	return not CyGlobalContext().isDebugBuild()

def getGridSize(argsList):
	"Because this is such a land-heavy map, override getGridSize() to make the map smaller"
	# <!-- custom: Added ARENA and SAS sizes (24, 32, 40, 48 players) using DLL WorldSizeTypes values aligned with XML order. (Claude code Opus 4.5; GPT-5.5; ChatGPT-5.5) -->
	grid_sizes = {
		WorldSizeTypes.WORLDSIZE_ARENA: (5, 5),
		WorldSizeTypes.WORLDSIZE_DUEL: (5, 5),
		WorldSizeTypes.WORLDSIZE_TINY: (5, 5),
		WorldSizeTypes.WORLDSIZE_SMALL: (5, 5),
		WorldSizeTypes.WORLDSIZE_STANDARD: (5, 5),
		WorldSizeTypes.WORLDSIZE_LARGE: (5, 5),
		WorldSizeTypes.WORLDSIZE_HUGE: (5, 5),
		WorldSizeTypes.WORLDSIZE_SAS24: (5, 5),
		WorldSizeTypes.WORLDSIZE_SAS32: (5, 5),
		WorldSizeTypes.WORLDSIZE_SAS40: (5, 5),
		WorldSizeTypes.WORLDSIZE_SAS48: (5, 5),
	}

	if (argsList[0] == -1): # (-1,) is passed to function on loads
		return []
	[eWorldSize] = argsList
	# <!-- custom: Convert enum to int for lookup, with fallback for unknown sizes (Claude code Opus 4.5) -->
	return sas_lookup_world_size(eWorldSize, grid_sizes)

def getWrapX(): return False
def getWrapY(): return False

def generatePlotTypes():
	NiTextOut("Setting Plot Types (Python Continents) ...")
	fractal_world = FractalWorld()
	fractal_world.initFractal()
	return fractal_world.generatePlotTypes()

def generateTerrainTypes():
	NiTextOut("Generating Terrain (Python Continents) ...")
	terraingen = TerrainGenerator()
	terrainTypes = terraingen.generateTerrain()
	return terrainTypes

def addFeatures():
	NiTextOut("Adding Features (Python Continents) ...")
	featuregen = FeatureGenerator()
	featuregen.addFeatures()
	return 0
