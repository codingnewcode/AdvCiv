#pragma once

#ifndef CITY_SITE_EVALUATOR_H
#define CITY_SITE_EVALUATOR_H

class CvPlayerAI;
class CvTeamAI;
class CvCityAI;
class CvPlot;
class CvArea;
class CvGame;

// advc: New classes extracted from CvPlayerAI::AI_foundValue

// Corresponds to K-Mod's CvPlayerAI::CvFoundSettings
class CitySiteEvaluator
{
public:
	CitySiteEvaluator(CvPlayerAI const& kPlayer, int iMinRivalRange = -1,
			bool bStartingLoc = false, bool bNormalize = false);
	// <!-- custom: found-value path uses int (not short) to avoid overflow/underflow. (GPT-5.2-Codex (summarized)) -->
	int evaluate(CvPlot const& kPlot) const;
	int evaluate(int iX, int iY) const;
	// <!-- custom: Opt-in diagnostic entry point: return the same found value while filling an exact stage-by-stage summary. Callers guard its use behind logging so normal city-site evaluation does no string formatting. (GPT-5.5) -->
	int evaluateWithBreakdown(CvPlot const& kPlot, CvString& szBreakdown) const;
	int evaluateWithLogging(CvPlot const& kPlot) const; // advc.031c
	scaled evaluateWorkablePlot(CvPlot const& kPlot) const; // advc.027
	CvPlayerAI const& getPlayer() const { return m_kPlayer; }
	bool isStartingLoc() const { return m_bStartingLoc; }
	bool isScenario() const { return m_bScenario; }
	bool isNormalizing() const { return m_bNormalize; } // advc.031e
	int getMinRivalRange() const { return m_iMinRivalRange; }
	// <advc.300>
	void discourageBarbarians(int iRange);
	int getBarbarianDiscouragedRange() const { return m_iBarbDiscouragedRange; }
	// </advc.300>  <advc.027>
	void setIgnoreStartingSurroundings(bool b);
	bool isIgnoreStartingSurroundings() const { return m_bIgnoreStartingSurroundings; }
	// </advc.027>
	bool isAdvancedStart() const { return m_bAdvancedStart; } // advc
	/*  <advc.007> Ignores whether kPlot is close to another tentative city site
		and treats kPlayer as non-human */
	void setDebug(bool b);
	bool isDebug() const { return m_bDebug; }
	// </advc.007>
	//int getGreed() const; // advc.031: Not used anymore
	// (The comments below about the found settings are from K-Mod)
	// culture required to pop the 2nd borders (as in BtS)
	int getClaimThreshold() const { return m_iClaimThreshold; }
	/*	doesn't need vision of a plot to know what's there
		[But doesn't necessarily reveal resources; see AIFoundValue::getBonus.] */
	bool isAllSeeing() const { return m_bAllSeeing; }
	// <!-- custom: First-settler roaming needs starting-capital weights without map-generation omniscience. (GPT-5.5) -->
	void setAllSeeing(bool b) { m_bAllSeeing = b; }
	// some trait information that will influence where we settle ...
	// easy for us to pop the culture to the 2nd border
	bool isEasyCulture() const { return m_bEasyCulture; }
	// expectation of taking foreign land, either by culture or by force
	bool isAmbitious() const { return m_bAmbitious; }
	// more value for rivers
	bool isExtraYieldThreshold() const { return m_bExtraYieldThresh; }
	bool isExtraYieldNaturalThreshold() const { return m_bExtraYieldNaturalThresh; }
	// more value for settlings on hills
	bool isDefensive() const { return m_bDefensive; }
	// special affection for coast cities due to unique building or unit.
	bool isSeafaring() const { return m_bSeafaring; }
	// willing to place cities further apart. (not directly based on the expansive trait)
	bool isExpansive() const { return m_bExpansive; }
	// <advc.031c>
	void log(CvPlot const& kPlot);
	// <!-- custom: Detailed found-value breakdown for comparison sites logged next to a selected site, e.g. adjacent/current-site checks that explained and fixed the uMgungundlovu stale-target case in KI#185. (GPT-5.5) -->
	void logComparedSiteBreakdown(char const* szLabel, CvPlot const& kPlot) const;
	void logSettings() const; // </advc.031c>

private:
	CvPlayerAI const& m_kPlayer;
	bool m_bStartingLoc;
	bool m_bScenario; // advc
	bool m_bNormalize;
	int m_iMinRivalRange;
	int m_iBarbDiscouragedRange; // advc.300
	bool m_bIgnoreStartingSurroundings; // advc.027
	bool m_bAdvancedStart; // advc
	bool m_bDebug; // advc.007
	bool m_bAllSeeing;
	int m_iClaimThreshold;
	bool m_bEasyCulture;
	bool m_bAmbitious;
	bool m_bExtraYieldThresh; // (advc.908a: "bFinancial" in K-Mod)
	bool m_bExtraYieldNaturalThresh; // advc.908a
	bool m_bDefensive;
	bool m_bSeafaring;
	bool m_bExpansive;
};

/*  AIFoundValue::evaluate corresponds to K-Mod's CvPlayerAI::AI_foundValue_bulk
	and gets called by the constructor. */
