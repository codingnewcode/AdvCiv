#
#	FILE:	 Great_Plains.py
#	AUTHOR:  Bob Thomas (Sirian)
#	PURPOSE: Regional map script - Great Plains, North America
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
	return "TXT_KEY_MAP_SCRIPT_GREAT_PLAINS_DESCR"

def isAdvancedMap():
	# <!-- custom: hide this map from Simple Game to keep that list curated; still available in Custom Game. (GPT-5.3-Codex) -->
	return 1

def isClimateMap():
	return 0

def isSeaLevelMap():
	return 0

def getNumCustomMapOptions():
	return 1

def getNumHiddenCustomMapOptions():
	return 1

def getCustomMapOptionName(argsList):
	translated_text = unicode(CyTranslator().getText("TXT_KEY_MAP_WORLD_WRAP", ()))
	return translated_text

def getNumCustomMapOptionValues(argsList):
	return 3

def getCustomMapOptionDescAt(argsList):
	iSelection = argsList[1]
	selection_names = ["TXT_KEY_MAP_WRAP_FLAT", "TXT_KEY_MAP_WRAP_CYLINDER", "TXT_KEY_MAP_WRAP_TOROID"]
	translated_text = unicode(CyTranslator().getText(selection_names[iSelection], ()))
	return translated_text

def getCustomMapOptionDefault(argsList):
	return 0

def isRandomCustomMapOption(argsList):
	return false

def getWrapX():
	map = CyMap()
	return (map.getCustomMapOption(0) == 1 or map.getCustomMapOption(0) == 2)

def getWrapY():
	map = CyMap()
	return (map.getCustomMapOption(0) == 2)

def getTopLatitude():
	return 45

def getBottomLatitude():
	return 25

def getGridSize(argsList):
	# This map is almost 100% land with no ice.
	# Sizes reduced MORE THAN two levels compared to global maps.
	# Dimensions also pushed more toward being squarish.
	# <!-- custom: Keep Great Plains as the source compact almost-all-land profile via SAS_WorldSizeUtils helper (same ARENA/SAS dimensions, centralized for reuse). (GPT-5.3-Codex) -->

	if (argsList[0] == -1): # (-1,) is passed to function on loads
		return []
	[eWorldSize] = argsList
	return sas_get_compact_almost_all_land_grid_size(eWorldSize)

def minStartingDistanceModifier():
	return -25

