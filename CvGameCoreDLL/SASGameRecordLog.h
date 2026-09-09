#pragma once

#ifndef SAS_GAME_RECORD_LOG_H
#define SAS_GAME_RECORD_LOG_H

#include <vector> // <!-- custom: Rare goody-result logging passes the actual created unit pointers to the recorder for compact canonical type/id/position formatting. (ChatGPT-5.6-Sol) -->

// <!-- custom: Structured game-record rows for autoplay comparison, game analysis, user-assistance summaries, and external LLM review. This is not a classic BBAI diagnostic category: it has its own XML defines, its own SASGameRecord_*.log files, and its own lightweight public header. Call sites should still gate before invoking helpers so disabled logging does not compute logging-only arguments. Pointer-only hooks use forward declarations here to avoid pulling city/unit headers into ordinary game files. (ChatGPT-5.5 + GPT-5.5) -->
bool isSASGameRecordLogEnabled();
int getSASGameRecordLogLevel();
int getSASGameRecordTurnInterval();
// <!-- custom: Finalize buffered observations in the old game state before a new game or loaded save resets/replaces it. See KI#382. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
void finalizeSASGameRecordLogSession();
void startSASGameRecordLogForNewGame();
void logSASGameRecordNewGameStarted();
void startSASGameRecordLogForLoadedSave();