class AIFoundValue
{
public:
	// <!-- custom: A non-null breakdown output enables diagnostic accounting; normal evaluation passes NULL and keeps that work disabled. (GPT-5.5) -->
	// <!-- custom: Let SPI construct the shared workable-plot context without also running and discarding a complete city-site evaluation. See KI#492. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
	AIFoundValue(CvPlot const& kPlot, CitySiteEvaluator const& kSettings, CvString* pszBreakdown = NULL, bool bEvaluateSite = true);
	int get() const { return m_iResult; }
	scaled evaluateWorkablePlot(CvPlot const& p) const; // advc.027

	// <advc.031c> Will have to enable the found log in BBAILog.h in addition
	static void setLoggingEnabled(bool b);
	static bool isLoggingEnabled() { return bLoggingEnabled; } // </advc.031c>

private:
	int m_iResult;
	CvString* m_pszBreakdown;
	/*  The rest aren't prefixed with "m_"; too awkward. Note that the order of
		the reference members needs to match their order in the ctor initalizer list. */
	CvPlot const& kPlot;
	CvArea const& kArea;
	CitySiteEvaluator const& kSet;
	CvPlayerAI const& kPlayer;
	PlayerTypes ePlayer;
	TeamTypes eTeam;
	CvTeamAI const& kTeam;
	CvGame const& kGame;
	bool bBarbarian;
	EraTypes eEra;
	scaled rAIEraFactor;
	int iX, iY;
	CvCityAI const* pCapital;
	bool bCoastal;
	int iAreaCities;
	int iCities;
	// Intermediate values ...
	std::vector<int> aiCitySiteRadius;
	// <advc.035>
	std::vector<bool> abOwnCityRadius;
	std::vector<bool> abFlip; // </advc.035>
	bool bFirstColony;
	int iUnrevealedTiles; // advc.040
	// <advc.031c>
	static bool bLoggingEnabled;
	static wchar const* cityName(CvCity const& kCity);
	void logSite() const;
	void logPlot(CvPlot const& p, int iPlotValue, int const* aiYield, int iCultureModifier, BonusTypes eBonus, ImprovementTypes eBonusImprovement, bool bCanTradeBonus, bool bCanSoonTradeBonus, bool bCanImproveBonus, bool bCanSoonImproveBonus, bool bEasyAccess, int iFeatureProduction, bool bPersistentFeature, bool bRemovableFeature) const;
	// </advc.031c>
	int evaluate();
	// Subroutines of evaluate ...
	bool isHome(CvPlot const& p) const { return (&p == &kPlot); }
	bool isSiteValid() const;
	bool computeOverlap();
	bool isPrioritizeAsFirstColony() const; // advc.040
	int countBadTiles(int& iInner, int& iUnrevealed, int& iLand, int& iRevealedDecentLand) const;
	bool isTooManyBadTiles(int iBadTiles, int iInnerBadTiles) const;
	int baseCityValue() const;
	bool isUsablePlot(CityPlotTypes ePlot, int& iTakenTiles, bool& bCityRadius, bool& bForeignOwned, bool& bAnyForeignOwned, bool& bShare, bool& bSteal) const;
	bool isRemovableFeature(CvPlot const& p, bool& bPersistent, int& iFeatureProduction) const;
	bool isRevealed(CvPlot const& p) const;
	PlayerTypes getRevealedOwner(CvPlot const& p) const;
	TeamTypes getRevealedTeam(CvPlot const& p) const;
	// <!-- custom: Count only cities the evaluating team owns or can locate when an ordinary found-value heuristic asks about an area's settlement state. See KI#493 and KI#495. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
	int countKnownCities(CvArea const& kLoopArea, PlayerTypes eOwner = NO_PLAYER) const;
	BonusTypes getBonus(CvPlot const& p) const;
	ImprovementTypes getBonusImprovement(BonusTypes eBonus, CvPlot const& p, bool& bCanTrade, bool& bCanTradeSoon, int* aiImprovementYield, bool& bCanImprove, bool& bCanImproveSoon, bool& bRemoveFeature) const;
	bool isNearTech(TechTypes eTech) const;
	int calculateCultureModifier(CvPlot const& p, bool bForeignOwned, bool bShare, bool bCityRadius, bool bSteal, bool bFlip, bool bOwnExcl, int& iTakenTiles, int& iStealPercent) const;
	int removableFeatureYieldVal(FeatureTypes eFeature, bool bRemovableFeature, bool bBonus) const;
	scaled estimateImprovementProduction(CvPlot const& p, bool bPersistentFeature) const;
	int evaluateYield(int const* aiYield, CvPlot const* p = NULL, bool bCanNeverImprove = false) const;
	int evaluateFreshWater(CvPlot const& p, int const* aiYield, bool bSteal, int& iRiverTiles, int& iGreenTiles) const;
	// <!-- custom: removed and now added inline in parent caller AIFoundValue::evaluate() directly, as it seems to be called only once, and we'd have more parameters to fine tune it further in parent caller rather, it is also clearer this way i think -->
	//int foundOnResourceValue(int const* aiBonusImprovementYield) const;
	int applyCultureModifier(CvPlot const& p, int iPlotValue, int iCultureModifier, bool bShare) const;
	// <!-- custom: Base AdvCiv had no diagnostic pointer parameters here; our previous SAS diagnostics exposed baseBonusVal/multiplier/earlyPercent from the broad AI_bonusVal path. Replace those with explicit new health/happiness value, plausible ordinary and capped special-building health/happiness effects, final adjustment, and water-penalty diagnostics. The old first-growth/luxury extra is not a separate parameter because new health/happiness already represents that empire-wide effect; adding a flat first-bonus value would double-count it. See KI#178. (GPT-5.5 + GPT-5.6-Sol) -->
	bool isPlotInKnownRivalFutureBFC(CvPlot const& p) const;
	bool isBonusOwnedOrClaimedByFutureBFC(BonusTypes eBonus) const;
	bool isBonusBuildingEffectValued(BuildingTypes eBuilding, int iMaxPrereqEra, bool bCoastal) const;
	int calculateBonusBuildingHappyHealthValue(BonusTypes eBonus, bool bCoastal) const;
	int calculateBonusConnectionEra(BonusTypes eBonus, CvPlot const& p) const;
	int nonYieldBonusValue(CvPlot const& p, BonusTypes eBonus, bool bCanTrade, bool bEasyAccess, bool bCoastal, std::vector<int>* paiBonusCount, int iCultureModifier, int* piHappyHealthValue = NULL, int* piBuildingHappyHealthValue = NULL, int* piAdjustPercent = NULL, int* piWaterPenalty = NULL) const;
	int calculateSpecialYieldModifier(int iCultureModifier, bool bEasyAccess, bool bBonus, bool bCanSoonImproveBonus, bool bCanImproveBonus) const;
	void calculateSpecialYields(CvPlot const& p, int const* aiBonusImprovementYield, int const* aiNatureYield, int iModifier, int* aiSpecialYield, int& iSpecialFoodPlus, int& iSpecialFoodMinus, int& iSpecialYieldTiles) const;
	void calculateBuildingYields(CvPlot const& p, int const* aiNatureYield, int* aiBuildingYield) const;
	int sumUpPlotValues(std::vector<int>& aiPlotValues) const;
	// <!-- custom: Disabled after XML-tunable SAS bonus-improvement yield valuation made this obscure hardcoded path redundant and retaining both overscored bonus-heavy sites in KI#173 follow-up testing. Kept commented with its implementation for reference. (GPT-5.5) -->
	// int evaluateSpecialYields(int const* aiSpecialYield, int iSpecialYieldTiles, int iSpecialFoodPlus, int iSpecialFoodMinus) const;
	// <!-- custom: simplify logic and attempt to spread cities more, currently they are way too crowded which is inefficient -->
	// bool isTooManyTakenTiles(int iTaken, int iResourceValue, bool bLowValue) const;
	bool isTooManyTakenTiles(int iTaken, int iResourceValue) const;
	int evaluateLongTermHealth(int& iHealthPercent) const;
	int evaluateFeatureProduction(int iProduction) const;
	int evaluateSeaAccess(bool bGoodFirstColony, scaled rProductionModifier, int iLandTiles) const;
	// int evaluateDefense() const;
	int evaluateGoodies(int iGoodies) const;
	int adjustToLandAreaBoundary(int iValue) const;
	int adjustToStartingSurroundings(int iValue) const;
	int adjustToStartingChoices(int iValue) const;
	// <!-- custom: try to remove this interference as we have a finer algorithm now, and this old code may lead to unexpected results -->
	// int adjustToFood(int iValue, int iSpecialFoodPlus, int iSpecialFoodMinus,
	// 		int iGreenTiles) const;
	// <!-- custom: see code comment there for details -->
	// int adjustToProduction(int iValue, scaled rBaseProduction) const;
	// <!-- custom: same -->
	//int adjustToBarbarianSurroundings(int iValue) const;
	// <!-- custom: this adjustToCivSurroundings caused a bug of AI settler settling on bonus camel desert which is very bad in a desert surroudning even worse, it is seemingly called only once in AIFoundValue::evaluate, may as well disable it since it is so complicated and who knows where the bugs is(/are?) and instead migrate only a very simplified version of the logic we want directly inline in its only caller so in AIFoundValue::evaluate, done so with the help of chatgpt 5, check if accurate, see known issue as of now 54 for details -->
	// int adjustToCivSurroundings(int iValue, int iStealPercent) const;
	int adjustToCitiesPerArea(int iValue) const;
	int adjustToBonusCount(int iValue, std::vector<int> const& aiBonusCount) const;
	// <!-- custom: try to remove this interference as we have a finer algorithm now, and this old code may lead to unexpected results -->
	// int adjustToBadTiles(int iValue, int iBadTiles) const;
	// <!-- custom: same -->
	//int adjustToBadHealth(int iValue, int iGoodHealth) const;
	int countDeadlockedBonuses() const;
	bool isDeadlockedBonus(CvPlot const& kBonusPlot, int iMinRange) const;
};

#endif