# subclass FractalWorld to enable regional assignment of plots.
class GreatPlainsFractalWorld(CvMapGeneratorUtil.FractalWorld):
	def generatePlotTypes(self, water_percent=0, shift_plot_types=False, grain_amount=3):
		gc = CyGlobalContext()
		map = CyMap()
		dice = gc.getGame().getMapRand()
		self.iFlags = self.map.getMapFractalFlags() # Defaults for that map type.

		# Varying grains for reducing "clumping" of hills/peaks on larger maps.
		sizekey = map.getWorldSize()
		# <!-- custom: Added ARENA and SAS sizes (24, 32, 40, 48 players) (GPT-5.2-Codex; GPT-5.5; ChatGPT-5.5) -->
		grainvalues = {
			WorldSizeTypes.WORLDSIZE_ARENA: 3,
			WorldSizeTypes.WORLDSIZE_DUEL: 3,
			WorldSizeTypes.WORLDSIZE_TINY: 3,
			WorldSizeTypes.WORLDSIZE_SMALL: 3,
			WorldSizeTypes.WORLDSIZE_STANDARD: 4,
			WorldSizeTypes.WORLDSIZE_LARGE: 5,
			WorldSizeTypes.WORLDSIZE_HUGE: 6,
			WorldSizeTypes.WORLDSIZE_SAS24: 7,
			WorldSizeTypes.WORLDSIZE_SAS32: 8,
			WorldSizeTypes.WORLDSIZE_SAS40: 9,
			WorldSizeTypes.WORLDSIZE_SAS48: 10,
			}
		grain_amount = sas_lookup_world_size(sizekey, grainvalues)

		self.hillsFrac.fracInit(self.iNumPlotsX, self.iNumPlotsY, grain_amount, self.mapRand, self.iFlags, self.fracXExp, self.fracYExp)
		self.peaksFrac.fracInit(self.iNumPlotsX, self.iNumPlotsY, grain_amount+1, self.mapRand, self.iFlags, self.fracXExp, self.fracYExp)
		regionsFrac = CyFractal()
		regionsFrac.fracInit(self.iNumPlotsX, self.iNumPlotsY, 3, dice, self.iFlags, self.fracXExp, self.fracYExp)

		iPlainsThreshold = self.hillsFrac.getHeightFromPercent(8)
		iHillsBottom1 = self.hillsFrac.getHeightFromPercent(20)
		iHillsTop1 = self.hillsFrac.getHeightFromPercent(30)
		iHillsBottom2 = self.hillsFrac.getHeightFromPercent(70)
		iHillsTop2 = self.hillsFrac.getHeightFromPercent(80)
		iForty = self.hillsFrac.getHeightFromPercent(40)
		iFifty = self.hillsFrac.getHeightFromPercent(50)
		iSixty = self.hillsFrac.getHeightFromPercent(60)
		iPeakThreshold = self.peaksFrac.getHeightFromPercent(25)
		iPeakRockies = self.peaksFrac.getHeightFromPercent(37)

		# Define six regions:
		# 1. Rockies  2. Plains  3. Eastern Grasslands
		# 4. The Gulf  5. Ozarks  6. SW Desert
		rockies = []
		plains = []
		grass = []
		gulf = []
		ozarks = []
		swDesert = []
		plainsWest = 0.2 # regional divide between Rockies and Plains
		plainsEast = 0.67 # divide between Plains and East
		south = self.iNumPlotsY / 4 # divide between Rockies and SW
		middle = self.iNumPlotsX / 2 # western edge of the Gulf

		# first define the Gulf, which will always be in the SE corner.
		NiTextOut("Simulate the Gulf of Mexico (Python Great Plains) ...")
		coast = middle
		for y in range(south):
			coast += dice.get(4, "Gulf of Mexico - Great Plains PYTHON")
			if coast > self.iNumPlotsX:
				break
			for x in range(coast, self.iNumPlotsX):
				i = y*self.iNumPlotsX + x
				gulf.append(i)

		# now define the Ozark Mountains, a randomly placed region of hilly terrain.
		# First roll the location in which the Ozarks will be placed.
		NiTextOut("Simulate the Ozarks (Python Great Plains) ...")
		leftX = int(self.iNumPlotsX * 0.38)
		rightX = int(self.iNumPlotsX * 0.71)
		rangeX = rightX - leftX
		bottomY = int(self.iNumPlotsY * 0.35)
		topY = int(self.iNumPlotsY * 0.67)
		rangeY = topY - bottomY
		slideX = dice.get(rangeX, "Ozarks Placement - Great Plains PYTHON")
		slideY = dice.get(rangeY, "Ozarks Placement - Great Plains PYTHON")
		# Now set the boundaries and scope of the Ozarks.
		leftOzark = leftX + slideX
		botOzark = bottomY + slideY
		widthOzark = self.iNumPlotsX / 6
		heightOzark = self.iNumPlotsY / 6
		rightOzark = leftOzark + widthOzark + 1
		topOzark = botOzark + heightOzark + 1
		midOzarkY = botOzark + heightOzark/2
		# Now loop the plots and append their index numbers to the Ozark list.
		# Run two loops for Y, both starting in middle, one to north, one to south.
		varLeft = leftOzark
		varRight = rightOzark
		for y in range(midOzarkY + 1, topOzark):
			if varLeft > varRight:
				break
			for x in range(varLeft, varRight):
				i = y*self.iNumPlotsX + x
				ozarks.append(i)
			leftSeed = dice.get(5, "Ozarks Shape - Great Plains PYTHON")
			if leftSeed == 4:
				leftSeed = 0
			if leftSeed == 3:
				leftSeed = 1
			varLeft += leftSeed
			rightSeed = dice.get(5, "Ozarks Shape - Great Plains PYTHON")
			if rightSeed == 4:
				rightSeed = 0
			if rightSeed == 3:
				rightSeed = 1
			varRight -= rightSeed
		# Second Loop
		varLeft = leftOzark
		varRight = rightOzark
		for y in range(midOzarkY, botOzark, -1):
			if varLeft > varRight:
				break
			for x in range(varLeft, varRight):
				i = y*self.iNumPlotsX + x
				ozarks.append(i)
			leftSeed = dice.get(5, "Ozarks Shape - Great Plains PYTHON")
			if leftSeed == 4:
				leftSeed = 0
			if leftSeed == 3:
				leftSeed = 1
			varLeft += leftSeed
			rightSeed = dice.get(5, "Ozarks Shape - Great Plains PYTHON")
			if rightSeed == 4:
				rightSeed = 0
			if rightSeed == 3:
				rightSeed = 1
			varRight -= rightSeed
		# now define the four easiest regions and append their plots to their plot lists
		NiTextOut("Simulate the Rockies (Python Great Plains) ...")
		for x in range(self.iNumPlotsX):
			for y in range(self.iNumPlotsY):
				i = y*self.iNumPlotsX + x
				lat = x/float(self.iNumPlotsX)
				lat += (128 - regionsFrac.getHeight(x, y))/(255.0 * 5.0)
				if lat < 0:
					lat = 0.0
				if lat > 1:
					lat = 1.0
				if y >= south and lat <= plainsWest:
					rockies.append(i)
				elif y < south and lat <= plainsWest:
					swDesert.append(i)
				elif lat >= plainsEast:
					if (i not in gulf) and (i not in ozarks):
						grass.append(i)
				else:
					if (i not in gulf) and (i not in ozarks):
						plains.append(i)

		# Now the main loop, which will assign the plot types.
		for x in range(self.iNumPlotsX):
			for y in range(self.iNumPlotsY):
				i = y*self.iNumPlotsX + x
				# Regional membership checked, effects chosen.
				if i in gulf:
					self.plotTypes[i] = PlotTypes.PLOT_OCEAN
				elif i in swDesert:
					hillVal = self.hillsFrac.getHeight(x,y)
					if ((hillVal >= iHillsBottom1 and hillVal <= iForty) or (hillVal >= iSixty and hillVal <= iHillsTop2)):
						peakVal = self.peaksFrac.getHeight(x,y)
						if (peakVal <= iPeakThreshold):
							self.plotTypes[i] = PlotTypes.PLOT_PEAK
						else:
							self.plotTypes[i] = PlotTypes.PLOT_HILLS
					else:
						self.plotTypes[i] = PlotTypes.PLOT_LAND
				elif i in ozarks:
					hillVal = self.hillsFrac.getHeight(x,y)
					if ((hillVal <= iHillsTop1) or (hillVal >= iForty and hillVal <= iFifty) or (hillVal >= iSixty and hillVal <= iHillsBottom2) or (hillVal >= iHillsTop2)):
						self.plotTypes[i] = PlotTypes.PLOT_HILLS
					else:
						self.plotTypes[i] = PlotTypes.PLOT_LAND
				elif i in rockies:
					hillVal = self.hillsFrac.getHeight(x,y)
					if hillVal >= iHillsTop1:
						peakVal = self.peaksFrac.getHeight(x,y)
						if (peakVal <= iPeakRockies):
							self.plotTypes[i] = PlotTypes.PLOT_PEAK
						else:
							self.plotTypes[i] = PlotTypes.PLOT_HILLS
					else:
						self.plotTypes[i] = PlotTypes.PLOT_LAND
				elif i in grass:
					hillVal = self.hillsFrac.getHeight(x,y)
					if ((hillVal >= iHillsBottom1 and hillVal <= iHillsTop1) or (hillVal >= iHillsBottom2 and hillVal <= iHillsTop2)):
						self.plotTypes[i] = PlotTypes.PLOT_HILLS
					else:
						self.plotTypes[i] = PlotTypes.PLOT_LAND
				else: # Plot is in the plains.
					hillVal = self.hillsFrac.getHeight(x,y)
					if hillVal < iPlainsThreshold:
						self.plotTypes[i] = PlotTypes.PLOT_HILLS
					else:
						self.plotTypes[i] = PlotTypes.PLOT_LAND

		if shift_plot_types:
			self.shiftPlotTypes()

		return self.plotTypes

