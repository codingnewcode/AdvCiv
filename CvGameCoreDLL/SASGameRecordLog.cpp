#include "CvGameCoreDLL.h"
#include "SASGameRecordLog.h"
#include "CvGame.h" // <!-- custom: Needed for game-record turn, game-state, victory, RNG, and map-classification context rows. (GPT-5.5) -->
#include "CvPlayer.h" // <!-- custom: Needed directly for active-player civilization/handicap context in this smaller AdvCiv 1.14 port slice; do not rely on later SASGameRecord headers to complete CvPlayer transitively. (ChatGPT-5.6-Sol) -->
#include "CvPlayerAI.h" // <!-- custom: Needed for attitude/glance values in game-record advisor rows. (ChatGPT-5.5) -->
#include "CvTeamAI.h" // <!-- custom: Needed for team-level worst-enemy state in game-record diplomacy-status rows. (ChatGPT-5.5) -->
#include "CvCity.h" // <!-- custom: Needed to count player-city religions and corporations in periodic policy snapshots. (ChatGPT-5.6-Sol) -->
#include "CvCityAI.h" // <!-- custom: Needed only to read the existing Avoid Growth AI emphasis flag in city snapshots/aggregates; CvCity.h only forward-declares CvCityAI. This is a compile-time type dependency and does not alter AI state or gameplay. (ChatGPT-5.6-Sol) -->
#include "CityPlotIterator.h" // <!-- custom: Needed by compact game-record worked-plot composition rows. (ChatGPT-5.5) -->
#include "CvPlot.h" // <!-- custom: Needed to classify Spy deployment and stationary mission preparation in periodic espionage snapshots. (ChatGPT-5.6-Sol) -->
#include "CvPlotGroup.h" // <!-- custom: Needed to identify connected city networks in game-record city rows. (ChatGPT-5.5) -->
#include "CvArea.h" // <!-- custom: Needed for area-wide city happiness/health detail rows. (ChatGPT-5.5) -->
#include "CvTeam.h" // <!-- custom: Needed directly for finalized initial-team state and technology grouping in this smaller AdvCiv 1.14 port slice; GET_TEAM is defined by CvTeam.h. (ChatGPT-5.6-Sol) -->
#include "CvUnit.h" // <!-- custom: Needed for the mature SASGameRecord distinction between actual combat-capable units and Civ4's separate bMilitarySupport counter in periodic player snapshots. (ChatGPT-5.6-Sol) -->
#include "CvSelectionGroup.h" // <!-- custom: Needed to inspect worker/settler mission queues in game-record rows. (ChatGPT-5.5) -->
#include "CvInfo_Organization.h" // <!-- custom: Needed for religion/corporation type names in game-record action rows. (GPT-5.5) -->
#include "CvInfo_Civics.h" // <!-- custom: Needed for policy/civic names in game-record advisor rows. (ChatGPT-5.5) -->
#include "CvInfo_Civilization.h" // <!-- custom: Needed to attribute player-wide extra happiness/health to traits instead of leaving effects from loaded-mod rules under an opaque `extra` label. (GPT-5.6-Sol) -->
#include "CvInfo_Tech.h" // <!-- custom: Needed for stable technology type names and XML trade-capability source mapping. (ChatGPT-5.6-Sol) -->
#include "CvInfo_Terrain.h" // <!-- custom: Needed for terrain/feature/bonus type names in game-record context rows. (ChatGPT-5.5) -->
#include "CvInfo_Build.h" // <!-- custom: Needed for worker build-type names and build target classification in game-record rows. (ChatGPT-5.5) -->
#include "CvInfo_Command.h" // <!-- custom: Needed for mission-type names in worker/settler game-record rows. (ChatGPT-5.5) -->
#include "CvInfo_Building.h" // <!-- custom: Needed to classify city production and wonders in game-record city aggregate rows. (ChatGPT-5.5) -->
#include "CvInfo_City.h" // <!-- custom: Needed for specialist and process type names in game-record city rows. (ChatGPT-5.5) -->
#include "CvInfo_Unit.h" // <!-- custom: Needed to classify unit composition and city production in game-record rows. (ChatGPT-5.5) -->
#include "CvInfo_Misc.h" // <!-- custom: Needed directly for era type names in periodic team technology summaries; base AdvCiv only forward-declares CvEraInfo through CvGlobals. (ChatGPT-5.6-Sol) -->
#include "CvInfo_Symbol.h" // <!-- custom: Needed to log actual assigned player-color and primary-color context; CvGlobals only forward-declares their info classes. (GPT-5.6-Sol) -->
#include "CvGameCoreUtils.h" // <!-- custom: Needed for shared machine-readable diagnostic quoting/list helpers used by SASGameRecord. (ChatGPT-5.6-Sol) -->
#include "CvInfo_GameOption.h" // <!-- custom: Needed to log enabled game-option type names; CvGlobals only forward-declares CvGameOptionInfo. (GPT-5.5) -->
#include "CvMap.h" // <!-- custom: Needed to log map dimensions; CvGlobals only forward-declares CvMap. (GPT-5.5) -->
#include <algorithm>
#include <utility> // <!-- custom: Needed for Great Person odds pairs in game-record city rows. (ChatGPT-5.5) -->
#include <vector> // <!-- custom: Used for grouped finalized initial-team technology payloads. (ChatGPT-5.6-Sol) -->
#include <time.h>

static int getClampedSASGameRecordLogLevel(char const* szDefineName)
{
	const int iLevel = GC.getDefineINT(szDefineName);
	if (iLevel < 0)
		return 0;
	if (iLevel > 3)
		return 3;
	return iLevel;
}

// <!-- custom: Dedicated structured game-record log for autoplay comparison, general game analysis, and external LLM review.
// This is independent from SAS_BBAI_LOG_ENABLE because it is a run-report artifact rather than classic AI-decision diagnostics, and writes to SASGameRecord_*.log when enabled.
// Use ACTION rows rather than EVENT rows to avoid confusion with Civ4 random events.
// Keep the recorder portable across Civ4 mods by enumerating loaded XML and using generic field meanings instead of hardcoding AdvCiv-SAS types or copying the full XML.
// Mod-specific rules can still be named in comments as concrete examples: TECH_DEPOPULATION currently applies negative player-wide health and happiness in AdvCiv-SAS, but the recorder attributes health/happiness from every loaded trait, civic and technology dynamically.
// The record describes the current format; do not add schema-version maintenance unless independently evolving consumers later require it. (ChatGPT-5.5 + GPT-5.5 + GPT-5.6-Sol) -->
int getSASGameRecordLogLevel()
{
	static const int iLevel = getClampedSASGameRecordLogLevel("SAS_GAME_RECORD_LOG_LEVEL");
	return iLevel;
}

bool isSASGameRecordLogEnabled()
{
	static const bool bEnabled = (getSASGameRecordLogLevel() > 0);
	return bEnabled;
}

int getSASGameRecordTurnInterval()
{
	// <!-- custom: Separate snapshot frequency from detail level. Level 0 disables the game-record rows; the interval is still clamped so modulo callers are safe. (ChatGPT-5.5) -->
	static const int iInterval = std::max(1, GC.getDefineINT("SAS_GAME_RECORD_INTERVAL_TURNS_UNSCALED_GAMESPEED"));
	return iInterval;
}

static CvString g_szSASGameRecordLogTimestamp;
static int g_iSASGameRecordLogSequence = 0;
static CvString g_szSASGameRecordLogContext;

// <!-- custom: Keep only the team fields already consumed by this first periodic snapshot slice. Later player/global snapshot ports can extend their own recorder-local baselines independently. (ChatGPT-5.6-Sol) -->
struct SASGameRecordTeamPrevious
{
	bool bValid;
	bool bContactsValid;
	int iTechs;
	int iLand;
	int iLandPctX100;
	int iPopulation;
	int iPopPctX100;
	int iMetTeams;
};

static SASGameRecordTeamPrevious g_akSASGameRecordTeamPrevious[MAX_TEAMS];

// <!-- custom: Keep the portable high-level player fields first. More specialized bonus, espionage, unit-posture, worker, territory and city baselines are added with the corresponding snapshot rows rather than existing as unused state. (ChatGPT-5.6-Sol) -->
struct SASGameRecordPlayerPrevious
{
	bool bValid;
	int iScore;
	int iCities;
	int iPopulation;
	int iLand;
	int iUnits;
	int iCombatUnits;
	int iMilitarySupportUnits;
	int iPower;
	int iGold;
	int iGoldRate;
	int iResearchRate;
	int iBonusTypes;
	int iBonusInstances;
	int iBonusImports;
	int iBonusExports;
	int iHistoryScore;
	int iHistoryEconomy;
	int iHistoryIndustry;
	int iHistoryAgriculture;
	int iHistoryPower;
	int iHistoryCulture;
	int iHistoryEspionage;
	int iEspionageRate;
	int iEspionagePercent;
	int iTeamEP;
	int iUnspentEP;
	int iDemoScore;
	int iDemoPopulation;
	int iDemoLand;
	int iDemoFood;
	int iDemoProduction;
	int iDemoCommerce;
	int iDemoResearch;
	int iDemoCulture;
	int iDemoEspionage;
	int iDemoGoldRate;
	int iDemoPower;
	int iUnitTotal;
	int iUnitMilitary;
	int iUnitWorkers;
	int iUnitSettlers;
	int iUnitFieldArmy;
	int iUnitCityDefenders;
	int iUnitEnemyUnitsInTerritory;
	int iUnitTotalExperience;
	int iUnitPromotionReady;
	int iWorkerWorkers;
	int iWorkerBuilding;
	int iWorkerIdle;
	int iWorkerMoving;
	int iWorkerWaiting;
	int iWorkerThreatened;
	int iSettlerSettlers;
	int iSettlerFoundMission;
	int iSettlerMoving;
	int iSettlerIdle;
	int iSettlerWaiting;
	int iSettlerThreatened;
	int iCityCount;
	int iCityConnectedToCapital;
	int iCityFoodSurplus;
	int iCityHappySurplus;
	int iCityHealthSurplus;
	int iCityFood;
	int iCityProduction;
	int iCityCommerce;
	int iCityTradeRoutes;
	int iCityTradeCommerce;
	int iCitySpecialists;
	int iCityFreeSpecialists;
	int iCityGarrison;
};

static SASGameRecordPlayerPrevious g_akSASGameRecordPlayerPrevious[MAX_PLAYERS];

static int getSASGameRecordDelta(bool bValid, int iCurrent, int iPrevious)
{
	return bValid ? iCurrent - iPrevious : 0;
}

static void resetSASGameRecordTeamPrevious()
{
	for (int iI = 0; iI < MAX_TEAMS; iI++)
	{
		g_akSASGameRecordTeamPrevious[iI].bValid = false;
		g_akSASGameRecordTeamPrevious[iI].bContactsValid = false;
	}
}

static void resetSASGameRecordPlayerPrevious()
{
	for (int iI = 0; iI < MAX_PLAYERS; iI++)
		g_akSASGameRecordPlayerPrevious[iI].bValid = false;
}

static CvString createSASGameRecordUtcTimestamp()
{
	time_t kNow;
	time(&kNow);
	char szBuffer[32];
	struct tm* pUtcTime = gmtime(&kNow);
	if (pUtcTime != NULL && strftime(szBuffer, sizeof(szBuffer), "%Y%m%dT%H%M%SZ", pUtcTime) > 0)
		return CvString(szBuffer);
	return CvString("unknown_time");
}

static CvString getSASGameRecordLogTimestamp()
{
	if (g_szSASGameRecordLogTimestamp.empty())
		g_szSASGameRecordLogTimestamp = createSASGameRecordUtcTimestamp();
	return g_szSASGameRecordLogTimestamp;
}

static bool isSASGameRecordTimestampedFilenameEnabled()
{
	static const bool bUseTimestampedFilename = (GC.getDefineINT("SAS_GAME_RECORD_LOG_USE_TIMESTAMPED_FILENAME") > 0);
	return bUseTimestampedFilename;
}

static CvString getSASGameRecordLogName()
{
	CvString szLogName;
	if (GC.getGame().isNetworkMultiPlayer())
	{
		if (isSASGameRecordTimestampedFilenameEnabled())
		{
			if (!g_szSASGameRecordLogContext.empty())
				szLogName.Format("SASGameRecord%d_%s_%s.log", (int)GC.getGame().getActivePlayer(), getSASGameRecordLogTimestamp().GetCString(), g_szSASGameRecordLogContext.GetCString());
			else szLogName.Format("SASGameRecord%d_%s.log", (int)GC.getGame().getActivePlayer(), getSASGameRecordLogTimestamp().GetCString());
		}
		else szLogName.Format("SASGameRecord%d.log", (int)GC.getGame().getActivePlayer());
	}
	else
	{
		if (isSASGameRecordTimestampedFilenameEnabled())
		{
			if (!g_szSASGameRecordLogContext.empty())
				szLogName.Format("SASGameRecord_%s_%s.log", getSASGameRecordLogTimestamp().GetCString(), g_szSASGameRecordLogContext.GetCString());
			else szLogName.Format("SASGameRecord_%s.log", getSASGameRecordLogTimestamp().GetCString());
		}
		else szLogName = "SASGameRecord.log";
	}
	return szLogName;
}

static void rollSASGameRecordLog(const char* szContext)
{
	g_szSASGameRecordLogTimestamp = createSASGameRecordUtcTimestamp();
	g_szSASGameRecordLogContext.clear();
	if (isSASGameRecordTimestampedFilenameEnabled())
	{
		g_iSASGameRecordLogSequence++;
		g_szSASGameRecordLogContext.Format("%s%d", szContext, g_iSASGameRecordLogSequence);
	}
}

static void appendSASGameRecordType(CvString& szTypes, char const* szType)
{
	if (!szTypes.empty()) szTypes += ",";
	szTypes += szType;
}

static void logSASGameRecordLogSettings()
{
	logSASGameRecord("GAME_RECORD_LOG_SETTINGS SAS_GAME_RECORD_LOG_LEVEL=%d SAS_GAME_RECORD_INTERVAL_TURNS_UNSCALED_GAMESPEED=%d SAS_GAME_RECORD_LOG_USE_TIMESTAMPED_FILENAME=%d",
			getSASGameRecordLogLevel(), getSASGameRecordTurnInterval(), isSASGameRecordTimestampedFilenameEnabled());
}

// <!-- custom: Compact finalized team rows preserve which technologies each team owns and which diplomacy capabilities are active, but replacing setup-time TECH_ACQUIRED spam otherwise loses which technology grants each capability.
// Record the loaded XML mapping once for the whole session instead of repeating the same effect fields for every initial team-tech pair. (GPT-5.6-Sol) -->
static void logSASGameRecordTechCapabilitySources()
{
	CvString szMapTrading, szTechTrading, szGoldTrading, szOpenBordersTrading, szDefensivePactTrading, szPermanentAllianceTrading, szVassalStateTrading;
	FOR_EACH_ENUM(Tech)
	{
		CvTechInfo const& kTech = GC.getInfo(eLoopTech);
		if (kTech.isMapTrading()) appendSASGameRecordType(szMapTrading, kTech.getType());
		if (kTech.isTechTrading()) appendSASGameRecordType(szTechTrading, kTech.getType());
		if (kTech.isGoldTrading()) appendSASGameRecordType(szGoldTrading, kTech.getType());
		if (kTech.isOpenBordersTrading()) appendSASGameRecordType(szOpenBordersTrading, kTech.getType());
		if (kTech.isDefensivePactTrading()) appendSASGameRecordType(szDefensivePactTrading, kTech.getType());
		if (kTech.isPermanentAllianceTrading()) appendSASGameRecordType(szPermanentAllianceTrading, kTech.getType());
		if (kTech.isVassalStateTrading()) appendSASGameRecordType(szVassalStateTrading, kTech.getType());
	}
	logSASGameRecord("GAME_RECORD_TECH_CAPABILITY_SOURCES mapTrading=%s techTrading=%s goldTrading=%s openBordersTrading=%s defensivePactTrading=%s permanentAllianceTrading=%s vassalStateTrading=%s source=LOADED_XML",
			getSASDiagnosticOrDash(szMapTrading).GetCString(), getSASDiagnosticOrDash(szTechTrading).GetCString(), getSASDiagnosticOrDash(szGoldTrading).GetCString(), getSASDiagnosticOrDash(szOpenBordersTrading).GetCString(), getSASDiagnosticOrDash(szDefensivePactTrading).GetCString(), getSASDiagnosticOrDash(szPermanentAllianceTrading).GetCString(), getSASDiagnosticOrDash(szVassalStateTrading).GetCString());
}

// <!-- custom: Record every stored map-script option, including hidden values. Keep numeric values durable so setup can be reconstructed without relying on localized descriptions or a currently available Python map script. (ChatGPT-5.6-Sol) -->
static void logSASGameRecordMapOptions(CvInitCore const& kInitCore)
{
	const int iNumOptions = kInitCore.getNumCustomMapOptions();
	const int iNumHiddenOptions = std::min(iNumOptions, std::max(0, kInitCore.getNumHiddenCustomMapOptions()));
	logSASGameRecord("GAME_RECORD_MAP_OPTIONS count=%d hidden=%d", iNumOptions, iNumHiddenOptions);
	for (int iOption = 0; iOption < iNumOptions; iOption++)
	{
		const bool bHidden = (iOption >= iNumOptions - iNumHiddenOptions);
		logSASGameRecord("GAME_RECORD_MAP_OPTION index=%d hidden=%d value=%d", iOption, bHidden, kInitCore.getCustomMapOption(iOption));
	}
}

static const char* getSASGameRecordReligionType(ReligionTypes eReligion)
{
	return (eReligion == NO_RELIGION ? "-" : GC.getInfo(eReligion).getType());
}

static const char* getSASGameRecordCorporationType(CorporationTypes eCorporation)
{
	return (eCorporation == NO_CORPORATION ? "-" : GC.getInfo(eCorporation).getType());
}

static const char* getSASGameRecordCivicType(CivicTypes eCivic)
{
	return (eCivic == NO_CIVIC ? "-" : GC.getInfo(eCivic).getType());
}

static const char* getSASGameRecordEraType(EraTypes eEra)
{
	return (eEra == NO_ERA ? "-" : GC.getInfo(eEra).getType());
}

static const char* getSASGameRecordTechType(TechTypes eTech)
{
	return (eTech == NO_TECH ? "-" : GC.getInfo(eTech).getType());
}

static const char* getSASGameRecordBonusType(BonusTypes eBonus)
{
	return (eBonus == NO_BONUS ? "-" : GC.getInfo(eBonus).getType());
}

static const char* getSASGameRecordUnitType(UnitTypes eUnit)
{
	return (eUnit == NO_UNIT ? "-" : GC.getInfo(eUnit).getType());
}

static const char* getSASGameRecordUnitAIType(UnitAITypes eUnitAI)
{
	return (eUnitAI == NO_UNITAI ? "-" : GC.getInfo(eUnitAI).getType());
}

static const char* getSASGameRecordUnitCombatType(UnitCombatTypes eUnitCombat)
{
	return (eUnitCombat == NO_UNITCOMBAT ? "-" : GC.getInfo(eUnitCombat).getType());
}

static const char* getSASGameRecordPromotionType(PromotionTypes ePromotion)
{
	return (ePromotion == NO_PROMOTION ? "-" : GC.getInfo(ePromotion).getType());
}

static const char* getSASGameRecordSpecialistType(SpecialistTypes eSpecialist)
{
	return (eSpecialist == NO_SPECIALIST ? "-" : GC.getInfo(eSpecialist).getType());
}

static const char* getSASGameRecordBuildingType(BuildingTypes eBuilding)
{
	return (eBuilding == NO_BUILDING ? "-" : GC.getInfo(eBuilding).getType());
}

static const char* getSASGameRecordProjectType(ProjectTypes eProject)
{
	return (eProject == NO_PROJECT ? "-" : GC.getInfo(eProject).getType());
}

static const char* getSASGameRecordProcessType(ProcessTypes eProcess)
{
	return (eProcess == NO_PROCESS ? "-" : GC.getInfo(eProcess).getType());
}

static const char* getSASGameRecordCommerceType(CommerceTypes eCommerce)
{
	return (eCommerce == NO_COMMERCE ? "-" : GC.getInfo(eCommerce).getType());
}

static const char* getSASGameRecordBuildType(BuildTypes eBuild)
{
	return (eBuild == NO_BUILD ? "-" : GC.getInfo(eBuild).getType());
}

static const char* getSASGameRecordMissionType(MissionTypes eMission)
{
	return (eMission == NO_MISSION ? "-" : GC.getInfo(eMission).getType());
}

static void appendSASGameRecordTypeCount(CvString& szList, const char* szType, int iCount)
{
	if (iCount <= 0)
		return;
	CvString szItem;
	szItem.Format(szList.empty() ? "%s:%d" : ",%s:%d", szType, iCount);
	szList += szItem;
}

static void appendSASGameRecordPositiveValue(CvString& szList, const char* szName, int iValue)
{
	if (iValue <= 0)
		return;
	CvString szItem;
	szItem.Format(szList.empty() ? "%s:%d" : ",%s:%d", szName, iValue);
	szList += szItem;
}


// <!-- custom: Aggregate worked-plot snapshots share the compact landscape composition structure used by the mature AdvCiv-SAS city/map diagnostics; level-3 per-city detail rows reuse the same composition instead of rescanning their worked plots independently. (ChatGPT-5.6-Sol) -->
struct SASGameRecordPlotComposition
{
	int iPlots;
	int iLand;
	int iWater;
	int iHills;
	int iPeaks;
	int iRiverSide;
	int iFreshWater;
	int iCoastal;
	int iImproved;
	int iUnimprovedLand;
	int iRoaded;
	int iBonusImproved;
	int iBonusUnimproved;
	int iWorked;
	int iWorkedImproved;
	int iWorkedUnimproved;
	int iNatureFood;
	int iNatureProduction;
	int iNatureCommerce;
	int iCurrentFood;
	int iCurrentProduction;
	int iCurrentCommerce;
	std::vector<int> aiTerrains;
	std::vector<int> aiFeatures;
	std::vector<int> aiBonuses;
	std::vector<int> aiImprovements;
	std::vector<int> aiRoutes;

	SASGameRecordPlotComposition() : iPlots(0), iLand(0), iWater(0), iHills(0), iPeaks(0), iRiverSide(0), iFreshWater(0), iCoastal(0), iImproved(0), iUnimprovedLand(0), iRoaded(0), iBonusImproved(0), iBonusUnimproved(0), iWorked(0), iWorkedImproved(0), iWorkedUnimproved(0), iNatureFood(0), iNatureProduction(0), iNatureCommerce(0), iCurrentFood(0), iCurrentProduction(0), iCurrentCommerce(0), aiTerrains(GC.getNumTerrainInfos(), 0), aiFeatures(GC.getNumFeatureInfos(), 0), aiBonuses(GC.getNumBonusInfos(), 0), aiImprovements(GC.getNumImprovementInfos(), 0), aiRoutes(GC.getNumRouteInfos(), 0) {}
};

static const char* getSASGameRecordTerrainType(TerrainTypes eTerrain)
{
	return (eTerrain == NO_TERRAIN ? "-" : GC.getInfo(eTerrain).getType());
}

static const char* getSASGameRecordFeatureType(FeatureTypes eFeature)
{
	return (eFeature == NO_FEATURE ? "-" : GC.getInfo(eFeature).getType());
}

static const char* getSASGameRecordImprovementType(ImprovementTypes eImprovement)
{
	return (eImprovement == NO_IMPROVEMENT ? "-" : GC.getInfo(eImprovement).getType());
}

static const char* getSASGameRecordRouteType(RouteTypes eRoute)
{
	return (eRoute == NO_ROUTE ? "-" : GC.getInfo(eRoute).getType());
}

static CvWString getSASGameRecordQuotedCityName(CvCity const* pCity)
{
	return pCity == NULL ? L"-" : getSASDiagnosticQuoted(pCity->getName().GetCString());
}

static void addSASGameRecordPlotComposition(SASGameRecordPlotComposition& kComposition, CvPlot const& kPlot, TeamTypes eTeam)
{
	kComposition.iPlots++;
	if (kPlot.isWater())
		kComposition.iWater++;
	else kComposition.iLand++;
	if (kPlot.isHills()) kComposition.iHills++;
	if (kPlot.isPeak()) kComposition.iPeaks++;
	if (kPlot.isRiverSide()) kComposition.iRiverSide++;
	if (kPlot.isFreshWater()) kComposition.iFreshWater++;
	if (kPlot.isCoastalLand()) kComposition.iCoastal++;
	if (kPlot.getTerrainType() != NO_TERRAIN) kComposition.aiTerrains[kPlot.getTerrainType()]++;
	if (kPlot.getFeatureType() != NO_FEATURE) kComposition.aiFeatures[kPlot.getFeatureType()]++;
	ImprovementTypes const eImprovement = kPlot.getImprovementType();
	if (eImprovement != NO_IMPROVEMENT)
	{
		kComposition.iImproved++;
		kComposition.aiImprovements[eImprovement]++;
	}
	else if (!kPlot.isWater()) kComposition.iUnimprovedLand++;
	RouteTypes const eRoute = kPlot.getRouteType();
	if (eRoute != NO_ROUTE)
	{
		kComposition.iRoaded++;
		kComposition.aiRoutes[eRoute]++;
	}
	BonusTypes const eBonus = kPlot.getBonusType(eTeam);
	if (eBonus != NO_BONUS)
	{
		kComposition.aiBonuses[eBonus]++;
		if (eImprovement != NO_IMPROVEMENT) kComposition.iBonusImproved++;
		else kComposition.iBonusUnimproved++;
	}
	if (kPlot.isBeingWorked())
	{
		kComposition.iWorked++;
		if (eImprovement != NO_IMPROVEMENT) kComposition.iWorkedImproved++;
		else kComposition.iWorkedUnimproved++;
	}
	kComposition.iNatureFood += kPlot.calculateBestNatureYield(YIELD_FOOD, eTeam);
	kComposition.iNatureProduction += kPlot.calculateBestNatureYield(YIELD_PRODUCTION, eTeam);
	kComposition.iNatureCommerce += kPlot.calculateBestNatureYield(YIELD_COMMERCE, eTeam);
	kComposition.iCurrentFood += kPlot.calculateYield(YIELD_FOOD);
	kComposition.iCurrentProduction += kPlot.calculateYield(YIELD_PRODUCTION);
	kComposition.iCurrentCommerce += kPlot.calculateYield(YIELD_COMMERCE);
}

