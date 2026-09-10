#
#	FILE:	 Fantasy_Realm.py
#	AUTHOR:  Bob Thomas (Sirian)
#	PURPOSE: Regional map script - Fantastical terrain, X and Y Wrap
#-----------------------------------------------------------------------------
#	Copyright (c) 2005 Firaxis Games, Inc. All rights reserved.
#-----------------------------------------------------------------------------
#
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#

from CvPythonExtensions import *
import CvUtil
import CvMapGeneratorUtil
import random
import sys
from SAS_WorldSizeUtils import *
from CvMapGeneratorUtil import FractalWorld
from CvMapGeneratorUtil import TerrainGenerator
from CvMapGeneratorUtil import FeatureGenerator
from SASUtils import getInfoTypeOrFail

def getDescription():
	return "TXT_KEY_MAP_SCRIPT_FANTASY_REALM_DESCR"

def isAdvancedMap():
	"This map should not show up in simple mode"
	return 1

def getNumCustomMapOptions():
	return 2

def getNumHiddenCustomMapOptions():
	return 1

def getCustomMapOptionName(argsList):
	[iOption] = argsList
	option_names = {
		0:	"TXT_KEY_MAP_SCRIPT_RESOURCE_APPEARANCE",
		1:	"TXT_KEY_MAP_WORLD_WRAP"
		}
	if not option_names.has_key(iOption):
		sas_warn_simple_game_stale_option_once(iOption, getNumCustomMapOptions())
	translated_text = unicode(CyTranslator().getText(option_names[iOption], ()))
	return translated_text

def getNumCustomMapOptionValues(argsList):
	[iOption] = argsList
	option_values = {
		0:	3,
		1:	3
		}
	return option_values[iOption]

def getCustomMapOptionDescAt(argsList):
	[iOption, iSelection] = argsList
	selection_names = {
		0:	{
			0: "TXT_KEY_MAP_SCRIPT_LOGICAL",
			1: "TXT_KEY_MAP_SCRIPT_IRRATIONAL",
			2: "TXT_KEY_MAP_SCRIPT_CRAZY"
			},
		1:	{
			0: "TXT_KEY_MAP_WRAP_FLAT",
			1: "TXT_KEY_MAP_WRAP_CYLINDER",
			2: "TXT_KEY_MAP_WRAP_TOROID"
			}
		}
	if not selection_names.has_key(iOption):
		sas_warn_simple_game_stale_option_once(iOption, getNumCustomMapOptions())
	translated_text = unicode(CyTranslator().getText(selection_names[iOption][iSelection], ()))
	return translated_text

def getCustomMapOptionDefault(argsList):
	[iOption] = argsList
	option_defaults = {
		0:	1,
		1:	2
		}
	return option_defaults[iOption]

def isRandomCustomMapOption(argsList):
	[iOption] = argsList
	option_random = {
		0:	false,
		1:	false
		}
	return option_random[iOption]

def getWrapX():
	map = CyMap()
	return (map.getCustomMapOption(1) == 1 or map.getCustomMapOption(1) == 2)

def getWrapY():
	map = CyMap()
	return (map.getCustomMapOption(1) == 2)

def isClimateMap():
	return 0

def isBonusIgnoreLatitude():
	return True

def getGridSize(argsList):
	"Because this map is almost entirely land, use a smaller grid to avoid overly wide city spacing."
	if (argsList[0] == -1): # (-1,) is passed to function on loads
		return []
	[eWorldSize] = argsList
	return sas_get_compact_almost_all_land_grid_size(eWorldSize)

def minStartingDistanceModifier():
	return -25

def findStartingArea(argsList):
	"make sure all players are on the biggest area"
	[playerID] = argsList
	gc = CyGlobalContext()
	return gc.getMap().findBiggestArea(False).getID()