def generatePlotTypes():
	NiTextOut("Setting Plot Types (Python Great Plains) ...")

	# Plot types assigned by region.
	global plotgen
	plotgen = GreatPlainsFractalWorld()

	plotgen.initFractal(continent_grain = 5, rift_grain = 2, has_center_rift = False)
	return plotgen.generatePlotTypes(water_percent = 0, shift_plot_types = False, grain_amount = 3)

# subclass TerrainGenerator to redefine everything. This is a regional map.
class GreatPlainsTerrainGenerator(CvMapGeneratorUtil.TerrainGenerator):
	def __init__(self, iRockyDesertPercent=50, iRockyPlainsPercent=30, iGrassPercent=17, iDesertPercent=8, iTexDesertPercent=20, iEastDesertPercent=2, iEastPlainsPercent=23, fWestLongitude=0.15, fEastLongitude=0.65, fTexLat=0.37, fTexEast=0.55, fracXExp=-1, fracYExp=-1, grain_amount=4):
		# Note: If you change longitude values here, then you will...
		# ...need to change them elsewhere in the script, as well.
		self.gc = CyGlobalContext()
		self.map = CyMap()

		self.grain_amount = grain_amount + self.gc.getWorldInfo(self.map.getWorldSize()).getTerrainGrainChange()

		self.iWidth = self.map.getGridWidth()
		self.iHeight = self.map.getGridHeight()

		self.mapRand = self.gc.getGame().getMapRand()

		self.iFlags = 0  # Disallow FRAC_POLAR flag, to prevent "zero row" problems.

		self.rocky=CyFractal()
		self.plains=CyFractal()
		self.east=CyFractal()
		self.variation=CyFractal()

		self.iRockyDTopPercent = 100
		self.iRockyDBottomPercent = max(0,int(100-iRockyDesertPercent))
		self.iRockyPTopPercent = int(100 - iRockyDesertPercent)
		self.iRockyPBottomPercent = max(0,int(100-iRockyDesertPercent-iRockyPlainsPercent))
		self.iDesertTopPercent = 100
		self.iDesertBottomPercent = max(0,int(100-iDesertPercent))
		self.iTexDesertTopPercent = 70
		self.iTexDesertBottomPercent = max(0,int(70-iTexDesertPercent))
		self.iGrassTopPercent = int(iGrassPercent)
		self.iGrassBottomPercent = 0
		self.iTexGrassBottomPercent = 6
		self.iEastDTopPercent = 100
		self.iEastDBottomPercent = max(0,int(100-iEastDesertPercent))
		self.iEastPTopPercent = int(100 - iEastDesertPercent)
		self.iEastPBottomPercent = max(0,int(100-iEastDesertPercent-iEastPlainsPercent))
		self.iMountainTopPercent = 75
		self.iMountainBottomPercent = 60

		self.fWestLongitude = fWestLongitude
		self.fEastLongitude = fEastLongitude
		self.fTexLat = fTexLat
		self.fTexEast = fTexEast

		self.iGrassPercent = iGrassPercent
		self.iDesertPercent = iDesertPercent
		self.iTexDesertPercent = iTexDesertPercent
		self.iEastDesertPercent = iEastDesertPercent
		self.iEastPlainsPercent = iEastPlainsPercent
		self.iRockyDesertPercent = iRockyDesertPercent
		self.iRockyPlainsPercent = iRockyPlainsPercent

		self.fracXExp = fracXExp
		self.fracYExp = fracYExp

		self.initFractals()

	def initFractals(self):
		self.rocky.fracInit(self.iWidth, self.iHeight, self.grain_amount, self.mapRand, self.iFlags, self.fracXExp, self.fracYExp)
		self.iRockyDTop = self.rocky.getHeightFromPercent(self.iRockyDTopPercent)
		self.iRockyDBottom = self.rocky.getHeightFromPercent(self.iRockyDBottomPercent)
		self.iRockyPTop = self.rocky.getHeightFromPercent(self.iRockyPTopPercent)
		self.iRockyPBottom = self.rocky.getHeightFromPercent(self.iRockyPBottomPercent)

		self.plains.fracInit(self.iWidth, self.iHeight, self.grain_amount+1, self.mapRand, self.iFlags, self.fracXExp, self.fracYExp)
		self.iDesertTop = self.plains.getHeightFromPercent(self.iDesertTopPercent)
		self.iDesertBottom = self.plains.getHeightFromPercent(self.iDesertBottomPercent)
		self.iTexDesertTop = self.plains.getHeightFromPercent(self.iTexDesertTopPercent)
		self.iTexDesertBottom = self.plains.getHeightFromPercent(self.iTexDesertBottomPercent)
		self.iGrassTop = self.plains.getHeightFromPercent(self.iGrassTopPercent)
		self.iGrassBottom = self.plains.getHeightFromPercent(self.iGrassBottomPercent)
		self.iTexGrassBottom = self.plains.getHeightFromPercent(self.iTexGrassBottomPercent)

		self.east.fracInit(self.iWidth, self.iHeight, self.grain_amount, self.mapRand, self.iFlags, self.fracXExp, self.fracYExp)
		self.iEastDTop = self.east.getHeightFromPercent(self.iEastDTopPercent)
		self.iEastDBottom = self.east.getHeightFromPercent(self.iEastDBottomPercent)
		self.iEastPTop = self.east.getHeightFromPercent(self.iEastPTopPercent)
		self.iEastPBottom = self.east.getHeightFromPercent(self.iEastPBottomPercent)

		self.variation.fracInit(self.iWidth, self.iHeight, self.grain_amount, self.mapRand, self.iFlags, self.fracXExp, self.fracYExp)

		self.terrainDesert = getInfoTypeOrFail("TERRAIN_DESERT")
		self.terrainPlains = getInfoTypeOrFail("TERRAIN_PLAINS")
		self.terrainGrass = getInfoTypeOrFail("TERRAIN_GRASS")

	def getLatitudeAtPlot(self, iX, iY):
		lat = iX/float(self.iWidth) # 0.0 = west

		# Adjust latitude using self.variation fractal, to mix things up:
		lat += (128 - self.variation.getHeight(iX, iY))/(255.0 * 5.0)

		# Limit to the range [0, 1]:
		if lat < 0:
			lat = 0.0
		if lat > 1:
			lat = 1.0

		return lat

	def generateTerrain(self):		
		terrainData = [0]*(self.iWidth*self.iHeight)
		for x in range(self.iWidth):
			for y in range(self.iHeight):
				iI = y*self.iWidth + x
				terrain = self.generateTerrainAtPlot(x, y)
				terrainData[iI] = terrain
		return terrainData

	def generateTerrainAtPlot(self,iX,iY):
		lat = self.getLatitudeAtPlot(iX,iY)

		if (self.map.plot(iX, iY).isWater()):
			return self.map.plot(iX, iY).getTerrainType()

		if lat <= self.fWestLongitude:
			val = self.rocky.getHeight(iX, iY)
			if val >= self.iRockyDBottom and val <= self.iRockyDTop:
				terrainVal = self.terrainDesert
			elif val >= self.iRockyPBottom and val <= self.iRockyPTop:
				terrainVal = self.terrainPlains
			else:
				long = iY/float(self.iHeight)
				if long > 0.23:
					terrainVal = self.terrainGrass
				else:
					terrainVal = self.terrainDesert
		elif lat > self.fEastLongitude:
			val = self.east.getHeight(iX, iY)
			if val >= self.iEastDBottom and val <= self.iEastDTop:
				terrainVal = self.terrainDesert
			elif val >= self.iEastPBottom and val <= self.iEastPTop:
				terrainVal = self.terrainPlains
			else:
				terrainVal = self.terrainGrass
		elif lat > self.fWestLongitude and lat <= self.fTexEast and iY/float(self.iHeight) <= self.fTexLat:
			# More desert added to Texas region at Soren's request.
			val = self.east.getHeight(iX, iY)
			if val >= self.iDesertBottom and val <= self.iDesertTop:
				terrainVal = self.terrainDesert
			elif val >= self.iTexDesertBottom and val <= self.iTexDesertTop:
				terrainVal = self.terrainDesert
			elif val >= self.iTexGrassBottom and val <= self.iGrassTop:
				terrainVal = self.terrainGrass
			else:
				terrainVal = self.terrainPlains
		else:
			val = self.plains.getHeight(iX, iY)
			if val >= self.iDesertBottom and val <= self.iDesertTop:
				terrainVal = self.terrainDesert
			elif val >= self.iGrassBottom and val <= self.iGrassTop:
				terrainVal = self.terrainGrass
			else:
				terrainVal = self.terrainPlains

		if (terrainVal == TerrainTypes.NO_TERRAIN):
			return self.map.plot(iX, iY).getTerrainType()

		return terrainVal