static SASGameRecordPlotComposition getSASGameRecordWorkedPlotComposition(CvCity const& kCity)
{
	SASGameRecordPlotComposition kComposition;
	TeamTypes const eTeam = GET_PLAYER(kCity.getOwner()).getTeam();
	// <!-- custom: Exclude the city center from worked-plot allocation records because it is always worked and would blur comparisons of citizen plot choices and improvement coverage between benchmark runs. (GPT-5.5) -->
	for (WorkingPlotIter it(kCity, false); it.hasNext(); ++it)
		addSASGameRecordPlotComposition(kComposition, *it, eTeam);
	return kComposition;
}

static void addSASGameRecordPlotComposition(SASGameRecordPlotComposition& kTarget, SASGameRecordPlotComposition const& kSource)
{
	kTarget.iPlots += kSource.iPlots;
	kTarget.iLand += kSource.iLand;
	kTarget.iWater += kSource.iWater;
	kTarget.iHills += kSource.iHills;
	kTarget.iPeaks += kSource.iPeaks;
	kTarget.iRiverSide += kSource.iRiverSide;
	kTarget.iFreshWater += kSource.iFreshWater;
	kTarget.iCoastal += kSource.iCoastal;
	kTarget.iImproved += kSource.iImproved;
	kTarget.iUnimprovedLand += kSource.iUnimprovedLand;
	kTarget.iRoaded += kSource.iRoaded;
	kTarget.iBonusImproved += kSource.iBonusImproved;
	kTarget.iBonusUnimproved += kSource.iBonusUnimproved;
	kTarget.iWorked += kSource.iWorked;
	kTarget.iWorkedImproved += kSource.iWorkedImproved;
	kTarget.iWorkedUnimproved += kSource.iWorkedUnimproved;
	kTarget.iNatureFood += kSource.iNatureFood;
	kTarget.iNatureProduction += kSource.iNatureProduction;
	kTarget.iNatureCommerce += kSource.iNatureCommerce;
	kTarget.iCurrentFood += kSource.iCurrentFood;
	kTarget.iCurrentProduction += kSource.iCurrentProduction;
	kTarget.iCurrentCommerce += kSource.iCurrentCommerce;
	for (int iI = 0; iI < GC.getNumTerrainInfos(); iI++) kTarget.aiTerrains[iI] += kSource.aiTerrains[iI];
	for (int iI = 0; iI < GC.getNumFeatureInfos(); iI++) kTarget.aiFeatures[iI] += kSource.aiFeatures[iI];
	for (int iI = 0; iI < GC.getNumBonusInfos(); iI++) kTarget.aiBonuses[iI] += kSource.aiBonuses[iI];
	for (int iI = 0; iI < GC.getNumImprovementInfos(); iI++) kTarget.aiImprovements[iI] += kSource.aiImprovements[iI];
	for (int iI = 0; iI < GC.getNumRouteInfos(); iI++) kTarget.aiRoutes[iI] += kSource.aiRoutes[iI];
}

static void getSASGameRecordPlotCompositionTypes(SASGameRecordPlotComposition const& kComposition, CvString& szTerrains, CvString& szFeatures, CvString& szBonuses, CvString& szImprovements, CvString& szRoutes)
{
	for (int iI = 0; iI < GC.getNumTerrainInfos(); iI++) appendSASGameRecordTypeCount(szTerrains, getSASGameRecordTerrainType((TerrainTypes)iI), kComposition.aiTerrains[iI]);
	for (int iI = 0; iI < GC.getNumFeatureInfos(); iI++) appendSASGameRecordTypeCount(szFeatures, getSASGameRecordFeatureType((FeatureTypes)iI), kComposition.aiFeatures[iI]);
	for (int iI = 0; iI < GC.getNumBonusInfos(); iI++) appendSASGameRecordTypeCount(szBonuses, getSASGameRecordBonusType((BonusTypes)iI), kComposition.aiBonuses[iI]);
	for (int iI = 0; iI < GC.getNumImprovementInfos(); iI++) appendSASGameRecordTypeCount(szImprovements, getSASGameRecordImprovementType((ImprovementTypes)iI), kComposition.aiImprovements[iI]);
	for (int iI = 0; iI < GC.getNumRouteInfos(); iI++) appendSASGameRecordTypeCount(szRoutes, getSASGameRecordRouteType((RouteTypes)iI), kComposition.aiRoutes[iI]);
}

// <!-- custom: These unit classifiers are defined later with the unit-posture helpers; declare them here because the city aggregate slice now reuses them earlier in this translation unit. MSVC 2003 requires the declaration before first use. (ChatGPT-5.6-Sol) -->
static bool isSASGameRecordMilitaryUnit(CvUnit const& kUnit);
static bool isSASGameRecordWorkerUnit(CvUnit const& kUnit);
static bool isSASGameRecordSettlerUnit(CvUnit const& kUnit);

struct SASGameRecordCityPlotUnitCounts
{
	int iUnits;
	int iMilitaryUnits;
	int iCivilianUnits;
	int iDefenders;
	int iHealthyDefenders;
	int iWoundedDefenders;
	int iSettlers;
	int iWorkers;
	int iAttackers;
	SASGameRecordCityPlotUnitCounts() : iUnits(0), iMilitaryUnits(0), iCivilianUnits(0), iDefenders(0), iHealthyDefenders(0), iWoundedDefenders(0), iSettlers(0), iWorkers(0), iAttackers(0) {}
};

static void collectSASGameRecordCityPlotUnitCounts(CvPlot const& kPlot, PlayerTypes ePlayer, SASGameRecordCityPlotUnitCounts& kCounts)
{
	for (CLLNode<IDInfo> const* pUnitNode = kPlot.headUnitNode(); pUnitNode != NULL; pUnitNode = kPlot.nextUnitNode(pUnitNode))
	{
		CvUnit const* pLoopUnit = ::getUnit(pUnitNode->m_data);
		if (pLoopUnit == NULL || pLoopUnit->getOwner() != ePlayer) continue;
		kCounts.iUnits++;
		if (isSASGameRecordMilitaryUnit(*pLoopUnit)) kCounts.iMilitaryUnits++;
		else kCounts.iCivilianUnits++;
		if (pLoopUnit->canDefend(&kPlot))
		{
			kCounts.iDefenders++;
			if (pLoopUnit->getDamage() <= 25) kCounts.iHealthyDefenders++;
			else kCounts.iWoundedDefenders++;
		}
		if (isSASGameRecordSettlerUnit(*pLoopUnit)) kCounts.iSettlers++;
		if (isSASGameRecordWorkerUnit(*pLoopUnit)) kCounts.iWorkers++;
		if (pLoopUnit->canAttack()) kCounts.iAttackers++;
	}
}

static void appendSASGameRecordValue(CvString& szList, const char* szName, int iValue)
{
	CvString szItem;
	szItem.Format(szList.empty() ? "%s:%d" : ",%s:%d", szName, iValue);
	szList += szItem;
}

static int getSASGameRecordPercentX100(int iValue, int iTotal)
{
	return (iTotal <= 0 ? -1 : (10000 * iValue) / iTotal);
}

static void appendSASGameRecordSignedValue(CvString& szList, const char* szName, int iValue)
{
	if (iValue == 0)
		return;
	CvString szItem;
	szItem.Format(szList.empty() ? "%s:%+d" : ",%s:%+d", szName, iValue);
	szList += szItem;
}

static CvString getSASGameRecordTeamMembers(TeamTypes eTeam)
{
	CvString szList;
	for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		PlayerTypes eLoopPlayer = (PlayerTypes)iI;
		CvPlayer const& kLoopPlayer = GET_PLAYER(eLoopPlayer);
		if (kLoopPlayer.isAlive() && kLoopPlayer.getTeam() == eTeam)
			appendSASDiagnosticIntListValue(szList, eLoopPlayer);
	}
	return getSASDiagnosticOrDash(szList);
}

static CvString getSASGameRecordWarTeams(TeamTypes eTeam)
{
	CvString szList;
	CvTeam const& kTeam = GET_TEAM(eTeam);
	for (int iI = 0; iI < MAX_CIV_TEAMS; iI++)
	{
		TeamTypes eLoopTeam = (TeamTypes)iI;
		if (eLoopTeam != eTeam && GET_TEAM(eLoopTeam).isAlive() && kTeam.isAtWar(eLoopTeam))
			appendSASDiagnosticIntListValue(szList, eLoopTeam);
	}
	return getSASDiagnosticOrDash(szList);
}

static CvString getSASGameRecordVassalTeams(TeamTypes eTeam)
{
	CvString szList;
	for (int iI = 0; iI < MAX_CIV_TEAMS; iI++)
	{
		TeamTypes eLoopTeam = (TeamTypes)iI;
		if (eLoopTeam != eTeam && GET_TEAM(eLoopTeam).isAlive() && GET_TEAM(eLoopTeam).isVassal(eTeam))
			appendSASDiagnosticIntListValue(szList, eLoopTeam);
	}
	return getSASDiagnosticOrDash(szList);
}

static CvString getSASGameRecordMetTeams(TeamTypes eTeam)
{
	CvString szMetTeams;
	for (int iI = 0; iI < MAX_CIV_TEAMS; iI++)
	{
		TeamTypes eLoopTeam = (TeamTypes)iI;
		if (eLoopTeam == eTeam || !GET_TEAM(eLoopTeam).isAlive() || GET_TEAM(eLoopTeam).isBarbarian())
			continue;
		if (GET_TEAM(eTeam).isHasMet(eLoopTeam))
			appendSASDiagnosticIntListValue(szMetTeams, eLoopTeam);
	}
	return getSASDiagnosticOrDash(szMetTeams);
}

static int getSASGameRecordMetTeamCount(TeamTypes eTeam)
{
	int iCount = 0;
	for (int iI = 0; iI < MAX_CIV_TEAMS; iI++)
	{
		TeamTypes eLoopTeam = (TeamTypes)iI;
		if (eLoopTeam != eTeam && GET_TEAM(eLoopTeam).isAlive() && !GET_TEAM(eLoopTeam).isBarbarian() && GET_TEAM(eTeam).isHasMet(eLoopTeam))
			iCount++;
	}
	return iCount;
}

static void logSASGameRecordTeamContacts(TeamTypes eTeam, int iGameTurn, const char* szReason)
{
	SASGameRecordTeamPrevious& kPrevious = g_akSASGameRecordTeamPrevious[eTeam];
	const int iMetTeams = getSASGameRecordMetTeamCount(eTeam);
	logSASGameRecord("GAME_RECORD_CONTACTS turn=%d reason=%s team=%d deltaValid=%d metCount=%d metCountDelta=%+d metTeams=%s",
			iGameTurn, szReason, eTeam, kPrevious.bContactsValid, iMetTeams, getSASGameRecordDelta(kPrevious.bContactsValid, iMetTeams, kPrevious.iMetTeams), getSASGameRecordMetTeams(eTeam).GetCString());
	kPrevious.bContactsValid = true;
	kPrevious.iMetTeams = iMetTeams;
}

static CvString getSASGameRecordTechEraCounts(TeamTypes eTeam)
{
	std::vector<int> aiEras(GC.getNumEraInfos(), 0);
	CvTeam const& kTeam = GET_TEAM(eTeam);
	FOR_EACH_ENUM(Tech)
	{
		if (!kTeam.isHasTech(eLoopTech))
			continue;
		EraTypes eEra = GC.getInfo(eLoopTech).getEra();
		if (eEra != NO_ERA)
			aiEras[eEra]++;
	}
	CvString szList;
	for (int iI = 0; iI < GC.getNumEraInfos(); iI++)
		appendSASGameRecordTypeCount(szList, getSASGameRecordEraType((EraTypes)iI), aiEras[iI]);
	return getSASDiagnosticOrDash(szList);
}

static void seedSASGameRecordTeamPreviousFromCurrentState(TeamTypes eTeam)
{
	CvGame const& kGame = GC.getGame();
	CvTeam const& kTeam = GET_TEAM(eTeam);
	SASGameRecordTeamPrevious& kPrevious = g_akSASGameRecordTeamPrevious[eTeam];
	int const iLand = kTeam.getTotalLand();
	int const iPopulation = kTeam.getTotalPopulation();
	kPrevious.bValid = true;
	kPrevious.iTechs = kTeam.getTechCount();
	kPrevious.iLand = iLand;
	kPrevious.iLandPctX100 = (10000 * iLand) / std::max(1, GC.getMap().getLandPlots());
	kPrevious.iPopulation = iPopulation;
	kPrevious.iPopPctX100 = (10000 * iPopulation) / std::max(1, kGame.getTotalPopulation());
	kPrevious.bContactsValid = true;
	kPrevious.iMetTeams = getSASGameRecordMetTeamCount(eTeam);
}

// <!-- custom: Project completion rows alone do not show whether a project-based victory has its minimum/full component set or an active launch countdown. Build one compact shared state for periodic progress and later explicit launch actions. (GPT-5.6-Sol) -->
static bool getSASGameRecordVictoryProjectState(TeamTypes eTeam, VictoryTypes eVictory, int& iPartsBuilt, int& iPartsMinimum, int& iPartsMaximum, bool& bMinimumComplete, CvString& szProjectParts)
{
	iPartsBuilt = 0;
	iPartsMinimum = 0;
	iPartsMaximum = 0;
	bMinimumComplete = true;
	szProjectParts.clear();
	CvTeam const& kTeam = GET_TEAM(eTeam);
	FOR_EACH_ENUM(Project)
	{
		CvProjectInfo const& kProject = GC.getInfo(eLoopProject);
		int const iMinimum = kProject.getVictoryMinThreshold(eVictory);
		int const iMaximum = kProject.getVictoryThreshold(eVictory);
		if (iMinimum <= 0 && iMaximum <= 0)
			continue;
		int const iBuilt = kTeam.getProjectCount(eLoopProject);
		iPartsBuilt += iBuilt;
		iPartsMinimum += iMinimum;
		iPartsMaximum += iMaximum;
		if (iBuilt < iMinimum)
			bMinimumComplete = false;
		CvString szItem;
		szItem.Format(szProjectParts.empty() ? "%s:%d/%d/%d" : ",%s:%d/%d/%d", getSASGameRecordProjectType(eLoopProject), iBuilt, iMinimum, iMaximum);
		szProjectParts += szItem;
	}
	return !szProjectParts.empty();
}

static char const* getSASGameRecordVictoryType(VictoryTypes eVictory)
{
	return eVictory == NO_VICTORY ? "-" : GC.getInfo(eVictory).getType();
}

typedef std::pair<int, CvCity const*> SASGameRecordCultureCity;

static bool compareSASGameRecordCultureCities(SASGameRecordCultureCity const& kFirst, SASGameRecordCultureCity const& kSecond)
{
	return kFirst.first > kSecond.first;
}

static CvString getSASGameRecordCultureVictoryCities(TeamTypes eTeam, int iRequired, int iThreshold, int& iComplete)
{
	std::vector<SASGameRecordCultureCity> aCities;
	for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		CvPlayer const& kMember = GET_PLAYER((PlayerTypes)iI);
		if (!kMember.isAlive() || kMember.getTeam() != eTeam)
			continue;
		int iLoop = 0;
		for (CvCity const* pCity = kMember.firstCity(&iLoop); pCity != NULL; pCity = kMember.nextCity(&iLoop))
			aCities.push_back(std::make_pair(pCity->getCulture(pCity->getOwner()), pCity));
	}
	std::sort(aCities.begin(), aCities.end(), compareSASGameRecordCultureCities);
	iComplete = 0;
	for (int iI = 0; iI < (int)aCities.size(); iI++)
	{
		if (aCities[iI].first >= iThreshold)
			iComplete++;
	}
	CvString szCities;
	for (int iI = 0; iI < std::min(iRequired, (int)aCities.size()); iI++)
	{
		CvCity const& kCity = *aCities[iI].second;
		CvString szItem;
		szItem.Format(szCities.empty() ? "P%d:C%d@%d:%d=%d/%d" : ",P%d:C%d@%d:%d=%d/%d", kCity.getOwner(), kCity.getID(), kCity.getX(), kCity.getY(), aCities[iI].first, iThreshold);
		szCities += szItem;
	}
	return getSASDiagnosticOrDash(szCities);
}

static void logSASGameRecordTeamProjects(TeamTypes eTeam, int iGameTurn)
{
	CvString szProjects;
	FOR_EACH_ENUM(Project)
		appendSASGameRecordTypeCount(szProjects, getSASGameRecordProjectType(eLoopProject), GET_TEAM(eTeam).getProjectCount(eLoopProject));
	if (!szProjects.empty())
		logSASGameRecord("GAME_RECORD_TEAM_PROJECTS turn=%d team=%d projects=%s", iGameTurn, eTeam, szProjects.GetCString());
}

static void logSASGameRecordTeamSnapshot(TeamTypes eTeam, int iGameTurn)
{
	CvGame const& kGame = GC.getGame();
	CvTeam const& kTeam = GET_TEAM(eTeam);
	bool const bLogTeamDetails = (getSASGameRecordLogLevel() >= 2);
	const int iLandPlots = std::max(1, GC.getMap().getLandPlots());
	const int iGamePopulation = std::max(1, kGame.getTotalPopulation());
	const int iTechs = kTeam.getTechCount();
	const int iLand = kTeam.getTotalLand();
	const int iLandPctX100 = (10000 * iLand) / iLandPlots;
	const int iPopulation = kTeam.getTotalPopulation();
	const int iPopPctX100 = (10000 * iPopulation) / iGamePopulation;
	SASGameRecordTeamPrevious& kPrevious = g_akSASGameRecordTeamPrevious[eTeam];
	TeamTypes const eMaster = (kTeam.isAVassal() ? kTeam.getMasterTeam() : NO_TEAM);
	logSASGameRecord("GAME_RECORD_TEAM turn=%d team=%d members=%s alive=%d deltaValid=%d techs=%d techsDelta=%+d techEraCounts=%s techTrading=%d goldTrading=%d land=%d landDelta=%+d landPctX100=%d landPctX100Delta=%+d pop=%d popDelta=%+d popPctX100=%d popPctX100Delta=%+d wars=%s vassals=%s master=%d",
			iGameTurn, eTeam, getSASGameRecordTeamMembers(eTeam).GetCString(), kTeam.isAlive(), kPrevious.bValid,
			iTechs, getSASGameRecordDelta(kPrevious.bValid, iTechs, kPrevious.iTechs), getSASGameRecordTechEraCounts(eTeam).GetCString(), kTeam.isTechTrading(), kTeam.isGoldTrading(),
			iLand, getSASGameRecordDelta(kPrevious.bValid, iLand, kPrevious.iLand), iLandPctX100, getSASGameRecordDelta(kPrevious.bValid, iLandPctX100, kPrevious.iLandPctX100),
			iPopulation, getSASGameRecordDelta(kPrevious.bValid, iPopulation, kPrevious.iPopulation), iPopPctX100, getSASGameRecordDelta(kPrevious.bValid, iPopPctX100, kPrevious.iPopPctX100),
			getSASGameRecordWarTeams(eTeam).GetCString(), getSASGameRecordVassalTeams(eTeam).GetCString(), eMaster);
	if (bLogTeamDetails) logSASGameRecordTeamContacts(eTeam, iGameTurn, "snapshot");
	seedSASGameRecordTeamPreviousFromCurrentState(eTeam);

	VictoryTypes eScoreVictory = NO_VICTORY;
	VictoryTypes eTimeVictory = NO_VICTORY;
	VictoryTypes eConquestVictory = NO_VICTORY;
	VictoryTypes eCultureVictory = NO_VICTORY;
	VictoryTypes eDiplomaticVictory = NO_VICTORY;
	int iCultureCitiesRequired = 0;
	int iCultureThreshold = 0;
	FOR_EACH_ENUM(Victory)
	{
		if (!kGame.isVictoryValid(eLoopVictory))
			continue;
		CvVictoryInfo const& kVictory = GC.getInfo(eLoopVictory);
		if (kVictory.isTargetScore()) eScoreVictory = eLoopVictory;
		if (kVictory.isEndScore()) eTimeVictory = eLoopVictory;
		if (kVictory.isConquest()) eConquestVictory = eLoopVictory;
		if (kVictory.isDiploVote()) eDiplomaticVictory = eLoopVictory;
		if (kVictory.getCityCulture() != NO_CULTURELEVEL && kVictory.getNumCultureCities() > 0)
		{
			eCultureVictory = eLoopVictory;
			iCultureCitiesRequired = kVictory.getNumCultureCities();
			iCultureThreshold = kGame.getCultureThreshold((CultureLevelTypes)kVictory.getCityCulture());
		}
	}
	CvString szConquestRivals;
	int iConquestRivalCities = 0;
	if (eConquestVictory != NO_VICTORY)
	{
		for (int iI = 0; iI < MAX_CIV_TEAMS; iI++)
		{
			TeamTypes const eRival = (TeamTypes)iI;
			CvTeam const& kRival = GET_TEAM(eRival);
			if (eRival == eTeam || !kRival.isAlive() || kRival.isBarbarian() || kRival.isVassal(eTeam) || kRival.getNumCities() <= 0)
				continue;
			appendSASDiagnosticIntListValue(szConquestRivals, eRival);
			iConquestRivalCities += kRival.getNumCities();
		}
	}
	int iBestRivalScore = -1;
	for (int iI = 0; iI < MAX_CIV_TEAMS; iI++)
	{
		TeamTypes const eRival = (TeamTypes)iI;
		if (eRival != eTeam && GET_TEAM(eRival).isAlive() && !GET_TEAM(eRival).isBarbarian())
			iBestRivalScore = std::max(iBestRivalScore, kGame.getTeamScore(eRival));
	}
	int const iTeamScore = kGame.getTeamScore(eTeam);
	int const iTurnsRemaining = (kGame.getMaxTurns() <= 0 ? -1 : std::max(0, kGame.getMaxTurns() - kGame.getElapsedGameTurns()));
	int iCultureCitiesComplete = 0;
	CvString szCultureCities;
	if (eCultureVictory == NO_VICTORY) szCultureCities = "-";
	else szCultureCities = getSASGameRecordCultureVictoryCities(eTeam, iCultureCitiesRequired, iCultureThreshold, iCultureCitiesComplete);
	// <!-- custom: Domination and Space already have detailed per-victory rows, and diplomatic vote-source rows can later contain exact vote thresholds.
	// Add one compact general row per team rather than one new row per missing victory, so Score/Time, Conquest, and Cultural progress become explicit without multiplying snapshot noise.
	// Culture lists only the required number of leading cities. The vote-source companion rows remain a later periodic/global slice in this incremental 1.14 port. (GPT-5.6-Sol + ChatGPT-5.6-Sol) -->
	logSASGameRecord("GAME_RECORD_VICTORY_PROGRESS_GENERAL turn=%d team=%d scoreVictory=%s timeVictory=%s conquestVictory=%s culturalVictory=%s diplomaticVictory=%s teamScore=%d bestRivalScore=%d scoreLead=%+d targetScore=%d turnsRemaining=%d conquestRivals=%s conquestRivalCities=%d cultureCitiesComplete=%d cultureCitiesRequired=%d cultureThreshold=%d cultureCities=%s",
			iGameTurn, eTeam, getSASGameRecordVictoryType(eScoreVictory), getSASGameRecordVictoryType(eTimeVictory), getSASGameRecordVictoryType(eConquestVictory), getSASGameRecordVictoryType(eCultureVictory), getSASGameRecordVictoryType(eDiplomaticVictory),
			iTeamScore, iBestRivalScore, iBestRivalScore < 0 ? iTeamScore : iTeamScore - iBestRivalScore, kGame.getTargetScore(), iTurnsRemaining, getSASDiagnosticOrDash(szConquestRivals).GetCString(), iConquestRivalCities,
			iCultureCitiesComplete, iCultureCitiesRequired, iCultureThreshold, szCultureCities.GetCString());

	FOR_EACH_ENUM(Victory)
	{
		if (!kGame.isVictoryValid(eLoopVictory))
			continue;
		const int iLandNeed = kGame.getAdjustedLandPercent(eLoopVictory);
		const int iPopNeed = kGame.getAdjustedPopulationPercent(eLoopVictory);
		int iPartsBuilt = 0;
		int iPartsMinimum = 0;
		int iPartsMaximum = 0;
		bool bMinimumComplete = false;
		CvString szProjectParts;
		bool const bProjectVictory = getSASGameRecordVictoryProjectState(eTeam, eLoopVictory, iPartsBuilt, iPartsMinimum, iPartsMaximum, bMinimumComplete, szProjectParts);
		if (iLandNeed > 0 || iPopNeed > 0 || bProjectVictory)
		{
			int const iCountdown = kTeam.getVictoryCountdown(eLoopVictory);
			int const iTravelTurns = (bProjectVictory && bMinimumComplete ? kTeam.getVictoryDelay(eLoopVictory) : -1);
			logSASGameRecord("GAME_RECORD_VICTORY_PROGRESS turn=%d team=%d victory=%s landPctX100=%d landNeed=%d popPctX100=%d popNeed=%d projectVictory=%d launched=%d countdown=%d arrivalTurn=%d canLaunch=%d launchSuccessPercent=%d travelTurns=%d partsBuilt=%d partsMinimum=%d partsMaximum=%d projectParts=%s",
				iGameTurn, eTeam, GC.getInfo(eLoopVictory).getType(), iLandPctX100, iLandNeed, iPopPctX100, iPopNeed, bProjectVictory, bProjectVictory && iCountdown >= 0, iCountdown, iCountdown < 0 ? -1 : iGameTurn + iCountdown, bProjectVictory && kTeam.canLaunch(eLoopVictory), bProjectVictory ? kTeam.getLaunchSuccessRate(eLoopVictory) : -1, iTravelTurns, iPartsBuilt, iPartsMinimum, iPartsMaximum, bProjectVictory ? szProjectParts.GetCString() : "-");
		}
	}
	if (bLogTeamDetails) logSASGameRecordTeamProjects(eTeam, iGameTurn);
}

static bool isSASGameRecordMilitaryUnit(CvUnit const& kUnit)
{
	// <!-- custom: A failed NO_UNIT creation left an unplaced/reset object in the owner container, and the end-turn snapshot crashed while reading its combat state. Unplaced units are not part of military posture; short-circuit before unit-info-backed checks. See KI#524.6. (GPT-5.6-Sol) -->
	CvPlot const* pPlot = kUnit.plot();
	return pPlot != NULL && (kUnit.canDefend(pPlot) || kUnit.baseCombatStr() > 0 || kUnit.airBaseCombatStr() > 0);
}

static bool isSASGameRecordWorkerUnit(CvUnit const& kUnit)
{
	UnitAITypes eUnitAI = kUnit.AI_getUnitAIType();
	return eUnitAI == UNITAI_WORKER || eUnitAI == UNITAI_WORKER_SEA || kUnit.workRate(true) > 0;
}

static bool isSASGameRecordSettlerUnit(CvUnit const& kUnit)
{
	return kUnit.AI_getUnitAIType() == UNITAI_SETTLE || kUnit.isFound();
}