def beforeGeneration():
	global crazy_food, crazy_luxury, crazy_strategic, crazy_late_game
	global eliminated_resources, crazy_types
	eliminated_resources = []
	crazy_types = []
	map = CyMap()
	userInputResources = map.getCustomMapOption(0)
	if userInputResources != 2:
		return

	# Set up "Crazy" resources.
	food_list = ["BONUS_BANANA", "BONUS_MAIZE", "BONUS_PIG", "BONUS_RICE", "BONUS_SHEEP", "BONUS_WHEAT"]
	luxury_list = ["BONUS_GEMSTONES", "BONUS_GOLD", "BONUS_INCENSE", "BONUS_SILK", "BONUS_SILVER", "BONUS_GRAPES"]
	strategic_list = ["BONUS_COPPER", "BONUS_HORSE", "BONUS_IRON", "BONUS_ELEPHANTS", "BONUS_MARBLE", "BONUS_STONE"]
	late_list = ["BONUS_ALUMINUM", "BONUS_COAL", "BONUS_OIL", "BONUS_URANIUM"]
	sea_list = ["BONUS_MOLLUSCS", "BONUS_CRAB", "BONUS_FISH", "BONUS_WHALE"]
	leftovers_list = ["BONUS_DYE", "BONUS_FUR", "BONUS_SPICES", "BONUS_SUGAR", "BONUS_CATTLE", "BONUS_DEER"]

	# Choose the four "Crazy" resources.
	gc = CyGlobalContext()
	dice = gc.getGame().getMapRand()
	foodRoll = dice.get(6, "Crazy Food - Fantasy Realm PYTHON")
	crazy_food = food_list[foodRoll]
	del food_list[foodRoll]
	luxuryRoll = dice.get(6, "Crazy Luxury - Fantasy Realm PYTHON")
	crazy_luxury = luxury_list[luxuryRoll]
	del luxury_list[luxuryRoll]
	strategicRoll = dice.get(6, "Crazy Strategic - Fantasy Realm PYTHON")
	crazy_strategic = strategic_list[strategicRoll]
	del strategic_list[strategicRoll]
	lateRoll = dice.get(4, "Crazy Late Game - Fantasy Realm PYTHON")
	crazy_late_game = late_list[lateRoll]
	del late_list[lateRoll]

	# Now choose the EIGHT (8!) resources that will not appear at all in this game!
	for loopy in range(2):
		foodRoll = dice.get(len(food_list), "Eliminated Food - Fantasy Realm PYTHON")
		eliminated_resources.append(food_list[foodRoll])
		del food_list[foodRoll]
		luxuryRoll = dice.get(len(luxury_list), "Eliminated Luxury - Fantasy Realm PYTHON")
		eliminated_resources.append(luxury_list[luxuryRoll])
		del luxury_list[luxuryRoll]
		strategicRoll = dice.get(len(strategic_list), "Eliminated Strategic - Fantasy Realm PYTHON")
		eliminated_resources.append(strategic_list[strategicRoll])
		del strategic_list[strategicRoll]
	lateRoll = dice.get(3, "Eliminated Late Game - Fantasy Realm PYTHON")
	eliminated_resources.append(late_list[lateRoll])
	del late_list[lateRoll]
	seaRoll = dice.get(4, "Eliminated Sea - Fantasy Realm PYTHON")
	eliminated_resources.append(sea_list[seaRoll])
	del sea_list[seaRoll]

	# Crazy variables all finished.
	return