def generateTerrainTypes():
	NiTextOut("Generating Terrain (Python Great Plains) ...")
	terraingen = GreatPlainsTerrainGenerator()
	terrainTypes = terraingen.generateTerrain()
	return terrainTypes

class GreatPlainsFeatureGenerator(CvMapGeneratorUtil.FeatureGenerator):
	def __init__(self, iJunglePercent=40, iEastForestPercent=45, iForestPercent=8, iRockyForestPercent=55, forest_grain=6, fracXExp=-1, fracYExp=-1):
		self.gc = CyGlobalContext()
		self.map = CyMap()
		self.mapRand = self.gc.getGame().getMapRand()
		self.forests = CyFractal()

		self.iFlags = 0  # Disallow FRAC_POLAR flag, to prevent "zero row" problems.

		self.iGridW = self.map.getGridWidth()
		self.iGridH = self.map.getGridHeight()

		self.iJunglePercent = iJunglePercent
		self.iForestPercent = iForestPercent
		self.iEastForestPercent = iEastForestPercent
		self.iRockyForestPercent = iRockyForestPercent

		self.forest_grain = forest_grain + self.gc.getWorldInfo(self.map.getWorldSize()).getFeatureGrainChange()

		self.fracXExp = fracXExp
		self.fracYExp = fracYExp

		self.__initFractals()
		self.__initFeatureTypes()

	def __initFractals(self):
		self.forests.fracInit(self.iGridW, self.iGridH, self.forest_grain, self.mapRand, self.iFlags, self.fracXExp, self.fracYExp)

		self.iJungleLevel = self.forests.getHeightFromPercent(100 - self.iJunglePercent)
		self.iForestLevel = self.forests.getHeightFromPercent(self.iForestPercent)
		self.iEastForestLevel = self.forests.getHeightFromPercent(self.iEastForestPercent)
		self.iRockyForestLevel = self.forests.getHeightFromPercent(self.iRockyForestPercent)

	def __initFeatureTypes(self):
		self.featureJungle = getInfoTypeOrFail("FEATURE_JUNGLE")
		self.featureForest = getInfoTypeOrFail("FEATURE_FOREST")
		self.featureOasis = getInfoTypeOrFail("FEATURE_OASIS")

	def getLatitudeAtPlot(self, iX, iY):
		# 0.0 = bottom edge, 1.0 = top edge.
		return iX/float(self.iGridW)

	def addFeaturesAtPlot(self, iX, iY):
		"adds any appropriate features at the plot (iX, iY) where (0,0) is in the SW"
		long = iX/float(self.iGridW)
		lat = iY/float(self.iGridH)
		pPlot = self.map.sPlot(iX, iY)

		for iI in range(self.gc.getNumFeatureInfos()):
			if pPlot.canHaveFeature(iI):
				if self.mapRand.get(10000, "Add Feature PYTHON") < self.gc.getFeatureInfo(iI).getAppearanceProbability():
					pPlot.setFeatureType(iI, -1)

		if (pPlot.getFeatureType() == FeatureTypes.NO_FEATURE):
			# Jungles only in Louisiana Wetlands!
			if long > 0.65 and lat < 0.45:
				self.addJunglesAtPlot(pPlot, iX, iY, lat)

		if (pPlot.getFeatureType() == FeatureTypes.NO_FEATURE):
			self.addForestsAtPlot(pPlot, iX, iY, lat, long)

	def addIceAtPlot(self, pPlot, iX, iY, lat):
		# We don' need no steeking ice. M'kay? Alrighty then.
		ice = 0

	def addJunglesAtPlot(self, pPlot, iX, iY, lat):
		# Warning: this version of JunglesAtPlot is using the forest fractal!
		if pPlot.canHaveFeature(self.featureJungle):
			if (self.forests.getHeight(iX, iY) >= self.iJungleLevel):
				pPlot.setFeatureType(self.featureJungle, -1)

	def addForestsAtPlot(self, pPlot, iX, iY, lat, long):
		# Evergreens in the Rockies. (Pinion Pines! Juniper! Spruce!)
		# Deciduous trees elsewhere.
		if (long < 0.16 and lat > 0.23) and (pPlot.isFlatlands() or pPlot.isHills()):
			if self.forests.getHeight(iX, iY) <= self.iRockyForestLevel:
				pPlot.setFeatureType(self.featureForest, 2)
		elif long > 0.72 and pPlot.canHaveFeature(self.featureForest):
			if self.forests.getHeight(iX, iY) <= self.iEastForestLevel:
				pPlot.setFeatureType(self.featureForest, 0)
		else:
			if pPlot.canHaveFeature(self.featureForest):
				if self.forests.getHeight(iX, iY) <= self.iForestLevel:
					pPlot.setFeatureType(self.featureForest, 0)