static MissionTypes getSASGameRecordUnitMissionType(CvUnit const& kUnit)
{
	CvSelectionGroup const* pGroup = kUnit.getGroup();
	return pGroup == NULL ? NO_MISSION : pGroup->getMissionType(0);
}

static int getSASGameRecordBuildTurnsLeft(CvUnit const& kUnit, BuildTypes eBuild)
{
	return eBuild == NO_BUILD ? -1 : kUnit.getPlot().getBuildTurnsLeft(eBuild, kUnit.getOwner(), 0, 0);
}

static bool isSASGameRecordUnitGuarded(CvUnit const& kUnit)
{
	CvPlot const* pPlot = kUnit.plot();
	return pPlot != NULL && pPlot->getNumDefenders(kUnit.getOwner()) > 0;
}

static bool isSASGameRecordUnitThreatened(CvUnit const& kUnit)
{
	CvPlot const* pPlot = kUnit.plot();
	return pPlot != NULL && pPlot->isVisibleEnemyUnit(kUnit.getOwner());
}

static CvString getSASGameRecordCivicList(CvPlayer const& kPlayer)
{
	CvString szList;
	FOR_EACH_ENUM(CivicOption)
	{
		CivicTypes eCivic = kPlayer.getCivics(eLoopCivicOption);
		if (eCivic == NO_CIVIC)
			continue;
		CvString szItem;
		szItem.Format(szList.empty() ? "%s:%s" : ",%s:%s", GC.getInfo(eLoopCivicOption).getType(), getSASGameRecordCivicType(eCivic));
		szList += szItem;
	}
	return getSASDiagnosticOrDash(szList);
}


static CvString getSASGameRecordPlayerCityReligions(CvPlayer const& kPlayer)
{
	std::vector<int> aiCounts(GC.getNumReligionInfos(), 0);
	int iLoop = 0;
	for (CvCity const* pLoopCity = kPlayer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kPlayer.nextCity(&iLoop))
	{
		FOR_EACH_ENUM(Religion)
		{
			if (pLoopCity->isHasReligion(eLoopReligion))
				aiCounts[eLoopReligion]++;
		}
	}
	CvString szList;
	FOR_EACH_ENUM(Religion)
		appendSASGameRecordTypeCount(szList, getSASGameRecordReligionType(eLoopReligion), aiCounts[eLoopReligion]);
	return getSASDiagnosticOrDash(szList);
}


static CvString getSASGameRecordPlayerCityCorporations(CvPlayer const& kPlayer)
{
	std::vector<int> aiCounts(GC.getNumCorporationInfos(), 0);
	int iLoop = 0;
	for (CvCity const* pLoopCity = kPlayer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kPlayer.nextCity(&iLoop))
	{
		FOR_EACH_ENUM(Corporation)
		{
			if (pLoopCity->isHasCorporation(eLoopCorporation))
				aiCounts[eLoopCorporation]++;
		}
	}
	CvString szList;
	FOR_EACH_ENUM(Corporation)
		appendSASGameRecordTypeCount(szList, getSASGameRecordCorporationType(eLoopCorporation), aiCounts[eLoopCorporation]);
	return getSASDiagnosticOrDash(szList);
}


static void getSASGameRecordPlayerExtraSources(CvPlayer const& kPlayer, CvString& szHealthSources, CvString& szHappinessSources)
{
	int iKnownHealth = 0;
	int iKnownHappiness = 0;
	FOR_EACH_ENUM(Trait)
	{
		if (!kPlayer.hasTrait(eLoopTrait))
			continue;
		CvTraitInfo const& kTrait = GC.getInfo(eLoopTrait);
		iKnownHealth += kTrait.getHealth();
		iKnownHappiness += kTrait.getHappiness();
		appendSASGameRecordSignedValue(szHealthSources, kTrait.getType(), kTrait.getHealth());
		appendSASGameRecordSignedValue(szHappinessSources, kTrait.getType(), kTrait.getHappiness());
	}
	FOR_EACH_ENUM(CivicOption)
	{
		CivicTypes const eCivic = kPlayer.getCivics(eLoopCivicOption);
		if (eCivic == NO_CIVIC)
			continue;
		CvCivicInfo const& kCivic = GC.getInfo(eCivic);
		iKnownHealth += kCivic.getExtraHealth();
		iKnownHappiness += kCivic.getExtraHappiness();
		appendSASGameRecordSignedValue(szHealthSources, kCivic.getType(), kCivic.getExtraHealth());
		appendSASGameRecordSignedValue(szHappinessSources, kCivic.getType(), kCivic.getExtraHappiness());
	}
	CvTeam const& kTeam = GET_TEAM(kPlayer.getTeam());
	FOR_EACH_ENUM(Tech)
	{
		if (!kTeam.isHasTech(eLoopTech))
			continue;
		CvTechInfo const& kTech = GC.getInfo(eLoopTech);
		iKnownHealth += kTech.getHealth();
		iKnownHappiness += kTech.getHappiness();
		appendSASGameRecordSignedValue(szHealthSources, kTech.getType(), kTech.getHealth());
		appendSASGameRecordSignedValue(szHappinessSources, kTech.getType(), kTech.getHappiness());
	}
	appendSASGameRecordSignedValue(szHealthSources, "OTHER", kPlayer.getExtraHealth() - iKnownHealth);
	appendSASGameRecordSignedValue(szHappinessSources, "OTHER", kPlayer.getExtraHappiness() - iKnownHappiness);
}


// <!-- custom: Objective victory progress does not show which route currently guides AI strategy. Record the compact 0..4 route stages once per AI snapshot so city production and war choices can be interpreted without enabling detailed BBAI decisions. (GPT-5.6-Sol) -->
static void logSASGameRecordAIVictoryStages(PlayerTypes ePlayer, int iGameTurn)
{
	CvPlayerAI const& kPlayer = GET_PLAYER(ePlayer);
	if (kPlayer.isHuman() && !kPlayer.isHumanDisabled())
		return;
	AIVictoryStage const eStages = kPlayer.AI_getVictoryStageHash();
	int const iCultureStage = getSASCultureVictoryStageLevel(eStages);
	int const iSpaceStage = getSASSpaceVictoryStageLevel(eStages);
	int const iConquestStage = getSASConquestVictoryStageLevel(eStages);
	int const iDominationStage = getSASDominationVictoryStageLevel(eStages);
	int const iDiplomacyStage = getSASDiplomacyVictoryStageLevel(eStages);
	int const iPlayerMaxStage = std::max(std::max(iCultureStage, iSpaceStage), std::max(std::max(iConquestStage, iDominationStage), iDiplomacyStage));
	logSASGameRecord("GAME_RECORD_AI_VICTORY_STAGES turn=%d player=%d team=%d playerMaxStage=%d teamMaxStage=%d culture=%d space=%d conquest=%d domination=%d diplomacy=%d",
			iGameTurn, ePlayer, kPlayer.getTeam(), iPlayerMaxStage, getSASTeamMaxVictoryStage(kPlayer.getTeam()), iCultureStage, iSpaceStage, iConquestStage, iDominationStage, iDiplomacyStage);
}

// <!-- custom: Keep a compact periodic military-production pressure snapshot in the GameRecord so low/high army phases can be diagnosed even without detailed BBAI logging. The no-area maximum is a player-level reference; AI_chooseProduction can use a different city-area ceiling. (ChatGPT-5.6-Sol) -->
static void logSASGameRecordAIMilitaryProduction(PlayerTypes ePlayer, int iGameTurn)
{
	CvPlayerAI const& kPlayer = GET_PLAYER(ePlayer);
	if (kPlayer.isHuman() && !kPlayer.isHumanDisabled())
		return;
	CvTeamAI const& kTeam = GET_TEAM(kPlayer.getTeam());
	int const iPersonalityBuildProb = GC.getInfo(kPlayer.getPersonalityType()).getBuildUnitProb();
	int const iUnitSpending = kPlayer.AI_unitCostPerMil();
	int const iMaxUnitSpendingNoArea = kPlayer.AI_maxUnitCostPerMil();
	logSASGameRecord("GAME_RECORD_AI_MILITARY_PRODUCTION turn=%d player=%d personalityBuildProb=%d unitSpending=%d maxUnitSpendingNoArea=%d spendingGapNoArea=%d aggressiveAI=%d financialTrouble=%d economyFocus=%d getBetterUnits=%d focusWar=%d dagger=%d alert1=%d alert2=%d finalWar=%d totalWarPlans=%d preparingTotalWarPlans=%d sneakPreparing=%d sneakReady=%d",
		iGameTurn, ePlayer, iPersonalityBuildProb, iUnitSpending, iMaxUnitSpendingNoArea, iMaxUnitSpendingNoArea - iUnitSpending, GC.getGame().isOption(GAMEOPTION_AGGRESSIVE_AI),
		kPlayer.AI_isFinancialTrouble(), kPlayer.AI_isDoStrategy(AI_STRATEGY_ECONOMY_FOCUS), kPlayer.AI_isDoStrategy(AI_STRATEGY_GET_BETTER_UNITS), kPlayer.AI_isFocusWar(), kPlayer.AI_isDoStrategy(AI_STRATEGY_DAGGER),
		kPlayer.AI_isDoStrategy(AI_STRATEGY_ALERT1), kPlayer.AI_isDoStrategy(AI_STRATEGY_ALERT2), kPlayer.AI_isDoStrategy(AI_STRATEGY_FINAL_WAR), kTeam.AI_getNumWarPlans(WARPLAN_TOTAL), kTeam.AI_getNumWarPlans(WARPLAN_PREPARING_TOTAL),
		kTeam.AI_isSneakAttackPreparing(), kTeam.AI_isSneakAttackReady());
}

static void logSASGameRecordPolicies(PlayerTypes ePlayer, int iGameTurn)
{
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	CvString szExtraHealthSources;
	CvString szExtraHappinessSources;
	getSASGameRecordPlayerExtraSources(kPlayer, szExtraHealthSources, szExtraHappinessSources);
	logSASGameRecord("GAME_RECORD_POLICIES turn=%d player=%d civics=%s stateReligion=%s cityReligions=%s cityCorporations=%s playerExtraHealth=%d playerExtraHappiness=%d extraHealthSources=%s extraHappinessSources=%s",
			iGameTurn, ePlayer, getSASGameRecordCivicList(kPlayer).GetCString(), getSASGameRecordReligionType(kPlayer.getStateReligion()), getSASGameRecordPlayerCityReligions(kPlayer).GetCString(), getSASGameRecordPlayerCityCorporations(kPlayer).GetCString(),
			kPlayer.getExtraHealth(), kPlayer.getExtraHappiness(), getSASDiagnosticOrDash(szExtraHealthSources).GetCString(), getSASDiagnosticOrDash(szExtraHappinessSources).GetCString());
}


static void logSASGameRecordEspionage(PlayerTypes ePlayer, int iGameTurn)
{
	CvPlayerAI const& kPlayer = GET_PLAYER(ePlayer);
	CvTeam const& kTeam = GET_TEAM(kPlayer.getTeam());
	SASGameRecordPlayerPrevious& kPrevious = g_akSASGameRecordPlayerPrevious[ePlayer];
	CvString szWeights;
	CvString szSpending;
	CvString szPoints;
	CvString szModifiers;
	// <!-- custom: EP totals alone do not show whether Spies are reaching rivals or remaining idle at home. At periodic level-2 snapshots, summarize foreign deployment, city infiltration, stationary cost-reduction preparation, and current rival targets without logging movement choices. (GPT-5.6-Sol) -->
	CvString szSpyTargets;
	std::vector<int> aiSpiesAgainstPlayer(MAX_PLAYERS, 0);
	int iSpies = 0;
	int iGreatSpies = 0;
	int iSpiesInForeignTerritory = 0;
	int iSpiesInForeignCities = 0;
	int iStationarySpies = 0;
	int iMaxFortifyTurns = 0;
	int iUnitLoop = 0;
	for (CvUnit const* pLoopUnit = kPlayer.firstUnit(&iUnitLoop); pLoopUnit != NULL; pLoopUnit = kPlayer.nextUnit(&iUnitLoop))
	{
		UnitAITypes const eUnitAI = pLoopUnit->AI_getUnitAIType();
		if (!pLoopUnit->isSpy() && eUnitAI != UNITAI_GREAT_SPY)
			continue;
		iSpies++;
		if (eUnitAI == UNITAI_GREAT_SPY)
			iGreatSpies++;
		CvPlot const& kPlot = pLoopUnit->getPlot();
		// <!-- custom: Fortified ordinary Spies at home and fortified Great Spies inflated the stationary mission-discount diagnostic even though they were not preparing a valid foreign espionage mission.
		// Count fortify turns only for ordinary Spies on a structurally valid mission plot; bTestVisible deliberately ignores whether the mission button is usable at this exact snapshot. See KI#376. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
		if (pLoopUnit->isSpy() && pLoopUnit->getFortifyTurns() > 0 && pLoopUnit->canEspionage(&kPlot, true))
		{
			iStationarySpies++;
			iMaxFortifyTurns = std::max(iMaxFortifyTurns, pLoopUnit->getFortifyTurns());
		}
		PlayerTypes const ePlotOwner = kPlot.getOwner();
		if (ePlotOwner != NO_PLAYER && kPlot.getTeam() != kPlayer.getTeam())
		{
			iSpiesInForeignTerritory++;
			if (kPlot.isCity())
				iSpiesInForeignCities++;
			aiSpiesAgainstPlayer[ePlotOwner]++;
		}
	}
	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (aiSpiesAgainstPlayer[iI] <= 0)
			continue;
		CvString szItem;
		szItem.Format(szSpyTargets.empty() ? "%d:%d" : ",%d:%d", iI, aiSpiesAgainstPlayer[iI]);
		szSpyTargets += szItem;
	}
	for (int iI = 0; iI < MAX_CIV_TEAMS; iI++)
	{
		TeamTypes eLoopTeam = (TeamTypes)iI;
		if (eLoopTeam == kPlayer.getTeam() || !GET_TEAM(eLoopTeam).isAlive() || GET_TEAM(eLoopTeam).isBarbarian())
			continue;
		const int iWeight = kPlayer.getEspionageSpendingWeightAgainstTeam(eLoopTeam);
		const int iSpending = kTeam.isHasMet(eLoopTeam) ? kPlayer.getEspionageSpending(eLoopTeam) : -1;
		const int iPoints = kTeam.getEspionagePointsAgainstTeam(eLoopTeam);
		const int iModifier = kTeam.getEspionageModifier(eLoopTeam);
		if (iWeight > 0)
		{
			CvString szItem;
			szItem.Format(szWeights.empty() ? "%d:%d" : ",%d:%d", eLoopTeam, iWeight);
			szWeights += szItem;
		}
		if (iSpending > 0)
		{
			CvString szItem;
			szItem.Format(szSpending.empty() ? "%d:%d" : ",%d:%d", eLoopTeam, iSpending);
			szSpending += szItem;
		}
		if (iPoints > 0)
		{
			CvString szItem;
			szItem.Format(szPoints.empty() ? "%d:%d" : ",%d:%d", eLoopTeam, iPoints);
			szPoints += szItem;
		}
		if (iModifier != 0)
		{
			CvString szItem;
			szItem.Format(szModifiers.empty() ? "%d:%+d" : ",%d:%+d", eLoopTeam, iModifier);
			szModifiers += szItem;
		}
	}
	const int iEspionageRate = kPlayer.getCommerceRate(COMMERCE_ESPIONAGE);
	const int iEspionagePercent = kPlayer.getCommercePercent(COMMERCE_ESPIONAGE);
	const int iTeamEP = kTeam.getEspionagePointsEver();
	const int iUnspentEP = kTeam.getTotalUnspentEspionage();
	// <!-- custom: Weights show intent but not the rounded EP distribution that the game actually applies. Record actual per-rival spending plus the two high-level espionage strategy flags; detailed reasons for enabling those strategies remain BBAI territory. (ChatGPT-5.6-Sol) -->
	const bool bBigEspionage = kPlayer.AI_isDoStrategy(AI_STRATEGY_BIG_ESPIONAGE);
	const bool bEspionageEconomy = kPlayer.AI_isDoStrategy(AI_STRATEGY_ESPIONAGE_ECONOMY);
	logSASGameRecord("GAME_RECORD_ESPIONAGE turn=%d player=%d team=%d espionageRate=%d espionagePercent=%d teamEP=%d unspentEP=%d weights=%s spending=%s pointsAgainst=%s modifiers=%s bigEspionage=%d espionageEconomy=%d spies=%d greatSpies=%d spiesInForeignTerritory=%d spiesInForeignCities=%d stationarySpies=%d maxFortifyTurns=%d spyTargets=%s",
			iGameTurn, ePlayer, kPlayer.getTeam(), iEspionageRate, iEspionagePercent, iTeamEP, iUnspentEP, getSASDiagnosticOrDash(szWeights).GetCString(), getSASDiagnosticOrDash(szSpending).GetCString(), getSASDiagnosticOrDash(szPoints).GetCString(), getSASDiagnosticOrDash(szModifiers).GetCString(), bBigEspionage, bEspionageEconomy, iSpies, iGreatSpies, iSpiesInForeignTerritory, iSpiesInForeignCities, iStationarySpies, iMaxFortifyTurns, getSASDiagnosticOrDash(szSpyTargets).GetCString());
	logSASGameRecord("GAME_RECORD_ESPIONAGE_DELTAS turn=%d player=%d deltaValid=%d espionageRateDelta=%+d espionagePercentDelta=%+d teamEPDelta=%+d unspentEPDelta=%+d",
			iGameTurn, ePlayer, kPrevious.bValid, getSASGameRecordDelta(kPrevious.bValid, iEspionageRate, kPrevious.iEspionageRate), getSASGameRecordDelta(kPrevious.bValid, iEspionagePercent, kPrevious.iEspionagePercent), getSASGameRecordDelta(kPrevious.bValid, iTeamEP, kPrevious.iTeamEP), getSASGameRecordDelta(kPrevious.bValid, iUnspentEP, kPrevious.iUnspentEP));
	kPrevious.iEspionageRate = iEspionageRate;
	kPrevious.iEspionagePercent = iEspionagePercent;
	kPrevious.iTeamEP = iTeamEP;
	kPrevious.iUnspentEP = iUnspentEP;
}


static void logSASGameRecordPlayerBonuses(PlayerTypes ePlayer, int iGameTurn, SASGameRecordPlayerPrevious const& kPrevious)
{
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	CvString szAvailable;
	CvString szTradeable;
	CvString szImports;
	CvString szExports;
	int iBonusTypes = 0;
	int iBonusInstances = 0;
	int iBonusImports = 0;
	int iBonusExports = 0;
	FOR_EACH_ENUM(Bonus)
	{
		const int iAvailable = kPlayer.getNumAvailableBonuses(eLoopBonus);
		const int iTradeable = kPlayer.getNumTradeableBonuses(eLoopBonus);
		const int iImport = kPlayer.getBonusImport(eLoopBonus);
		const int iExport = kPlayer.getBonusExport(eLoopBonus);
		if (iAvailable > 0)
		{
			iBonusTypes++;
			iBonusInstances += iAvailable;
			appendSASGameRecordTypeCount(szAvailable, getSASGameRecordBonusType(eLoopBonus), iAvailable);
		}
		appendSASGameRecordTypeCount(szTradeable, getSASGameRecordBonusType(eLoopBonus), iTradeable);
		if (iImport > 0)
		{
			iBonusImports += iImport;
			appendSASGameRecordTypeCount(szImports, getSASGameRecordBonusType(eLoopBonus), iImport);
		}
		if (iExport > 0)
		{
			iBonusExports += iExport;
			appendSASGameRecordTypeCount(szExports, getSASGameRecordBonusType(eLoopBonus), iExport);
		}
	}
	logSASGameRecord("GAME_RECORD_BONUSES turn=%d player=%d deltaValid=%d bonusTypes=%d bonusTypesDelta=%+d bonusInstances=%d bonusInstancesDelta=%+d imports=%d importsDelta=%+d exports=%d exportsDelta=%+d",
			iGameTurn, ePlayer, kPrevious.bValid, iBonusTypes, getSASGameRecordDelta(kPrevious.bValid, iBonusTypes, kPrevious.iBonusTypes), iBonusInstances, getSASGameRecordDelta(kPrevious.bValid, iBonusInstances, kPrevious.iBonusInstances), iBonusImports, getSASGameRecordDelta(kPrevious.bValid, iBonusImports, kPrevious.iBonusImports), iBonusExports, getSASGameRecordDelta(kPrevious.bValid, iBonusExports, kPrevious.iBonusExports));
	logSASGameRecord("GAME_RECORD_BONUSES_AVAILABLE turn=%d player=%d available=%s", iGameTurn, ePlayer, getSASDiagnosticOrDash(szAvailable).GetCString());
	logSASGameRecord("GAME_RECORD_BONUSES_TRADEABLE turn=%d player=%d tradeable=%s", iGameTurn, ePlayer, getSASDiagnosticOrDash(szTradeable).GetCString());
	logSASGameRecord("GAME_RECORD_BONUSES_IMPORT_EXPORT turn=%d player=%d imported=%s exported=%s", iGameTurn, ePlayer, getSASDiagnosticOrDash(szImports).GetCString(), getSASDiagnosticOrDash(szExports).GetCString());
}


static CvString getSASGameRecordCommercePercents(CvPlayer const& kPlayer)
{
	CvString szList;
	FOR_EACH_ENUM(Commerce)
	{
		CvString szItem;
		szItem.Format(szList.empty() ? "%s:%d" : ",%s:%d", GC.getInfo(eLoopCommerce).getType(), kPlayer.getCommercePercent(eLoopCommerce));
		szList += szItem;
	}
	return getSASDiagnosticOrDash(szList);
}

static CvString getSASGameRecordCommerceRates(CvPlayer const& kPlayer)
{
	CvString szList;
	FOR_EACH_ENUM(Commerce)
	{
		CvString szItem;
		szItem.Format(szList.empty() ? "%s:%d" : ",%s:%d", GC.getInfo(eLoopCommerce).getType(), kPlayer.getCommerceRate(eLoopCommerce));
		szList += szItem;
	}
	return getSASDiagnosticOrDash(szList);
}

static CvString getSASGameRecordCommerceFlexible(CvPlayer const& kPlayer)
{
	CvString szList;
	FOR_EACH_ENUM(Commerce)
	{
		CvString szItem;
		szItem.Format(szList.empty() ? "%s:%d" : ",%s:%d", GC.getInfo(eLoopCommerce).getType(), kPlayer.isCommerceFlexible(eLoopCommerce));
		szList += szItem;
	}
	return getSASDiagnosticOrDash(szList);
}

static void logSASGameRecordDemographics(PlayerTypes ePlayer, int iGameTurn)
{
	CvGame const& kGame = GC.getGame();
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	SASGameRecordPlayerPrevious& kPrevious = g_akSASGameRecordPlayerPrevious[ePlayer];
	const int iScore = kPlayer.calculateScore();
	const int iPopulation = kPlayer.getTotalPopulation();
	const int iLand = kPlayer.getTotalLand();
	const int iFood = kPlayer.calculateTotalYield(YIELD_FOOD);
	const int iProduction = kPlayer.calculateTotalYield(YIELD_PRODUCTION);
	const int iCommerce = kPlayer.calculateTotalYield(YIELD_COMMERCE);
	const int iResearch = kPlayer.getCommerceRate(COMMERCE_RESEARCH);
	const int iCulture = kPlayer.getCommerceRate(COMMERCE_CULTURE);
	const int iEspionage = kPlayer.getCommerceRate(COMMERCE_ESPIONAGE);
	const int iGoldRate = kPlayer.calculateGoldRate();
	const int iPower = kPlayer.getPower();
	logSASGameRecord("GAME_RECORD_DEMOGRAPHICS turn=%d player=%d rank=%d score=%d population=%d land=%d food=%d production=%d commerce=%d research=%d culture=%d espionage=%d goldRate=%d power=%d",
			iGameTurn, ePlayer, kGame.getPlayerRank(ePlayer) + 1, iScore, iPopulation, iLand, iFood, iProduction, iCommerce, iResearch, iCulture, iEspionage, iGoldRate, iPower);
	logSASGameRecord("GAME_RECORD_DEMOGRAPHICS_DELTAS turn=%d player=%d deltaValid=%d scoreDelta=%+d populationDelta=%+d landDelta=%+d foodDelta=%+d productionDelta=%+d commerceDelta=%+d researchDelta=%+d cultureDelta=%+d espionageDelta=%+d goldRateDelta=%+d powerDelta=%+d",
			iGameTurn, ePlayer, kPrevious.bValid,
			getSASGameRecordDelta(kPrevious.bValid, iScore, kPrevious.iDemoScore), getSASGameRecordDelta(kPrevious.bValid, iPopulation, kPrevious.iDemoPopulation), getSASGameRecordDelta(kPrevious.bValid, iLand, kPrevious.iDemoLand),
			getSASGameRecordDelta(kPrevious.bValid, iFood, kPrevious.iDemoFood), getSASGameRecordDelta(kPrevious.bValid, iProduction, kPrevious.iDemoProduction), getSASGameRecordDelta(kPrevious.bValid, iCommerce, kPrevious.iDemoCommerce),
			getSASGameRecordDelta(kPrevious.bValid, iResearch, kPrevious.iDemoResearch), getSASGameRecordDelta(kPrevious.bValid, iCulture, kPrevious.iDemoCulture), getSASGameRecordDelta(kPrevious.bValid, iEspionage, kPrevious.iDemoEspionage),
			getSASGameRecordDelta(kPrevious.bValid, iGoldRate, kPrevious.iDemoGoldRate), getSASGameRecordDelta(kPrevious.bValid, iPower, kPrevious.iDemoPower));
	kPrevious.iDemoScore = iScore;
	kPrevious.iDemoPopulation = iPopulation;
	kPrevious.iDemoLand = iLand;
	kPrevious.iDemoFood = iFood;
	kPrevious.iDemoProduction = iProduction;
	kPrevious.iDemoCommerce = iCommerce;
	kPrevious.iDemoResearch = iResearch;
	kPrevious.iDemoCulture = iCulture;
	kPrevious.iDemoEspionage = iEspionage;
	kPrevious.iDemoGoldRate = iGoldRate;
	kPrevious.iDemoPower = iPower;
}

static void logSASGameRecordAttitudes(PlayerTypes ePlayer, int iGameTurn)
{
	CvPlayerAI const& kPlayer = GET_PLAYER(ePlayer);
	CvString szToward;
	for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		PlayerTypes eLoopPlayer = (PlayerTypes)iI;
		if (eLoopPlayer == ePlayer || !GET_PLAYER(eLoopPlayer).isAlive() || GET_PLAYER(eLoopPlayer).isBarbarian())
			continue;
		if (!GET_TEAM(kPlayer.getTeam()).isHasMet(GET_PLAYER(eLoopPlayer).getTeam()))
			continue;
		const int iValue = kPlayer.AI_getAttitudeVal(eLoopPlayer);
		CvString szItem;
		szItem.Format(szToward.empty() ? "%d:%+d" : ",%d:%+d", eLoopPlayer, iValue);
		szToward += szItem;
	}
	logSASGameRecord("GAME_RECORD_ATTITUDES turn=%d player=%d towardValues=%s", iGameTurn, ePlayer, getSASDiagnosticOrDash(szToward).GetCString());
}