# Subclass
class FantasyFractalWorld(CvMapGeneratorUtil.FractalWorld):
	def initFractal(self, continent_grain = 2, rift_grain = 2, has_center_rift = True, invert_heights = False):
		"For no rifts, use rift_grain = -1"
		iFlags = 0
		if invert_heights:
			iFlags = iFlags | CyFractal.FracVals.FRAC_INVERT_HEIGHTS
		if rift_grain >= 0:
			self.riftsFrac = CyFractal()
			self.riftsFrac.fracInit(self.iNumPlotsX, self.iNumPlotsY, rift_grain, self.mapRand, iFlags, self.fracXExp, self.fracYExp)
			if has_center_rift:
				iFlags = iFlags | CyFractal.FracVals.FRAC_CENTER_RIFT
			self.continentsFrac.fracInitRifts(self.iNumPlotsX, self.iNumPlotsY, continent_grain, self.mapRand, iFlags, self.riftsFrac, self.fracXExp, self.fracYExp)
		else:
			self.continentsFrac.fracInit(self.iNumPlotsX, self.iNumPlotsY, continent_grain, self.mapRand, iFlags, self.fracXExp, self.fracYExp)

	def generatePlotTypes(self, water_percent=9, shift_plot_types=True, grain_amount=3):
		# Check for changes to User Input variances.
		self.checkForOverrideDefaultUserInputVariances()

		self.hillsFrac.fracInit(self.iNumPlotsX, self.iNumPlotsY, grain_amount, self.mapRand, 0, self.fracXExp, self.fracYExp)
		self.peaksFrac.fracInit(self.iNumPlotsX, self.iNumPlotsY, grain_amount+1, self.mapRand, 0, self.fracXExp, self.fracYExp)

		water_percent += self.seaLevelChange
		water_percent = min(water_percent, 14)
		water_percent = max(water_percent, 7)

		iWaterThreshold = self.continentsFrac.getHeightFromPercent(water_percent)
		iHillsBottom1 = self.hillsFrac.getHeightFromPercent(max((self.hillGroupOneBase - self.hillGroupOneRange), 0))
		iHillsTop1 = self.hillsFrac.getHeightFromPercent(min((self.hillGroupOneBase + self.hillGroupOneRange), 100))
		iHillsBottom2 = self.hillsFrac.getHeightFromPercent(max((self.hillGroupTwoBase - self.hillGroupTwoRange), 0))
		iHillsTop2 = self.hillsFrac.getHeightFromPercent(min((self.hillGroupTwoBase + self.hillGroupTwoRange), 100))
		iPeakThreshold = self.peaksFrac.getHeightFromPercent(self.peakPercent)

		for x in range(self.iNumPlotsX):
			for y in range(self.iNumPlotsY):
				i = y*self.iNumPlotsX + x
				val = self.continentsFrac.getHeight(x,y)
				if val <= iWaterThreshold:
					self.plotTypes[i] = PlotTypes.PLOT_OCEAN
				else:
					hillVal = self.hillsFrac.getHeight(x,y)
					if ((hillVal >= iHillsBottom1 and hillVal <= iHillsTop1) or (hillVal >= iHillsBottom2 and hillVal <= iHillsTop2)):
						peakVal = self.peaksFrac.getHeight(x,y)
						if (peakVal <= iPeakThreshold):
							self.plotTypes[i] = PlotTypes.PLOT_PEAK
						else:
							self.plotTypes[i] = PlotTypes.PLOT_HILLS
					else:
						self.plotTypes[i] = PlotTypes.PLOT_LAND

		if shift_plot_types:
			self.shiftPlotTypes()

		return self.plotTypes

def generatePlotTypes():
	"generate a very grainy world for lots of little lakes"
	NiTextOut("Setting Plot Types (Python Fantasy Realm) ...")
	global fractal_world
	fractal_world = FantasyFractalWorld()
	fractal_world.initFractal(continent_grain = 3, rift_grain = -1, has_center_rift = False, invert_heights = True)
	plot_types = fractal_world.generatePlotTypes(water_percent = 10)
	return plot_types

# subclass TerrainGenerator to redefine everything. This is a regional map.
class FantasyTerrainGenerator(CvMapGeneratorUtil.TerrainGenerator):
	def __init__(self, fracXExp=-1, fracYExp=-1):
		self.gc = CyGlobalContext()
		self.map = CyMap()

		self.grain_amount = 7

		self.iWidth = self.map.getGridWidth()
		self.iHeight = self.map.getGridHeight()

		self.mapRand = self.gc.getGame().getMapRand()

		self.iFlags = 0  # Disallow FRAC_POLAR flag, to prevent "zero row" problems.

		self.fantasy=CyFractal()

		self.fracXExp = fracXExp
		self.fracYExp = fracYExp

		self.initFractals()

	def initFractals(self):
		self.fantasy.fracInit(self.iWidth, self.iHeight, self.grain_amount, self.mapRand, self.iFlags, self.fracXExp, self.fracYExp)
		self.iTen = self.fantasy.getHeightFromPercent(10)
		self.iTwenty = self.fantasy.getHeightFromPercent(20)
		self.iTwentySeven = self.fantasy.getHeightFromPercent(27)
		self.iThirtyFive = self.fantasy.getHeightFromPercent(35)
		self.iFortyFive = self.fantasy.getHeightFromPercent(45)
		self.iFiftyFive = self.fantasy.getHeightFromPercent(55)
		self.iSixtyFive = self.fantasy.getHeightFromPercent(65)
		self.iSeventyFive = self.fantasy.getHeightFromPercent(75)
		self.iEighty = self.fantasy.getHeightFromPercent(80)
		self.iNinety = self.fantasy.getHeightFromPercent(90)

		self.terrainDesert = getInfoTypeOrFail("TERRAIN_DESERT")
		self.terrainPlains = getInfoTypeOrFail("TERRAIN_PLAINS")
		self.terrainGrass = getInfoTypeOrFail("TERRAIN_GRASS")
		self.terrainIce = getInfoTypeOrFail("TERRAIN_SNOW")
		self.terrainTundra = getInfoTypeOrFail("TERRAIN_TUNDRA")

	def generateTerrainAtPlot(self,iX,iY):
		if (self.map.plot(iX, iY).isWater()):
			return self.map.plot(iX, iY).getTerrainType()
		else:
			val = self.fantasy.getHeight(iX, iY)
			if val >= self.iNinety:
				terrainVal = self.terrainIce
			elif val >= self.iEighty:
				terrainVal = self.terrainGrass
			elif val >= self.iSeventyFive:
				terrainVal = self.terrainDesert
			elif val >= self.iSixtyFive:
				terrainVal = self.terrainPlains
			elif val >= self.iFiftyFive:
				terrainVal = self.terrainTundra
			elif val >= self.iFortyFive:
				terrainVal = self.terrainGrass
			elif val >= self.iThirtyFive:
				terrainVal = self.terrainPlains
			elif val >= self.iTwentySeven:
				terrainVal = self.terrainTundra
			elif val >= self.iTwenty:
				terrainVal = self.terrainIce
			elif val < self.iTen:
				terrainVal = self.terrainGrass
			else:
				terrainVal = self.terrainDesert

		if (terrainVal == TerrainTypes.NO_TERRAIN):
			return self.map.plot(iX, iY).getTerrainType()

		return terrainVal