def addFeatures():
	global featuregen
	NiTextOut("Adding Features (Python Great Plains) ...")
	featuregen = GreatPlainsFeatureGenerator()
	featuregen.addFeatures()
	NiTextOut("Simulate Louisiana Wetlands (Jungles - Python Great Plains) ...")
	return 0

def normalizeAddRiver():
	return None

def normalizeRemovePeaks():
	return None

def normalizeRemoveBadFeatures():
	return None

def normalizeRemoveBadTerrain():
	return None

def normalizeAddFoodBonuses():
	return None

def normalizeAddGoodTerrain():
	return None

def normalizeAddExtras():
	return None

# Sirian's "Sahara Regional Bonus Placement" system.

# Init all bonuses. This is your master key.
resourcesInRockies = ("BONUS_ALUMINUM", "BONUS_FUR", "BONUS_GOLD", "BONUS_SILVER", "BONUS_SHEEP", "BONUS_DEER", "BONUS_URANIUM", "BONUS_HORSE", "BONUS_DYE")
resourcesInPlains = ("BONUS_HORSE", "BONUS_GEMSTONES", "BONUS_SPICES")
resourcesInEast = ("BONUS_MAIZE", "BONUS_FUR", "BONUS_DEER", "BONUS_PIG", "BONUS_MARBLE", "BONUS_SUGAR", "BONUS_SPICES", "BONUS_HORSE")
resourcesInSW = ("BONUS_INCENSE", "BONUS_SILVER")
resourcesInNorth = ("BONUS_WHEAT", "BONUS_GRAPES")
resourcesInTexas = ("BONUS_OIL")
resourcesInGulf = ("BONUS_MOLLUSCS", "BONUS_FISH")
buffalo = ("BONUS_CATTLE")
swCorn = ("BONUS_MAIZE")
rockyDyes = ("BONUS_DYE")

resourcesToEliminate = ("BONUS_RICE", "BONUS_WHALE", "BONUS_SILK", "BONUS_BANANA", "BONUS_ELEPHANTS")
resourcesHandledByDefault = ("BONUS_IRON", "BONUS_COPPER", "BONUS_COAL", "BONUS_STONE")

resourcesToForce = ("BONUS_FUR", "BONUS_GEMSTONES", "BONUS_SILVER", "BONUS_DEER", "BONUS_SHEEP", "BONUS_OIL", "BONUS_URANIUM", "BONUS_SPICES", "BONUS_DYE")
forcePlacementOnFlats = ("BONUS_FUR", "BONUS_DEER", "BONUS_SPICES", "BONUS_OIL")
forcePlacementOnHills = ("BONUS_SHEEP", "BONUS_SILVER", "BONUS_GEMSTONES", "BONUS_URANIUM")
forceRarity = ("BONUS_MARBLE", "BONUS_GEMSTONES", "BONUS_DYE", "BONUS_INCENSE", "BONUS_SUGAR")
forceAbundance = ("BONUS_SILVER", "BONUS_FUR", "BONUS_MAIZE", "BONUS_HORSE", "BONUS_WHEAT")
forceNoRarity = ("BONUS_ALUMINUM", "BONUS_GOLD", "BONUS_URANIUM", "BONUS_SHEEP", "BONUS_DEER")