static void logSASGameRecordDiplomaticMemories(PlayerTypes ePlayer, int iGameTurn)
{
	CvPlayerAI const& kPlayer = GET_PLAYER(ePlayer);
	CvTeam const& kTeam = GET_TEAM(kPlayer.getTeam());
	for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		PlayerTypes const eTowardPlayer = (PlayerTypes)iI;
		if (eTowardPlayer == ePlayer || !GET_PLAYER(eTowardPlayer).isAlive() || GET_PLAYER(eTowardPlayer).isBarbarian() || !kTeam.isHasMet(GET_PLAYER(eTowardPlayer).getTeam()))
			continue;
		CvString szMemories;
		int iMemoryAttitude = 0;
		for (int iJ = 0; iJ < NUM_MEMORY_TYPES; iJ++)
		{
			MemoryTypes const eMemory = (MemoryTypes)iJ;
			int const iCount = kPlayer.AI_getMemoryCount(eTowardPlayer, eMemory);
			if (iCount <= 0)
				continue;
			int const iAttitude = kPlayer.AI_getMemoryAttitude(eTowardPlayer, eMemory);
			iMemoryAttitude += iAttitude;
			CvString szItem;
			szItem.Format(szMemories.empty() ? "%s=%d/%+d" : ",%s=%d/%+d", getSASMemoryType(eMemory), iCount, iAttitude);
			szMemories += szItem;
		}
		if (!szMemories.empty())
		{
			// <!-- custom: Level-3 memory rows explain why the existing attitude value changed. Each item is MEMORY_TYPE=count/attitudeContribution; periodic snapshots avoid logging every routine memory decay. (GPT-5.6-Sol) -->
			logSASGameRecord("GAME_RECORD_DIPLO_MEMORIES turn=%d player=%d toward=%d attitudeValue=%+d memoryAttitude=%+d memories=%s", iGameTurn, ePlayer, eTowardPlayer, kPlayer.AI_getAttitudeVal(eTowardPlayer), iMemoryAttitude, szMemories.GetCString());
		}
	}
}

static void logSASGameRecordDiploStatus(PlayerTypes ePlayer, int iGameTurn)
{
	CvPlayerAI const& kPlayer = GET_PLAYER(ePlayer);
	CvTeam const& kTeam = GET_TEAM(kPlayer.getTeam());
	const TeamTypes eWorstEnemy = kTeam.AI().AI_getWorstEnemy();
	CvString szWorstEnemyPlayers;
	CvString szWorstEnemyOfTeams;
	CvString szAtWar;
	CvString szOpenBorders;
	CvString szDefensivePacts;
	CvString szForcePeace;
	CvString szCanContact;
	CvString szCanContactWilling;
	CvString szWontTalkTo;
	CvString szWontTalkFrom;
	for (int iI = 0; iI < MAX_CIV_TEAMS; iI++)
	{
		TeamTypes eLoopTeam = (TeamTypes)iI;
		if (eLoopTeam == kPlayer.getTeam() || !GET_TEAM(eLoopTeam).isAlive() || GET_TEAM(eLoopTeam).isBarbarian())
			continue;
		if (!kTeam.isHasMet(eLoopTeam))
			continue;
		if (kTeam.isAtWar(eLoopTeam))
			appendSASDiagnosticIntListValue(szAtWar, eLoopTeam);
		if (kTeam.isOpenBorders(eLoopTeam))
			appendSASDiagnosticIntListValue(szOpenBorders, eLoopTeam);
		if (kTeam.isDefensivePact(eLoopTeam))
			appendSASDiagnosticIntListValue(szDefensivePacts, eLoopTeam);
		if (kTeam.isForcePeace(eLoopTeam))
			appendSASDiagnosticIntListValue(szForcePeace, eLoopTeam);
		if (GET_TEAM(eLoopTeam).AI().AI_getWorstEnemy() == kPlayer.getTeam())
			appendSASDiagnosticIntListValue(szWorstEnemyOfTeams, eLoopTeam);
	}
	for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		PlayerTypes eLoopPlayer = (PlayerTypes)iI;
		if (eLoopPlayer == ePlayer || !GET_PLAYER(eLoopPlayer).isAlive() || GET_PLAYER(eLoopPlayer).isBarbarian())
			continue;
		if (!kTeam.isHasMet(GET_PLAYER(eLoopPlayer).getTeam()))
			continue;
		if (GET_PLAYER(eLoopPlayer).getTeam() == eWorstEnemy)
			appendSASDiagnosticIntListValue(szWorstEnemyPlayers, eLoopPlayer);
		if (kPlayer.canContact(eLoopPlayer, false))
			appendSASDiagnosticIntListValue(szCanContact, eLoopPlayer);
		if (kPlayer.canContact(eLoopPlayer, true))
			appendSASDiagnosticIntListValue(szCanContactWilling, eLoopPlayer);
		if (!kPlayer.AI_isWillingToTalk(eLoopPlayer))
			appendSASDiagnosticIntListValue(szWontTalkTo, eLoopPlayer);
		if (!GET_PLAYER(eLoopPlayer).AI_isWillingToTalk(ePlayer))
			appendSASDiagnosticIntListValue(szWontTalkFrom, eLoopPlayer);
	}
	logSASGameRecord("GAME_RECORD_DIPLO_STATUS turn=%d player=%d team=%d worstEnemyTeam=%d worstEnemyPlayers=%s worstEnemyOfTeams=%s atWar=%s openBorders=%s defensivePacts=%s forcePeace=%s canContact=%s canContactWilling=%s wontTalkTo=%s wontTalkFrom=%s",
			iGameTurn, ePlayer, kPlayer.getTeam(), eWorstEnemy,
			getSASDiagnosticOrDash(szWorstEnemyPlayers).GetCString(), getSASDiagnosticOrDash(szWorstEnemyOfTeams).GetCString(),
			getSASDiagnosticOrDash(szAtWar).GetCString(), getSASDiagnosticOrDash(szOpenBorders).GetCString(), getSASDiagnosticOrDash(szDefensivePacts).GetCString(), getSASDiagnosticOrDash(szForcePeace).GetCString(),
			getSASDiagnosticOrDash(szCanContact).GetCString(), getSASDiagnosticOrDash(szCanContactWilling).GetCString(), getSASDiagnosticOrDash(szWontTalkTo).GetCString(), getSASDiagnosticOrDash(szWontTalkFrom).GetCString());
}

static void logSASGameRecordEconomy(PlayerTypes ePlayer, int iGameTurn)
{
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	CvTeam const& kTeam = GET_TEAM(kPlayer.getTeam());
	TechTypes eResearch = kPlayer.getCurrentResearch();
	int const iResearchProgress = (eResearch == NO_TECH ? -1 : kTeam.getResearchProgress(eResearch));
	int const iResearchCost = (eResearch == NO_TECH ? -1 : kTeam.getResearchCost(eResearch));
	// <!-- custom: currentResearch=- does not mean that science is lost: CvPlayer::doResearch stores the nominal research rate as overflow until another technology can be selected.
	// Exact shared progress/cost makes partially researched technologies visible at ordinary snapshots instead of only when completion or redirection happens. (GPT-5.6-Sol + ChatGPT-5.6-Sol) -->
	logSASGameRecord("GAME_RECORD_ECONOMY turn=%d player=%d gold=%d goldRate=%d totalCommerce=%d sliders=%s commerceTypeRates=%s flexible=%s currentResearch=%s currentResearchTeamProgress=%d currentResearchCost=%d researchRate=%d researchOverflow=%d noResearchAvailable=%d researchTurns=%d",
			iGameTurn, ePlayer, kPlayer.getGold(), kPlayer.calculateGoldRate(), kPlayer.calculateTotalYield(YIELD_COMMERCE), getSASGameRecordCommercePercents(kPlayer).GetCString(), getSASGameRecordCommerceRates(kPlayer).GetCString(), getSASGameRecordCommerceFlexible(kPlayer).GetCString(), getSASGameRecordTechType(eResearch), iResearchProgress, iResearchCost, kPlayer.calculateResearchRate(eResearch), kPlayer.getOverflowResearch(), kPlayer.isNoResearchAvailable(), eResearch == NO_TECH ? -1 : kPlayer.getResearchTurnsLeft(eResearch, true));
}

// <!-- custom: Production-pipeline aggregation is declared here because the helper that normalizes unavailable current-production cost is defined a little later with the other city-output helpers. (ChatGPT-5.6-Sol) -->
static int getSASGameRecordCityProductionNeeded(CvCity const& kCity);

// <!-- custom: Current city rows show only the active target, so repeated AI switches can leave a strategically important bank of partial production invisible.
// At periodic level-2 snapshots, enumerate stored non-current unit/building/project production once per city and summarize fragmentation; level 3 adds the exact parked inventory, where each unit/building @ value is the engine's accumulated inactive-turn counter (it pauses rather than resets when production resumes).
// Cheap isAnyProductionProgress guards skip each loaded-XML scan when that city has no stored production of the corresponding kind.
// Food-produced units such as Settlers/Workers use the same per-unit production bank, so split them as a useful subset rather than invent a separate parked-food quantity.
// AI unit/building production is not labeled "lost" because inherited K-Mod/AdvCiv CvCity::doDecay only reduces it for human cities. (ChatGPT-5.6-Sol) -->
static void logSASGameRecordProductionPipeline(PlayerTypes ePlayer, int iGameTurn)
{
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	bool const bLogParkedDetails = (gGameRecordLogLevel >= 3);
	int iActiveFiniteItems = 0;
	int iActiveStored = 0;
	int iActiveNeeded = 0;
	int iActiveProcesses = 0;
	int iActiveFoodProductionUnits = 0;
	int iActiveFoodProductionUnitStored = 0;
	int iParkedItems = 0;
	int iParkedStored = 0;
	int iParkedNeeded = 0;
	int iParkedUnitItems = 0;
	int iParkedUnitStored = 0;
	int iParkedFoodProductionUnitItems = 0;
	int iParkedFoodProductionUnitStored = 0;
	int iParkedBuildingItems = 0;
	int iParkedBuildingStored = 0;
	int iParkedWonderItems = 0;
	int iParkedWonderStored = 0;
	int iParkedProjectItems = 0;
	int iParkedProjectStored = 0;
	int iCitiesWithParked = 0;
	int iMaxParkedItemsOneCity = 0;
	int iMaxParkedStoredOneCity = 0;
	int iParkedHalfComplete = 0;
	int iParkedThreeQuarterComplete = 0;
	int iInactivityCounterItems = 0;
	int iAccumulatedInactiveTurnsTotal = 0;
	int iMaxAccumulatedInactiveTurns = 0;
	CvString szParked;
	int iCityLoop = 0;
	for (CvCity const* pCity = kPlayer.firstCity(&iCityLoop); pCity != NULL; pCity = kPlayer.nextCity(&iCityLoop))
	{
		UnitTypes const eCurrentUnit = pCity->getProductionUnit();
		BuildingTypes const eCurrentBuilding = pCity->getProductionBuilding();
		ProjectTypes const eCurrentProject = pCity->getProductionProject();
		ProcessTypes const eCurrentProcess = pCity->getProductionProcess();
		if (eCurrentUnit != NO_UNIT || eCurrentBuilding != NO_BUILDING || eCurrentProject != NO_PROJECT)
		{
			iActiveFiniteItems++;
			iActiveStored += pCity->getProduction();
			iActiveNeeded += getSASGameRecordCityProductionNeeded(*pCity);
			if (eCurrentUnit != NO_UNIT && pCity->isFoodProduction(eCurrentUnit))
			{
				iActiveFoodProductionUnits++;
				iActiveFoodProductionUnitStored += pCity->getUnitProduction(eCurrentUnit);
			}
		}
		else if (eCurrentProcess != NO_PROCESS) iActiveProcesses++;
		int iCityParkedItems = 0;
		int iCityParkedStored = 0;
		if (pCity->isAnyProductionProgress(ORDER_TRAIN))
		{
			FOR_EACH_ENUM(Unit)
			{
				int const iStored = pCity->getUnitProduction(eLoopUnit);
				if (iStored <= 0 || eLoopUnit == eCurrentUnit)
					continue;
				int const iNeeded = pCity->getProductionNeeded(eLoopUnit);
				int const iInactiveTurns = pCity->getUnitProductionTime(eLoopUnit);
				iParkedItems++; iParkedStored += iStored; iParkedNeeded += iNeeded;
				iParkedUnitItems++; iParkedUnitStored += iStored;
				if (pCity->isFoodProduction(eLoopUnit))
				{
					iParkedFoodProductionUnitItems++;
					iParkedFoodProductionUnitStored += iStored;
				}
				iCityParkedItems++; iCityParkedStored += iStored;
				if (iNeeded > 0 && 2 * iStored >= iNeeded) iParkedHalfComplete++;
				if (iNeeded > 0 && 4 * iStored >= 3 * iNeeded) iParkedThreeQuarterComplete++;
				iInactivityCounterItems++; iAccumulatedInactiveTurnsTotal += iInactiveTurns; iMaxAccumulatedInactiveTurns = std::max(iMaxAccumulatedInactiveTurns, iInactiveTurns);
				if (bLogParkedDetails)
				{
					CvString szItem;
					szItem.Format(szParked.empty() ? "%d:UNIT:%s:%d/%d@%d" : ",%d:UNIT:%s:%d/%d@%d", pCity->getID(), getSASGameRecordUnitType(eLoopUnit), iStored, iNeeded, iInactiveTurns);
					szParked += szItem;
				}
			}
		}
		if (pCity->isAnyProductionProgress(ORDER_CONSTRUCT))
		{
			FOR_EACH_ENUM(Building)
			{
				int const iStored = pCity->getBuildingProduction(eLoopBuilding);
				if (iStored <= 0 || eLoopBuilding == eCurrentBuilding)
					continue;
				int const iNeeded = pCity->getProductionNeeded(eLoopBuilding);
				int const iInactiveTurns = pCity->getBuildingProductionTime(eLoopBuilding);
				bool const bWonder = GC.getInfo(eLoopBuilding).isLimited();
				iParkedItems++; iParkedStored += iStored; iParkedNeeded += iNeeded;
				if (bWonder) { iParkedWonderItems++; iParkedWonderStored += iStored; }
				else { iParkedBuildingItems++; iParkedBuildingStored += iStored; }
				iCityParkedItems++; iCityParkedStored += iStored;
				if (iNeeded > 0 && 2 * iStored >= iNeeded) iParkedHalfComplete++;
				if (iNeeded > 0 && 4 * iStored >= 3 * iNeeded) iParkedThreeQuarterComplete++;
				iInactivityCounterItems++; iAccumulatedInactiveTurnsTotal += iInactiveTurns; iMaxAccumulatedInactiveTurns = std::max(iMaxAccumulatedInactiveTurns, iInactiveTurns);
				if (bLogParkedDetails)
				{
					CvString szItem;
					szItem.Format(szParked.empty() ? "%d:%s:%s:%d/%d@%d" : ",%d:%s:%s:%d/%d@%d", pCity->getID(), bWonder ? "WONDER" : "BUILDING", getSASGameRecordBuildingType(eLoopBuilding), iStored, iNeeded, iInactiveTurns);
					szParked += szItem;
				}
			}
		}
		if (pCity->isAnyProductionProgress(ORDER_CREATE))
		{
			FOR_EACH_ENUM(Project)
			{
				int const iStored = pCity->getProjectProduction(eLoopProject);
				if (iStored <= 0 || eLoopProject == eCurrentProject)
					continue;
				int const iNeeded = pCity->getProductionNeeded(eLoopProject);
				iParkedItems++; iParkedStored += iStored; iParkedNeeded += iNeeded;
				iParkedProjectItems++; iParkedProjectStored += iStored;
				iCityParkedItems++; iCityParkedStored += iStored;
				if (iNeeded > 0 && 2 * iStored >= iNeeded) iParkedHalfComplete++;
				if (iNeeded > 0 && 4 * iStored >= 3 * iNeeded) iParkedThreeQuarterComplete++;
				if (bLogParkedDetails)
				{
					CvString szItem;
					szItem.Format(szParked.empty() ? "%d:PROJECT:%s:%d/%d@-" : ",%d:PROJECT:%s:%d/%d@-", pCity->getID(), getSASGameRecordProjectType(eLoopProject), iStored, iNeeded);
					szParked += szItem;
				}
			}
		}
		if (iCityParkedItems > 0)
		{
			iCitiesWithParked++;
			iMaxParkedItemsOneCity = std::max(iMaxParkedItemsOneCity, iCityParkedItems);
			iMaxParkedStoredOneCity = std::max(iMaxParkedStoredOneCity, iCityParkedStored);
		}
	}
	logSASGameRecord("GAME_RECORD_PRODUCTION_PIPELINE turn=%d player=%d activeFiniteItems=%d activeStored=%d activeNeeded=%d activeProcesses=%d activeFoodProductionUnits=%d activeFoodProductionUnitStored=%d parkedItems=%d parkedStored=%d parkedNeeded=%d citiesWithParked=%d maxParkedItemsOneCity=%d maxParkedStoredOneCity=%d parkedHalfComplete=%d parkedThreeQuarterComplete=%d parkedUnitItems=%d parkedUnitStored=%d parkedFoodProductionUnitItems=%d parkedFoodProductionUnitStored=%d parkedBuildingItems=%d parkedBuildingStored=%d parkedWonderItems=%d parkedWonderStored=%d parkedProjectItems=%d parkedProjectStored=%d inactivityCounterItems=%d accumulatedInactiveTurnsTotal=%d maxAccumulatedInactiveTurns=%d",
		iGameTurn, ePlayer, iActiveFiniteItems, iActiveStored, iActiveNeeded, iActiveProcesses, iActiveFoodProductionUnits, iActiveFoodProductionUnitStored, iParkedItems, iParkedStored, iParkedNeeded, iCitiesWithParked, iMaxParkedItemsOneCity, iMaxParkedStoredOneCity, iParkedHalfComplete, iParkedThreeQuarterComplete,
		iParkedUnitItems, iParkedUnitStored, iParkedFoodProductionUnitItems, iParkedFoodProductionUnitStored, iParkedBuildingItems, iParkedBuildingStored, iParkedWonderItems, iParkedWonderStored, iParkedProjectItems, iParkedProjectStored, iInactivityCounterItems, iAccumulatedInactiveTurnsTotal, iMaxAccumulatedInactiveTurns);
	if (bLogParkedDetails && !szParked.empty())
		logSASGameRecord("GAME_RECORD_PRODUCTION_PARKED turn=%d player=%d items=%s", iGameTurn, ePlayer, szParked.GetCString());
}