def generateTerrainTypes():
	NiTextOut("Generating Terrain (Python Fantasy Realm) ...")
	terraingen = FantasyTerrainGenerator()
	terrainTypes = terraingen.generateTerrain()
	return terrainTypes

class FantasyFeatureGenerator(CvMapGeneratorUtil.FeatureGenerator):
	def __init__(self, iJunglePercent=20, iForestPercent=30, forest_grain=6, fracXExp=-1, fracYExp=-1):
		self.gc = CyGlobalContext()
		self.map = CyMap()
		self.mapRand = self.gc.getGame().getMapRand()
		self.forests = CyFractal()

		self.iFlags = 0 

		self.iGridW = self.map.getGridWidth()
		self.iGridH = self.map.getGridHeight()

		self.iJunglePercent = iJunglePercent
		self.iForestPercent = iForestPercent

		self.forest_grain = forest_grain + self.gc.getWorldInfo(self.map.getWorldSize()).getFeatureGrainChange()

		self.fracXExp = fracXExp
		self.fracYExp = fracYExp

		self.__initFractals()
		self.__initFeatureTypes()

	def __initFractals(self):
		self.forests.fracInit(self.iGridW, self.iGridH, self.forest_grain, self.mapRand, self.iFlags, self.fracXExp, self.fracYExp)

		self.iJungleLevel = self.forests.getHeightFromPercent(100 - self.iJunglePercent)
		self.iForestLevel = self.forests.getHeightFromPercent(self.iForestPercent)

	def __initFeatureTypes(self):
		self.featureJungle = getInfoTypeOrFail("FEATURE_JUNGLE")
		self.featureForest = getInfoTypeOrFail("FEATURE_FOREST")
		self.featureOasis = getInfoTypeOrFail("FEATURE_OASIS")
		self.featureFlood = getInfoTypeOrFail("FEATURE_FLOOD_PLAINS")
		self.featureIce = getInfoTypeOrFail("FEATURE_ICE")

	def addFeaturesAtPlot(self, iX, iY):
		pPlot = self.map.sPlot(iX, iY)

		if pPlot.isPeak():
			pass

		elif pPlot.isWater():
			self.addIceAtPlot(pPlot, iX, iY)

		else:
			if pPlot.isRiverSide() and pPlot.isFlatlands():
				self.addFloodAtPlot(pPlot, iX, iY)

			if (pPlot.getFeatureType() == FeatureTypes.NO_FEATURE):
				self.addOasisAtPlot(pPlot, iX, iY)

			if (pPlot.getFeatureType() == FeatureTypes.NO_FEATURE):
				self.addJunglesAtPlot(pPlot, iX, iY)

			if (pPlot.getFeatureType() == FeatureTypes.NO_FEATURE):
				self.addForestsAtPlot(pPlot, iX, iY)

	def addIceAtPlot(self, pPlot, iX, iY):
		iceRoll = self.mapRand.get(35, "Add Feature PYTHON")
		if iceRoll < 3:
			pPlot.setFeatureType(self.featureIce, -1)

	def addFloodAtPlot(self, pPlot, iX, iY):
		if pPlot.getTerrainType() == getInfoTypeOrFail("TERRAIN_DESERT") or pPlot.getTerrainType() == getInfoTypeOrFail("TERRAIN_SNOW"):
			pPlot.setFeatureType(self.featureFlood, -1)

	def addOasisAtPlot(self, pPlot, iX, iY):
		if not pPlot.isFreshWater():
			if pPlot.getTerrainType() != getInfoTypeOrFail("TERRAIN_GRASS"):
				if self.mapRand.get(30, "Add Feature PYTHON") == 23:
					pPlot.setFeatureType(self.featureOasis, -1)

	def addJunglesAtPlot(self, pPlot, iX, iY):
		# Warning: this version of JunglesAtPlot is using the forest fractal!
		if (self.forests.getHeight(iX, iY) >= self.iJungleLevel):
			pPlot.setFeatureType(self.featureJungle, -1)

	def addForestsAtPlot(self, pPlot, iX, iY):
		if self.forests.getHeight(iX, iY) <= self.iForestLevel:
			varietyRoll = self.mapRand.get(3, "Forest Variety - Fantasy PYTHON")
			pPlot.setFeatureType(self.featureForest, varietyRoll)