def addBonusType(argsList):
	[iBonusType] = argsList
	gc = CyGlobalContext()
	map = CyMap()
	dice = gc.getGame().getMapRand()
	type_string = gc.getBonusInfo(iBonusType).getType()

	if (type_string in resourcesToEliminate):
		return None # These bonus types will not appear, at all.
	elif (type_string in resourcesHandledByDefault):
		CyPythonMgr().allowDefaultImpl() # Let C handle this bonus in the default way.
	else: # Current bonus type is meant to be regional. Assignments to follow.
		# init definition of regions.
		northlat = 0.6
		texlat = 0.4
		southlat = 0.25
		westlong = 0.2
		eastlong = 0.67
		iW = map.getGridWidth()
		iH = map.getGridHeight()

		# init forced-eligibility flags
		if (type_string not in resourcesToForce):
			unforced = True
		else:
			unforced = False
		forceFlats = False
		forceHills = False
		if (type_string in forcePlacementOnFlats):
			forceFlats = True
		if (type_string in forcePlacementOnHills):
			forceHills = True

		# init bonus in regions. NOTE: a bonus can exist in more than one region!
		inRockies = False
		inPlains = False
		inEast = False
		inNorth = False
		inSW = False
		inTexas = False
		if (type_string in resourcesInRockies):
			inRockies = True
		if (type_string in resourcesInPlains):
			inPlains = True
		if (type_string in resourcesInEast):
			inEast = True
		if (type_string in resourcesInSW):
			inSW = True
		if (type_string in resourcesInNorth):
			inNorth = True
		if (type_string in resourcesInTexas):
			inTexas = True

		# Generate buffalo herds (Cows!) all over the Great Plains!
		# Note: any fractal assignment of bonuses, like this one, must come before determining the count for regional bonuses.
		if (type_string not in buffalo):
			pass
		else:
			NiTextOut("Placing Buffalo Herds (Cows - Python Great Plains) ...")
			herds = CyFractal()
			herds.fracInit(iW, iH, 7, dice, 0, -1, -1)
			iHerdsBottom1 = herds.getHeightFromPercent(24)
			iHerdsTop1 = herds.getHeightFromPercent(27)
			iHerdsBottom2 = herds.getHeightFromPercent(73)
			iHerdsTop2 = herds.getHeightFromPercent(75)
			# More herds in the northern 5/8ths of the map.
			herdNorth = iH - 1
			herdSouth = int(iH * 0.37)
			herdWest = iW / 5
			herdEast = (2 * iW) / 3
			herdSlideRange = (herdEast - herdWest) / 6
			for y in range(herdSouth, herdNorth):
				herdLeft = herdWest + dice.get(herdSlideRange, "Herds, West Boundary - Great Plains PYTHON")
				herdRight = herdEast - dice.get(herdSlideRange, "Herds, East Boundary - Great Plains PYTHON")
				for x in range(herdLeft, herdRight):
					# Fractalized placement of herds
					pPlot = map.plot(x,y)
					if pPlot.isWater() or pPlot.isPeak() or pPlot.getFeatureType() == getInfoTypeOrFail("FEATURE_OASIS"):
						continue # No buffalo at the water hole, sorry!
					herdVal = herds.getHeight(x,y)
					if pPlot.getBonusType(-1) == -1 and ((herdVal >= iHerdsBottom1 and herdVal <= iHerdsTop1) or (herdVal >= iHerdsBottom2 and herdVal <= iHerdsTop2)):
						map.plot(x,y).setBonusType(iBonusType)
			# Fewer herds in the southern 3/8ths of the map.
			herdNorth = int(iH * 0.37)
			herdSouth = int(iH * 0.15)
			herdWest = int(iW * 0.32)
			herdEast = int(iW * 0.59)
			herdSlideRange = (herdEast - herdWest) / 5
			for y in range(herdSouth, herdNorth):
				herdLeft = herdWest + dice.get(herdSlideRange, "Herds, West Boundary - Great Plains PYTHON")
				herdRight = herdEast - dice.get(herdSlideRange, "Herds, East Boundary - Great Plains PYTHON")
				for x in range(herdLeft, herdRight):
					# Fractalized placement of herds
					pPlot = map.plot(x,y)
					if pPlot.isWater() or pPlot.isPeak() or pPlot.getFeatureType() == getInfoTypeOrFail("FEATURE_OASIS"):
						continue # No buffalo at the water hole, sorry!
					herdVal = herds.getHeight(x,y)
					if pPlot.getBonusType(-1) == -1 and ((herdVal >= iHerdsBottom1 and herdVal <= iHerdsTop1) or (herdVal >= iHerdsBottom2 and herdVal <= iHerdsTop2)):
						map.plot(x,y).setBonusType(iBonusType)
			return None
			# Cows are all done now. Mooooooooo!
			# Can you say "Holy Cow"? =)

		# determine number of bonuses to place (defined as count)
		# size modifier is a fixed component based on world size
		sizekey = int(map.getWorldSize())
		sizevalues = sas_default_sizevalues()
		sizemodifier = sas_lookup_world_size(sizekey, sizevalues)
		# playermodifier involves two layers of randomnity.
		players = gc.getGame().countCivPlayersEverAlive()
		plrcomponent1 = int(players / 3.0) # Bonus Method Fixed Component
		plrcomponent2 = dice.get(players, "Bonus Method Abundant Component - Great Plains PYTHON") + 1
		plrcomponent3 = dice.get(int(players / 1.6), "Bonus Method Medium Component - Great Plains PYTHON") - 1
		plrcomponent4 = dice.get((int(players / 4.0) + 1), "Bonus Method Sparse Component - Great Plains PYTHON") - 2
		plrmethods = [plrcomponent1, plrcomponent2, plrcomponent3, plrcomponent4]

		if (type_string in resourcesInTexas):
			count = sizemodifier + plrcomponent1
		elif (type_string in forceNoRarity):
			playermodifier = plrmethods[dice.get(3, "Forced Bonus NoRarity - Great Plains PYTHON")]
			count = sizemodifier + playermodifier
		elif (type_string in forceRarity):
			count = 1 + dice.get(sizemodifier + 1, "Forced Bonus Rarity - Great Plains PYTHON")
		elif (type_string in forceAbundance):
			count = sizemodifier + plrcomponent2
		else:
			playermodifier = plrmethods[dice.get(4, "Bonus Method - Great Plains PYTHON")]
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
				if pPlot.getBonusType(-1) != -1:
					continue # to next plot.
				if pPlot.getFeatureType() == getInfoTypeOrFail("FEATURE_OASIS"):
					continue # Soren wants no bonuses in oasis plots. So mote it be.
				# Check plot type and features for eligibility.
				if (pPlot.canHaveBonus(iBonusType, True) and unforced):
					pass
				elif forceHills and pPlot.isHills():
					pass
				elif forceFlats and pPlot.isFlatlands():
					pass
				elif (type_string in rockyDyes) and pPlot.getTerrainType() == getInfoTypeOrFail("TERRAIN_DESERT") and pPlot.getFeatureType() == getInfoTypeOrFail("FEATURE_FOREST") and pPlot.isHills():
					pass
				else:
					continue # to next plot.
				# re-init regional plot membership tests for each pass.
				plotInRockies = False
				plotInPlains = False
				plotInEast = False
				plotInNorth = False
				plotInSW = False
				plotInTexas = False
				# Check regional membership of current plot.
				# Sahara is using latitude to check, but other
				# tests could be designed and used, including lists.
				long = x/float(iW)
				lat = y/float(iH)
				if long <= westlong and lat >= southlat:
					plotInRockies = True
				if long > westlong and long <= eastlong:
					plotInPlains = True
				if long > eastlong:
					plotInEast = True
				if long > westlong and lat > northlat:
					plotInNorth = True
				if long <= westlong and lat < southlat:
					plotInSW = True
				if (long > westlong and long < eastlong) and lat <= texlat:
					plotInTexas = True
				# Check regional bonus eligibility vs plot membership.
				if (inNorth and plotInNorth):
					pass
				elif (inRockies and plotInRockies):
					pass
				elif (inSW and plotInSW):
					pass
				elif (inPlains and plotInPlains):
					pass
				elif (inEast and plotInEast):
					pass
				elif (inTexas and plotInTexas):
					pass
				else:
					continue # on to next plot. This plot not eligible.
				#
				# Finally we have run all the checks.
				# 1. The plot has no bonus.
				# 2. The plot has an eligible terrain and feature type.
				# 3. The plot is in one or more eligible regions.
				# Now we append this plot to the eligible list.
				eligible.append([x,y])

		# Now we assign the bonuses to eligible plots chosen completely at random.
		while count > 0:
			if eligible == []:
				break # No eligible plots left!
			index = dice.get(len(eligible), "Bonus Placement - Great Plains PYTHON")
			[x,y] = eligible[index]
			map.plot(x,y).setBonusType(iBonusType)
			del eligible[index] # Remove this plot from the eligible list.
			count = count - 1  # Reduce number of bonuses left to place.
		# This bonus type is done.

		# Small amount of Corn forced in the SW.
		# Can appear in the desert. (Hopi! Stalks must be spaced wider, though.)
		if (type_string in swCorn):
			top = int(iH * southlat)
			right = int(iW * westlong)
			corn = []
			corncount = sizemodifier
			for x in range(right):
				for y in range(top):
					pPlot = map.plot(x,y)
					if pPlot.getBonusType(-1) == -1 and pPlot.isFlatlands() and not pPlot.getFeatureType() == getInfoTypeOrFail("FEATURE_OASIS"):
						corn.append([x,y])
			while corncount > 0:
				if corn == []:
					break # No eligible plots left!
				index = dice.get(len(corn), "SW Corn Placement - Great Plains PYTHON")
				[x,y] = corn[index]
				map.plot(x,y).setBonusType(iBonusType)
				del corn[index] # Remove this plot from the eligible list.
				corncount = corncount - 1  # Reduce number of bonuses left to place.

		# Seafood forced in the Gulf.
		if (type_string in resourcesInGulf):
			top = int(iH * southlat)
			left = iW / 2
			right = iW
			seafood = []
			seafoodcount = sizemodifier
			for x in range(left, right):
				for y in range(top):
					pPlot = map.plot(x,y)
					if pPlot.getBonusType(-1) == -1 and pPlot.canHaveBonus(iBonusType, True):
						seafood.append([x,y])
			while seafoodcount > 0:
				if seafood == []:
					break # No eligible plots left!
				index = dice.get(len(seafood), "Seafood Placement - Great Plains PYTHON")
				[x,y] = seafood[index]
				map.plot(x,y).setBonusType(iBonusType)
				del seafood[index] # Remove this plot from the eligible list.
				seafoodcount = seafoodcount - 1  # Reduce number of bonuses left to place.

		# Oil gets default placement on top of forced appearance in Texas
		if (type_string in resourcesInTexas):
			CyPythonMgr().allowDefaultImpl()
		# Finito
		else:
			return None # The default handler is not to place any more of this type.

