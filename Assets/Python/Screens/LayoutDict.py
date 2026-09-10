# advc.092: New module. (Rectangular) layout data for widgets on the main screen,
# i.e. mostly for use by CvMainInterface and PLE; however, other modules may also
# want to align widgets with the main screen occasionally.
#
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
# <!-- custom: changes here are minor (e.g. collapsed multiline statements to single-line for grep/readability) -->

from RectLayout import *
from math import pow

def _isPrintLayoutKey(szKey):
	return (
		False and
		# Toggle to True to enable output. For long sequences of widgets, print only the first one or first few ...
		# Lower left unit button when numPlotListButtonsPerRow is 35 and numPlotListRows is 10. (9*35=315)
		(not szKey.startswith("PlotListButton") or szKey.startswith("PlotListButton315")) and
		(not szKey.startswith("AngryCitizen") or szKey.startswith("AngryCitizen0") or szKey.startswith("AngryCitizenChevron0")) and
		(not szKey.startswith("IncreaseSpecialist") or szKey.startswith("IncreaseSpecialist0")) and
		(not szKey.startswith("DecreaseSpecialist") or szKey.startswith("DecreaseSpecialist0")) and
		(not szKey.startswith("SpecialistDisabledButton") or szKey.startswith("SpecialistDisabledButton0")) and
		(not szKey.startswith("CitizenButton") or szKey.startswith("CitizenButton0")) and
		(not szKey.startswith("CitizenChevron") or szKey.startswith("CitizenChevron0")) and
		(not szKey.startswith("Stacker_FreeSpecialist") or szKey.startswith("Stacker_FreeSpecialist0")) and
		(not szKey.startswith("Stacker_AngryCitizen") or szKey.startswith("Stacker_AngryCitizen0")) and
		(not szKey.startswith("IncrCitizenBanner") or szKey.startswith("IncrCitizenBanner0")) and
		(not szKey.startswith("IncrCitizenButton") or szKey.startswith("IncrCitizenButton0")) and
		(not szKey.startswith("PromotionButton") or szKey.endswith("Button0")) and
		(not szKey.startswith("PLE_PROMO_BUTTONS_UNITINFO") or szKey.startswith("PLE_PROMO_BUTTONS_UNITINFO0")) and
		(not szKey.startswith("DecrCitizenButton") or szKey.startswith("DecrCitizenButton0")) and
		(not szKey.startswith("CityBonus") or szKey.endswith("_0")))

def _iround(f):
	return int(round(f))

gRectLayoutDict = {}
def gSetRectangle(szKeyName, lRect):
	gRectLayoutDict[szKeyName] = lRect
	if _isPrintLayoutKey(szKeyName):
		print(szKeyName + " " + str(lRect))
def gSetRect(szKeyName, szParentKey, fX, fY, fWidth, fHeight, bOffScreen = False):
	gSetRectangle(szKeyName, RectLayout(gRect(szParentKey), fX, fY, fWidth, fHeight, bOffScreen))
def gSetSquare(szKeyName, szParentKey, fX, fY, fSideLen, bOffScreen = False):
	gSetRectangle(szKeyName, SquareLayout(gRect(szParentKey), fX, fY, fSideLen, bOffScreen))
def gRect(szKeyName):
	return gRectLayoutDict[szKeyName]
def gIsRect(szKeyName):
	return szKeyName in gRectLayoutDict
# Same deal for 2D points
gPointLayoutDict = {}
def gSetPoint(szKeyName, lPoint):
	gPointLayoutDict[szKeyName] = lPoint
	if _isPrintLayoutKey(szKeyName):
		print(szKeyName + " " + str(lPoint))
def gPoint(szKeyName):
	return gPointLayoutDict[szKeyName]
# rect can be a RectLayout or the string key of a global RectLayout
def gOffSetPoint(szPointKeyName, rect, fDeltaX, fDeltaY):
	if isinstance(rect, basestring):
		lRect = gRect(rect)
	else:
		lRect = rect
	gSetPoint(szPointKeyName, RectLayout.offsetPoint(lRect, fDeltaX, fDeltaY))
gHorizontalScaleFactor = 1.0
gVerticalScaleFactor = 1.0
gSquareButtonScaleFactor = 1.0
gSpaceScaleFactor = 1.0
# Sets the global variables above. All positional data that goes through the functions below will then be scaled accordingly.
# fSpace: This one should be less than 1 b/c space in between widgets should (if at all) be only slighty affected by the screen resolution.
def gSetScaleFactors(fHorizontal, fVertical, fSquare, fSpace):
	global gHorizontalScaleFactor, gVerticalScaleFactor, gSquareButtonScaleFactor, gSpaceScaleFactor
	gHorizontalScaleFactor = fHorizontal
	gVerticalScaleFactor = fVertical
	gSquareButtonScaleFactor = fSquare
	gSpaceScaleFactor = fSpace

# Magnify small distances disproportionately
def _dispropMagnMult(fMult, iDist, fExp):
	if fExp <= 0 and iDist > 0:
		fExp = (iDist + 1.0) / iDist
	return pow(fMult, fExp)
def _scaleWidgetLenToRes(fScaleMult, iLen, fExp):
	if iLen <= 0:
		# Let's allow 0 for tiny lengths that should be 0 when the resolution is small.
		assert iLen == 0
		return _iround(fScaleMult - 1)
	return _iround(iLen * _dispropMagnMult(fScaleMult, iLen, fExp))
def _scaleSpaceToRes(fScaleMult, iSpacing, fExp):
	fScaleMult *= gSpaceScaleFactor
	if iSpacing == 0:
		return max(0, _iround(fScaleMult - 1))
	elif iSpacing < 0:
		# Negative space is overlap. Overlap should get smaller on higher resolutions.
		fScaleMult = 1 / fScaleMult
	return _iround(iSpacing * _dispropMagnMult(fScaleMult, iSpacing, fExp))
# Short names b/c there will be lots of call sites.
# The optional fExp params allow a caller to exponentiate the resolution-based multipliers. Use fExp > 1 to increase the impact of screen resolution on a distance, use 0 < fExp < 1 to decrease the impact.
def HLEN(iWidth, fExp = 0):
	return _scaleWidgetLenToRes(gHorizontalScaleFactor, iWidth, fExp)
def VLEN(iHeight, fExp = 0):
	return _scaleWidgetLenToRes(gVerticalScaleFactor, iHeight, fExp)
def BTNSZ(iSize, fExp = 0):
	return _scaleWidgetLenToRes(gSquareButtonScaleFactor, iSize, fExp)
# Negative arguments are assumed to be overlap, i.e. those will move closer to 0 on higher resolution. Callers should pass negative values only when there is overlap!
def HSPACE(iWidth, fExp = 0):
	return _scaleSpaceToRes(gHorizontalScaleFactor, iWidth, fExp)
def VSPACE(iHeight, fExp = 0):
	return _scaleSpaceToRes(gVerticalScaleFactor, iHeight, fExp)