def addFeatures():
	global featuregen
	NiTextOut("Adding Features (Python Fantasy Realm) ...")
	featuregen = FantasyFeatureGenerator()
	featuregen.addFeatures()
	return 0

# Init bonus lists.
forcePlacementOnFlats = ("BONUS_GOLD", "BONUS_SILVER", "BONUS_COAL", "BONUS_ALUMINUM")
forcePlacementOnHills = ("BONUS_BANANA", "BONUS_RICE", "BONUS_SUGAR", "BONUS_OIL")
forcePlacementInFloodPlains = ("BONUS_INCENSE")
forcePlacementInJungle = ("BONUS_HORSE", "BONUS_WHEAT")
forcePlacementInForest = ("BONUS_GOLD", "BONUS_SILVER", "BONUS_COAL", "BONUS_ALUMINUM")
forceNotInGrass = ("BONUS_CATTLE", "BONUS_MAIZE", "BONUS_RICE", "BONUS_PIG", "BONUS_ELEPHANTS")
forceNotInDesert = ("BONUS_OIL", "BONUS_STONE", "BONUS_IRON", "BONUS_COPPER")
forceNotInSnow = ("BONUS_SILVER", "BONUS_DEER", "BONUS_FUR")
forceNotInPlains = ("BONUS_GRAPES", "BONUS_SHEEP", "BONUS_MARBLE", "BONUS_ELEPHANTS")
forceNotInJungle = ("BONUS_BANANA", "BONUS_SUGAR", "BONUS_DYE", "BONUS_OIL", "BONUS_GEMSTONES")
forceNotInForest = ("BONUS_DEER", "BONUS_SILK", "BONUS_SPICES", "BONUS_URANIUM")
forceNotInFreshWater = ("BONUS_RICE", "BONUS_MAIZE", "BONUS_WHEAT", "BONUS_SUGAR")
seaResources = ("BONUS_MOLLUSCS", "BONUS_CRAB", "BONUS_FISH", "BONUS_WHALE")