# "Nile Style" Custom River Placement System designed by Sirian.
def addNileStyleRiverFlowingSouth(center, left, right, maxshift, startX, startY, iDirectionOdds):
	# Nile-Style river flowing south, will stop at first water plot.
	#
	# WARNING: This routine does not protect against running off the 
	# left/right edge of the map!
	# Use center and maxshift to ensure that the river range is legal.
	#
	# WARNING: This routine does not detect or handle the presence of 
	# other rivers. You either need to keep its lane clear of other 
	# rivers or add new code (or find a version with the code added) 
	# to handle intersections with other rivers.
	#
	# Please keep these warnings in place if you borrow this function.
	#
	# This instance heavily customized to create the Mississippi River.
	global mississippi_x_coords
	mississippi_x_coords = []
	gc = CyGlobalContext()
	map = CyMap()
	dice = gc.getGame().getMapRand()
	iW = map.getGridWidth()
	iH = map.getGridHeight()
	# Init river and place first segment.
	iX = startX
	iY = startY
	pRiverPlot = map.plot(iX, iY)
	#
	# IMPORTANT NOTE: RiverID -MUST- be set for each plot of the river, 
	# if you want to combine manually set rivers with automatically generated ones!
	#
	iThisRiverID = gc.getMap().getNextRiverID()
	gc.getMap().incrementNextRiverID()
	pRiverPlot.setRiverID(iThisRiverID)
	pRiverPlot.setWOfRiver(true, CardinalDirectionTypes.CARDINALDIRECTION_SOUTH)
	mississippi_x_coords.append(iX)
	# Loop through addition of more segments until the river terminates.
	while iY > 0:
		southPlotWest = map.plot(iX, iY-1)
		southPlotEast = map.plot(iX+1, iY-1)
		# Checking both plots for water.
		if southPlotEast.isWater() or southPlotWest.isWater():
			break
		pRiverPlot = map.plot(iX, iY)
		direction = dice.get(iDirectionOdds, "River Direction - PYTHON")
		segmentLength = 1 + dice.get(maxshift, "River Direction - PYTHON")
		# WEST
		if direction < 2: # Turn to the West, then South again.
			# Turn west.
			for segment in range(segmentLength):
				if iX <= left:
					break
				else:
					pRiverPlot.setRiverID(iThisRiverID)
					pRiverPlot.setNOfRiver(true, CardinalDirectionTypes.CARDINALDIRECTION_WEST)
					plotWest = map.plot(iX-1, iY-1)
					# Only checking South-West Plot for Sahara.
					if plotWest.isWater():
						break
					iX -= 1
					pRiverPlot = map.plot(iX, iY)
			# Now turn back toward the south.
			iY -= 1
			pRiverPlot = map.plot(iX, iY)
			pRiverPlot.setRiverID(iThisRiverID)
			pRiverPlot.setWOfRiver(true, CardinalDirectionTypes.CARDINALDIRECTION_SOUTH)
			mississippi_x_coords.append(iX)
		# EAST
		elif direction > 8: # Turn to the East, then South again.
			if iX >= right:
				continue
			# Turn east.
			for segment in range(segmentLength):
				if iX >= right:
					break
				else: 
					iX += 1
					pRiverPlot = map.plot(iX, iY)
					pRiverPlot.setRiverID(iThisRiverID)
					pRiverPlot.setNOfRiver(true, CardinalDirectionTypes.CARDINALDIRECTION_EAST)
					plotEast = map.plot(iX+1, iY)
					# Only checking South-East Plot for Sahara.
					if plotEast.isWater():
						break
			# Now turn back toward the south.
			plotEast = map.plot(iX+1, iY-1)
			if plotEast.isWater():
				break
			iY -= 1
			pRiverPlot = map.plot(iX, iY)
			pRiverPlot.setRiverID(iThisRiverID)
			pRiverPlot.setWOfRiver(true, CardinalDirectionTypes.CARDINALDIRECTION_SOUTH)
			mississippi_x_coords.append(iX)
		# SOUTH
		else: # Run straight South for a segment. Note: 60% chance of South.
			for segment in range(segmentLength):
				if iY == 0:
					break
				southPlotWest = map.plot(iX, iY-1)
				southPlotEast = map.plot(iX+1, iY-1)
				if southPlotEast.isWater() or southPlotWest.isWater():
					break
				iY -= 1
				pRiverPlot = map.plot(iX, iY)
				pRiverPlot.setRiverID(iThisRiverID)
				pRiverPlot.setWOfRiver(true, CardinalDirectionTypes.CARDINALDIRECTION_SOUTH)
				mississippi_x_coords.append(iX)
	# River is finished.
	return (iX, iY)