class CvCity;
// <!-- custom: Required by random-event APIs using CvPlayer references; this lightweight declaration fixed the resulting AgentIterator/CvPlayer compile errors. (GPT-5.6-Sol) -->
class CvPlayer;
class CvPlot;
class CvUnit;
// <!-- custom: Forward-declare the vote payload because SASGameRecord only passes it by pointer.
// This keeps the lightweight recorder header from needing CvStructs.h solely for diplomatic vote-source history hooks. (ChatGPT-5.6-Sol) -->
struct VoteTriggeredData;
// <!-- custom: Random-event lifecycle diagnostics pass the existing player-local trigger payload by const pointer/reference without exposing its save-layout definition through this lightweight recorder header. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
struct EventTriggeredData;
// <!-- custom: Observe one AI_chooseProduction call as a scope so every early return is handled without teaching the AI decision tree about recorder schema.
// When level 2+ is pre-enabled by the caller, the destructor compares the final head order with the entry state and records only meaningful switches, clears, or resumptions of stored production.
// Earlier versions put the whole constructor/destructor in SASGameRecordLog.cpp. In ordinary Release builds that hides the disabled fast path across the translation-unit boundary.
// Final_Release /GL + /LTCG may recover such inlining, but keeping these tiny wrappers here makes the cheap path visible in every build while begin/end remain cold level-2+ work. (ChatGPT-5.6-Sol) -->
class SASGameRecordAIProductionChoiceScope
{
public:
	// <!-- custom: m_pCity remains the gate: disabled construction leaves it NULL, and the inline destructor keeps the old null check before calling end().
	// No recorder state besides the pointer is touched at level 0/1. (ChatGPT-5.6-Sol) -->
	SASGameRecordAIProductionChoiceScope(CvCity const& kCity, bool bEnabled) : m_pCity(NULL)
	{
		if (bEnabled) begin(kCity);
	}
	~SASGameRecordAIProductionChoiceScope()
	{
		if (m_pCity != NULL) end();
	}
private:
	void begin(CvCity const& kCity);
	void end();
	CvCity const* m_pCity;
	OrderTypes m_eOldOrder;
	int m_iOldData1;
	int m_iOldStored;
	int m_iOldNeeded;
	int m_iOldTurnsLeft;
	int m_iOldAccumulatedInactiveTurns;
};
// <!-- custom: Goody huts are rare but can have compound randomized effects (gold, map reveal, partial/full tech, free/promoted units, hostile units and AdvCiv follow-up outcomes).
// Collect only realized effects at the authoritative gameplay boundary, then let SASGameRecord own the stable row formatting. Callers must gate level 2+ before doing logging-only measurements.
// The result object itself is intentionally cheap to default-construct (scalar sentinels plus empty vectors); avoid pointer/optional lifetime machinery merely to skip this tiny setup when recording is disabled. (ChatGPT-5.6-Sol) -->
struct SASGameRecordGoodyResult
{
	SASGameRecordGoodyResult();
	bool bFollowupOutcome;
	bool bUpgradeRoll;
	bool bUpgradeApplied;
	bool bAdditionalOutcomeAttempted;
	int iGold;
	int iNewlyRevealedPlots;
	int iExperienceGained;
	int iDamageHealed;
	TechTypes eTech;
	int iTechRewardValue;
	int iTechProgressBefore;
	int iTechProgressAfter;
	int iTechCost;
	bool bTechCompleted;
	int iFreePromotionsGranted;
	std::vector<CvUnit const*> apFreeUnits;
	std::vector<CvUnit const*> apBarbarianUnits;
};
// <!-- custom: Diplomacy logging declarations below use TradeData and CLinkList only by reference. Forward declarations avoid pulling trade-list implementation headers into every SASGameRecordLog.h includer. (ChatGPT-5.6-Sol) -->
struct TradeData;
template <class tVARTYPE> class CLinkList;
// <!-- custom: Capture a plot before a logical action so one combined record can describe terrain, feature, resource, improvement, route and permanent event-yield changes.
// The default constructor initializes safe NO_* enum and zero yield values without map lookups, letting caller-gated hooks avoid capture work when game-record logging is disabled. (GPT-5.6-Sol) -->
struct SASGameRecordPlotState
{
	SASGameRecordPlotState();
	explicit SASGameRecordPlotState(CvPlot const& kPlot);
	TerrainTypes eTerrain;
	FeatureTypes eFeature;
	BonusTypes eBonus;
	ImprovementTypes eImprovement;
	RouteTypes eRoute;
	int aiExtraYield[NUM_YIELD_TYPES];
};
void logSASGameRecordTurn(int iGameTurn);
// <!-- custom: Plot changes and permanent team map revelation are buffered into compact coordinate lists and flushed once per turn; detailed before/after rows are reserved for non-routine causes. The environment hook receives values already computed by CvGame::doGlobalWarming so it adds no map scan. (GPT-5.6-Sol) -->
void flushSASGameRecordTurnChanges(int iGameTurn);
void recordSASGameRecordPlotChange(CvPlot const& kPlot, SASGameRecordPlotState const& kOldState, char const* szCategory, char const* szCause, bool bDetailed);
// <!-- custom: Directional river edits are rare and independent from ordinary plot-state actions, so record them separately instead of adding unused river fields to every detailed plot-change row. Callers gate this helper before computing logging-only arguments. (GPT-5.6-Sol) -->
void logSASGameRecordRiverEdgeChanged(CvPlot const& kPlot, bool bOldSouthBoundary, bool bOldEastBoundary);
void recordSASGameRecordPlotRevealed(CvPlot const& kPlot, TeamTypes eTeam);
// <!-- custom: Map-visible technologies reveal every plot through thousands of ordinary setRevealed calls. Bracket that bulk operation so the record writes one exact full-map row instead of redundant coordinate chunks. (GPT-5.6-Sol) -->
void beginSASGameRecordFullMapRevelation(TeamTypes eTeam, TechTypes eTech);
void endSASGameRecordFullMapRevelation(TeamTypes eTeam, TechTypes eTech);
void logSASGameRecordEnvironmentTurn(int iPollution, int iSustainabilityThreshold, int iLandDefense, int iIndexBefore, int iIndexBeforeRestoration, int iIndexEnd, int iWarmingChances, int iEventTally);
// <!-- custom: High-level research-plan mutations can tag a shared ResearchTargetChangeCause for the recorder; the later player-turn observer emits it only if an invested incomplete-tech redirection actually materializes. (ChatGPT-5.6-Sol) -->
void noteSASGameRecordResearchTargetChangeCause(PlayerTypes ePlayer, ResearchTargetChangeCause eCause);
// <!-- custom: Preserve the exact fresh-research/carried-overflow split only for level-2 ordinary research application; the recorder consumes it if that same call completes the technology. (ChatGPT-5.6-Sol) -->
void noteSASGameRecordResearchApplication(PlayerTypes ePlayer, TechTypes eTech, int iModifiedResearchRate, int iIncomingOverflowUnmodified, int iIncomingOverflowModified);
// <!-- custom: This mixed-level player-turn updater is called whenever SASGameRecord is enabled because Golden Age/anarchy counters support lower-detail rows.
// Only the research-target comparison self-gates at level 2+, so do not pre-gate the whole helper at level 2. (ChatGPT-5.6-Sol) -->
void updateSASGameRecordPlayerTurnState(PlayerTypes ePlayer);
// <!-- custom: Fog-spawn call sites pass the explicit Barbarian-unit source; ordinary city production continues through logSASGameRecordUnitCompleted. (GPT-5.6-Sol) -->
void logSASGameRecordBarbarianSpawn(CvUnit const* pUnit, char const* szCause);
// <!-- custom: Level-2 goody rows preserve each realized hut outcome; level 3 keeps the existing exact Barbarian-spawn rows as complementary tactical detail. (ChatGPT-5.6-Sol) -->
void logSASGameRecordGoodyReceived(PlayerTypes ePlayer, CvPlot const* pPlot, CvUnit const* pTriggerUnit, GoodyTypes eGoody, SASGameRecordGoodyResult const& kResult);
void logSASGameRecordGoodyNoOutcome(PlayerTypes ePlayer, CvPlot const* pPlot, CvUnit const* pTriggerUnit, GoodyTypes eTaboo, int iAttempts);
// <!-- custom: Civ4 EventInfo/random-event history is deliberately separate from generic GAME_RECORD_ACTION rows.
// Calls are level-2 pre-gated at authoritative gameplay boundaries so disabled/level-1 runs do not collect target validity or construct diagnostic strings. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
void logSASGameRecordRandomEventTriggered(CvPlayer const& kPlayer, EventTriggeredData const& kTriggeredData, char const* szDeliveryPath);
void logSASGameRecordRandomEventNoSelection(CvPlayer const& kPlayer, EventTriggeredData const& kTriggeredData, char const* szResolution);
void logSASGameRecordRandomEventApply(CvPlayer const& kPlayer, EventTypes eEvent, int iTriggeredId, EventTriggeredData const* pTriggeredData, bool bUpdateTrigger, char const* szDisposition, int iCanDoEvent, int iTriggerFiredBefore, int iEventOccurredBefore);
void logSASGameRecordRandomEventGoldResult(CvPlayer const& kPlayer, EventTypes eEvent, int iTriggeredId, int iRangeLow, int iRangeHigh, int iPlayerGoldDelta, PlayerTypes eOtherPlayer, bool bGoldToPlayer);
void logSASGameRecordRandomEventTechResult(CvPlayer const& kPlayer, EventTypes eEvent, int iTriggeredId, TechTypes eTech, int iTechPercent, int iResearchBefore, int iBeakersApplied, int iResearchAfter, int iTechCost, int iCompleted);
void logSASGameRecordRandomEventFreeUnitsResult(PlayerTypes ePlayer, PlayerTypes eAffectedPlayer, EventTypes eEvent, int iTriggeredId, UnitClassTypes eUnitClass, UnitTypes eUnit, int iRequestedCount, int iCreatedCount, CvCity const* pSpawnCity);
void logSASGameRecordRandomEventOccurrenceCleared(CvPlayer const& kPlayer, EventTypes eSourceEvent, EventTypes eClearedEvent, int iTriggeredId, int iClearChance, char const* szScope, TeamTypes eScopeTeam, int iScopePlayerSlots, int iScopeEverAlivePlayers, int iClearedOccurrences);
void logSASGameRecordRandomEventExpired(CvPlayer const& kPlayer, EventTypes eEvent, EventTriggeredData const& kTriggeredData, char const* szReason);
void logSASGameRecordRandomEventPillageResult(char const* szScope, PlayerTypes ePlayer, PlayerTypes eAffectedPlayer, int iCityId, int iTriggeredId, EventTypes eEvent, int iMinPillage, int iMaxPillage, int iAttempts, int iDestroyed);
void logSASGameRecordRandomEventCountdownScheduled(CvPlayer const& kPlayer, EventTypes eSourceEvent, EventTypes eFollowupEvent, int iTriggeredId, int iRequestedDueTurn, int iPreviousDueTurn, int iScheduledDueTurn);
void logSASGameRecordUnitCompleted(CvCity const* pCity, CvUnit const* pUnit, bool bConscripted, int iRawModifiedOverflow = 0, int iUnmodifiedOverflow = 0, int iKeptOverflow = 0, int iLostProduction = 0, int iUnusedOverflowCapacity = 0, int iOverflowGold = 0);
// <!-- custom: Research completion has its own accounting row because generic TECH_ACQUIRED also covers trades, free technologies, espionage and other sources where research overflow fields would be meaningless.
// Call only for actual TECH_ACQUISITION_RESEARCH threshold crossings at level 2+. (ChatGPT-5.6-Sol) -->
void logSASGameRecordResearchCompleted(TechTypes eTech, TeamTypes eTeam, PlayerTypes ePlayer, int iProgressBefore, int iProgressBeforeClamp, int iResearchModifier, int iUnmodifiedOverflow);
// <!-- custom: Added eCause so the existing TECH_ACQUIRED action can name its explicit source. (GPT-5.6-Sol + GPT-5.6 Thinking) -->
void logSASGameRecordTechAcquired(TechTypes eType, TeamTypes eTeam, PlayerTypes ePlayer, TechAcquisitionCause eCause);
void logSASGameRecordCityBuilt(CvCity const* pCity);
// <!-- custom: City razing is rare but strategically consequential. Capture level-2 context immediately before destruction, then finalize after disband so exact land/population/victory deltas reflect what the raze actually changed.
// The recorder owns the context stack and formatting; callers only pre-gate and bracket the existing raze. (ChatGPT-5.6-Sol) -->
void beginSASGameRecordCityRaze(CvCity const* pCity, PlayerTypes ePlayer);
void endSASGameRecordCityRaze(PlayerTypes ePlayer);
void logSASGameRecordCityAcquired(PlayerTypes eOldOwner, PlayerTypes eNewOwner, CvCity const* pCity, bool bConquest, bool bTrade);
void logSASGameRecordWarStarted(TeamTypes eDeclarer, TeamTypes eTarget, WarPlanTypes eWarPlan, bool bPrimaryDoW, bool bNewDiplo, PlayerTypes eSponsor, bool bRandomEvent, WarDeclarationCause eCause);
// <!-- custom: Added the pre-reset war-success and peace-context parameters so synthetic war summaries retain the final result before Base AdvCiv's AI_postMakePeace clears it. (GPT-5.6-Sol) -->
void logSASGameRecordWarEnded(TeamTypes eTeam, TeamTypes eOtherTeam, int iTeamAWarSuccess, int iTeamBWarSuccess, bool bCapitulate, TeamTypes eBroker, bool bRandomEvent, bool bReparations);
// <!-- custom: Record the exact team-membership boundary before CvTeam::addTeam reassigns the absorbed players and erases their old membership. Call only at level 2+. (ChatGPT-5.6-Sol) -->
void logSASGameRecordTeamMerged(TeamTypes eSurvivingTeam, TeamTypes eAbsorbedTeam);
void logSASGameRecordTeamMet(TeamTypes eTeam, TeamTypes eOtherTeam, bool bNewDiplo, int iX1, int iY1, int iX2, int iY2, CvPlot const* pTeamContactPlot, CvPlot const* pOtherContactPlot);
void logSASGameRecordPlayerGoldTrade(PlayerTypes eFromPlayer, PlayerTypes eToPlayer, int iAmount);
// <!-- custom: Capture only the compact relationship state needed to describe a resolved human diplomacy interaction before/after CvPlayer::handleDiploEvent. Callers gate at SASGameRecord level 2+ before capture. (ChatGPT-5.6-Sol) -->
struct SASGameRecordDiploRelationState
{
	int iActorAttitude;
	int iOtherAttitude;
	int aiActorMemory[NUM_MEMORY_TYPES];
	int aiOtherMemory[NUM_MEMORY_TYPES];
	WarPlanTypes eActorWarPlan;
	WarPlanTypes eOtherWarPlan;
	bool bAtWar;
};
bool isSASGameRecordResolvedDiploInteraction(DiploEventTypes eDiploEvent);
void captureSASGameRecordDiploRelationState(PlayerTypes eActor, PlayerTypes eOther, SASGameRecordDiploRelationState& kState);
void logSASGameRecordResolvedDiploInteraction(PlayerTypes eActor, DiploEventTypes eDiploEvent, PlayerTypes eOther, int iData1, SASGameRecordDiploRelationState const& kBefore, SASGameRecordDiploRelationState const& kAfter);
// <!-- custom: EXE trade-table wrappers expose authoritative submitted human->AI offer/counterproposal boundaries. Call only at level 2+ so exact trade-list serialization stays out of disabled/low-detail logging. (ChatGPT-5.6-Sol) -->
void logSASGameRecordDiploOfferEvaluated(PlayerTypes eProposer, PlayerTypes eResponder, CLinkList<TradeData> const& kProposerGives, CLinkList<TradeData> const& kResponderGives, int iChange, bool bAccepted, SASGameRecordDiploRelationState const& kBefore, SASGameRecordDiploRelationState const& kAfter);
void logSASGameRecordDiploCounterProposal(PlayerTypes eProposer, PlayerTypes eResponder, CLinkList<TradeData> const& kOriginalProposerGives, CLinkList<TradeData> const& kOriginalResponderGives, CLinkList<TradeData> const& kProposerAdds, CLinkList<TradeData> const& kResponderAdds, bool bProposed);
// <!-- custom: AI->human rejected ordinary offers are visible only at the Python diplomacy UI boundary; the CyGame bridge rebuilds these lists and calls this canonical recorder formatter. (ChatGPT-5.6-Sol) -->
void logSASGameRecordAIToHumanOfferRejected(PlayerTypes eProposer, PlayerTypes eResponder, CLinkList<TradeData> const& kProposerGives, CLinkList<TradeData> const& kResponderGives);
// <!-- custom: Diplomatic vote-source elections and resolutions (the Apostolic Palace and United Nations in standard BtS) are rare, high-impact factual boundaries.
// Record the real triggered proposal and one compact final weighted ballot/result rather than speculative AI vote reasoning or one row per cast ballot. Caller-gate at SASGameRecord level 2+. (ChatGPT-5.6-Sol) -->
void logSASGameRecordVoteTriggered(VoteTriggeredData const* pVoteTriggered);
void logSASGameRecordVoteResult(VoteTriggeredData const* pVoteTriggered, bool bThresholdPassed, bool bPassed, bool bCancelled, qword uiDefaultedAbstain, qword uiDefiers, qword uiEndorsers);
void logSASGameRecordReligionFounded(ReligionTypes eReligion, PlayerTypes ePlayer);
void logSASGameRecordCorporationFounded(CorporationTypes eCorporation, PlayerTypes ePlayer);
// <!-- custom: Preserve consequential factual city/unit actions that otherwise disappear between periodic snapshots: hurrying, natural growth/starvation, culture expansion, pillage attribution/economic gain, naval blockade lifecycle/plunder, unit gifting, religion/corporation membership changes, and circumnavigation.
// These hooks record only realized gameplay outcomes at level 2+; routine natural population changes are interval-compacted unless level 3 requests exact city transitions, and AI choice reasoning remains in BBAI diagnostics. (ChatGPT-5.6-Sol) -->
void logSASGameRecordCityHurry(CvCity const* pCity, HurryTypes eHurry, int iProductionBefore, int iProductionAdded, int iGoldCost, int iPopulationCost, int iHurryAngerAdded, int iGoldBefore, int iPopulationBefore, int iHurryAngerBefore);
void logSASGameRecordCityGrowthPrevented(CvCity const* pCity, int iFoodDiscarded);
void logSASGameRecordCityPopulationChanged(CvCity const* pCity, bool bGrowth, int iPopulationBefore, int iFoodDifference, int iFoodBefore, int iFoodAfterDifference, int iFoodKeptBefore, int iFoodKeptBeforePopulationChange, int iGrowthThresholdBefore);
void logSASGameRecordCityCultureExpanded(CvCity const* pCity);
void logSASGameRecordPillage(CvUnit const* pUnit, SASGameRecordPlotState const& kOldPlotState, PlayerTypes eVictimPlayer, int iGoldGained);
void logSASGameRecordBlockadeChanged(CvUnit const* pUnit, bool bStarting);
void logSASGameRecordBlockadePlunder(CvUnit const* pUnit, CvCity const* pCity, int iGold, int iTradeRoutes, int iProfitPerRoute);
void logSASGameRecordUnitGifted(CvUnit const* pUnit, PlayerTypes eGiftingPlayer, CvPlot const* pPlotLocation);
void logSASGameRecordReligionChanged(ReligionTypes eReligion, PlayerTypes ePlayer, CvCity const* pCity, bool bAdded);
// <!-- custom: Membership-change rows say what ultimately changed; these compact attempt rows preserve the consumed Missionary/Executive, exact chance and failed/displacing outcomes that otherwise vanish. Callers pre-gate level 2+. (ChatGPT-5.6-Sol) -->
void logSASGameRecordReligionSpreadAttempt(CvUnit const* pUnit, ReligionTypes eReligion, CvCity const* pCity, int iDirectSpreadChance, bool bSuccess, ReligionTypes eDisplacedReligion);
void logSASGameRecordCorporationChanged(CorporationTypes eCorporation, PlayerTypes ePlayer, CvCity const* pCity, bool bAdded);
void logSASGameRecordCorporationSpreadAttempt(CvUnit const* pUnit, CorporationTypes eCorporation, CvCity const* pCity, int iSpreadChance, int iGoldCost, int iGoldBefore, bool bSuccess);
void logSASGameRecordCircumnavigated(TeamTypes eTeam, int iFreeSeaMoves, bool bBonusApplied, int iSeaExtraMovesBefore, int iSeaExtraMovesAfter);
// <!-- custom: Financial strike is a rare player-wide economic crisis; periodic player rows preserve current/cumulative state while this level-2 action records each realized strike turn and any resulting forced unit loss. Callers pre-gate before collecting logging-only unit counts. (ChatGPT-5.6-Sol) -->
void logSASGameRecordFinancialStrikeTurn(PlayerTypes ePlayer, int iGoldBefore, int iCalculatedGoldRate, int iGoldAfterClamp, int iCumulativeStrikeTurns, int iUnitsBeforeDisband, int iUnitsAfterDisband);
void logSASGameRecordGoldenAge(PlayerTypes ePlayer, bool bStart);
void logSASGameRecordGoldenAgeTurnsChanged(PlayerTypes ePlayer, int iChange, int iOldGoldenAgeTurns, int iNewGoldenAgeTurns);
void logSASGameRecordAnarchy(PlayerTypes ePlayer, bool bStart);
void logSASGameRecordCivicChanged(PlayerTypes ePlayer, CivicOptionTypes eCivicOption, CivicTypes eOldCivic, CivicTypes eNewCivic, ReligionTypes eOldEffectiveStateReligion, ReligionTypes eNewEffectiveStateReligion);
void logSASGameRecordLastStateReligionChanged(PlayerTypes ePlayer, ReligionTypes eOldReligion, ReligionTypes eNewReligion);
void logSASGameRecordBuildingCompletedByProduction(CvCity const* pCity, BuildingTypes eBuilding, int iRawModifiedOverflow, int iUnmodifiedOverflow, int iKeptOverflow, int iLostProduction, int iUnusedOverflowCapacity, int iOverflowGold);
void logSASGameRecordBuildingBuilt(CvCity const* pCity, BuildingTypes eBuilding);
void logSASGameRecordProjectBuilt(CvCity const* pCity, ProjectTypes eProject, int iRawModifiedOverflow, int iUnmodifiedOverflow, int iKeptOverflow, int iLostProduction, int iUnusedOverflowCapacity, int iOverflowGold);
// <!-- custom: City snapshots expose stored overflow and interval production-flow rows summarize it. Level-3 production-completion rows additionally preserve the exact overflow result of each completed unit/building/project; the standalone hook remains for aggregation plus lower-level exceptional/Barbarian rows. (ChatGPT-5.6-Sol) -->
void logSASGameRecordProductionOverflow(CvCity const* pCity, int iRawModifiedOverflow, int iUnmodifiedOverflow, int iKeptOverflow, int iLostProduction, int iUnusedCapacity, int iGold);
void logSASGameRecordProductionFailed(CvCity const* pCity, int iOrderData, bool bProject, int iInvestedProduction, int iGold);
// <!-- custom: Production lifecycle diagnostics distinguish strategic target switching from actual mechanical loss. Decay is a real loss path; obsolete-unit production transfer preserves stored production under a new unit type. Call only at level 2+. (ChatGPT-5.6-Sol) -->
void logSASGameRecordProductionDecay(CvCity const* pCity, OrderTypes eOrder, int iData1, int iBefore, int iAfter, int iInactiveTurns);
void logSASGameRecordProductionInvalidated(CvCity const* pCity, OrderTypes eOrder, int iData1, int iStoredLost, bool bActiveTarget, bool bQueued);
void logSASGameRecordProductionUpgraded(CvCity const* pCity, UnitTypes eOldUnit, UnitTypes eNewUnit, int iProductionTransferred, int iDestinationProductionBefore);
// <!-- custom: Added the victory type so SASGameRecord can distinguish an actual spaceship launch from ordinary project completion and record its countdown. (GPT-5.6-Sol) -->
void logSASGameRecordVictoryLaunched(PlayerTypes ePlayer, VictoryTypes eVictory);
// <!-- custom: Capital loss and a failed arrival roll both call resetVictoryProgress, which otherwise silently erases the active countdown and spaceship projects. These hooks preserve the distinct cause and exact pre-reset state. (GPT-5.6-Sol) -->
void logSASGameRecordVictoryProgressResetForCapital(CvCity const* pCapital);
void logSASGameRecordSpaceshipFailed(TeamTypes eTeam, VictoryTypes eVictory, int iLaunchSuccessPercent);
void logSASGameRecordVassalState(TeamTypes eMaster, TeamTypes eVassal, bool bVassal);
void logSASGameRecordVictory(TeamTypes eWinner, VictoryTypes eVictory);
void logSASGameRecordRunStatus(char const* szReason);
void logSASGameRecordPlayerEliminated(PlayerTypes ePlayer);
void logSASGameRecordPlayerAliveChanged(PlayerTypes ePlayer, bool bRevived);
// <!-- custom: Record successful game-level Debug-mode toggles at their authoritative DLL boundary. (ChatGPT-5.6-Sol) -->
void logSASGameRecordDebugModeChanged(bool bOldDebugMode, bool bNewDebugMode);
// <!-- custom: Added eEndCause for explicit completion attribution; active-player changes update per-request and per-log-session control-transfer counts. Callers remain logging-gated. See KI#203. (GPT-5.6-Sol) -->
void logSASGameRecordAutoPlayChanged(int iOldValue, int iNewValue, bool bChangePlayerStatus, SASAutoPlayEndCause eEndCause);
void logSASGameRecordActivePlayerChanged(PlayerTypes eOldPlayer, PlayerTypes eNewPlayer);
void logSASGameRecordAutoPlayPopupDismissed(char const* szPopupKind);
void logSASGameRecordGreatPersonBorn(CvUnit const* pUnit, PlayerTypes ePlayer, CvCity const* pCity);
void logSASGameRecordGreatPersonJoined(CvUnit const* pUnit, CvCity const* pCity, SpecialistTypes eSpecialist);
void logSASGameRecordGreatPersonConstructed(CvUnit const* pUnit, CvCity const* pCity, BuildingTypes eBuilding);
void logSASGameRecordGreatPersonDiscovered(CvUnit const* pUnit, TechTypes eTech, int iResearch);
void logSASGameRecordGreatPersonHurried(CvUnit const* pUnit, CvCity const* pCity, BuildingTypes eBuilding, int iProduction);
void logSASGameRecordGreatPersonTradeMission(CvUnit const* pUnit, CvCity const* pCity, int iGold);
void logSASGameRecordGreatPersonGreatWork(CvUnit const* pUnit, CvCity const* pCity, int iCulture);
void logSASGameRecordGreatPersonInfiltrated(CvUnit const* pUnit, CvCity const* pCity, int iEspionage);
void logSASGameRecordGreatPersonGoldenAgeConsumed(CvUnit const* pUnit);
// <!-- custom: Combat can supply its actual target because a dead attacker still reports its origin; other death paths retain the unit's current plot. See KI#377. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
void logSASGameRecordGreatPersonDied(CvUnit const* pUnit, PlayerTypes eResponsiblePlayer, char const* szCause, CvPlot const* pDeathPlot = NULL);
void logSASGameRecordEspionageMission(CvUnit const* pUnit, EspionageMissionTypes eMission, PlayerTypes eTargetPlayer, CvPlot const* pPlot, int iExtraData, int iCost, int iEPBefore, int iEPAfter, ImprovementTypes eTargetImprovement, RouteTypes eTargetRoute, UnitTypes eTargetUnit, int iEffectValue, char const* szEffectKind);
void logSASGameRecordSpyIntercepted(CvUnit const* pUnit, PlayerTypes eTargetPlayer, char const* szPhase, int iModifier, int iInterceptChanceX100, EspionageMissionTypes eMission, int iExtraData, ImprovementTypes eTargetImprovement, RouteTypes eTargetRoute, UnitTypes eTargetUnit);
void logSASGameRecordGreatGeneralAttached(CvUnit const* pGreatGeneral, CvUnit const* pTargetUnit, PromotionTypes ePromotion);
void logSASGameRecordUnitScrapped(CvUnit const* pUnit);
void logSASGameRecordUnitUpgraded(CvUnit const* pOldUnit, CvUnit const* pNewUnit, int iCost);
void logSASGameRecordUnitCaptured(PlayerTypes eOldOwner, UnitTypes eOldUnitType, CvUnit const* pNewUnit);
// <!-- custom: Military-quality history records actual promotion selections and realized XP changes as compact interval/cumulative facts instead of inferring them from surviving units. Callers pre-gate level 2+ so disabled/low-detail logging pays no recorder cost. (ChatGPT-5.6-Sol) -->
void logSASGameRecordUnitPromoted(CvUnit const* pUnit, PromotionTypes ePromotion);
void logSASGameRecordExperienceChange(CvUnit const* pUnit, int iAdjustedChange, int iActualChange, bool bFromCombat);
// <!-- custom: Bracket real combats only when SASGameRecord level 2+ needs outcome-quality history.
// The recorder keeps tiny transient attacker/defender context so delayed visible-combat callbacks retain the true attacker and pre-combat odds without adding gameplay/save state. (ChatGPT-5.6-Sol) -->
void noteSASGameRecordCombatStarted(CvUnit const* pAttacker, CvUnit const* pDefender, CvPlot const* pBattlePlot);
void logSASGameRecordNonlethalCombat(CvUnit const* pAttacker, CvUnit const* pDefender, CvPlot const* pBattlePlot, bool bCombatLimitReached);
void logSASGameRecordWarPlanChanged(TeamTypes eTeam, TeamTypes eTarget, WarPlanTypes eOldWarPlan, WarPlanTypes eNewWarPlan, bool bWar, int iOldStateCounter);
// <!-- custom: Level-3 tactical rows complement periodic city defense and unit-composition snapshots: synthesize consecutive siege/naval/air city bombardment into compact sequences while recording actual air strikes, interception combat, and air bombing of plot structures, so external analysis can distinguish merely owning those units from using them effectively. (GPT-5.6 Thinking) -->
void logSASGameRecordCityBombard(CvUnit const* pUnit, CvCity const* pCity, char const* szMode, int iBombardRate, bool bIgnoreBuildingDefense, int iDefenseModifierBefore, int iDefenseDamageBefore);
void logSASGameRecordAirStrike(CvUnit const* pUnit, CvUnit const* pDefender, int iDefenderDamageBefore, int iDefenderDamageAfter);
void logSASGameRecordAirInterception(CvUnit const* pAttacker, CvUnit const* pInterceptor, CvPlot const* pTargetPlot, int iAttackerDamageTaken, int iInterceptorDamageTaken);
void logSASGameRecordAirBombPlot(CvUnit const* pUnit, CvPlot const* pTargetPlot, char const* szTargetKind, char const* szTarget, bool bSuccess);
// <!-- custom: Record each actual nuke launch with its resolved interception result and pre-detonation target/affected-team context.
// pabAffectedTeams is the caller's MAX_TEAMS copy of CvUnit::nuke's already-computed victim map; call only at level 2+. (ChatGPT-5.6-Sol) -->
void logSASGameRecordNukeLaunched(CvUnit const* pUnit, CvPlot const* pTargetPlot, bool const* pabAffectedTeams, bool bIntercepted, TeamTypes eBestInterceptorTeam, int iInterceptionChance);
// <!-- custom: Record realized post-detonation damage totals already gathered by CvPlot::nukeExplosion. Call only at level 2+. (ChatGPT-5.6-Sol) -->
void logSASGameRecordNukeEffects(CvUnit const* pUnit, CvPlot const* pTargetPlot, int iFalloutPlotsCreated, int iImprovementsDestroyed, int iFeaturesDestroyed, int iUnitsDamaged, int iUnitsKilled, int iBuildingsDestroyed, int iCitiesAffected, int iPopulationKilled);
// <!-- custom: pBattlePlot is the actual target supplied by CvUnit; deriving it from pLoser is wrong when the attacker loses. See KI#377. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
void logSASGameRecordCombatResult(CvUnit const* pWinner, CvUnit const* pLoser, CvPlot const* pBattlePlot);
void logSASGameRecordBonusChanged(CvPlot const* pPlot, BonusTypes eOldBonus, BonusTypes eNewBonus);

#define gGameRecordLogLevel getSASGameRecordLogLevel() // <!-- custom: Structured game-state/action record for autoplay comparison and external review, independent from the classic BBAI master switch. (ChatGPT-5.5 + GPT-5.5) -->
#define gGameRecordTurnInterval getSASGameRecordTurnInterval() // <!-- custom: Periodic game-record snapshot interval in game turns. (ChatGPT-5.5) -->

void logSASGameRecord(TCHAR* format, ... );

#endif