def addBonusType(argsList):
	[iBonusType] = argsList
	gc = CyGlobalContext()
	map = CyMap()
	userInputResources = map.getCustomMapOption(0)
	if userInputResources == 0:
		CyPythonMgr().allowDefaultImpl()
		return

	# Skip eliminated or crazy resources, plus handle sea-based resources in default way.
	type_string = gc.getBonusInfo(iBonusType).getType()
	global crazy_food, crazy_luxury, crazy_strategic, crazy_late_game
	global eliminated_resources
	global crazy_types
	if userInputResources == 2:
		if type_string in eliminated_resources: # None of this type will appear!
			return None
		if type_string == crazy_food or type_string == crazy_luxury or type_string == crazy_strategic or type_string == crazy_late_game:
			crazy_types.append(iBonusType)
			return None # Crazy resources will be added in afterGeneration()
	if type_string in seaResources:
		CyPythonMgr().allowDefaultImpl()
		return

	# Now place the rest of the resources.
	dice = gc.getGame().getMapRand()
	iW = map.getGridWidth()
	iH = map.getGridHeight()

	# init forced-eligibility flags
	forceFlats = False
	forceHills = False
	forceFlood = False
	forceJungle = False
	forceForest = False
	forceNoGrass = False
	forceNoDesert = False
	forceNoSnow = False
	forceNoPlains = False
	forceNoJungle = False
	forceNoForest = False
	forceNoFresh = False
	if type_string in forcePlacementOnFlats:
		forceFlats = True
	if type_string in forcePlacementOnHills:
		forceHills = True
	if type_string in forcePlacementInFloodPlains:
		forceFlood = True
	if type_string in forcePlacementInJungle:
		forceJungle = True
	if type_string in forcePlacementInForest:
		forceForest = True
	if type_string in forceNotInGrass:
		forceNoGrass = True
	if type_string in forceNotInDesert:
		forceNoDesert = True
	if type_string in forceNotInSnow:
		forceNoSnow = True
	if type_string in forceNotInPlains:
		forceNoPlains = True
	if type_string in forceNotInJungle:
		forceNoJungle = True
	if type_string in forceNotInForest:
		forceNoForest = True
	if type_string in forceNotInFreshWater:
		forceNoFresh = True

	# determine number of bonuses to place (defined as count)
	# size modifier is a fixed component based on world size
	# <!-- custom: Added ARENA and SAS sizes (24, 32, 40, 48 players) using integer indices for compatibility. Credit: Claude Opus 4.5. (GPT-5.2-Codex) -->
	# <!-- custom: note: now that we added worldsizes to DLL as enums, not sure this is still needed but left as such for no tedium as of now. -->
	sizekey = int(map.getWorldSize())
	sizevalues = sas_default_sizevalues()
	sizemodifier = sas_lookup_world_size(sizekey, sizevalues)
	# playermodifier involves two layers of randomnity.
	players = gc.getGame().countCivPlayersEverAlive()
	plrcomponent1 = int(players / 3.0) # Bonus Method Fixed Component
	plrcomponent2 = dice.get(players, "Bonus Method Abundant Component - Fantasy Realm PYTHON") + 1
	plrcomponent3 = dice.get(int(players / 1.6), "Bonus Method Medium Component - Fantasy Realm PYTHON") - 1
	plrmethods = [plrcomponent1, plrcomponent2, plrcomponent3, plrcomponent2]

	playermodifier = plrmethods[dice.get(4, "Bonus Method - Fantasy Realm PYTHON")]
	count = sizemodifier + playermodifier
	if count <= 0:
		return None # This bonus drew a short straw. None will be placed!

	# Set plot eligibility for current bonus.
	# Begin by initiating the list, into which eligible plots will be recorded.
	eligible = []
	# Loop through all plots on the map, adding eligible plots to the list.
	for x in range(iW):
		for y in range(iH):
			# First check the plot for an existing bonus.
			pPlot = map.plot(x,y)
			if pPlot.isPeak() or pPlot.isWater():
				continue # to next plot.
			if pPlot.getBonusType(-1) != -1:
				continue # to next plot.
			if pPlot.getFeatureType() == getInfoTypeOrFail("FEATURE_OASIS"):
				continue # Soren wants no bonuses in oasis plots. So mote it be.
			# Check plot type and features for eligibility.
			if forceHills and not pPlot.isHills():
				continue
			if forceFlats and not pPlot.isFlatlands():
				continue
			if forceFlood and not pPlot.getFeatureType() == getInfoTypeOrFail("FEATURE_FLOOD_PLAINS"):
				continue
			if forceJungle and not pPlot.getFeatureType() == getInfoTypeOrFail("FEATURE_JUNGLE"):
				continue
			if forceForest and not pPlot.getFeatureType() == getInfoTypeOrFail("FEATURE_FOREST"):
				continue
			if forceNoGrass and pPlot.getTerrainType() == getInfoTypeOrFail("TERRAIN_GRASS"):
				continue
			if forceNoDesert and pPlot.getTerrainType() == getInfoTypeOrFail("TERRAIN_DESERT"):
				continue
			if forceNoSnow and (pPlot.getTerrainType() == getInfoTypeOrFail("TERRAIN_TUNDRA") or pPlot.getTerrainType() == getInfoTypeOrFail("TERRAIN_SNOW")):
				continue
			if forceNoPlains and pPlot.getTerrainType() == getInfoTypeOrFail("TERRAIN_PLAINS"):
				continue
			if forceNoJungle and pPlot.getFeatureType() == getInfoTypeOrFail("FEATURE_JUNGLE"):
				continue
			if forceNoForest and pPlot.getFeatureType() == getInfoTypeOrFail("FEATURE_FOREST"):
				continue
			if forceNoFresh and pPlot.isFreshWater():
				continue
			#
			# Finally we have run all the checks.
			# 1. The plot has no bonus.
			# 2. The plot has an eligible terrain and feature type.
			# Now we append this plot to the eligible list.
			eligible.append([x,y])

	# Now we assign the bonuses to eligible plots chosen completely at random.
	while count > 0:
		if eligible == []:
			break # No eligible plots left!
		index = dice.get(len(eligible), "Bonus Placement - Fantasy Realm PYTHON")
		[x,y] = eligible[index]
		map.plot(x,y).setBonusType(iBonusType)
		del eligible[index] # Remove this plot from the eligible list.
		count = count - 1  # Reduce number of bonuses left to place.

	# This bonus type is done.
	return None