static void logSASGameRecordUnitPosture(PlayerTypes ePlayer, int iGameTurn)
{
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	TeamTypes eTeam = kPlayer.getTeam();
	SASGameRecordPlayerPrevious& kPrevious = g_akSASGameRecordPlayerPrevious[ePlayer];
	// <!-- custom: Promotion-detail work is level 3 only. Cache the immutable detail gate once instead of querying it for every unit and every promotion container. (ChatGPT-5.6-Sol) -->
	bool const bLogPromotionDetails = (gGameRecordLogLevel >= 3);
	int iTotal = 0;
	int iMilitary = 0;
	int iLandMilitary = 0;
	int iSeaMilitary = 0;
	int iAirMilitary = 0;
	int iAttackAir = 0;
	int iDefenseAir = 0;
	int iCarrierAir = 0;
	int iMissileAir = 0;
	int iICBM = 0;
	int iCarrierSea = 0;
	int iMissileCarrierSea = 0;
	int iAirCargo = 0;
	int iCarrierAirCargo = 0;
	int iMissileCargo = 0;
	int iNukes = 0;
	int iBlockadingUnits = 0;
	int iUnitCombatTotal = 0;
	int iWorkers = 0;
	int iSettlers = 0;
	int iRecon = 0;
	int iCityDefenders = 0;
	int iFieldArmy = 0;
	int iOwnTerritory = 0;
	int iEnemyTerritory = 0;
	int iNeutralTerritory = 0;
	int iUnitsInCities = 0;
	int iEnemyUnitsInTerritory = 0;
	int iTotalExperience = 0;
	int iMaxExperience = 0;
	int iPromotionReady = 0;
	int iLevel2Plus = 0;
	int iLevel4Plus = 0;
	int iLevel6Plus = 0;
	// <!-- custom: Military-only quality complements all-unit totals. Keep health in percentX100 (10000 = full health) so averages remain precise without floating-point logging.
	// Promotion-instance scans remain level 3 only. (ChatGPT-5.6-Sol) -->
	int iMilitaryExperience = 0;
	int iMaxMilitaryExperience = 0;
	int iMilitaryLevelTotal = 0;
	int iMaxMilitaryLevel = 0;
	int iMilitaryPromotionReady = 0;
	int iGreatGeneralLedMilitary = 0;
	int iMilitaryXmlProductionCost = 0;
	int iMilitaryProductionNeeded = 0;
	int iMilitaryCostedUnits = 0;
	int iWoundedMilitary = 0;
	int iMilitaryHealthMeasured = 0;
	int iMilitaryHealthX100Total = 0;
	int iMinMilitaryHealthX100 = -1;
	int iMaxMilitaryHealthX100 = -1;
	int iMilitaryHealthFull = 0;
	int iMilitaryHealthHigh = 0;
	int iMilitaryHealthMedium = 0;
	int iMilitaryHealthLow = 0;
	int iPromotionInstances = (bLogPromotionDetails ? 0 : -1);
	int iMilitaryPromotionInstances = (bLogPromotionDetails ? 0 : -1);
	std::vector<int> aiUnitTypes(GC.getNumUnitInfos(), 0);
	std::vector<int> aiUnitAI(NUM_UNITAI_TYPES, 0);
	std::vector<int> aiUnitCombat(GC.getNumUnitCombatInfos(), 0);
	std::vector<int> aiPromotions(bLogPromotionDetails ? GC.getNumPromotionInfos() : 0, 0);
	std::vector<int> aiMilitaryPromotions(bLogPromotionDetails ? GC.getNumPromotionInfos() : 0, 0);
	int iLoop = 0;
	for (CvUnit const* pLoopUnit = kPlayer.firstUnit(&iLoop); pLoopUnit != NULL; pLoopUnit = kPlayer.nextUnit(&iLoop))
	{
		iTotal++;
		if (pLoopUnit->getUnitType() != NO_UNIT)
			aiUnitTypes[pLoopUnit->getUnitType()]++;
		const int iExperience = pLoopUnit->getExperience();
		iTotalExperience += iExperience;
		iMaxExperience = std::max(iMaxExperience, iExperience);
		if (pLoopUnit->isPromotionReady())
			iPromotionReady++;
		if (pLoopUnit->getLevel() >= 2)
			iLevel2Plus++;
		if (pLoopUnit->getLevel() >= 4)
			iLevel4Plus++;
		if (pLoopUnit->getLevel() >= 6)
			iLevel6Plus++;
		CvPlot const* pPlot = pLoopUnit->plot();
		const bool bMilitary = isSASGameRecordMilitaryUnit(*pLoopUnit);
		if (bMilitary)
		{
			iMilitary++;
			iMilitaryExperience += iExperience;
			iMaxMilitaryExperience = std::max(iMaxMilitaryExperience, iExperience);
			iMilitaryLevelTotal += pLoopUnit->getLevel();
			iMaxMilitaryLevel = std::max(iMaxMilitaryLevel, pLoopUnit->getLevel());
			if (pLoopUnit->isPromotionReady()) iMilitaryPromotionReady++;
			if (pLoopUnit->getLeaderUnitType() != NO_UNIT) iGreatGeneralLedMilitary++;
			int const iXmlCost = (pLoopUnit->getUnitType() == NO_UNIT ? -1 : GC.getInfo(pLoopUnit->getUnitType()).getProductionCost());
			if (iXmlCost > 0)
			{
				iMilitaryCostedUnits++;
				iMilitaryXmlProductionCost += iXmlCost;
				iMilitaryProductionNeeded += kPlayer.getProductionNeeded(pLoopUnit->getUnitType());
			}
			if (pLoopUnit->getDamage() > 0) iWoundedMilitary++;
			int const iMaxHP = pLoopUnit->maxHitPoints();
			if (iMaxHP > 0)
			{
				iMilitaryHealthMeasured++;
				int const iHealthX100 = (10000 * pLoopUnit->currHitPoints()) / iMaxHP;
				iMilitaryHealthX100Total += iHealthX100;
				iMinMilitaryHealthX100 = (iMinMilitaryHealthX100 < 0 ? iHealthX100 : std::min(iMinMilitaryHealthX100, iHealthX100));
				iMaxMilitaryHealthX100 = std::max(iMaxMilitaryHealthX100, iHealthX100);
				if (iHealthX100 >= 10000) iMilitaryHealthFull++;
				else if (iHealthX100 > 6600) iMilitaryHealthHigh++;
				else if (iHealthX100 > 3300) iMilitaryHealthMedium++;
				else iMilitaryHealthLow++;
			}
			if (pLoopUnit->getDomainType() == DOMAIN_SEA)
				iSeaMilitary++;
			else if (pLoopUnit->getDomainType() == DOMAIN_AIR)
				iAirMilitary++;
			else iLandMilitary++;
			if (pPlot != NULL && pPlot->isCity() && pLoopUnit->canDefend(pPlot))
				iCityDefenders++;
			else iFieldArmy++;
		}
		UnitAITypes eUnitAI = pLoopUnit->AI_getUnitAIType();
		if (eUnitAI >= 0 && eUnitAI < NUM_UNITAI_TYPES)
		{
			aiUnitAI[eUnitAI]++;
			if (eUnitAI == UNITAI_ATTACK_AIR) iAttackAir++;
			else if (eUnitAI == UNITAI_DEFENSE_AIR) iDefenseAir++;
			else if (eUnitAI == UNITAI_CARRIER_AIR) iCarrierAir++;
			else if (eUnitAI == UNITAI_MISSILE_AIR) iMissileAir++;
			else if (eUnitAI == UNITAI_ICBM) iICBM++;
			else if (eUnitAI == UNITAI_CARRIER_SEA) iCarrierSea++;
			else if (eUnitAI == UNITAI_MISSILE_CARRIER_SEA) iMissileCarrierSea++;
		}
		if (pLoopUnit->getDomainType() == DOMAIN_AIR && pLoopUnit->isCargo())
		{
			iAirCargo++;
			if (eUnitAI == UNITAI_CARRIER_AIR) iCarrierAirCargo++;
			else if (eUnitAI == UNITAI_MISSILE_AIR) iMissileCargo++;
		}
		if (pLoopUnit->isNuke()) iNukes++;
		if (pLoopUnit->isBlockading()) iBlockadingUnits++;
		UnitCombatTypes eUnitCombat = pLoopUnit->getUnitCombatType();
		if (eUnitCombat != NO_UNITCOMBAT)
		{
			aiUnitCombat[eUnitCombat]++;
			iUnitCombatTotal++;
		}
		if (bLogPromotionDetails)
		{
			FOR_EACH_ENUM(Promotion)
			{
				if (pLoopUnit->isHasPromotion(eLoopPromotion))
				{
					aiPromotions[eLoopPromotion]++;
					iPromotionInstances++;
					if (bMilitary)
					{
						aiMilitaryPromotions[eLoopPromotion]++;
						iMilitaryPromotionInstances++;
					}
				}
			}
		}
		if (isSASGameRecordWorkerUnit(*pLoopUnit))
			iWorkers++;
		if (isSASGameRecordSettlerUnit(*pLoopUnit))
			iSettlers++;
		if (eUnitAI == UNITAI_EXPLORE || eUnitAI == UNITAI_EXPLORE_SEA)
			iRecon++;
		if (pPlot != NULL)
		{
			if (pPlot->isCity())
				iUnitsInCities++;
			if (pPlot->getOwner() == ePlayer)
				iOwnTerritory++;
			else if (pPlot->getTeam() != NO_TEAM && GET_TEAM(eTeam).isAtWar(pPlot->getTeam()))
				iEnemyTerritory++;
			else iNeutralTerritory++;
		}
	}
	for (int iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++)
	{
		PlayerTypes eLoopPlayer = (PlayerTypes)iPlayer;
		CvPlayer const& kLoopPlayer = GET_PLAYER(eLoopPlayer);
		if (!kLoopPlayer.isAlive() || kLoopPlayer.getTeam() == eTeam || !GET_TEAM(eTeam).isAtWar(kLoopPlayer.getTeam()))
			continue;
		int iEnemyLoop = 0;
		for (CvUnit const* pLoopUnit = kLoopPlayer.firstUnit(&iEnemyLoop); pLoopUnit != NULL; pLoopUnit = kLoopPlayer.nextUnit(&iEnemyLoop))
		{
			CvPlot const* pPlot = pLoopUnit->plot();
			if (pPlot != NULL && pPlot->getOwner() == ePlayer)
				iEnemyUnitsInTerritory++;
		}
	}
	CvString szUnitTypes;
	CvString szUnitAI;
	CvString szUnitCombat;
	CvString szUnitCombatPercentX100;
	CvString szPromotions;
	CvString szMilitaryPromotions;
	// <!-- custom: UnitAI and combat class are useful but too coarse for game-record review: a Galley and Galleon can share naval transport roles, and a Camel Archer and Dragoon can sit in similar mounted/combat buckets despite very different strength and era impact.
	// Include actual unit-type counts so LLM/autoplay review can see army and navy quality without per-unit spam. (GPT-5.5) -->
	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
		appendSASGameRecordTypeCount(szUnitTypes, getSASGameRecordUnitType((UnitTypes)iI), aiUnitTypes[iI]);
	for (int iI = 0; iI < NUM_UNITAI_TYPES; iI++)
		appendSASGameRecordTypeCount(szUnitAI, getSASGameRecordUnitAIType((UnitAITypes)iI), aiUnitAI[iI]);
	for (int iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		appendSASGameRecordTypeCount(szUnitCombat, getSASGameRecordUnitCombatType((UnitCombatTypes)iI), aiUnitCombat[iI]);
		if (aiUnitCombat[iI] > 0)
			appendSASGameRecordValue(szUnitCombatPercentX100, getSASGameRecordUnitCombatType((UnitCombatTypes)iI), getSASGameRecordPercentX100(aiUnitCombat[iI], iUnitCombatTotal));
	}
	if (bLogPromotionDetails)
	{
		FOR_EACH_ENUM(Promotion)
		{
			appendSASGameRecordTypeCount(szPromotions, getSASGameRecordPromotionType(eLoopPromotion), aiPromotions[eLoopPromotion]);
			appendSASGameRecordTypeCount(szMilitaryPromotions, getSASGameRecordPromotionType(eLoopPromotion), aiMilitaryPromotions[eLoopPromotion]);
		}
	}
	// <!-- custom: Keep late-game air/missile/nuclear posture and current naval-blockade count on the existing unit row rather than adding repetitive snapshot rows.
	// Exact blockade unit/range history remains event-driven. UnitAI-specific counts make carrier filling and missile/nuke inventories directly visible. (GPT-5.6 + ChatGPT-5.6-Sol) -->
	logSASGameRecord("GAME_RECORD_UNIT_POSTURE turn=%d player=%d total=%d military=%d landMilitary=%d seaMilitary=%d airMilitary=%d attackAir=%d defenseAir=%d carrierAir=%d missileAir=%d icbm=%d carrierSea=%d missileCarrierSea=%d airCargo=%d carrierAirCargo=%d missileCargo=%d nukes=%d blockadingUnits=%d workers=%d settlers=%d recon=%d cityDefenders=%d fieldArmy=%d ownTerritory=%d enemyTerritory=%d neutralTerritory=%d unitsInCities=%d enemyUnitsInTerritory=%d totalXP=%d avgXpX100=%d maxXP=%d promotionReady=%d level2Plus=%d level4Plus=%d level6Plus=%d promotionInstances=%d militaryXP=%d avgMilitaryXpX100=%d maxMilitaryXP=%d avgMilitaryLevelX100=%d maxMilitaryLevel=%d militaryPromotionReady=%d greatGeneralLedMilitary=%d militaryCostedUnits=%d militaryXmlProductionCost=%d militaryProductionNeeded=%d woundedMilitary=%d militaryHealthMeasured=%d avgMilitaryHealthX100=%d minMilitaryHealthX100=%d maxMilitaryHealthX100=%d militaryHealthFull=%d militaryHealthHigh=%d militaryHealthMedium=%d militaryHealthLow=%d militaryPromotionInstances=%d",
			iGameTurn, ePlayer, iTotal, iMilitary, iLandMilitary, iSeaMilitary, iAirMilitary, iAttackAir, iDefenseAir, iCarrierAir, iMissileAir, iICBM, iCarrierSea, iMissileCarrierSea, iAirCargo, iCarrierAirCargo, iMissileCargo, iNukes, iBlockadingUnits, iWorkers, iSettlers, iRecon, iCityDefenders, iFieldArmy, iOwnTerritory, iEnemyTerritory, iNeutralTerritory, iUnitsInCities, iEnemyUnitsInTerritory, iTotalExperience, iTotal == 0 ? 0 : (100 * iTotalExperience) / iTotal, iMaxExperience, iPromotionReady, iLevel2Plus, iLevel4Plus, iLevel6Plus, iPromotionInstances,
			iMilitaryExperience, iMilitary == 0 ? 0 : (100 * iMilitaryExperience) / iMilitary, iMaxMilitaryExperience, iMilitary == 0 ? 0 : (100 * iMilitaryLevelTotal) / iMilitary, iMaxMilitaryLevel, iMilitaryPromotionReady, iGreatGeneralLedMilitary, iMilitaryCostedUnits, iMilitaryXmlProductionCost, iMilitaryProductionNeeded, iWoundedMilitary, iMilitaryHealthMeasured, iMilitaryHealthMeasured == 0 ? -1 : iMilitaryHealthX100Total / iMilitaryHealthMeasured, iMinMilitaryHealthX100, iMaxMilitaryHealthX100, iMilitaryHealthFull, iMilitaryHealthHigh, iMilitaryHealthMedium, iMilitaryHealthLow, iMilitaryPromotionInstances);
	logSASGameRecord("GAME_RECORD_UNIT_POSTURE_DELTAS turn=%d player=%d deltaValid=%d totalDelta=%+d militaryDelta=%+d workersDelta=%+d settlersDelta=%+d fieldArmyDelta=%+d cityDefendersDelta=%+d enemyUnitsInTerritoryDelta=%+d totalXPDelta=%+d promotionReadyDelta=%+d",
			iGameTurn, ePlayer, kPrevious.bValid, getSASGameRecordDelta(kPrevious.bValid, iTotal, kPrevious.iUnitTotal), getSASGameRecordDelta(kPrevious.bValid, iMilitary, kPrevious.iUnitMilitary), getSASGameRecordDelta(kPrevious.bValid, iWorkers, kPrevious.iUnitWorkers), getSASGameRecordDelta(kPrevious.bValid, iSettlers, kPrevious.iUnitSettlers), getSASGameRecordDelta(kPrevious.bValid, iFieldArmy, kPrevious.iUnitFieldArmy), getSASGameRecordDelta(kPrevious.bValid, iCityDefenders, kPrevious.iUnitCityDefenders), getSASGameRecordDelta(kPrevious.bValid, iEnemyUnitsInTerritory, kPrevious.iUnitEnemyUnitsInTerritory), getSASGameRecordDelta(kPrevious.bValid, iTotalExperience, kPrevious.iUnitTotalExperience), getSASGameRecordDelta(kPrevious.bValid, iPromotionReady, kPrevious.iUnitPromotionReady));
	kPrevious.iUnitTotal = iTotal;
	kPrevious.iUnitMilitary = iMilitary;
	kPrevious.iUnitWorkers = iWorkers;
	kPrevious.iUnitSettlers = iSettlers;
	kPrevious.iUnitFieldArmy = iFieldArmy;
	kPrevious.iUnitCityDefenders = iCityDefenders;
	kPrevious.iUnitEnemyUnitsInTerritory = iEnemyUnitsInTerritory;
	kPrevious.iUnitTotalExperience = iTotalExperience;
	kPrevious.iUnitPromotionReady = iPromotionReady;
	// <!-- custom: Record UnitCombat shares alongside the raw counts already collected so army mix (e.g. siege-heavy vs. siege-light) is immediately comparable without LLM/manual summing.
	// PercentX100 uses only units with a real UnitCombat as the denominator, excluding Workers, Great People and other non-combat-class units. (GPT-5.6) -->
	logSASGameRecord("GAME_RECORD_UNIT_COMPOSITION turn=%d player=%d unitTypes=%s unitAI=%s unitCombatTotal=%d unitCombat=%s unitCombatPercentX100=%s",
		iGameTurn, ePlayer, getSASDiagnosticOrDash(szUnitTypes).GetCString(), getSASDiagnosticOrDash(szUnitAI).GetCString(), iUnitCombatTotal, getSASDiagnosticOrDash(szUnitCombat).GetCString(), getSASDiagnosticOrDash(szUnitCombatPercentX100).GetCString());
	if (bLogPromotionDetails) logSASGameRecord("GAME_RECORD_UNIT_PROMOTIONS turn=%d player=%d promotions=%s militaryPromotions=%s",
		iGameTurn, ePlayer, getSASDiagnosticOrDash(szPromotions).GetCString(), getSASDiagnosticOrDash(szMilitaryPromotions).GetCString());
}


static void logSASGameRecordWorkers(PlayerTypes ePlayer, int iGameTurn)
{
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	TeamTypes eTeam = kPlayer.getTeam();
	SASGameRecordPlayerPrevious& kPrevious = g_akSASGameRecordPlayerPrevious[ePlayer];
	bool const bLogWorkerDetails = (gGameRecordLogLevel >= 3);
	int iWorkers = 0;
	int iSeaWorkers = 0;
	int iIdle = 0;
	int iBuilding = 0;
	int iBuildingImprovement = 0;
	int iBuildingRoute = 0;
	int iMoving = 0;
	int iWaiting = 0;
	int iOwnTerritory = 0;
	int iEnemyTerritory = 0;
	int iNeutralTerritory = 0;
	int iGuarded = 0;
	int iUnguarded = 0;
	int iThreatened = 0;
	std::vector<int> aiBuilds(GC.getNumBuildInfos(), 0);
	int iLoop = 0;
	for (CvUnit const* pLoopUnit = kPlayer.firstUnit(&iLoop); pLoopUnit != NULL; pLoopUnit = kPlayer.nextUnit(&iLoop))
	{
		if (!isSASGameRecordWorkerUnit(*pLoopUnit))
			continue;
		iWorkers++;
		if (pLoopUnit->AI_getUnitAIType() == UNITAI_WORKER_SEA || pLoopUnit->getDomainType() == DOMAIN_SEA)
			iSeaWorkers++;
		CvPlot const* pPlot = pLoopUnit->plot();
		MissionTypes eMission = getSASGameRecordUnitMissionType(*pLoopUnit);
		BuildTypes eBuild = pLoopUnit->getBuildType();
		if (eBuild != NO_BUILD)
		{
			iBuilding++;
			aiBuilds[eBuild]++;
			if (GC.getInfo(eBuild).getImprovement() != NO_IMPROVEMENT)
				iBuildingImprovement++;
			if (GC.getInfo(eBuild).getRoute() != NO_ROUTE)
				iBuildingRoute++;
		}
		else if (eMission == MISSION_MOVE_TO || eMission == MISSION_ROUTE_TO || eMission == MISSION_MOVE_TO_UNIT)
			iMoving++;
		else if (pLoopUnit->canMove())
			iIdle++;
		else iWaiting++;
		if (pPlot != NULL)
		{
			if (pPlot->getOwner() == ePlayer)
				iOwnTerritory++;
			else if (pPlot->getTeam() != NO_TEAM && GET_TEAM(eTeam).isAtWar(pPlot->getTeam()))
				iEnemyTerritory++;
			else iNeutralTerritory++;
		}
		// <!-- custom: These two checks feed both the level-2 aggregate and level-3 detail row. Compute them once per worker instead of repeating the plot queries for detail logging. (ChatGPT-5.6-Sol) -->
		bool const bGuarded = isSASGameRecordUnitGuarded(*pLoopUnit);
		bool const bThreatened = isSASGameRecordUnitThreatened(*pLoopUnit);
		if (bGuarded)
			iGuarded++;
		else iUnguarded++;
		if (bThreatened)
			iThreatened++;
		if (bLogWorkerDetails && pPlot != NULL)
		{
			logSASGameRecord("GAME_RECORD_WORKER turn=%d player=%d unitId=%d unit=%s unitAI=%s x=%d y=%d mission=%s build=%s buildTurnsLeft=%d plotOwner=%d plotTerrain=%s plotFeature=%s plotBonus=%s plotImprovement=%s plotRoute=%s guarded=%d threatened=%d",
					iGameTurn, ePlayer, pLoopUnit->getID(), getSASGameRecordUnitType(pLoopUnit->getUnitType()), getSASGameRecordUnitAIType(pLoopUnit->AI_getUnitAIType()), pLoopUnit->getX(), pLoopUnit->getY(),
					getSASGameRecordMissionType(eMission), getSASGameRecordBuildType(eBuild), getSASGameRecordBuildTurnsLeft(*pLoopUnit, eBuild), pPlot->getOwner(),
					getSASGameRecordTerrainType(pPlot->getTerrainType()), getSASGameRecordFeatureType(pPlot->getFeatureType()), getSASGameRecordBonusType(pPlot->getBonusType(pLoopUnit->getTeam())),
					getSASGameRecordImprovementType(pPlot->getImprovementType()), getSASGameRecordRouteType(pPlot->getRouteType()), bGuarded, bThreatened);
		}
	}
	CvString szBuilds;
	for (int iI = 0; iI < GC.getNumBuildInfos(); iI++)
		appendSASGameRecordTypeCount(szBuilds, getSASGameRecordBuildType((BuildTypes)iI), aiBuilds[iI]);
	logSASGameRecord("GAME_RECORD_WORKERS turn=%d player=%d workers=%d seaWorkers=%d idle=%d building=%d buildingImprovement=%d buildingRoute=%d moving=%d waiting=%d ownTerritory=%d enemyTerritory=%d neutralTerritory=%d guarded=%d unguarded=%d threatened=%d builds=%s",
			iGameTurn, ePlayer, iWorkers, iSeaWorkers, iIdle, iBuilding, iBuildingImprovement, iBuildingRoute, iMoving, iWaiting, iOwnTerritory, iEnemyTerritory, iNeutralTerritory, iGuarded, iUnguarded, iThreatened, getSASDiagnosticOrDash(szBuilds).GetCString());
	logSASGameRecord("GAME_RECORD_WORKERS_DELTAS turn=%d player=%d deltaValid=%d workersDelta=%+d buildingDelta=%+d idleDelta=%+d movingDelta=%+d waitingDelta=%+d threatenedDelta=%+d",
			iGameTurn, ePlayer, kPrevious.bValid, getSASGameRecordDelta(kPrevious.bValid, iWorkers, kPrevious.iWorkerWorkers), getSASGameRecordDelta(kPrevious.bValid, iBuilding, kPrevious.iWorkerBuilding),
			getSASGameRecordDelta(kPrevious.bValid, iIdle, kPrevious.iWorkerIdle), getSASGameRecordDelta(kPrevious.bValid, iMoving, kPrevious.iWorkerMoving), getSASGameRecordDelta(kPrevious.bValid, iWaiting, kPrevious.iWorkerWaiting), getSASGameRecordDelta(kPrevious.bValid, iThreatened, kPrevious.iWorkerThreatened));
	kPrevious.iWorkerWorkers = iWorkers;
	kPrevious.iWorkerBuilding = iBuilding;
	kPrevious.iWorkerIdle = iIdle;
	kPrevious.iWorkerMoving = iMoving;
	kPrevious.iWorkerWaiting = iWaiting;
	kPrevious.iWorkerThreatened = iThreatened;
}

static void logSASGameRecordSettlers(PlayerTypes ePlayer, int iGameTurn)
{
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	TeamTypes eTeam = kPlayer.getTeam();
	SASGameRecordPlayerPrevious& kPrevious = g_akSASGameRecordPlayerPrevious[ePlayer];
	bool const bLogSettlerDetails = (gGameRecordLogLevel >= 3);
	int iSettlers = 0;
	int iFoundMission = 0;
	int iMoving = 0;
	int iIdle = 0;
	int iWaiting = 0;
	int iOwnTerritory = 0;
	int iEnemyTerritory = 0;
	int iNeutralTerritory = 0;
	int iGuarded = 0;
	int iUnguarded = 0;
	int iThreatened = 0;
	int iLoop = 0;
	for (CvUnit const* pLoopUnit = kPlayer.firstUnit(&iLoop); pLoopUnit != NULL; pLoopUnit = kPlayer.nextUnit(&iLoop))
	{
		if (!isSASGameRecordSettlerUnit(*pLoopUnit))
			continue;
		iSettlers++;
		CvPlot const* pPlot = pLoopUnit->plot();
		MissionTypes eMission = getSASGameRecordUnitMissionType(*pLoopUnit);
		if (eMission == MISSION_FOUND)
			iFoundMission++;
		else if (eMission == MISSION_MOVE_TO || eMission == MISSION_ROUTE_TO || eMission == MISSION_MOVE_TO_UNIT)
			iMoving++;
		else if (pLoopUnit->canMove())
			iIdle++;
		else iWaiting++;
		if (pPlot != NULL)
		{
			if (pPlot->getOwner() == ePlayer)
				iOwnTerritory++;
			else if (pPlot->getTeam() != NO_TEAM && GET_TEAM(eTeam).isAtWar(pPlot->getTeam()))
				iEnemyTerritory++;
			else iNeutralTerritory++;
		}
		bool const bGuarded = isSASGameRecordUnitGuarded(*pLoopUnit);
		bool const bThreatened = isSASGameRecordUnitThreatened(*pLoopUnit);
		if (bGuarded)
			iGuarded++;
		else iUnguarded++;
		if (bThreatened)
			iThreatened++;
		if (bLogSettlerDetails && pPlot != NULL)
		{
			CvCity const* pNearestCity = GC.getMap().findCity(pLoopUnit->getX(), pLoopUnit->getY(), ePlayer, NO_TEAM, false);
			const int iNearestDistance = pNearestCity == NULL ? -1 : plotDistance(pLoopUnit->getX(), pLoopUnit->getY(), pNearestCity->getX(), pNearestCity->getY());
			logSASGameRecord("GAME_RECORD_SETTLER turn=%d player=%d unitId=%d unit=%s unitAI=%s x=%d y=%d mission=%s plotOwner=%d plotTerrain=%s plotFeature=%s plotBonus=%s plotImprovement=%s plotRoute=%s guarded=%d threatened=%d nearestCityId=%d nearestCity=%S nearestCityDistance=%d",
					iGameTurn, ePlayer, pLoopUnit->getID(), getSASGameRecordUnitType(pLoopUnit->getUnitType()), getSASGameRecordUnitAIType(pLoopUnit->AI_getUnitAIType()), pLoopUnit->getX(), pLoopUnit->getY(),
					getSASGameRecordMissionType(eMission), pPlot->getOwner(), getSASGameRecordTerrainType(pPlot->getTerrainType()), getSASGameRecordFeatureType(pPlot->getFeatureType()),
					getSASGameRecordBonusType(pPlot->getBonusType(pLoopUnit->getTeam())), getSASGameRecordImprovementType(pPlot->getImprovementType()), getSASGameRecordRouteType(pPlot->getRouteType()),
					bGuarded, bThreatened, pNearestCity == NULL ? -1 : pNearestCity->getID(), getSASGameRecordQuotedCityName(pNearestCity).GetCString(), iNearestDistance);
		}
	}
	logSASGameRecord("GAME_RECORD_SETTLERS turn=%d player=%d settlers=%d foundMission=%d moving=%d idle=%d waiting=%d ownTerritory=%d enemyTerritory=%d neutralTerritory=%d guarded=%d unguarded=%d threatened=%d",
			iGameTurn, ePlayer, iSettlers, iFoundMission, iMoving, iIdle, iWaiting, iOwnTerritory, iEnemyTerritory, iNeutralTerritory, iGuarded, iUnguarded, iThreatened);
	logSASGameRecord("GAME_RECORD_SETTLERS_DELTAS turn=%d player=%d deltaValid=%d settlersDelta=%+d foundMissionDelta=%+d movingDelta=%+d idleDelta=%+d waitingDelta=%+d threatenedDelta=%+d",
			iGameTurn, ePlayer, kPrevious.bValid, getSASGameRecordDelta(kPrevious.bValid, iSettlers, kPrevious.iSettlerSettlers), getSASGameRecordDelta(kPrevious.bValid, iFoundMission, kPrevious.iSettlerFoundMission),
			getSASGameRecordDelta(kPrevious.bValid, iMoving, kPrevious.iSettlerMoving), getSASGameRecordDelta(kPrevious.bValid, iIdle, kPrevious.iSettlerIdle), getSASGameRecordDelta(kPrevious.bValid, iWaiting, kPrevious.iSettlerWaiting), getSASGameRecordDelta(kPrevious.bValid, iThreatened, kPrevious.iSettlerThreatened));
	kPrevious.iSettlerSettlers = iSettlers;
	kPrevious.iSettlerFoundMission = iFoundMission;
	kPrevious.iSettlerMoving = iMoving;
	kPrevious.iSettlerIdle = iIdle;
	kPrevious.iSettlerWaiting = iWaiting;
	kPrevious.iSettlerThreatened = iThreatened;
}

static CvString getSASGameRecordCitySpecialists(CvCity const& kCity, bool bFree)
{
	CvString szList;
	FOR_EACH_ENUM(Specialist)
	{
		const int iCount = (bFree ? kCity.getFreeSpecialistCount(eLoopSpecialist) : kCity.getSpecialistCount(eLoopSpecialist));
		appendSASGameRecordTypeCount(szList, getSASGameRecordSpecialistType(eLoopSpecialist), iCount);
	}
	return getSASDiagnosticOrDash(szList);
}

static CvString getSASGameRecordCityGPOdds(CvCity const& kCity)
{
	CvString szList;
	std::vector<std::pair<UnitTypes,int> > aeiProjection;
	kCity.GPProjection(aeiProjection);
	for (size_t iI = 0; iI < aeiProjection.size(); iI++)
		appendSASGameRecordTypeCount(szList, getSASGameRecordUnitType(aeiProjection[iI].first), aeiProjection[iI].second);
	return getSASDiagnosticOrDash(szList);
}

static CvString getSASGameRecordCityHappySources(CvCity const& kCity)
{
	CvString szList;
	CvPlayer const& kOwner = GET_PLAYER(kCity.getOwner());
	appendSASGameRecordPositiveValue(szList, "largestCity", std::max(0, kCity.getLargestCityHappiness()));
	appendSASGameRecordPositiveValue(szList, "military", std::max(0, kCity.getMilitaryHappiness()));
	appendSASGameRecordPositiveValue(szList, "stateReligion", std::max(0, kCity.getCurrentStateReligionHappiness()));
	appendSASGameRecordPositiveValue(szList, "building", std::max(0, kCity.getBuildingGoodHappiness()));
	appendSASGameRecordPositiveValue(szList, "extraBuilding", std::max(0, kCity.getExtraBuildingGoodHappiness()));
	appendSASGameRecordPositiveValue(szList, "surrounding", std::max(0, kCity.getSurroundingGoodHappiness()));
	appendSASGameRecordPositiveValue(szList, "bonus", std::max(0, kCity.getBonusGoodHappiness()));
	appendSASGameRecordPositiveValue(szList, "religion", std::max(0, kCity.getReligionGoodHappiness()));
	appendSASGameRecordPositiveValue(szList, "commerce", std::max(0, kCity.getCommerceHappiness()));
	appendSASGameRecordPositiveValue(szList, "areaBuilding", std::max(0, kCity.getArea().getBuildingHappiness(kCity.getOwner())));
	appendSASGameRecordPositiveValue(szList, "playerBuilding", std::max(0, kOwner.getBuildingHappiness()));
	appendSASGameRecordPositiveValue(szList, "extra", std::max(0, kCity.getExtraHappiness() + kOwner.getExtraHappiness()));
	appendSASGameRecordPositiveValue(szList, "handicap", std::max(0, GC.getInfo(kCity.getHandicapType()).getHappyBonus()));
	appendSASGameRecordPositiveValue(szList, "vassal", std::max(0, kCity.getVassalHappiness()));
	appendSASGameRecordPositiveValue(szList, "temporary", kCity.getHappinessTimer() > 0 ? GC.getDefineINT("TEMP_HAPPY") : 0);
	return getSASDiagnosticOrDash(szList);
}

