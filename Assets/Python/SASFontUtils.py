# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md).

from CvPythonExtensions import *

gc = CyGlobalContext()
_SAS_UI_FONT_SIZE_CACHE = {}

def _getSASUIFontSize(szDefineName):
	if szDefineName in _SAS_UI_FONT_SIZE_CACHE:
		return _SAS_UI_FONT_SIZE_CACHE[szDefineName]
	iSize = gc.getDefineINT(szDefineName)
	if iSize < 1 or iSize > 4:
		_SAS_UI_FONT_SIZE_CACHE[szDefineName] = None
		raise RuntimeError("SASFontUtils: %s=%d out of [1,4] - check GlobalDefines_advciv_sas.xml" % (szDefineName, iSize))
	_SAS_UI_FONT_SIZE_CACHE[szDefineName] = iSize
	print("SASFontUtils: initialized %s = %d" % (szDefineName, iSize))
	return iSize

def getSASUIFontTiny():
	return _getSASUIFontSize("SAS_UI_FONT_TINY")

def getSASUIFontBody():
	return _getSASUIFontSize("SAS_UI_FONT_BODY")

def getSASUIFontLabel():
	return _getSASUIFontSize("SAS_UI_FONT_LABEL")

def getSASUIFontTitle():
	return _getSASUIFontSize("SAS_UI_FONT_TITLE")

def getSASUIFontHover():
	return _getSASUIFontSize("SAS_UI_FONT_HOVER")

class _DynamicFontTag:
	def __init__(self, szDefineName, bBold=False):
		self.szDefineName = szDefineName
		self._isBold = bBold
		self._cachedTag = None
		if not bBold:
			self.bold = _DynamicFontTag(szDefineName, True)

	def _build(self):
		if self._cachedTag is not None:
			return self._cachedTag
		iSize = _getSASUIFontSize(self.szDefineName)
		if iSize is None:
			self._cachedTag = u""
		elif self._isBold:
			self._cachedTag = u"<font=%db>" % iSize
		else:
			self._cachedTag = u"<font=%d>" % iSize
		return self._cachedTag

	def __unicode__(self):
		return self._build()

	def __str__(self):
		return self._build().encode("ascii")

	def __add__(self, szOther):
		return self._build() + szOther

	def __radd__(self, szOther):
		return szOther + self._build()

SAS_FONT_TAG_CLOSE = u"</font>"

sasFontTagTiny = _DynamicFontTag("SAS_UI_FONT_TINY")
sasFontTagBody = _DynamicFontTag("SAS_UI_FONT_BODY")
sasFontTagLabel = _DynamicFontTag("SAS_UI_FONT_LABEL")
sasFontTagTitle = _DynamicFontTag("SAS_UI_FONT_TITLE")
sasFontTagHover = _DynamicFontTag("SAS_UI_FONT_HOVER")