def afterGeneration():
	gc = CyGlobalContext()
	map = CyMap()
	userInputResources = map.getCustomMapOption(0)
	if userInputResources != 2: # Logical or Irrational resources (already handled).
		return None

	# Place "Crazy" resources!
	NiTextOut("Placing Crazy Resources (Python Fantasy Realm) ...")
	global crazy_food, crazy_luxury, crazy_strategic, crazy_late_game, crazy_types
	dice = gc.getGame().getMapRand()
	iW = map.getGridWidth()
	iH = map.getGridHeight()
	crazies = CyFractal()
	crazies.fracInit(iW, iH, 7, dice, 0, -1, -1)
	crazyOne = crazies.getHeightFromPercent(10)
	crazyTwo = crazies.getHeightFromPercent(30)
	crazyThree = crazies.getHeightFromPercent(45)
	crazyFour = crazies.getHeightFromPercent(55)
	crazyFive = crazies.getHeightFromPercent(70)
	for x in range(iW):
		for y in range(iH):
			# Fractalized placement of crazy resources.
			pPlot = map.plot(x,y)
			if pPlot.getBonusType(-1) != -1:
				continue # A bonus already exists in this plot!
			if pPlot.isWater() or pPlot.isPeak() or pPlot.getFeatureType() == getInfoTypeOrFail("FEATURE_OASIS"):
				continue
			crazyVal = crazies.getHeight(x,y)
			for crazy_bonus in crazy_types:
				type_string = gc.getBonusInfo(crazy_bonus).getType()
				if type_string == crazy_food:
					if (crazyVal >= crazyTwo and crazyVal < crazyThree) and (pPlot.getTerrainType() == getInfoTypeOrFail("TERRAIN_TUNDRA") or pPlot.getTerrainType() == getInfoTypeOrFail("TERRAIN_SNOW")):
						map.plot(x,y).setBonusType(crazy_bonus)
				if type_string == crazy_luxury:
					if (crazyVal >= crazyFour and crazyVal < crazyFive) and pPlot.getTerrainType() == getInfoTypeOrFail("TERRAIN_GRASS"):
						map.plot(x,y).setBonusType(crazy_bonus)
				if type_string == crazy_strategic:
					if (crazyVal >= crazyThree and crazyVal < crazyFour) and pPlot.getTerrainType() == getInfoTypeOrFail("TERRAIN_PLAINS"):
						map.plot(x,y).setBonusType(crazy_bonus)
				if type_string == crazy_late_game:
					if (crazyVal >= crazyOne and crazyVal < crazyTwo) and pPlot.getTerrainType() == getInfoTypeOrFail("TERRAIN_DESERT"):
						map.plot(x,y).setBonusType(crazy_bonus)
	# Finito
	return None

def normalizeRemovePeaks():
	return None

def normalizeRemoveBadTerrain():
	return None

def normalizeRemoveBadFeatures():
	return None

def normalizeAddGoodTerrain():
	return None