static CvString getSASGameRecordCityFlatUnhappySources(CvCity const& kCity)
{
	CvString szList;
	CvPlayer const& kOwner = GET_PLAYER(kCity.getOwner());
	appendSASGameRecordPositiveValue(szList, "largestCity", -std::min(0, kCity.getLargestCityHappiness()));
	appendSASGameRecordPositiveValue(szList, "military", -std::min(0, kCity.getMilitaryHappiness()));
	appendSASGameRecordPositiveValue(szList, "stateReligion", -std::min(0, kCity.getCurrentStateReligionHappiness()));
	appendSASGameRecordPositiveValue(szList, "building", -std::min(0, kCity.getBuildingBadHappiness()));
	appendSASGameRecordPositiveValue(szList, "extraBuilding", -std::min(0, kCity.getExtraBuildingBadHappiness()));
	appendSASGameRecordPositiveValue(szList, "surrounding", -std::min(0, kCity.getSurroundingBadHappiness()));
	appendSASGameRecordPositiveValue(szList, "bonus", -std::min(0, kCity.getBonusBadHappiness()));
	appendSASGameRecordPositiveValue(szList, "religion", -std::min(0, kCity.getReligionBadHappiness()));
	appendSASGameRecordPositiveValue(szList, "commerce", -std::min(0, kCity.getCommerceHappiness()));
	appendSASGameRecordPositiveValue(szList, "areaBuilding", -std::min(0, kCity.getArea().getBuildingHappiness(kCity.getOwner())));
	appendSASGameRecordPositiveValue(szList, "playerBuilding", -std::min(0, kOwner.getBuildingHappiness()));
	appendSASGameRecordPositiveValue(szList, "extra", -std::min(0, kCity.getExtraHappiness() + kOwner.getExtraHappiness()));
	appendSASGameRecordPositiveValue(szList, "handicap", -std::min(0, GC.getInfo(kCity.getHandicapType()).getHappyBonus()));
	appendSASGameRecordPositiveValue(szList, "vassal", std::max(0, kCity.getVassalUnhappiness()));
	appendSASGameRecordPositiveValue(szList, "espionage", std::max(0, kCity.getEspionageHappinessCounter()));
	return getSASDiagnosticOrDash(szList);
}

static CvString getSASGameRecordCityAngerPercentSources(CvCity const& kCity)
{
	CvString szList;
	CvPlayer const& kOwner = GET_PLAYER(kCity.getOwner());
	int iCivicAnger = 0;
	FOR_EACH_ENUM(Civic)
		iCivicAnger += kOwner.getCivicPercentAnger(eLoopCivic);
	appendSASGameRecordPositiveValue(szList, "overcrowding", kCity.getOvercrowdingPercentAnger());
	appendSASGameRecordPositiveValue(szList, "noMilitary", kCity.getNoMilitaryPercentAnger());
	appendSASGameRecordPositiveValue(szList, "culture", kCity.getCulturePercentAnger());
	appendSASGameRecordPositiveValue(szList, "religion", kCity.getReligionPercentAnger());
	appendSASGameRecordPositiveValue(szList, "hurry", kCity.getHurryPercentAnger());
	appendSASGameRecordPositiveValue(szList, "conscript", kCity.getConscriptPercentAnger());
	appendSASGameRecordPositiveValue(szList, "defyResolution", kCity.getDefyResolutionPercentAnger());
	appendSASGameRecordPositiveValue(szList, "warWeariness", kCity.getWarWearinessPercentAnger());
	appendSASGameRecordPositiveValue(szList, "globalWarming", std::max(0, kOwner.getGwPercentAnger() * 10));
	appendSASGameRecordPositiveValue(szList, "civics", iCivicAnger);
	return getSASDiagnosticOrDash(szList);
}

static CvString getSASGameRecordCityHealthySources(CvCity const& kCity)
{
	CvString szList;
	CvPlayer const& kOwner = GET_PLAYER(kCity.getOwner());
	appendSASGameRecordPositiveValue(szList, "freshWater", std::max(0, kCity.getFreshWaterGoodHealth()));
	appendSASGameRecordPositiveValue(szList, "surrounding", std::max(0, kCity.getSurroundingGoodHealth()));
	appendSASGameRecordPositiveValue(szList, "power", std::max(0, kCity.getPowerGoodHealth()));
	appendSASGameRecordPositiveValue(szList, "bonus", std::max(0, kCity.getBonusGoodHealth()));
	appendSASGameRecordPositiveValue(szList, "building", std::max(0, kCity.totalGoodBuildingHealth()));
	appendSASGameRecordPositiveValue(szList, "extra", std::max(0, kCity.getExtraHealth() + kOwner.getExtraHealth()));
	appendSASGameRecordPositiveValue(szList, "handicap", std::max(0, GC.getInfo(kCity.getHandicapType()).getHealthBonus()));
	return getSASDiagnosticOrDash(szList);
}

static CvString getSASGameRecordCityUnhealthySources(CvCity const& kCity)
{
	CvString szList;
	CvPlayer const& kOwner = GET_PLAYER(kCity.getOwner());
	appendSASGameRecordPositiveValue(szList, "population", kCity.unhealthyPopulation());
	appendSASGameRecordPositiveValue(szList, "espionage", std::max(0, kCity.getEspionageHealthCounter()));
	appendSASGameRecordPositiveValue(szList, "freshWater", -std::min(0, kCity.getFreshWaterBadHealth()));
	appendSASGameRecordPositiveValue(szList, "surrounding", -std::min(0, kCity.getSurroundingBadHealth()));
	appendSASGameRecordPositiveValue(szList, "power", -std::min(0, kCity.getPowerBadHealth()));
	appendSASGameRecordPositiveValue(szList, "bonus", -std::min(0, kCity.getBonusBadHealth()));
	appendSASGameRecordPositiveValue(szList, "building", -std::min(0, kCity.totalBadBuildingHealth()));
	appendSASGameRecordPositiveValue(szList, "extra", -std::min(0, kCity.getExtraHealth() + kOwner.getExtraHealth()));
	appendSASGameRecordPositiveValue(szList, "handicap", -std::min(0, GC.getInfo(kCity.getHandicapType()).getHealthBonus()));
	return getSASDiagnosticOrDash(szList);
}

static const char* getSASGameRecordCityProductionKind(CvCity const& kCity)
{
	if (kCity.getProductionUnit() != NO_UNIT)
		return "UNIT";
	if (kCity.getProductionBuilding() != NO_BUILDING)
		return GC.getInfo(kCity.getProductionBuilding()).isLimited() ? "WONDER" : "BUILDING";
	if (kCity.getProductionProject() != NO_PROJECT)
		return "PROJECT";
	if (kCity.getProductionProcess() != NO_PROCESS)
		return "PROCESS";
	return "-";
}

static const char* getSASGameRecordCityProductionType(CvCity const& kCity)
{
	if (kCity.getProductionUnit() != NO_UNIT)
		return getSASGameRecordUnitType(kCity.getProductionUnit());
	if (kCity.getProductionBuilding() != NO_BUILDING)
		return getSASGameRecordBuildingType(kCity.getProductionBuilding());
	if (kCity.getProductionProject() != NO_PROJECT)
		return getSASGameRecordProjectType(kCity.getProductionProject());
	if (kCity.getProductionProcess() != NO_PROCESS)
		return getSASGameRecordProcessType(kCity.getProductionProcess());
	return "-";
}

// <!-- custom: A PROCESS production name identifies Wealth/Research/Culture but not its actual gain.
// Record the exact production-to-commerce contribution in hundredths, matching CvCity::updateCommerce without rounding away fractional output. (GPT-5.6-Sol) -->
static CvString getSASGameRecordCityProductionConversion(CvCity const& kCity)
{
	CvString szConversion;
	// <!-- custom: CvCity::updateCommerce suppresses both ordinary commerce and production-to-commerce conversion during disorder. Preserve the selected process elsewhere on the city row, but do not report output the city is not receiving. See KI#381. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
	if (kCity.getProductionProcess() == NO_PROCESS || kCity.isDisorder())
		return "-";
	for (int iI = 0; iI < NUM_COMMERCE_TYPES; iI++)
	{
		CommerceTypes const eCommerce = (CommerceTypes)iI;
		int const iRateX100 = kCity.getYieldRate(YIELD_PRODUCTION) * kCity.getProductionToCommerceModifier(eCommerce);
		if (iRateX100 > 0)
			appendSASGameRecordValue(szConversion, getSASGameRecordCommerceType(eCommerce), iRateX100);
	}
	return getSASDiagnosticOrDash(szConversion);
}

// <!-- custom: CvCity uses MAX_INT when no finite production amount or ETA exists, including processes, empty queues and zero production during disorder.
// Emit the GameRecord's ordinary unavailable-value sentinel instead of presenting 2147483647 as a real statistic. See KI#380. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
static int getSASGameRecordCityProductionTurns(CvCity const& kCity)
{
	int const iTurns = kCity.getProductionTurnsLeft();
	return iTurns == MAX_INT ? -1 : iTurns;
}

// <!-- custom: Apply the same unavailable-value contract to production cost because processes and empty queues have no finite amount needed. See KI#380. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
static int getSASGameRecordCityProductionNeeded(CvCity const& kCity)
{
	int const iNeeded = kCity.getProductionNeeded();
	return iNeeded == MAX_INT ? -1 : iNeeded;
}

static CvString getSASGameRecordCityTradePartners(CvCity const& kCity)
{
	CvString szList;
	for (int iI = 0; iI < kCity.getTradeRoutes(); iI++)
	{
		CvCity const* pTradeCity = kCity.getTradeCity(iI);
		if (pTradeCity == NULL)
			continue;
		CvString szItem;
		szItem.Format(szList.empty() ? "%d:%d:%S" : ",%d:%d:%S", pTradeCity->getOwner(), pTradeCity->getID(), pTradeCity->getName().GetCString());
		szList += szItem;
	}
	return szList.empty() ? "-" : getSASDiagnosticQuoted(szList.GetCString());
}

static CvString getSASGameRecordCityReligionList(CvCity const& kCity, bool bHolyOnly)
{
	CvString szResult;
	FOR_EACH_ENUM(Religion)
	{
		if ((bHolyOnly && !kCity.isHolyCity(eLoopReligion)) || (!bHolyOnly && !kCity.isHasReligion(eLoopReligion)))
			continue;
		CvString szItem;
		szItem.Format(szResult.empty() ? "%s" : ",%s", getSASGameRecordReligionType(eLoopReligion));
		szResult += szItem;
	}
	return getSASDiagnosticOrDash(szResult);
}

static CvString getSASGameRecordCityCorporationList(CvCity const& kCity, bool bHeadquartersOnly)
{
	CvString szResult;
	FOR_EACH_ENUM(Corporation)
	{
		if ((bHeadquartersOnly && !kCity.isHeadquarters(eLoopCorporation)) || (!bHeadquartersOnly && !kCity.isHasCorporation(eLoopCorporation)))
			continue;
		CvString szItem;
		szItem.Format(szResult.empty() ? "%s" : ",%s", getSASGameRecordCorporationType(eLoopCorporation));
		szResult += szItem;
	}
	return getSASDiagnosticOrDash(szResult);
}

// <!-- custom: Building-completion actions alone cannot reconstruct buildings inherited through conquest, granted for free, or already present when a log begins. At detail level, snapshot the exact owned buildings and compact regular/national/team/world-wonder totals for each city. (GPT-5.6-Sol) -->
static CvString getSASGameRecordCityBuildings(CvCity const& kCity, int& iTotal, int& iRegular, int& iNationalWonders, int& iTeamWonders, int& iWorldWonders)
{
	CvString szBuildings;
	iTotal = iRegular = iNationalWonders = iTeamWonders = iWorldWonders = 0;
	for (int iI = 0; iI < GC.getNumBuildingInfos(); iI++)
	{
		BuildingTypes const eBuilding = (BuildingTypes)iI;
		int const iCount = kCity.getNumBuilding(eBuilding);
		if (iCount <= 0)
			continue;
		iTotal += iCount;
		CvBuildingInfo const& kBuilding = GC.getInfo(eBuilding);
		if (kBuilding.isWorldWonder())
			iWorldWonders += iCount;
		else if (kBuilding.isTeamWonder())
			iTeamWonders += iCount;
		else if (kBuilding.isNationalWonder())
			iNationalWonders += iCount;
		else iRegular += iCount;
		CvString szItem;
		szItem.Format(szBuildings.empty() ? "%s:%d" : ",%s:%d", getSASGameRecordBuildingType(eBuilding), iCount);
		szBuildings += szItem;
	}
	return getSASDiagnosticOrDash(szBuildings);
}

// <!-- custom: Private level-3-only helper; logSASGameRecordCities owns the single detail-level gate so this function does not repeat it for each city/subrow.
// Consequently the detailed trade-partner row below intentionally has no local `gGameRecordLogLevel >= 3` check; adding it back would only duplicate the caller gate once per city/subrow. (ChatGPT-5.6-Sol) -->
static void logSASGameRecordCityDetail(CvCity const& kCity, int iGameTurn)
{
	CvPlotGroup const* pPlotGroup = kCity.plotGroup(kCity.getOwner());
	int const iTradeRoutes = kCity.getTradeRoutes();
	int iDomesticTradeRoutes = 0;
	int iForeignTradeRoutes = 0;
	for (int iI = 0; iI < iTradeRoutes; iI++)
	{
		CvCity const* pTradeCity = kCity.getTradeCity(iI);
		if (pTradeCity == NULL)
			continue;
		if (pTradeCity->getOwner() == kCity.getOwner())
			iDomesticTradeRoutes++;
		else iForeignTradeRoutes++;
	}
	CvPlayer const& kOwner = GET_PLAYER(kCity.getOwner());
	const SASGameRecordPlotComposition kWorkedPlots = getSASGameRecordWorkedPlotComposition(kCity);
	SASGameRecordCityPlotUnitCounts kCityUnits;
	collectSASGameRecordCityPlotUnitCounts(kCity.getPlot(), kCity.getOwner(), kCityUnits);
	// <!-- custom: Keep the periodic city row self-contained enough to explain growth/starvation and current economic/cultural status without creating more per-turn rows.
	// Stored food/granary state, occupation/culture/maintenance and commerce-type output are cheap current-state getters; religion/corporation lists are small loaded-XML scans already used by city-removal provenance. (ChatGPT-5.6-Sol) -->
	CultureLevelTypes const eCultureLevel = kCity.getCultureLevel();
	PlayerTypes const eHighestCulturePlayer = kCity.findHighestCulture();
	// <!-- custom: City-level commerce output/modifiers make each city's contribution to player-level gold/research/culture/espionage measurable; espionage defense remains a separate defensive modifier. (ChatGPT-5.6-Sol) -->
	// <!-- custom: Air-unit occupancy/capacity on the existing city row makes poor basing or saturated airbases visible without adding a separate late-game row. Cargo aircraft are intentionally excluded by CvPlot::countNumAirUnits, matching actual base-capacity use. (GPT-5.6) -->
	// <!-- custom: City defense snapshots expose both the current post-bombard defense modifier and its undamaged ceiling. DefenseDamage/MAX_CITY_DEFENSE_DAMAGE preserves the underlying bombardment state, while bombarded shows whether the city has already been hit this turn. This lets broad game records be paired with the level-3 tactical bombardment actions below. (GPT-5.6) -->
	logSASGameRecord("GAME_RECORD_CITY turn=%d player=%d cityId=%d city=%S x=%d y=%d originalOwner=%d capital=%d foundedTurn=%d acquiredTurn=%d pop=%d highestPop=%d foodStored=%d foodKept=%d growthThreshold=%d maxFoodKeptPercent=%d avoidGrowth=%d foodSurplus=%d happySurplus=%d healthSurplus=%d food=%d prod=%d commerce=%d maintenanceTimes100=%d maintenanceModifier=%d occupationTurns=%d disorder=%d ownerCultureTimes100=%d cultureLevel=%s cultureLevelId=%d nextCultureThreshold=%d cultureUpdateTurns=%d ownerCulturePercent=%d highestCulturePlayer=%d highestCulturePercent=%d religions=%s holyReligions=%s corporations=%s headquarters=%s goldRate=%d researchRate=%d cultureRate=%d espionageRate=%d goldRateModifier=%d researchRateModifier=%d cultureRateModifier=%d espionageRateModifier=%d espionageDefenseModifier=%d defenseModifier=%d totalDefense=%d defenseDamage=%d defenseDamageMax=%d bombarded=%d airUnits=%d airCapacity=%d airSpaceAvailable=%d worked=%d workedImproved=%d workedUnimproved=%d workedFood=%d workedProd=%d workedCommerce=%d garrison=%d cityUnits=%d militaryUnits=%d civilianUnits=%d defenders=%d healthyDefenders=%d woundedDefenders=%d settlers=%d workers=%d attackers=%d connectedToCapital=%d plotGroupId=%d tradeRoutes=%d domesticTradeRoutes=%d foreignTradeRoutes=%d tradeFood=%d tradeProd=%d tradeCommerce=%d productionKind=%s production=%s productionUsesFood=%d productionTurns=%d productionStored=%d productionNeeded=%d overflowProduction=%d featureProduction=%d productionConversionX100=%s specialists=%s freeSpecialists=%s gpProgress=%d gpThreshold=%d gpRate=%d gpTurnsLeft=%d gpOdds=%s",
			iGameTurn, kCity.getOwner(), kCity.getID(), getSASGameRecordQuotedCityName(&kCity).GetCString(), kCity.getX(), kCity.getY(),
			kCity.getOriginalOwner(), kCity.isCapital(), kCity.getGameTurnFounded(), kCity.getGameTurnAcquired(), kCity.getPopulation(), kCity.getHighestPopulation(),
			kCity.getFood(), kCity.getFoodKept(), kCity.growthThreshold(), kCity.getMaxFoodKeptPercent(), kCity.AI().AI_isEmphasizeAvoidGrowth() ? 1 : 0,
			kCity.foodDifference(), kCity.happyLevel() - kCity.unhappyLevel(), kCity.goodHealth() - kCity.badHealth(),
			kCity.getYieldRate(YIELD_FOOD), kCity.getYieldRate(YIELD_PRODUCTION), kCity.getYieldRate(YIELD_COMMERCE), kCity.getMaintenanceTimes100(), kCity.getMaintenanceModifier(),
			kCity.getOccupationTimer(), kCity.isDisorder() ? 1 : 0, kCity.getCultureTimes100(kCity.getOwner()), eCultureLevel == NO_CULTURELEVEL ? "-" : GC.getInfo(eCultureLevel).getType(), eCultureLevel,
			kCity.getCultureThreshold(), kCity.getCultureUpdateTimer(), kCity.calculateCulturePercent(kCity.getOwner()), eHighestCulturePlayer, eHighestCulturePlayer == NO_PLAYER ? 0 : kCity.calculateCulturePercent(eHighestCulturePlayer),
			getSASGameRecordCityReligionList(kCity, false).GetCString(), getSASGameRecordCityReligionList(kCity, true).GetCString(), getSASGameRecordCityCorporationList(kCity, false).GetCString(), getSASGameRecordCityCorporationList(kCity, true).GetCString(),
			kCity.getCommerceRate(COMMERCE_GOLD), kCity.getCommerceRate(COMMERCE_RESEARCH), kCity.getCommerceRate(COMMERCE_CULTURE), kCity.getCommerceRate(COMMERCE_ESPIONAGE),
			kCity.getTotalCommerceRateModifier(COMMERCE_GOLD), kCity.getTotalCommerceRateModifier(COMMERCE_RESEARCH), kCity.getTotalCommerceRateModifier(COMMERCE_CULTURE), kCity.getTotalCommerceRateModifier(COMMERCE_ESPIONAGE), kCity.getEspionageDefenseModifier(),
			kCity.getDefenseModifier(false), kCity.getTotalDefense(false), kCity.getDefenseDamage(), GC.getMAX_CITY_DEFENSE_DAMAGE(), kCity.isBombarded(),
			kCity.getPlot().countNumAirUnits(kCity.getTeam()), kCity.getAirUnitCapacity(kCity.getTeam()), kCity.getPlot().airUnitSpaceAvailable(kCity.getTeam()),
			kWorkedPlots.iWorked, kWorkedPlots.iWorkedImproved, kWorkedPlots.iWorkedUnimproved, kWorkedPlots.iCurrentFood, kWorkedPlots.iCurrentProduction, kWorkedPlots.iCurrentCommerce, kCity.plot()->getNumDefenders(kCity.getOwner()), kCityUnits.iUnits, kCityUnits.iMilitaryUnits, kCityUnits.iCivilianUnits, kCityUnits.iDefenders, kCityUnits.iHealthyDefenders, kCityUnits.iWoundedDefenders, kCityUnits.iSettlers, kCityUnits.iWorkers, kCityUnits.iAttackers,
			kCity.isConnectedToCapital(), pPlotGroup == NULL ? -1 : pPlotGroup->getID(), iTradeRoutes, iDomesticTradeRoutes, iForeignTradeRoutes, kCity.getTradeYield(YIELD_FOOD), kCity.getTradeYield(YIELD_PRODUCTION), kCity.getTradeYield(YIELD_COMMERCE),
			getSASGameRecordCityProductionKind(kCity), getSASGameRecordCityProductionType(kCity), kCity.isFoodProduction() ? 1 : 0, getSASGameRecordCityProductionTurns(kCity), kCity.getProduction(), getSASGameRecordCityProductionNeeded(kCity), kCity.getOverflowProduction(), kCity.getFeatureProduction(),
			getSASGameRecordCityProductionConversion(kCity).GetCString(), getSASGameRecordCitySpecialists(kCity, false).GetCString(), getSASGameRecordCitySpecialists(kCity, true).GetCString(),
			kCity.getGreatPeopleProgress(), kOwner.greatPeopleThreshold(false), kCity.getGreatPeopleRate(), kCity.GPTurnsLeft(), getSASGameRecordCityGPOdds(kCity).GetCString());
	// <!-- custom: Source lists show the magnitude/origin of temporary happiness effects.
	// Retain their existing turn counters too so snapshots say how long whipping, drafting, defiance, temporary happiness and espionage unhappiness remain without logging per-turn timer decrements. (ChatGPT-5.6-Sol) -->
	logSASGameRecord("GAME_RECORD_CITY_HAPPINESS turn=%d player=%d cityId=%d happy=%d unhappy=%d surplus=%d hurryAngerTurns=%d conscriptAngerTurns=%d defyResolutionAngerTurns=%d temporaryHappinessTurns=%d espionageUnhappinessTurns=%d happySources=%s flatUnhappySources=%s angerPercentSources=%s",
			iGameTurn, kCity.getOwner(), kCity.getID(), kCity.happyLevel(), kCity.unhappyLevel(), kCity.happyLevel() - kCity.unhappyLevel(),
			kCity.getHurryAngerTimer(), kCity.getConscriptAngerTimer(), kCity.getDefyResolutionAngerTimer(), kCity.getHappinessTimer(), kCity.getEspionageHappinessCounter(),
			getSASGameRecordCityHappySources(kCity).GetCString(), getSASGameRecordCityFlatUnhappySources(kCity).GetCString(), getSASGameRecordCityAngerPercentSources(kCity).GetCString());
	// <!-- custom: Espionage unhealth is itself a decrementing duration counter, so preserve its remaining turns next to the existing unhealthy-source magnitude rather than emitting a row whenever the counter ticks down. (ChatGPT-5.6-Sol) -->
	logSASGameRecord("GAME_RECORD_CITY_HEALTH turn=%d player=%d cityId=%d goodHealth=%d badHealth=%d surplus=%d powered=%d dirtyPower=%d areaCleanPower=%d powerGoodHealth=%d powerBadHealth=%d espionageUnhealthTurns=%d healthySources=%s unhealthySources=%s",
			iGameTurn, kCity.getOwner(), kCity.getID(), kCity.goodHealth(), kCity.badHealth(), kCity.goodHealth() - kCity.badHealth(),
			kCity.isPower(), kCity.isDirtyPower(), kCity.isAreaCleanPower(), kCity.getPowerGoodHealth(), kCity.getPowerBadHealth(), kCity.getEspionageHealthCounter(),
			getSASGameRecordCityHealthySources(kCity).GetCString(), getSASGameRecordCityUnhealthySources(kCity).GetCString());
	int iBuildings, iRegularBuildings, iNationalWonders, iTeamWonders, iWorldWonders;
	CvString const szBuildings = getSASGameRecordCityBuildings(kCity, iBuildings, iRegularBuildings, iNationalWonders, iTeamWonders, iWorldWonders);
	logSASGameRecord("GAME_RECORD_CITY_BUILDINGS turn=%d player=%d cityId=%d total=%d regular=%d nationalWonders=%d teamWonders=%d worldWonders=%d buildings=%s",
		iGameTurn, kCity.getOwner(), kCity.getID(), iBuildings, iRegularBuildings, iNationalWonders, iTeamWonders, iWorldWonders, szBuildings.GetCString());
	logSASGameRecord("GAME_RECORD_CITY_TRADE_PARTNERS turn=%d player=%d cityId=%d partners=%s",
		iGameTurn, kCity.getOwner(), kCity.getID(), getSASGameRecordCityTradePartners(kCity).GetCString());
	// <!-- custom: Current AdvCiv-SAS additionally emits GAME_RECORD_CITY_UNIT_COMPOSITION for city garrisons with at least six military units. That row depends on selection-group/MissionAI diagnostics not yet ported here; defer it with those helpers instead of locally reimplementing their state. (ChatGPT-5.6-Sol) -->
}

static void logSASGameRecordWorkedPlots(PlayerTypes ePlayer, int iGameTurn)
{
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	SASGameRecordPlotComposition kComposition;
	int iLoop = 0;
	for (CvCity const* pLoopCity = kPlayer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kPlayer.nextCity(&iLoop))
		addSASGameRecordPlotComposition(kComposition, getSASGameRecordWorkedPlotComposition(*pLoopCity));
	CvString szTerrains, szFeatures, szBonuses, szImprovements, szRoutes;
	getSASGameRecordPlotCompositionTypes(kComposition, szTerrains, szFeatures, szBonuses, szImprovements, szRoutes);
	logSASGameRecord("GAME_RECORD_WORKED_PLOTS turn=%d player=%d cities=%d worked=%d improved=%d unimproved=%d land=%d water=%d hills=%d riverSide=%d freshWater=%d bonusImproved=%d bonusUnimproved=%d currentFood=%d currentProd=%d currentCommerce=%d natureFood=%d natureProd=%d natureCommerce=%d terrains=%s features=%s bonuses=%s improvements=%s routes=%s",
		iGameTurn, ePlayer, kPlayer.getNumCities(), kComposition.iWorked, kComposition.iWorkedImproved, kComposition.iWorkedUnimproved, kComposition.iLand, kComposition.iWater,
		kComposition.iHills, kComposition.iRiverSide, kComposition.iFreshWater, kComposition.iBonusImproved, kComposition.iBonusUnimproved,
		kComposition.iCurrentFood, kComposition.iCurrentProduction, kComposition.iCurrentCommerce, kComposition.iNatureFood, kComposition.iNatureProduction, kComposition.iNatureCommerce,
		getSASDiagnosticOrDash(szTerrains).GetCString(), getSASDiagnosticOrDash(szFeatures).GetCString(), getSASDiagnosticOrDash(szBonuses).GetCString(), getSASDiagnosticOrDash(szImprovements).GetCString(), getSASDiagnosticOrDash(szRoutes).GetCString());
}