def addRivers():
	# Create the mighty Mississippi River, flowing in to the Gulf of Mexico.
	global mississippi_x_coords
	gc = CyGlobalContext()
	map = CyMap()
	dice = gc.getGame().getMapRand()
	iW = map.getGridWidth()
	iH = map.getGridHeight()

	# Set maxshift according to grid size.
	sizekey = map.getWorldSize()
	# <!-- custom: Added ARENA and SAS sizes (24, 32, 40, 48 players). (GPT-5.2-Codex; GPT-5.5; ChatGPT-5.5) -->
	shiftvalues = {
		WorldSizeTypes.WORLDSIZE_ARENA: 1,
		WorldSizeTypes.WORLDSIZE_DUEL: 1,
		WorldSizeTypes.WORLDSIZE_TINY: 2,
		WorldSizeTypes.WORLDSIZE_SMALL: 2,
		WorldSizeTypes.WORLDSIZE_STANDARD: 3,
		WorldSizeTypes.WORLDSIZE_LARGE: 3,
		WorldSizeTypes.WORLDSIZE_HUGE: 4,
		WorldSizeTypes.WORLDSIZE_SAS24: 5,
		WorldSizeTypes.WORLDSIZE_SAS32: 6,
		WorldSizeTypes.WORLDSIZE_SAS40: 7,
		WorldSizeTypes.WORLDSIZE_SAS48: 8,
		}
	maxshift = sas_lookup_world_size(sizekey, shiftvalues)

	# Place the Mississippi!
	NiTextOut("Charting the Mississippi (Python Great Plains) ...")
	nileQuadrant = dice.get(4, "Nile Placement - Sahara PYTHON")
	center = int((iW - 1) * 0.84)
	left = int((iW - 1) * 0.74)
	right = int((iW - 1) * 0.9)
	startX = int((iW - 1) * 0.78)
	startY = iH - 1
	#
	# Add the Mississippi! Record its X Coords in a list to be saved for later.
	(endX, endY) = addNileStyleRiverFlowingSouth(center, left, right, maxshift, startX, startY, 12)
	for loop in range(endY):
		mississippi_x_coords.append(endX)
	mississippi_x_coords.reverse() # Plots were added in reverse order.
	CyPythonMgr().allowDefaultImpl()

def getRiverStartCardinalDirection(argsList):
	pPlot = argsList[0]
	global mississippi_x_coords
	x, y = pPlot.getX(), pPlot.getY()

	# Check river start plot X coord vs the Mississippi X coord at the same Y coord for both.
	if x > mississippi_x_coords[y]: # River start plot is east of Mississippi.
		return CardinalDirectionTypes.CARDINALDIRECTION_WEST
	else: # West of Missississpi. All rivers lead to Rome! =)
		return CardinalDirectionTypes.CARDINALDIRECTION_EAST

def getRiverAltitude(argsList):
	pPlot = argsList[0]
	global mississippi_x_coords
	iW = CyMap().getGridWidth()
	iH = CyMap().getGridHeight()
	x, y = pPlot.getX(), pPlot.getY()

	CyPythonMgr().allowDefaultImpl()

	# Check current river plot X coord vs the Mississippi X coord at the same Y coord for both.
	if x > mississippi_x_coords[y]: # River start plot is east of Mississippi River path.
		if y < 0.45 * iH:
			return 0
		return int(y * 0.5)
	if y > 0.6 * iH:
		return int((iW - x) * 0.38)
	if y > 0.3 * iH:
		return int((iW - x) * 0.38) - (y * 0.1)
	return int(((iW - x) * 0.38) + (y * 0.1))