static void logSASGameRecordCities(PlayerTypes ePlayer, int iGameTurn)
{
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	SASGameRecordPlayerPrevious& kPrevious = g_akSASGameRecordPlayerPrevious[ePlayer];
	bool const bLogCityDetails = (gGameRecordLogLevel >= 3);
	int iCities = 0, iTotalFoodSurplus = 0, iTotalHappySurplus = 0, iTotalHealthSurplus = 0;
	int iTotalFoodYield = 0, iTotalProductionYield = 0, iTotalCommerceYield = 0, iTotalFoodStored = 0, iTotalFoodKept = 0, iTotalMaintenanceTimes100 = 0;
	int iTotalTradeRoutes = 0, iDomesticTradeRoutes = 0, iForeignTradeRoutes = 0, iTradeFood = 0, iTradeProduction = 0, iTradeCommerce = 0;
	int iConnectedToCapital = 0, iUnhappyCities = 0, iUnhealthyCities = 0, iStarvingCities = 0, iOccupiedCities = 0, iAvoidGrowthCities = 0;
	int iCitiesProducingUnits = 0, iCitiesProducingMilitary = 0, iCitiesProducingWorkers = 0, iCitiesProducingSettlers = 0, iCitiesProducingBuildings = 0, iCitiesProducingWonders = 0, iCitiesProducingProjects = 0, iCitiesProducingProcesses = 0;
	int iSpecialists = 0, iFreeSpecialists = 0, iGarrison = 0, iCityUnits = 0, iMilitaryUnitsInCities = 0, iCivilianUnitsInCities = 0, iDefendersInCities = 0, iSettlersInCities = 0, iWorkersInCities = 0;
	int iBestGPTurns = 1000000;
	CvCity const* pNextGPCity = NULL;
	CvCity const* pCapital = kPlayer.getCapital();
	int iLoop = 0;
	for (CvCity const* pLoopCity = kPlayer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kPlayer.nextCity(&iLoop))
	{
		iCities++;
		int const iFoodSurplus = pLoopCity->foodDifference();
		int const iHappySurplus = pLoopCity->happyLevel() - pLoopCity->unhappyLevel();
		int const iHealthSurplus = pLoopCity->goodHealth() - pLoopCity->badHealth();
		iTotalFoodSurplus += iFoodSurplus; iTotalHappySurplus += iHappySurplus; iTotalHealthSurplus += iHealthSurplus;
		iTotalFoodYield += pLoopCity->getYieldRate(YIELD_FOOD); iTotalProductionYield += pLoopCity->getYieldRate(YIELD_PRODUCTION); iTotalCommerceYield += pLoopCity->getYieldRate(YIELD_COMMERCE);
		iTotalFoodStored += pLoopCity->getFood(); iTotalFoodKept += pLoopCity->getFoodKept(); iTotalMaintenanceTimes100 += pLoopCity->getMaintenanceTimes100();
		int const iCityTradeRoutes = pLoopCity->getTradeRoutes();
		iTotalTradeRoutes += iCityTradeRoutes; iTradeFood += pLoopCity->getTradeYield(YIELD_FOOD); iTradeProduction += pLoopCity->getTradeYield(YIELD_PRODUCTION); iTradeCommerce += pLoopCity->getTradeYield(YIELD_COMMERCE);
		for (int iTrade = 0; iTrade < iCityTradeRoutes; iTrade++)
		{
			CvCity const* pTradeCity = pLoopCity->getTradeCity(iTrade);
			if (pTradeCity == NULL) continue;
			if (pTradeCity->getOwner() == ePlayer) iDomesticTradeRoutes++; else iForeignTradeRoutes++;
		}
		if (pLoopCity->isConnectedToCapital()) iConnectedToCapital++;
		if (iHappySurplus < 0) iUnhappyCities++;
		if (iHealthSurplus < 0) iUnhealthyCities++;
		if (iFoodSurplus < 0) iStarvingCities++;
		if (pLoopCity->isOccupation()) iOccupiedCities++;
		if (pLoopCity->AI().AI_isEmphasizeAvoidGrowth()) iAvoidGrowthCities++;
		iSpecialists += pLoopCity->getSpecialistPopulation();
		iFreeSpecialists += pLoopCity->totalFreeSpecialists();
		iGarrison += pLoopCity->plot()->getNumDefenders(ePlayer);
		SASGameRecordCityPlotUnitCounts kCityUnits;
		collectSASGameRecordCityPlotUnitCounts(pLoopCity->getPlot(), ePlayer, kCityUnits);
		iCityUnits += kCityUnits.iUnits; iMilitaryUnitsInCities += kCityUnits.iMilitaryUnits; iCivilianUnitsInCities += kCityUnits.iCivilianUnits; iDefendersInCities += kCityUnits.iDefenders; iSettlersInCities += kCityUnits.iSettlers; iWorkersInCities += kCityUnits.iWorkers;
		int const iGPTurns = pLoopCity->GPTurnsLeft();
		if (iGPTurns >= 0 && iGPTurns < iBestGPTurns) { iBestGPTurns = iGPTurns; pNextGPCity = pLoopCity; }
		UnitTypes const eProductionUnit = pLoopCity->getProductionUnit();
		BuildingTypes const eProductionBuilding = pLoopCity->getProductionBuilding();
		if (eProductionUnit != NO_UNIT)
		{
			iCitiesProducingUnits++;
			UnitAITypes const eUnitAI = GC.getInfo(eProductionUnit).getDefaultUnitAIType();
			if (GC.getInfo(eProductionUnit).isMilitaryProduction()) iCitiesProducingMilitary++;
			if (eUnitAI == UNITAI_WORKER || eUnitAI == UNITAI_WORKER_SEA) iCitiesProducingWorkers++;
			if (eUnitAI == UNITAI_SETTLE) iCitiesProducingSettlers++;
		}
		else if (eProductionBuilding != NO_BUILDING)
		{
			iCitiesProducingBuildings++;
			if (GC.getInfo(eProductionBuilding).isLimited()) iCitiesProducingWonders++;
		}
		else if (pLoopCity->getProductionProject() != NO_PROJECT) iCitiesProducingProjects++;
		else if (pLoopCity->getProductionProcess() != NO_PROCESS) iCitiesProducingProcesses++;
		if (bLogCityDetails) logSASGameRecordCityDetail(*pLoopCity, iGameTurn);
	}
	logSASGameRecord("GAME_RECORD_CITIES turn=%d player=%d cities=%d capitalId=%d capital=%S connectedToCapital=%d totalFoodSurplus=%d totalHappySurplus=%d totalHealthSurplus=%d totalFood=%d totalProd=%d totalCommerce=%d totalFoodStored=%d totalFoodKept=%d totalMaintenanceTimes100=%d tradeRoutes=%d domesticTradeRoutes=%d foreignTradeRoutes=%d tradeFood=%d tradeProd=%d tradeCommerce=%d unhappyCities=%d unhealthyCities=%d starvingCities=%d occupiedCities=%d avoidGrowthCities=%d specialists=%d freeSpecialists=%d garrison=%d cityUnits=%d militaryUnits=%d civilianUnits=%d defenders=%d settlers=%d workers=%d nextGPCityId=%d nextGPCity=%S nextGPTurns=%d nextGPRate=%d nextGPProgress=%d citiesProducingUnits=%d citiesProducingMilitary=%d citiesProducingWorkers=%d citiesProducingSettlers=%d citiesProducingBuildings=%d citiesProducingWonders=%d citiesProducingProjects=%d citiesProducingProcesses=%d",
		iGameTurn, ePlayer, iCities, pCapital == NULL ? -1 : pCapital->getID(), getSASGameRecordQuotedCityName(pCapital).GetCString(), iConnectedToCapital,
		iTotalFoodSurplus, iTotalHappySurplus, iTotalHealthSurplus, iTotalFoodYield, iTotalProductionYield, iTotalCommerceYield, iTotalFoodStored, iTotalFoodKept, iTotalMaintenanceTimes100,
		iTotalTradeRoutes, iDomesticTradeRoutes, iForeignTradeRoutes, iTradeFood, iTradeProduction, iTradeCommerce, iUnhappyCities, iUnhealthyCities, iStarvingCities, iOccupiedCities, iAvoidGrowthCities, iSpecialists, iFreeSpecialists,
		iGarrison, iCityUnits, iMilitaryUnitsInCities, iCivilianUnitsInCities, iDefendersInCities, iSettlersInCities, iWorkersInCities,
		pNextGPCity == NULL ? -1 : pNextGPCity->getID(), getSASGameRecordQuotedCityName(pNextGPCity).GetCString(), pNextGPCity == NULL ? -1 : iBestGPTurns, pNextGPCity == NULL ? 0 : pNextGPCity->getGreatPeopleRate(), pNextGPCity == NULL ? 0 : pNextGPCity->getGreatPeopleProgress(),
		iCitiesProducingUnits, iCitiesProducingMilitary, iCitiesProducingWorkers, iCitiesProducingSettlers, iCitiesProducingBuildings, iCitiesProducingWonders, iCitiesProducingProjects, iCitiesProducingProcesses);
	logSASGameRecord("GAME_RECORD_CITIES_DELTAS turn=%d player=%d deltaValid=%d citiesDelta=%+d connectedToCapitalDelta=%+d totalFoodSurplusDelta=%+d totalHappySurplusDelta=%+d totalHealthSurplusDelta=%+d totalFoodDelta=%+d totalProdDelta=%+d totalCommerceDelta=%+d tradeRoutesDelta=%+d tradeCommerceDelta=%+d specialistsDelta=%+d freeSpecialistsDelta=%+d garrisonDelta=%+d",
		iGameTurn, ePlayer, kPrevious.bValid,
		getSASGameRecordDelta(kPrevious.bValid, iCities, kPrevious.iCityCount), getSASGameRecordDelta(kPrevious.bValid, iConnectedToCapital, kPrevious.iCityConnectedToCapital), getSASGameRecordDelta(kPrevious.bValid, iTotalFoodSurplus, kPrevious.iCityFoodSurplus),
		getSASGameRecordDelta(kPrevious.bValid, iTotalHappySurplus, kPrevious.iCityHappySurplus), getSASGameRecordDelta(kPrevious.bValid, iTotalHealthSurplus, kPrevious.iCityHealthSurplus), getSASGameRecordDelta(kPrevious.bValid, iTotalFoodYield, kPrevious.iCityFood),
		getSASGameRecordDelta(kPrevious.bValid, iTotalProductionYield, kPrevious.iCityProduction), getSASGameRecordDelta(kPrevious.bValid, iTotalCommerceYield, kPrevious.iCityCommerce), getSASGameRecordDelta(kPrevious.bValid, iTotalTradeRoutes, kPrevious.iCityTradeRoutes),
		getSASGameRecordDelta(kPrevious.bValid, iTradeCommerce, kPrevious.iCityTradeCommerce), getSASGameRecordDelta(kPrevious.bValid, iSpecialists, kPrevious.iCitySpecialists), getSASGameRecordDelta(kPrevious.bValid, iFreeSpecialists, kPrevious.iCityFreeSpecialists), getSASGameRecordDelta(kPrevious.bValid, iGarrison, kPrevious.iCityGarrison));
	kPrevious.iCityCount = iCities;
	kPrevious.iCityConnectedToCapital = iConnectedToCapital;
	kPrevious.iCityFoodSurplus = iTotalFoodSurplus;
	kPrevious.iCityHappySurplus = iTotalHappySurplus;
	kPrevious.iCityHealthSurplus = iTotalHealthSurplus;
	kPrevious.iCityFood = iTotalFoodYield;
	kPrevious.iCityProduction = iTotalProductionYield;
	kPrevious.iCityCommerce = iTotalCommerceYield;
	kPrevious.iCityTradeRoutes = iTotalTradeRoutes;
	kPrevious.iCityTradeCommerce = iTradeCommerce;
	kPrevious.iCitySpecialists = iSpecialists;
	kPrevious.iCityFreeSpecialists = iFreeSpecialists;
	kPrevious.iCityGarrison = iGarrison;
}


// <!-- custom: Ordinary civilization snapshots intentionally omit the Barbarian player because diplomacy, economy and victory-strategy rows do not meaningfully apply. Preserve the strategically useful Barbarian pressure instead through one compact summary, concise city rows, and level-3 unit positions. (GPT-5.6-Sol) -->
static void logSASGameRecordBarbarians(int iGameTurn)
{
	CvPlayer const& kBarbarians = GET_PLAYER(BARBARIAN_PLAYER);
	std::vector<int> aiUnitTypes(GC.getNumUnitInfos(), 0);
	std::vector<int> aiUnitAI(NUM_UNITAI_TYPES, 0);
	std::vector<CvString> aszPositionChunks;
	CvString szPositionChunk;
	bool const bLogPositions = (gGameRecordLogLevel >= 3);
	int iUnits = 0;
	int iAnimals = 0;
	int iLandUnits = 0;
	int iSeaUnits = 0;
	int iCargoUnits = 0;
	int iUnitsInCities = 0;
	int iUnitsInBarbarianTerritory = 0;
	int iUnitsInUnownedTerritory = 0;
	int iUnitsInCivilizationTerritory = 0;
	int iWoundedUnits = 0;
	int iLoop = 0;
	for (CvUnit const* pLoopUnit = kBarbarians.firstUnit(&iLoop); pLoopUnit != NULL; pLoopUnit = kBarbarians.nextUnit(&iLoop))
	{
		iUnits++;
		if (pLoopUnit->isAnimal()) iAnimals++;
		if (pLoopUnit->getDomainType() == DOMAIN_LAND) iLandUnits++;
		else if (pLoopUnit->getDomainType() == DOMAIN_SEA) iSeaUnits++;
		if (pLoopUnit->isCargo()) iCargoUnits++;
		if (pLoopUnit->getDamage() > 0) iWoundedUnits++;
		CvPlot const* pPlot = pLoopUnit->plot();
		if (pPlot != NULL)
		{
			if (pPlot->isCity()) iUnitsInCities++;
			if (pPlot->getOwner() == BARBARIAN_PLAYER) iUnitsInBarbarianTerritory++;
			else if (!pPlot->isOwned()) iUnitsInUnownedTerritory++;
			else iUnitsInCivilizationTerritory++;
		}
		if (pLoopUnit->getUnitType() != NO_UNIT) aiUnitTypes[pLoopUnit->getUnitType()]++;
		UnitAITypes const eUnitAI = pLoopUnit->AI_getUnitAIType();
		if (eUnitAI >= 0 && eUnitAI < NUM_UNITAI_TYPES) aiUnitAI[eUnitAI]++;
		if (bLogPositions && pPlot != NULL)
		{
			CvString szItem;
			szItem.Format("%s%s:%d:%s@(%d,%d)", szPositionChunk.empty() ? "" : ",", getSASGameRecordUnitType(pLoopUnit->getUnitType()), pLoopUnit->getID(), getSASGameRecordUnitAIType(eUnitAI), pPlot->getX(), pPlot->getY());
			if (!szPositionChunk.empty() && szPositionChunk.length() + szItem.length() > 1500)
			{
				aszPositionChunks.push_back(szPositionChunk);
				szPositionChunk.clear();
				szItem.Format("%s:%d:%s@(%d,%d)", getSASGameRecordUnitType(pLoopUnit->getUnitType()), pLoopUnit->getID(), getSASGameRecordUnitAIType(eUnitAI), pPlot->getX(), pPlot->getY());
			}
			szPositionChunk += szItem;
		}
	}
	if (!szPositionChunk.empty()) aszPositionChunks.push_back(szPositionChunk);
	CvString szUnitTypes;
	CvString szUnitAI;
	for (int iI = 0; iI < GC.getNumUnitInfos(); iI++) appendSASGameRecordTypeCount(szUnitTypes, getSASGameRecordUnitType((UnitTypes)iI), aiUnitTypes[iI]);
	for (int iI = 0; iI < NUM_UNITAI_TYPES; iI++) appendSASGameRecordTypeCount(szUnitAI, getSASGameRecordUnitAIType((UnitAITypes)iI), aiUnitAI[iI]);
	// <!-- custom: Barbarian research advances many technologies concurrently instead of selecting one current target.
	// Preserve only incomplete technologies with nonzero stored progress in the existing periodic row.
	// Completed technologies remain exact TECH_ACQUIRED source=BARBARIAN_RESEARCH actions. (ChatGPT-5.6-Sol) -->
	CvTeam const& kBarbarianTeam = GET_TEAM(kBarbarians.getTeam());
	CvString szPartialResearch;
	int iPartialResearchTechs = 0;
	FOR_EACH_ENUM(Tech)
	{
		if (kBarbarianTeam.isHasTech(eLoopTech))
			continue;
		int const iProgress = kBarbarianTeam.getResearchProgress(eLoopTech);
		if (iProgress <= 0)
			continue;
		CvString szItem;
		szItem.Format("%s%s:%d/%d", szPartialResearch.empty() ? "" : ",", getSASGameRecordTechType(eLoopTech), iProgress, kBarbarianTeam.getResearchCost(eLoopTech));
		szPartialResearch += szItem;
		iPartialResearchTechs++;
	}
	logSASGameRecord("GAME_RECORD_BARBARIAN_SUMMARY turn=%d cities=%d population=%d units=%d animals=%d nonAnimals=%d landUnits=%d seaUnits=%d cargoUnits=%d unitsInCities=%d unitsInBarbarianTerritory=%d unitsInUnownedTerritory=%d unitsInCivilizationTerritory=%d woundedUnits=%d unitTypes=%s unitAI=%s partialResearchTechs=%d partialResearch=%s",
			iGameTurn, kBarbarians.getNumCities(), kBarbarians.getTotalPopulation(), iUnits, iAnimals, iUnits - iAnimals, iLandUnits, iSeaUnits, iCargoUnits,
			iUnitsInCities, iUnitsInBarbarianTerritory, iUnitsInUnownedTerritory, iUnitsInCivilizationTerritory, iWoundedUnits,
			getSASDiagnosticOrDash(szUnitTypes).GetCString(), getSASDiagnosticOrDash(szUnitAI).GetCString(), iPartialResearchTechs, getSASDiagnosticOrDash(szPartialResearch).GetCString());
	int iCityLoop = 0;
	for (CvCity const* pLoopCity = kBarbarians.firstCity(&iCityLoop); pLoopCity != NULL; pLoopCity = kBarbarians.nextCity(&iCityLoop))
	{
		// <!-- custom: The incremental AdvCiv 1.14 port currently carries the reduced city-only plot-unit counter rather than mature SAS's broader settler/action helper. These Barbarian city fields need only the shared city subset, so reuse it until the later event-state helper is ported. (ChatGPT-5.6-Sol) -->
		SASGameRecordCityPlotUnitCounts kCityUnits;
		collectSASGameRecordCityPlotUnitCounts(pLoopCity->getPlot(), BARBARIAN_PLAYER, kCityUnits);
		SASGameRecordPlotComposition const kWorkedPlots = getSASGameRecordWorkedPlotComposition(*pLoopCity);
		logSASGameRecord("GAME_RECORD_BARBARIAN_CITY turn=%d cityId=%d city=%S x=%d y=%d area=%d foundedTurn=%d age=%d pop=%d foodSurplus=%d prod=%d commerce=%d defenseModifier=%d totalDefense=%d cityUnits=%d defenders=%d healthyDefenders=%d woundedDefenders=%d workers=%d attackers=%d worked=%d workedImproved=%d workedUnimproved=%d productionKind=%s production=%s productionTurns=%d",
				iGameTurn, pLoopCity->getID(), getSASGameRecordQuotedCityName(pLoopCity).GetCString(), pLoopCity->getX(), pLoopCity->getY(), pLoopCity->getArea().getID(), pLoopCity->getGameTurnFounded(), iGameTurn - pLoopCity->getGameTurnFounded(),
				pLoopCity->getPopulation(), pLoopCity->foodDifference(), pLoopCity->getYieldRate(YIELD_PRODUCTION), pLoopCity->getYieldRate(YIELD_COMMERCE), pLoopCity->getDefenseModifier(false), pLoopCity->getTotalDefense(false),
				kCityUnits.iUnits, kCityUnits.iDefenders, kCityUnits.iHealthyDefenders, kCityUnits.iWoundedDefenders, kCityUnits.iWorkers, kCityUnits.iAttackers, kWorkedPlots.iWorked, kWorkedPlots.iWorkedImproved, kWorkedPlots.iWorkedUnimproved,
				getSASGameRecordCityProductionKind(*pLoopCity), getSASGameRecordCityProductionType(*pLoopCity), getSASGameRecordCityProductionTurns(*pLoopCity));
	}
	for (size_t iI = 0; iI < aszPositionChunks.size(); iI++)
		logSASGameRecord("GAME_RECORD_BARBARIAN_POSITIONS turn=%d part=%d parts=%d units=%s", iGameTurn, (int)iI + 1, (int)aszPositionChunks.size(), aszPositionChunks[iI].GetCString());
}

static void logSASGameRecordPlayerSnapshot(PlayerTypes ePlayer, int iGameTurn)
{
	CvGame const& kGame = GC.getGame();
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	CvTeam const& kTeam = GET_TEAM(kPlayer.getTeam());
	bool const bLogPlayerDetails = (getSASGameRecordLogLevel() >= 2);
	bool const bLogPlayerVerboseDetails = (getSASGameRecordLogLevel() >= 3);
	TechTypes const eResearch = kPlayer.getCurrentResearch();
	int const iScore = kPlayer.calculateScore();
	int const iCities = kPlayer.getNumCities();
	int const iPopulation = kPlayer.getTotalPopulation();
	int const iLand = kPlayer.getTotalLand();
	int const iUnits = kPlayer.getNumUnits();
	int const iMilitarySupportUnits = kPlayer.getNumMilitaryUnits();
	// <!-- custom: CvPlayer::getNumMilitaryUnits counts XML bMilitarySupport, which can fall sharply when an army upgrades into combat units that intentionally do not pay military support. Count actual combat-capable units with the same predicate used by GAME_RECORD_UNIT_POSTURE, and keep the raw Civ4 counter separately. This scan runs only when a GameRecord player snapshot is already being generated. (ChatGPT-5.6-Sol) -->
	int iCombatUnits = 0;
	int iCombatLoop = 0;
	for (CvUnit const* pLoopUnit = kPlayer.firstUnit(&iCombatLoop); pLoopUnit != NULL; pLoopUnit = kPlayer.nextUnit(&iCombatLoop))
	{
		if (isSASGameRecordMilitaryUnit(*pLoopUnit)) ++iCombatUnits;
	}
	int const iPower = kPlayer.getPower();
	int const iGold = kPlayer.getGold();
	int const iGoldRate = kPlayer.calculateGoldRate();
	// <!-- custom: Keep nominal science visible when no target is selected because that science becomes stored research overflow rather than disappearing. (GPT-5.6-Sol) -->
	int const iResearchRate = kPlayer.calculateResearchRate(eResearch);
	int const iResearchTurns = (eResearch == NO_TECH ? -1 : kPlayer.getResearchTurnsLeft(eResearch, true));
	int const iHistoryScore = kPlayer.getHistorySafe(PLAYER_HISTORY_SCORE, iGameTurn);
	int const iHistoryEconomy = kPlayer.getHistorySafe(PLAYER_HISTORY_ECONOMY, iGameTurn);
	int const iHistoryIndustry = kPlayer.getHistorySafe(PLAYER_HISTORY_INDUSTRY, iGameTurn);
	int const iHistoryAgriculture = kPlayer.getHistorySafe(PLAYER_HISTORY_AGRICULTURE, iGameTurn);
	int const iHistoryPower = kPlayer.getHistorySafe(PLAYER_HISTORY_POWER, iGameTurn);
	int const iHistoryCulture = kPlayer.getHistorySafe(PLAYER_HISTORY_CULTURE, iGameTurn);
	int const iHistoryEspionage = kPlayer.getHistorySafe(PLAYER_HISTORY_ESPIONAGE, iGameTurn);
	SASGameRecordPlayerPrevious& kPrevious = g_akSASGameRecordPlayerPrevious[ePlayer];
	char const* szCiv = (kPlayer.getCivilizationType() == NO_CIVILIZATION ? "-" : GC.getInfo(kPlayer.getCivilizationType()).getType());
	char const* szLeader = (kPlayer.getLeaderType() == NO_LEADER ? "-" : GC.getInfo(kPlayer.getLeaderType()).getType());
	bool const bCurrentlyHumanControlled = kPlayer.isHuman();
	bool const bAutoplayControlled = kPlayer.isHumanDisabled();
	bool const bHumanSlot = (bCurrentlyHumanControlled || bAutoplayControlled);
	// <!-- custom: Current AdvCiv-SAS also exposes recorder-observed golden-age/anarchy lifetime counters in this row. Their action hooks have not been ported yet, so this slice records authoritative current timers only rather than emitting misleading zero-valued session counters. (ChatGPT-5.6-Sol) -->
	logSASGameRecord("GAME_RECORD_PLAYER turn=%d player=%d team=%d civ=%s leader=%s isHuman=%d humanSlot=%d currentlyHumanControlled=%d autoplayControlled=%d rank=%d deltaValid=%d score=%d scoreDelta=%+d cities=%d citiesDelta=%+d pop=%d popDelta=%+d land=%d landDelta=%+d units=%d unitsDelta=%+d combatUnits=%d combatUnitsDelta=%+d militarySupportUnits=%d militarySupportUnitsDelta=%+d power=%d powerDelta=%+d gold=%d goldDelta=%+d gpt=%d gptDelta=%+d researchRate=%d researchRateDelta=%+d researchPercent=%d currentResearch=%s researchOverflow=%d noResearchAvailable=%d researchTurns=%d era=%s stateReligion=%s techScorePercent=%d combatXP=%d greatPeopleCreated=%d greatGeneralsCreated=%d greatGeneralThreshold=%d goldenAgeTurns=%d anarchyTurns=%d revolutionTimer=%d conversionTimer=%d wars=%s",
			iGameTurn, ePlayer, kPlayer.getTeam(), szCiv, szLeader, bCurrentlyHumanControlled, bHumanSlot, bCurrentlyHumanControlled, bAutoplayControlled, kGame.getPlayerRank(ePlayer) + 1, kPrevious.bValid,
			iScore, getSASGameRecordDelta(kPrevious.bValid, iScore, kPrevious.iScore), iCities, getSASGameRecordDelta(kPrevious.bValid, iCities, kPrevious.iCities), iPopulation, getSASGameRecordDelta(kPrevious.bValid, iPopulation, kPrevious.iPopulation), iLand, getSASGameRecordDelta(kPrevious.bValid, iLand, kPrevious.iLand),
			iUnits, getSASGameRecordDelta(kPrevious.bValid, iUnits, kPrevious.iUnits), iCombatUnits, getSASGameRecordDelta(kPrevious.bValid, iCombatUnits, kPrevious.iCombatUnits), iMilitarySupportUnits, getSASGameRecordDelta(kPrevious.bValid, iMilitarySupportUnits, kPrevious.iMilitarySupportUnits), iPower, getSASGameRecordDelta(kPrevious.bValid, iPower, kPrevious.iPower), iGold, getSASGameRecordDelta(kPrevious.bValid, iGold, kPrevious.iGold), iGoldRate, getSASGameRecordDelta(kPrevious.bValid, iGoldRate, kPrevious.iGoldRate),
			iResearchRate, getSASGameRecordDelta(kPrevious.bValid, iResearchRate, kPrevious.iResearchRate), kPlayer.getCommercePercent(COMMERCE_RESEARCH), getSASGameRecordTechType(eResearch), kPlayer.getOverflowResearch(), kPlayer.isNoResearchAvailable(), iResearchTurns, getSASGameRecordEraType(kPlayer.getCurrentEra()), getSASGameRecordReligionType(kPlayer.getStateReligion()), kTeam.getBestKnownTechScorePercent(), kPlayer.getCombatExperience(), kPlayer.getGreatPeopleCreated(), kPlayer.getGreatGeneralsCreated(), kPlayer.greatPeopleThreshold(true), kPlayer.getGoldenAgeTurns(), kPlayer.getAnarchyTurns(), kPlayer.getRevolutionTimer(), kPlayer.getConversionTimer(), getSASGameRecordWarTeams(kPlayer.getTeam()).GetCString());
	logSASGameRecord("GAME_RECORD_PLAYER_HISTORY turn=%d player=%d deltaValid=%d historyScore=%d historyScoreDelta=%+d historyEconomy=%d historyEconomyDelta=%+d historyIndustry=%d historyIndustryDelta=%+d historyAgriculture=%d historyAgricultureDelta=%+d historyPower=%d historyPowerDelta=%+d historyCulture=%d historyCultureDelta=%+d historyEspionage=%d historyEspionageDelta=%+d",
			iGameTurn, ePlayer, kPrevious.bValid, iHistoryScore, getSASGameRecordDelta(kPrevious.bValid, iHistoryScore, kPrevious.iHistoryScore), iHistoryEconomy, getSASGameRecordDelta(kPrevious.bValid, iHistoryEconomy, kPrevious.iHistoryEconomy), iHistoryIndustry, getSASGameRecordDelta(kPrevious.bValid, iHistoryIndustry, kPrevious.iHistoryIndustry), iHistoryAgriculture, getSASGameRecordDelta(kPrevious.bValid, iHistoryAgriculture, kPrevious.iHistoryAgriculture), iHistoryPower, getSASGameRecordDelta(kPrevious.bValid, iHistoryPower, kPrevious.iHistoryPower), iHistoryCulture, getSASGameRecordDelta(kPrevious.bValid, iHistoryCulture, kPrevious.iHistoryCulture), iHistoryEspionage, getSASGameRecordDelta(kPrevious.bValid, iHistoryEspionage, kPrevious.iHistoryEspionage));
	if (bLogPlayerDetails)
	{
		logSASGameRecordPlayerBonuses(ePlayer, iGameTurn, kPrevious);
		logSASGameRecordAIVictoryStages(ePlayer, iGameTurn);
		logSASGameRecordAIMilitaryProduction(ePlayer, iGameTurn);
		logSASGameRecordPolicies(ePlayer, iGameTurn);
		logSASGameRecordEconomy(ePlayer, iGameTurn);
		logSASGameRecordProductionPipeline(ePlayer, iGameTurn);
		// <!-- custom: Mature AdvCiv-SAS logs recorder-session statistics here too; those counters depend on action hooks not yet ported, so keep that separate until their observations are real rather than permanently zero. (ChatGPT-5.6-Sol) -->
		logSASGameRecordEspionage(ePlayer, iGameTurn);
		logSASGameRecordDemographics(ePlayer, iGameTurn);
		logSASGameRecordAttitudes(ePlayer, iGameTurn);
		if (bLogPlayerVerboseDetails) logSASGameRecordDiplomaticMemories(ePlayer, iGameTurn);
		logSASGameRecordDiploStatus(ePlayer, iGameTurn);
		logSASGameRecordUnitPosture(ePlayer, iGameTurn);
		logSASGameRecordWorkers(ePlayer, iGameTurn);
		// <!-- custom: Mature AdvCiv-SAS logs the broader expansion/territory-development snapshot between Workers and Settlers; keep that larger map-scan slice separate so this commit isolates mobile civilian-unit state and mission diagnostics. (ChatGPT-5.6-Sol) -->
		logSASGameRecordSettlers(ePlayer, iGameTurn);
		logSASGameRecordCities(ePlayer, iGameTurn);
		logSASGameRecordWorkedPlots(ePlayer, iGameTurn);
	}
	kPrevious.bValid = true;
	kPrevious.iScore = iScore;
	kPrevious.iCities = iCities;
	kPrevious.iPopulation = iPopulation;
	kPrevious.iLand = iLand;
	kPrevious.iUnits = iUnits;
	kPrevious.iCombatUnits = iCombatUnits;
	kPrevious.iMilitarySupportUnits = iMilitarySupportUnits;
	kPrevious.iPower = iPower;
	kPrevious.iGold = iGold;
	kPrevious.iGoldRate = iGoldRate;
	kPrevious.iResearchRate = iResearchRate;
	if (bLogPlayerDetails)
	{
		int iBonusTypes = 0;
		int iBonusInstances = 0;
		int iBonusImports = 0;
		int iBonusExports = 0;
		FOR_EACH_ENUM(Bonus)
		{
			const int iAvailable = kPlayer.getNumAvailableBonuses(eLoopBonus);
			if (iAvailable > 0)
			{
				iBonusTypes++;
				iBonusInstances += iAvailable;
			}
			iBonusImports += kPlayer.getBonusImport(eLoopBonus);
			iBonusExports += kPlayer.getBonusExport(eLoopBonus);
		}
		kPrevious.iBonusTypes = iBonusTypes;
		kPrevious.iBonusInstances = iBonusInstances;
		kPrevious.iBonusImports = iBonusImports;
		kPrevious.iBonusExports = iBonusExports;
	}
	kPrevious.iHistoryScore = iHistoryScore;
	kPrevious.iHistoryEconomy = iHistoryEconomy;
	kPrevious.iHistoryIndustry = iHistoryIndustry;
	kPrevious.iHistoryAgriculture = iHistoryAgriculture;
	kPrevious.iHistoryPower = iHistoryPower;
	kPrevious.iHistoryCulture = iHistoryCulture;
	kPrevious.iHistoryEspionage = iHistoryEspionage;
}

static void logSASGameRecordPlayerSetup(PlayerTypes ePlayer)
{
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	CvInitCore const& kInitCore = GC.getInitCore();
	const char* szCivType = (kPlayer.getCivilizationType() == NO_CIVILIZATION ? "-" : GC.getInfo(kPlayer.getCivilizationType()).getType());
	const char* szLeaderType = (kPlayer.getLeaderType() == NO_LEADER ? "-" : GC.getInfo(kPlayer.getLeaderType()).getType());
	const wchar* szLeaderName = (kPlayer.getLeaderType() == NO_LEADER ? L"-" : GC.getInfo(kPlayer.getLeaderType()).getDescription());
	// <!-- custom: During AI Auto Play, isHuman becomes false for the original human slot while isHumanDisabled becomes true. Record both states explicitly so setup/load rows do not make the same player appear ambiguously human in one place and AI-controlled in another. (GPT-5.6-Sol) -->
	const bool bCurrentlyHumanControlled = kPlayer.isHuman();
	const bool bAutoplayControlled = kPlayer.isHumanDisabled();
	const bool bHumanSlot = (bCurrentlyHumanControlled || bAutoplayControlled);
	PlayerColorTypes const ePlayerColor = kPlayer.getPlayerColor();
	char const* szPlayerColor = "-";
	char const* szPrimaryColor = "-";
	int iPrimaryRed = -1;
	int iPrimaryGreen = -1;
	int iPrimaryBlue = -1;
	if (ePlayerColor != NO_PLAYERCOLOR)
	{
		CvPlayerColorInfo const& kPlayerColor = GC.getInfo(ePlayerColor);
		ColorTypes const ePrimaryColor = kPlayerColor.getColorTypePrimary();
		szPlayerColor = kPlayerColor.getType();
		if (ePrimaryColor != NO_COLOR)
		{
			NiColorA const& kPrimaryColor = GC.getInfo(ePrimaryColor).getColor();
			szPrimaryColor = GC.getInfo(ePrimaryColor).getType();
			iPrimaryRed = (int)(255 * kPrimaryColor.r);
			iPrimaryGreen = (int)(255 * kPrimaryColor.g);
			iPrimaryBlue = (int)(255 * kPrimaryColor.b);
		}
	}
	CvString szTraits;
	FOR_EACH_ENUM(Trait)
	{
		if (!kPlayer.hasTrait(eLoopTrait))
			continue;
		if (!szTraits.empty())
			szTraits += ",";
		szTraits += GC.getInfo(eLoopTrait).getType();
	}
	// <!-- custom: Leader traits and favorites are fixed but materially explain AI behavior and economic results.
	// Record them once per setup/load rather than repeating them in periodic player or policy snapshots. (GPT-5.6-Sol) -->
	// <!-- custom: Log the assigned PlayerColor rather than the civilization default because Civ4 can reassign duplicates.
	// The primary ColorInfo and RGB values help connect text records to maps and screenshots without requiring the source XML. (GPT-5.6-Sol) -->
	// <!-- custom: CvInitCore preserves whether civilization and leader were assigned through Random.
	// Older/imported saves can lack that provenance, so keep unknown distinct from a verified manual choice. (ChatGPT-5.6-Sol) -->
	bool const bCivLeaderChoiceKnown = kInitCore.isCivLeaderSetupKnown();
	logSASGameRecord("GAME_RECORD_PLAYER_SETUP turn=%d player=%d team=%d alive=%d everAlive=%d human=%d humanSlot=%d currentlyHumanControlled=%d autoplayControlled=%d slotStatus=%d civLeaderChoiceKnown=%d civChosenRandomly=%d leaderChosenRandomly=%d playerName=%S civType=%s civName=%S civShortName=%S leaderType=%s leaderName=%S playerColor=%s primaryColor=%s primaryColorRGB=%d,%d,%d traits=%s favoriteCivic=%s favoriteReligion=%s handicap=%s",
			GC.getGame().getGameTurn(), ePlayer, kPlayer.getTeam(), kPlayer.isAlive(), kPlayer.isEverAlive(), bCurrentlyHumanControlled, bHumanSlot, bCurrentlyHumanControlled, bAutoplayControlled, kInitCore.getSlotStatus(ePlayer), bCivLeaderChoiceKnown, bCivLeaderChoiceKnown ? kInitCore.wasCivRandomlyChosen(ePlayer) : -1, bCivLeaderChoiceKnown ? kInitCore.wasLeaderRandomlyChosen(ePlayer) : -1,
			getSASDiagnosticQuoted(kPlayer.getName(0)).GetCString(), szCivType, getSASDiagnosticQuoted(kPlayer.getCivilizationDescription(0)).GetCString(), getSASDiagnosticQuoted(kPlayer.getCivilizationShortDescription(0)).GetCString(), szLeaderType, getSASDiagnosticQuoted(szLeaderName).GetCString(),
			szPlayerColor, szPrimaryColor, iPrimaryRed, iPrimaryGreen, iPrimaryBlue, getSASDiagnosticOrDash(szTraits).GetCString(), getSASGameRecordCivicType(kPlayer.getFavoriteCivic()), getSASGameRecordReligionType(kPlayer.getFavoriteReligion()), kPlayer.getHandicapType() == NO_HANDICAP ? "-" : GC.getInfo(kPlayer.getHandicapType()).getType());
}

static void logSASGameRecordAttitudeLegend()
{
	const int iFuriousMax = GC.getDefineINT(CvGlobals::RELATIONS_THRESH_FURIOUS);
	const int iAnnoyedMax = GC.getDefineINT(CvGlobals::RELATIONS_THRESH_ANNOYED);
	const int iPleasedMin = GC.getDefineINT(CvGlobals::RELATIONS_THRESH_PLEASED);
	const int iFriendlyMin = GC.getDefineINT(CvGlobals::RELATIONS_THRESH_FRIENDLY);
	logSASGameRecord("GAME_RECORD_ATTITUDE_LEGEND valueFrom=AI_getAttitudeVal furious=<=%d annoyed=%d..%d cautious=%d..%d pleased=%d..%d friendly=>=%d",
			iFuriousMax, iFuriousMax + 1, iAnnoyedMax, iAnnoyedMax + 1, iPleasedMin - 1, iPleasedMin, iFriendlyMin - 1, iFriendlyMin);
}

// <!-- custom: Team-state rows identify numeric members exactly, but placing readable player/civilization identities only after hundreds of geography and text-map rows made the initial team and technology records needlessly hard to interpret.
// Emit fixed slot bounds and player identities before team relations; later map legends can still reference the same PLAYER_SETUP rows without repeating them. (GPT-5.6-Sol) -->
static void logSASGameRecordInitialPlayerIdentities()
{
	logSASGameRecord("GAME_RECORD_SLOT_CONSTANTS MAX_CIV_PLAYERS=%d MAX_PLAYERS=%d BARBARIAN_PLAYER=%d MAX_CIV_TEAMS=%d MAX_TEAMS=%d BARBARIAN_TEAM=%d NO_PLAYER=%d NO_TEAM=%d", MAX_CIV_PLAYERS, MAX_PLAYERS, BARBARIAN_PLAYER, MAX_CIV_TEAMS, MAX_TEAMS, BARBARIAN_TEAM, NO_PLAYER, NO_TEAM);
	for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		PlayerTypes const eLoopPlayer = (PlayerTypes)iI;
		CvPlayer const& kLoopPlayer = GET_PLAYER(eLoopPlayer);
		if (kLoopPlayer.isEverAlive() && !kLoopPlayer.isBarbarian())
			logSASGameRecordPlayerSetup(eLoopPlayer);
	}
}

struct SASGameRecordInitialTechGroup
{
	CvString szTechFields;
	CvString szTeams;
	int iTeams;
};

// <!-- custom: Successful new-game initialization is best described by its authoritative result, not by the order in which Civ4 happened to call meet/declareWar/setHasTech/startTrade while constructing that result.
// Seed periodic team/contact deltas from this same finalized baseline.
// Group identical technology sets so a late-era start does not repeat the same long payload for every team; the explicit team lists keep arbitrary scenarios and mixed/modded setups exact.
// Record surviving initial deals from the same finalized boundary, collapsing only the deterministic Advanced-Start-shaped reciprocal peace matrix already represented by forcePeace team state. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
static void logSASGameRecordFinalizedInitialState(int& iTeamStateRows, int& iTechRows, int& iDeals)
{
	iTeamStateRows = 0;
	iTechRows = 0;
	iDeals = 0;
	std::vector<SASGameRecordInitialTechGroup> aTechGroups;
	for (int iI = 0; iI < MAX_TEAMS; iI++)
	{
		TeamTypes const eTeam = (TeamTypes)iI;
		if (!GET_TEAM(eTeam).isEverAlive())
			continue;
		logSASGameRecord("GAME_RECORD_INITIAL_TEAM_STATE %s", getSASInitialTeamStateFields(eTeam).GetCString());
		CvString const szTechFields = getSASInitialTeamTechLevelFields(eTeam);
		SASGameRecordInitialTechGroup* pGroup = NULL;
		for (size_t iGroup = 0; iGroup < aTechGroups.size(); iGroup++)
		{
			if (aTechGroups[iGroup].szTechFields == szTechFields)
			{
				pGroup = &aTechGroups[iGroup];
				break;
			}
		}
		if (pGroup == NULL)
		{
			SASGameRecordInitialTechGroup kGroup;
			kGroup.szTechFields = szTechFields;
			kGroup.iTeams = 0;
			aTechGroups.push_back(kGroup);
			pGroup = &aTechGroups.back();
		}
		appendSASDiagnosticIntListValue(pGroup->szTeams, eTeam);
		pGroup->iTeams++;
		seedSASGameRecordTeamPreviousFromCurrentState(eTeam);
		iTeamStateRows++;
	}
	for (size_t iGroup = 0; iGroup < aTechGroups.size(); iGroup++)
	{
		SASGameRecordInitialTechGroup const& kGroup = aTechGroups[iGroup];
		logSASGameRecord("GAME_RECORD_INITIAL_TEAM_TECHS teams=%s teamCount=%d %s", kGroup.szTeams.GetCString(), kGroup.iTeams, kGroup.szTechFields.GetCString());
		iTechRows++;
	}
	int iLoop = 0;
	for (CvDeal const* pDeal = GC.getGame().firstDeal(&iLoop); pDeal != NULL; pDeal = GC.getGame().nextDeal(&iLoop))
	{
		if (isSASCollapsibleAdvancedStartPeaceDeal(*pDeal))
			continue;
		logSASGameRecord("GAME_RECORD_INITIAL_DEAL %s", getSASInitialDealStateFields(*pDeal).GetCString());
		iDeals++;
	}
}

// <!-- custom: Use "row" wording for generic SAS game-record row prefixes because Civ4 also has EventInfo/random events. Keep GAME_RECORD_ACTION only for chronological gameplay action rows. (GPT-5.5) -->
static void logSASGameRecordGameState(const char* szRowType)
{
	CvGame& kGame = GC.getGame();
	CvInitCore const& kInitCore = GC.getInitCore();
	const PlayerTypes eActivePlayer = kGame.getActivePlayer();
	const char* szActiveCivilization = "-";
	const char* szActiveHandicap = "-";
	if (eActivePlayer != NO_PLAYER)
	{
		CvPlayer const& kActivePlayer = GET_PLAYER(eActivePlayer);
		if (kActivePlayer.getCivilizationType() != NO_CIVILIZATION)
			szActiveCivilization = GC.getInfo(kActivePlayer.getCivilizationType()).getType();
		if (kActivePlayer.getHandicapType() != NO_HANDICAP)
			szActiveHandicap = GC.getInfo(kActivePlayer.getHandicapType()).getType();
	}
	CvString szGameOptions;
	FOR_EACH_ENUM(GameOption)
	{
		if (!kGame.isOption(eLoopGameOption))
			continue;
		if (!szGameOptions.empty())
			szGameOptions += ",";
		szGameOptions += GC.getInfo(eLoopGameOption).getType();
	}
	if (szGameOptions.empty())
		szGameOptions = "-";
	CvString szVictories;
	FOR_EACH_ENUM(Victory)
	{
		if (!kGame.isVictoryValid(eLoopVictory))
			continue;
		if (!szVictories.empty())
			szVictories += ",";
		szVictories += GC.getInfo(eLoopVictory).getType();
	}
	if (szVictories.empty())
		szVictories = "-";
	const CvString szLogName = getSASGameRecordLogName();
	logSASGameRecord("%s utc=%s logFile=%s turn=%d elapsed=%d year=%d scenario=%d activePlayer=%d activeCivilization=%s activeHandicap=%s playersDefined=%d playersAlive=%d playersEverAlive=%d humans=%d",
			szRowType, getSASGameRecordLogTimestamp().GetCString(), getSASDiagnosticQuoted(szLogName.GetCString()).GetCString(), kGame.getGameTurn(), kGame.getElapsedGameTurns(), kGame.getGameTurnYear(), kGame.isScenario(), eActivePlayer, szActiveCivilization, szActiveHandicap, kInitCore.getNumDefinedPlayers(), kGame.countCivPlayersAlive(), kGame.countCivPlayersEverAlive(), kGame.getNumHumanPlayers());
	// <!-- custom: Enabled victories and their fixed turn/score limits determine which later victory-progress and AI-strategy rows are relevant. Record this compact setup context instead of requiring external XML or save inspection. (GPT-5.6-Sol) -->
	// <!-- custom: AdvCiv-SAS also records its own cached land-heavy/naval-heavy map classifications here. Base AdvCiv 1.14 has no equivalent generic cache, so this upstream port intentionally leaves those SAS-specific fields out rather than recreating mod policy inside the recorder. (ChatGPT-5.6-Sol) -->
	logSASGameRecord("GAME_RECORD_GAME_SETTINGS mapScript=%S map=%dx%d world=%s climate=%s seaLevel=%s gameSpeed=%s startEra=%s gameHandicap=%s maxTurns=%d targetScore=%d victories=%s options=%s",
			getSASDiagnosticQuoted(kInitCore.getMapScriptName().GetCString()).GetCString(), GC.getMap().getGridWidth(), GC.getMap().getGridHeight(), GC.getInfo(kInitCore.getWorldSize()).getType(), GC.getInfo(kInitCore.getClimate()).getType(), GC.getInfo(kInitCore.getSeaLevel()).getType(), GC.getInfo(kGame.getGameSpeedType()).getType(), GC.getInfo(kGame.getStartEra()).getType(), GC.getInfo(kGame.getHandicapType()).getType(), kGame.getMaxTurns(), kGame.getTargetScore(), szVictories.GetCString(), szGameOptions.GetCString());
	logSASGameRecordMapOptions(kInitCore);
	logSASGameRecord("GAME_RECORD_GAME_RNG mapRandState=%u syncRandState=%u", kGame.getMapRand().getSeed(), kGame.getSorenRand().getSeed());
}

void logSASGameRecord(TCHAR* format, ... )
{
	static const bool bEnabled = isSASGameRecordLogEnabled();
	if (!bEnabled)
		return;

	va_list args;
	va_start(args, format);
	std::string szLine;
	// <!-- custom: KI#161.2's explicit terminator stopped MSVC 7.1 truncation from leaving unsafe unterminated output, but the fixed 2048-byte buffer still silently discarded long structured rows such as late-game building, unit-type and promotion inventories.
	// Reuse CvString's grow-and-retry formatter so the complete machine-readable row reaches the log; abort the row if even that bounded formatter fails. See KI#375. (ChatGPT-5.5 + GPT-5.5; ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
	bool const bFormatted = CvString::formatv(szLine, format, args);
	va_end(args);
	FAssertMsg(bFormatted, "SASGameRecord row formatting failed");
	if (!bFormatted)
		return;

	CvString const szLogName = getSASGameRecordLogName();
	gDLL->logMsg(szLogName.GetCString(), szLine.c_str(), false, false);
}

static void logSASGameRecordSnapshot(int iGameTurn, char const* szReason)
{
	CvGame const& kGame = GC.getGame();
	logSASGameRecord("GAME_RECORD_TURN_BEGIN turn=%d reason=%s elapsed=%d year=%d playersAlive=%d teamsAlive=%d totalCities=%d totalPopulation=%d",
			iGameTurn, szReason, kGame.getElapsedGameTurns(), kGame.getGameTurnYear(), kGame.countCivPlayersAlive(), kGame.countCivTeamsAlive(), kGame.getNumCities(), kGame.getTotalPopulation());
	for (int iI = 0; iI < MAX_CIV_TEAMS; iI++)
	{
		TeamTypes eLoopTeam = (TeamTypes)iI;
		if (GET_TEAM(eLoopTeam).isAlive() && !GET_TEAM(eLoopTeam).isBarbarian())
			logSASGameRecordTeamSnapshot(eLoopTeam, iGameTurn);
	}
	for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		PlayerTypes eLoopPlayer = (PlayerTypes)iI;
		if (GET_PLAYER(eLoopPlayer).isAlive() && !GET_PLAYER(eLoopPlayer).isBarbarian())
			logSASGameRecordPlayerSnapshot(eLoopPlayer, iGameTurn);
	}
	if (gGameRecordLogLevel >= 2) logSASGameRecordBarbarians(iGameTurn);
	logSASGameRecord("GAME_RECORD_TURN_END turn=%d reason=%s", iGameTurn, szReason);
}

void logSASGameRecordTurn(int iGameTurn)
{
	logSASGameRecordSnapshot(iGameTurn, "interval");
}

void startSASGameRecordLogForNewGame()
{
	rollSASGameRecordLog("new");
	resetSASGameRecordTeamPrevious();
	resetSASGameRecordPlayerPrevious();
	CvString const szLogName = getSASGameRecordLogName();
	logSASGameRecord("GAME_RECORD_NEW_GAME_INITIALIZING utc=%s logFile=%s", getSASGameRecordLogTimestamp().GetCString(), getSASDiagnosticQuoted(szLogName.GetCString()).GetCString());
	logSASGameRecordLogSettings();
	logSASGameRecordTechCapabilitySources();
	logSASGameRecordAttitudeLegend();
}

void logSASGameRecordNewGameStarted()
{
	logSASGameRecordGameState("GAME_RECORD_NEW_GAME_STARTED");
	logSASGameRecordInitialPlayerIdentities();
	if (getSASGameRecordLogLevel() >= 2)
	{
		int iTeamStateRows = 0;
		int iTechRows = 0;
		int iDeals = 0;
		logSASGameRecordFinalizedInitialState(iTeamStateRows, iTechRows, iDeals);
		logSASGameRecord("GAME_RECORD_INITIAL_STATE_SUMMARY teamStateRows=%d techGroupRows=%d techTeamsCovered=%d %s source=FINALIZED_STATE", iTeamStateRows, iTechRows, iTeamStateRows, getSASInitialDealSummaryFields(true, iDeals).GetCString());
	}
}

void startSASGameRecordLogForLoadedSave()
{
	rollSASGameRecordLog("load");
	resetSASGameRecordTeamPrevious();
	resetSASGameRecordPlayerPrevious();
	logSASGameRecordGameState("GAME_RECORD_SAVE_LOADED");
	logSASGameRecordLogSettings();
	logSASGameRecordTechCapabilitySources();
	logSASGameRecordAttitudeLegend();
	logSASGameRecordInitialPlayerIdentities();
	// <!-- custom: Level-2+ new games already emitted authoritative INITIAL_TEAM_STATE metTeams and seeded the contact baseline.
	// Loaded saves have no finalized initial-team block in this session, so retain explicit setup contact rows for them. (ChatGPT-5.6-Sol) -->
	if (getSASGameRecordLogLevel() >= 2)
	{
		for (int iI = 0; iI < MAX_CIV_TEAMS; iI++)
		{
			TeamTypes eLoopTeam = (TeamTypes)iI;
			if (GET_TEAM(eLoopTeam).isAlive() && !GET_TEAM(eLoopTeam).isBarbarian())
				logSASGameRecordTeamContacts(eLoopTeam, GC.getGame().getGameTurn(), "setup");
		}
	}
}

