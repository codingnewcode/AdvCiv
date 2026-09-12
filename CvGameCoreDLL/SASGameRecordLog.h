// AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
// (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)

#pragma once

#ifndef SAS_GAME_RECORD_LOG_H
#define SAS_GAME_RECORD_LOG_H

// <!-- custom: Structured game-record rows for autoplay comparison, game analysis, user-assistance summaries, and external LLM review. This is not a classic BBAI diagnostic category: it has its own XML defines, its own SASGameRecord_*.log files, and its own lightweight public header. Call sites should still gate before invoking helpers so disabled logging does not compute logging-only arguments. Pointer-only hooks use forward declarations here to avoid pulling city/unit headers into ordinary game files. (ChatGPT-5.5 + GPT-5.5) -->
bool isSASGameRecordLogEnabled();
int getSASGameRecordLogLevel();
int getSASGameRecordTurnInterval();
void startSASGameRecordLogForNewGame();
void logSASGameRecordNewGameStarted();
void startSASGameRecordLogForLoadedSave();
void logSASGameRecordTurn(int iGameTurn);
// <!-- custom: High-level research-plan mutations can tag a shared ResearchTargetChangeCause for the recorder; the later player-turn observer emits it only if an invested incomplete-tech redirection actually materializes. (ChatGPT-5.6-Sol) -->
void noteSASGameRecordResearchTargetChangeCause(PlayerTypes ePlayer, ResearchTargetChangeCause eCause);
// <!-- custom: Preserve the exact fresh-research/carried-overflow split only for level-2 ordinary research application; the recorder consumes it if that same call completes the technology. (ChatGPT-5.6-Sol) -->
void noteSASGameRecordResearchApplication(PlayerTypes ePlayer, TechTypes eTech, int iModifiedResearchRate, int iIncomingOverflowUnmodified, int iIncomingOverflowModified);
// <!-- custom: This mixed-level player-turn helper observes finalized research-target changes and accumulates the session-local Golden Age/anarchy duration counters used by their lifecycle rows. Only the research-target comparison self-gates at level 2+. (ChatGPT-5.6-Sol) -->
void updateSASGameRecordPlayerTurnState(PlayerTypes ePlayer);
// <!-- custom: Research completion has its own accounting row because generic TECH_ACQUIRED also covers trades, free technologies, espionage and other sources where research overflow fields would be meaningless. Call only for actual TECH_ACQUISITION_RESEARCH threshold crossings at level 2+. (ChatGPT-5.6-Sol) -->
void logSASGameRecordResearchCompleted(TechTypes eTech, TeamTypes eTeam, PlayerTypes ePlayer, int iProgressBefore, int iProgressBeforePostCompletionAdjustment, int iResearchModifier, int iUnmodifiedOverflow);
// <!-- custom: Added eCause so the TECH_ACQUIRED action can name its explicit source without inferring provenance from announcement or first-discovery flags. (GPT-5.6-Sol + GPT-5.6 Thinking) -->
void logSASGameRecordTechAcquired(TechTypes eType, TeamTypes eTeam, PlayerTypes ePlayer, TechAcquisitionCause eCause);
// <!-- custom: City lifecycle actions use the existing Civ4 event boundaries; razing is bracketed directly around CvPlayer::disband so one row can preserve both the live city and exact post-destruction empire/victory consequences. (ChatGPT-5.6-Sol) -->
void logSASGameRecordCityBuilt(CvCity const* pCity);
void beginSASGameRecordCityRaze(CvCity const* pCity, PlayerTypes ePlayer);
void endSASGameRecordCityRaze(PlayerTypes ePlayer);
void logSASGameRecordCityAcquired(PlayerTypes eOldOwner, PlayerTypes eNewOwner, CvCity const* pCity, bool bConquest, bool bTrade);
// <!-- custom: Exact level-3 combat chronology keeps transient attacker/target context across Civ4's combat callbacks. Aggregate battle-quality is now complemented by later military-flow hooks; per-war statistics remain a later slice so this batch cannot emit partially populated summary counters. (ChatGPT-5.6-Sol) -->
void noteSASGameRecordCombatStarted(CvUnit const* pAttacker, CvUnit const* pDefender, CvPlot const* pBattlePlot);
void logSASGameRecordNonlethalCombat(CvUnit const* pAttacker, CvUnit const* pDefender, CvPlot const* pBattlePlot, bool bCombatLimitReached);
void logSASGameRecordCombatResult(CvUnit const* pWinner, CvUnit const* pLoser, CvPlot const* pBattlePlot);
// <!-- custom: Level-3 tactical air/bombard rows distinguish actual unit use from merely owning the capability. Consecutive equivalent city bombardments are synthesized compactly; interrupted higher-level missions are not guessed from the interception boundary. (GPT-5.6 + ChatGPT-5.6-Sol) -->
void logSASGameRecordCityBombard(CvUnit const* pUnit, CvCity const* pCity, char const* szMode, int iBombardRate, bool bIgnoreBuildingDefense, int iDefenseModifierBefore, int iDefenseDamageBefore);
void logSASGameRecordAirStrike(CvUnit const* pUnit, CvUnit const* pDefender, int iDefenderDamageBefore, int iDefenderDamageAfter);
void logSASGameRecordAirInterception(CvUnit const* pAttacker, CvUnit const* pInterceptor, CvPlot const* pTargetPlot, int iAttackerDamageTaken, int iInterceptorDamageTaken);
void logSASGameRecordAirBombPlot(CvUnit const* pUnit, CvPlot const* pTargetPlot, char const* szTargetKind, char const* szTarget, bool bSuccess);
// <!-- custom: Nuclear telemetry brackets each actual launch at the interception boundary and reuses the existing explosion pass for realized strategic/tactical effects. Level 2 records launch, aggregate effects and per-city consequences; level 3 retains exact affected-unit identities/damage. (ChatGPT-5.6-Sol) -->
void logSASGameRecordNukeLaunched(CvUnit const* pUnit, CvPlot const* pTargetPlot, bool const* pabAffectedTeams, bool bIntercepted, TeamTypes eBestInterceptorTeam, int iInterceptionChance);
void logSASGameRecordNukeEffects(CvUnit const* pUnit, CvPlot const* pTargetPlot, int iFalloutPlotsCreated, int iImprovementsDestroyed, int iFeaturesDestroyed, int iUnitsDamaged, int iUnitsKilled, int iBuildingsDestroyed, int iCitiesAffected, int iPopulationKilled);
void logSASGameRecordNukeCityEffect(CvUnit const* pNukeUnit, CvCity const* pCity, int iPopulationBefore, int iNukeModifier, std::vector<BuildingTypes> const& aeBuildingsDestroyed);
void logSASGameRecordNukeUnitEffect(CvUnit const* pNukeUnit, CvUnit const* pAffectedUnit, CvPlot const* pPlot, int iDamageBefore, int iDamageAfter, bool bKilled, char const* szCause);
// <!-- custom: Observe one AI_chooseProduction call as a scope so every early return is handled without teaching the AI decision tree about recorder schema.
// At level 2+, the destructor compares the final head order with the entry state and records only meaningful switches, clears, or resumptions of stored production. The disabled level-0/1 path stays a null-pointer check. (ChatGPT-5.6-Sol) -->
class SASGameRecordAIProductionChoiceScope
{
public:
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
// <!-- custom: Production-resolution hooks preserve exact completion, overflow and stored-production loss at authoritative CvCity boundaries; compact interval production-flow rows summarize the same evidence at level 2+. (ChatGPT-5.6-Sol) -->
void logSASGameRecordUnitCompleted(CvCity const* pCity, CvUnit const* pUnit, bool bConscripted, int iRawModifiedOverflow = 0, int iUnmodifiedOverflow = 0, int iKeptOverflow = 0, int iLostProduction = 0, int iUnusedOverflowCapacity = 0, int iOverflowGold = 0);
void logSASGameRecordBuildingCompletedByProduction(CvCity const* pCity, BuildingTypes eBuilding, int iRawModifiedOverflow, int iUnmodifiedOverflow, int iKeptOverflow, int iLostProduction, int iUnusedOverflowCapacity, int iOverflowGold);
void logSASGameRecordBuildingBuilt(CvCity const* pCity, BuildingTypes eBuilding);
void logSASGameRecordProjectBuilt(CvCity const* pCity, ProjectTypes eProject, int iRawModifiedOverflow, int iUnmodifiedOverflow, int iKeptOverflow, int iLostProduction, int iUnusedOverflowCapacity, int iOverflowGold);
void logSASGameRecordProductionOverflow(CvCity const* pCity, int iRawModifiedOverflow, int iUnmodifiedOverflow, int iKeptOverflow, int iLostProduction, int iUnusedCapacity, int iGold);
void logSASGameRecordProductionFailed(CvCity const* pCity, int iOrderData, bool bProject, int iInvestedProduction, int iGold);
void logSASGameRecordProductionDecay(CvCity const* pCity, OrderTypes eOrder, int iData1, int iBefore, int iAfter, int iInactiveTurns);
void logSASGameRecordProductionInvalidated(CvCity const* pCity, OrderTypes eOrder, int iData1, int iStoredLost, bool bActiveTarget, bool bQueued);
void logSASGameRecordProductionUpgraded(CvCity const* pCity, UnitTypes eOldUnit, UnitTypes eNewUnit, int iProductionTransferred, int iDestinationProductionBefore);
// <!-- custom: Compact military-quality flow records XP generation/caps, promotion choices and unit lifecycle changes at level 2+, with exact common actions retained at level 3 where useful. (ChatGPT-5.6-Sol) -->
void logSASGameRecordExperienceChange(CvUnit const* pUnit, int iAdjustedChange, int iActualChange, bool bFromCombat);
void logSASGameRecordUnitPromoted(CvUnit const* pUnit, PromotionTypes ePromotion);
// <!-- custom: Great Person lifecycle records exact birth, realized use and genuine death outcomes without logging candidate mission values. (ChatGPT-5.6-Sol) -->
void logSASGameRecordGreatPersonBorn(CvUnit const* pUnit, PlayerTypes ePlayer, CvCity const* pCity);
void logSASGameRecordGreatPersonJoined(CvUnit const* pUnit, CvCity const* pCity, SpecialistTypes eSpecialist);
void logSASGameRecordGreatPersonConstructed(CvUnit const* pUnit, CvCity const* pCity, BuildingTypes eBuilding);
void logSASGameRecordGreatPersonDiscovered(CvUnit const* pUnit, TechTypes eTech, int iResearch);
void logSASGameRecordGreatPersonHurried(CvUnit const* pUnit, CvCity const* pCity, BuildingTypes eBuilding, int iProduction);
void logSASGameRecordGreatPersonTradeMission(CvUnit const* pUnit, CvCity const* pCity, int iGold);
void logSASGameRecordGreatPersonGreatWork(CvUnit const* pUnit, CvCity const* pCity, int iCulture);
void logSASGameRecordGreatPersonInfiltrated(CvUnit const* pUnit, CvCity const* pCity, int iEspionage);
void logSASGameRecordGreatPersonGoldenAgeConsumed(CvUnit const* pUnit);
void logSASGameRecordGreatPersonDied(CvUnit const* pUnit, PlayerTypes eResponsiblePlayer, char const* szCause, CvPlot const* pDeathPlot = NULL);
// <!-- custom: Completed espionage missions and actual Spy interceptions preserve exact EP cost/outcome plus readable pre-mission target provenance; selection reasoning remains outside GameRecord. (ChatGPT-5.6-Sol) -->
void logSASGameRecordEspionageMission(CvUnit const* pUnit, EspionageMissionTypes eMission, PlayerTypes eTargetPlayer, CvPlot const* pPlot, int iExtraData, int iCost, int iEPBefore, int iEPAfter, ImprovementTypes eTargetImprovement, RouteTypes eTargetRoute, UnitTypes eTargetUnit, int iEffectValue, char const* szEffectKind);
void logSASGameRecordSpyIntercepted(CvUnit const* pUnit, PlayerTypes eTargetPlayer, char const* szPhase, int iModifier, int iInterceptChanceX100, EspionageMissionTypes eMission, int iExtraData, ImprovementTypes eTargetImprovement, RouteTypes eTargetRoute, UnitTypes eTargetUnit);
void logSASGameRecordGreatGeneralAttached(CvUnit const* pGreatGeneral, CvUnit const* pTargetUnit, PromotionTypes ePromotion);
void logSASGameRecordUnitScrapped(CvUnit const* pUnit);
void logSASGameRecordUnitUpgraded(CvUnit const* pOldUnit, CvUnit const* pNewUnit, int iCost);
void logSASGameRecordUnitCaptured(PlayerTypes eOldOwner, UnitTypes eOldUnitType, CvUnit const* pNewUnit);
// <!-- custom: Goody huts can resolve randomized gold/research/map/unit/combat effects and optional same-sign follow-up outcomes. Preserve the realized result as one compact level-2 record; level 3 can additionally identify exact hostile units through BARBARIAN_UNIT_SPAWNED. (ChatGPT-5.6-Sol) -->
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
// <!-- custom: Fog-created Barbarian units bypass ordinary production/completion history; preserve exact realized spawn cause/location at level 3. Goody-hut hostile spawns use the same detailed unit row. (ChatGPT-5.6-Sol) -->
void logSASGameRecordBarbarianSpawn(CvUnit const* pUnit, char const* szCause);
void logSASGameRecordGoodyReceived(PlayerTypes ePlayer, CvPlot const* pPlot, CvUnit const* pTriggerUnit, GoodyTypes eGoody, SASGameRecordGoodyResult const& kResult);
void logSASGameRecordGoodyNoOutcome(PlayerTypes ePlayer, CvPlot const* pPlot, CvUnit const* pTriggerUnit, GoodyTypes eTaboo, int iAttempts);
// <!-- custom: Required by random-event APIs using CvPlayer references; this lightweight declaration fixes the resulting AgentIterator/CvPlayer compile errors without adding a new include dependency. (GPT-5.6-Sol) -->
class CvPlayer;
// <!-- custom: Random-event lifecycle diagnostics pass the existing player-local trigger payload by const pointer/reference without exposing its save-layout definition through this lightweight recorder header. (ChatGPT-5.6-Sol) -->
struct EventTriggeredData;
// <!-- custom: Random-event city-result logging snapshots only realized city state that can otherwise disappear between periodic rows. The caller captures before/after only at level 2+, so disabled logging pays no city-query cost. (ChatGPT-5.6-Sol) -->
struct SASGameRecordRandomEventCityState
{
	SASGameRecordRandomEventCityState();
	explicit SASGameRecordRandomEventCityState(CvCity const& kCity, EventTypes eEvent);
	int iPopulation;
	int iFood;
	int iFoodYield;
	int iProductionYield;
	int iCommerceYield;
	int iGoldRate;
	int iResearchRate;
	int iCultureRate;
	int iEspionageRate;
	int iOwnerCultureTimes100;
	int iOccupationTurns;
	int iCultureUpdateTurns;
	int iExtraHappiness;
	int iExtraHealth;
	int iHurryAngerTurns;
	int iHappinessTurns;
	int iAngryPopulation;
	int iHappyLevel;
	int iUnhappyLevel;
	int iGoodHealth;
	int iBadHealth;
	int iSpaceProductionModifier;
	int iFreeSpecialistInstances;
	BuildingTypes eBuilding;
	int iRealBuildingCount;
};
// <!-- custom: Unit-local EventInfo effects can heal/award XP, immobilize, rename, promote or disband the selected stored unit.
// Snapshot only the fields consumed by CvUnit::applyEvent so the result row remains compact and gameplay-owned semantics stay authoritative. (ChatGPT-5.6-Sol) -->
struct SASGameRecordRandomEventUnitState
{
	SASGameRecordRandomEventUnitState();
	explicit SASGameRecordRandomEventUnitState(CvUnit const& kUnit, EventTypes eEvent);
	int iExists;
	int iUnitId;
	UnitTypes eUnit;
	UnitAITypes eUnitAI;
	int iX;
	int iY;
	int iDamage;
	int iExperience;
	int iImmobileTurns;
	PromotionTypes ePromotion;
	int iHasPromotion;
};
// <!-- custom: Player-level EventInfo consequences can otherwise be visible only indirectly at a later snapshot.
// Keep this to durable native values not already covered by dedicated gold/tech/Golden-Age/war rows. (ChatGPT-5.6-Sol) -->
struct SASGameRecordRandomEventPlayerState
{
	SASGameRecordRandomEventPlayerState();
	SASGameRecordRandomEventPlayerState(CvPlayer const& kPlayer, EventTypes eEvent, PlayerTypes eOtherPlayer);
	int iExtraHappiness;
	int iExtraHealth;
	int iBaseFreeUnits;
	int iSpaceProductionModifier;
	int iInflationRate;
	int iEspionagePointsAgainstOther;
	BonusTypes eBonusRevealed;
	int iForceRevealedBonus;
};
// <!-- custom: Random-event trigger delivery and reply/application lifecycle are recorded at level 2+ without changing Base AdvCiv 1.14 event semantics. Realized payload details remain separate follow-up slices. (ChatGPT-5.6-Sol) -->
void logSASGameRecordRandomEventTriggered(CvPlayer const& kPlayer, EventTriggeredData const& kTriggeredData, char const* szDeliveryPath);
void logSASGameRecordRandomEventNoSelection(CvPlayer const& kPlayer, EventTriggeredData const& kTriggeredData, char const* szResolution);
void logSASGameRecordRandomEventApply(CvPlayer const& kPlayer, EventTypes eEvent, int iTriggeredId, EventTriggeredData const* pTriggeredData, bool bUpdateTrigger, char const* szDisposition, int iCanDoEvent, int iTriggerFiredBefore, int iEventOccurredBefore);
void logSASGameRecordRandomEventGoldResult(CvPlayer const& kPlayer, EventTypes eEvent, int iTriggeredId, int iRangeLow, int iRangeHigh, int iPlayerGoldDelta, PlayerTypes eOtherPlayer, bool bGoldToPlayer);
void logSASGameRecordRandomEventTechResult(CvPlayer const& kPlayer, EventTypes eEvent, int iTriggeredId, TechTypes eTech, int iTechPercent, int iResearchBefore, int iBeakersApplied, int iResearchAfter, int iTechCost, int iCompleted);
// <!-- custom: Preserve realized deterministic city consequences separately from EventInfo selection. This complements canonical gold/tech/lifecycle rows without dumping static XML magnitudes into RANDOM_EVENT_APPLY. Call only at level 2+ with caller-captured before/after state. (ChatGPT-5.6-Sol) -->
void logSASGameRecordRandomEventCityResult(PlayerTypes ePlayer, PlayerTypes eAffectedPlayer, int iTriggeredId, EventTypes eEvent, char const* szScope, CvCity const& kCity, SASGameRecordRandomEventCityState const& kBefore, SASGameRecordRandomEventCityState const& kAfter);
void logSASGameRecordRandomEventBuildingModifierResults(CvPlayer const& kPlayer, EventTypes eEvent, int iTriggeredId, char const* szScope, CvCity const* pCity);
// <!-- custom: Preserve concrete stored-unit random-event consequences around the existing CvUnit::applyEvent call; caller snapshots only when one of its five unit-local EventInfo fields is present. (ChatGPT-5.6-Sol) -->
void logSASGameRecordRandomEventUnitResult(PlayerTypes ePlayer, int iTriggeredId, EventTypes eEvent, SASGameRecordRandomEventUnitState const& kBefore, SASGameRecordRandomEventUnitState const& kAfter);
void logSASGameRecordRandomEventPlayerResult(CvPlayer const& kPlayer, EventTypes eEvent, int iTriggeredId, PlayerTypes eOtherPlayer, SASGameRecordRandomEventPlayerState const& kBefore, SASGameRecordRandomEventPlayerState const& kAfter);
// <!-- custom: EventInfos can grant persistent free promotions to a UnitCombat or UnitClass while updating existing matching units immediately; record realized scope state without one row per unit. (ChatGPT-5.6-Sol) -->
void logSASGameRecordRandomEventFreePromotionResult(CvPlayer const& kPlayer, EventTypes eEvent, int iTriggeredId, char const* szScope, int iScopeId, PromotionTypes ePromotion, int iExistingUnitsNewlyPromoted, int iFreePromotionBefore, int iFreePromotionAfter);
// <!-- custom: EventInfo free units bypass normal production history; record the already resolved class/type, requested versus realized count and actual spawn city. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
void logSASGameRecordRandomEventFreeUnitsResult(PlayerTypes ePlayer, PlayerTypes eAffectedPlayer, EventTypes eEvent, int iTriggeredId, UnitClassTypes eUnitClass, UnitTypes eUnit, int iRequestedCount, int iCreatedCount, CvCity const* pSpawnCity);
// <!-- custom: Successful random-event occurrence clears and delayed follow-up scheduling are durable lifecycle state, so record them only after the original chance/scope/earliest-turn logic has resolved. (ChatGPT-5.6-Sol) -->
void logSASGameRecordRandomEventOccurrenceCleared(CvPlayer const& kPlayer, EventTypes eSourceEvent, EventTypes eClearedEvent, int iTriggeredId, int iClearChance, char const* szScope, TeamTypes eScopeTeam, int iScopePlayerSlots, int iScopeEverAlivePlayers, int iClearedOccurrences);
// <!-- custom: Preserve configured pillage endpoints and the already-realized attempt/destruction counts without modifying Base AdvCiv 1.14's existing random roll semantics. (GPT-5.6-Sol) -->
void logSASGameRecordRandomEventPillageResult(char const* szScope, PlayerTypes ePlayer, PlayerTypes eAffectedPlayer, int iCityId, int iTriggeredId, EventTypes eEvent, int iMinPillage, int iMaxPillage, int iAttempts, int iDestroyed);
void logSASGameRecordRandomEventCountdownScheduled(CvPlayer const& kPlayer, EventTypes eSourceEvent, EventTypes eFollowupEvent, int iTriggeredId, int iRequestedDueTurn, int iPreviousDueTurn, int iScheduledDueTurn);
// <!-- custom: War lifecycle hooks preserve factual declaration/cascade and peace context at the authoritative CvTeam boundaries. The incremental 1.14 port intentionally leaves mature per-war aggregate summaries for a later slice. (ChatGPT-5.6-Sol) -->
void logSASGameRecordWarStarted(TeamTypes eDeclarer, TeamTypes eTarget, WarPlanTypes eWarPlan, bool bPrimaryDoW, bool bNewDiplo, PlayerTypes eSponsor, bool bRandomEvent, WarDeclarationCause eCause);
void logSASGameRecordWarEnded(TeamTypes eTeam, TeamTypes eOtherTeam, int iTeamAWarSuccess, int iTeamBWarSuccess, bool bCapitulate, TeamTypes eBroker, bool bRandomEvent, bool bReparations);
void logSASGameRecordWarPlanChanged(TeamTypes eTeam, TeamTypes eTarget, WarPlanTypes eOldWarPlan, WarPlanTypes eNewWarPlan, bool bWar, int iOldStateCounter);
// <!-- custom: Consequential city economy/lifecycle actions complement periodic snapshots: hurrying preserves the exact realized production/cost mutation, natural growth/starvation is interval-compacted at level 2 while level 3 preserves exact transitions, and culture expansion uses its authoritative event boundary. (ChatGPT-5.6-Sol) -->
void logSASGameRecordCityHurry(CvCity const* pCity, HurryTypes eHurry, int iProductionBefore, int iProductionAdded, int iGoldCost, int iPopulationCost, int iHurryAngerAdded, int iGoldBefore, int iPopulationBefore, int iHurryAngerBefore);
// <!-- custom: Pillage and naval-blockade actions preserve the acting unit, exact structure/economic result and persistent blockade lifetime that periodic snapshots cannot reconstruct. (ChatGPT-5.6-Sol) -->
void logSASGameRecordPillage(CvUnit const* pUnit, ImprovementTypes eOldImprovement, RouteTypes eOldRoute, BonusTypes eOldBonus, PlayerTypes eVictimPlayer, int iGoldGained);
void logSASGameRecordBlockadeChanged(CvUnit const* pUnit, bool bStarting);
void logSASGameRecordBlockadePlunder(CvUnit const* pUnit, CvCity const* pCity, int iGold, int iTradeRoutes, int iProfitPerRoute);
// <!-- custom: Preserve authoritative one-shot unit/team transitions that periodic snapshots cannot attribute precisely: completed unit gifts and the global circumnavigation winner/sea-movement effect. (ChatGPT-5.6-Sol) -->
void logSASGameRecordUnitGifted(CvUnit const* pUnit, PlayerTypes eGiftingPlayer, CvPlot const* pPlotLocation);
void logSASGameRecordCircumnavigated(TeamTypes eTeam, int iFreeSeaMoves, bool bBonusApplied, int iSeaExtraMovesBefore, int iSeaExtraMovesAfter);
void logSASGameRecordCityGrowthPrevented(CvCity const* pCity, int iFoodDiscarded);
void logSASGameRecordCityPopulationChanged(CvCity const* pCity, bool bGrowth, int iPopulationBefore, int iFoodDifference, int iFoodBefore, int iFoodAfterDifference, int iFoodKeptBefore, int iFoodKeptBeforePopulationChange, int iGrowthThresholdBefore);
void logSASGameRecordCityCultureExpanded(CvCity const* pCity);
// <!-- custom: Authoritative religion/corporation founding and city membership changes, plus consumed Missionary/Executive spread-attempt provenance for failures and displacement outcomes. (ChatGPT-5.6-Sol) -->
void logSASGameRecordReligionFounded(ReligionTypes eReligion, PlayerTypes ePlayer);
void logSASGameRecordCorporationFounded(CorporationTypes eCorporation, PlayerTypes ePlayer);
void logSASGameRecordReligionChanged(ReligionTypes eReligion, PlayerTypes ePlayer, CvCity const* pCity, bool bAdded);
void logSASGameRecordReligionSpreadAttempt(CvUnit const* pUnit, ReligionTypes eReligion, CvCity const* pCity, int iDirectSpreadChance, bool bSuccess, ReligionTypes eDisplacedReligion);
void logSASGameRecordCorporationChanged(CorporationTypes eCorporation, PlayerTypes ePlayer, CvCity const* pCity, bool bAdded);
void logSASGameRecordCorporationSpreadAttempt(CvUnit const* pUnit, CorporationTypes eCorporation, CvCity const* pCity, int iSpreadChance, int iGoldCost, int iGoldBefore, bool bSuccess);
// <!-- custom: Financial strikes can begin, force unit disbands and end between periodic snapshots; preserve each realized strike turn from the values already produced by CvPlayer::doGold. (ChatGPT-5.6-Sol) -->
void logSASGameRecordFinancialStrikeTurn(PlayerTypes ePlayer, int iGoldBefore, int iCalculatedGoldRate, int iGoldAfterClamp, int iCumulativeStrikeTurns, int iUnitsBeforeDisband, int iUnitsAfterDisband);
// <!-- custom: Exact Golden Age/anarchy lifecycle actions complement periodic remaining-turn snapshots; logged duration counters are session-local observations. (ChatGPT-5.6-Sol) -->
void logSASGameRecordGoldenAge(PlayerTypes ePlayer, bool bStart);
void logSASGameRecordGoldenAgeTurnsChanged(PlayerTypes ePlayer, int iChange, int iOldGoldenAgeTurns, int iNewGoldenAgeTurns);
void logSASGameRecordAnarchy(PlayerTypes ePlayer, bool bStart);
// <!-- custom: Exact post-initialization civic and remembered state-religion transitions complement periodic policy snapshots without changing policy/religion gameplay. (ChatGPT-5.6-Sol) -->
void logSASGameRecordCivicChanged(PlayerTypes ePlayer, CivicOptionTypes eCivicOption, CivicTypes eOldCivic, CivicTypes eNewCivic, ReligionTypes eOldEffectiveStateReligion, ReligionTypes eNewEffectiveStateReligion);
void logSASGameRecordLastStateReligionChanged(PlayerTypes ePlayer, ReligionTypes eOldReligion, ReligionTypes eNewReligion);
// <!-- custom: Explicit victory/spaceship lifecycle rows preserve launch timing, arrival failure/reset causes, the final winner, and an exact victory-time snapshot instead of inferring them from project counts or later game state. (ChatGPT-5.6-Sol) -->
void logSASGameRecordVictoryLaunched(PlayerTypes ePlayer, VictoryTypes eVictory);
void logSASGameRecordVictoryProgressResetForCapital(CvCity const* pCapital);
void logSASGameRecordSpaceshipFailed(TeamTypes eTeam, VictoryTypes eVictory, int iLaunchSuccessPercent);
void logSASGameRecordVictory(TeamTypes eWinner, VictoryTypes eVictory);
// <!-- custom: Compact run/player lifecycle rows make elimination, later appearance/revival and the current run leader/winner state explicit instead of forcing consumers to infer them from missing periodic snapshots. (ChatGPT-5.6-Sol) -->
void logSASGameRecordRunStatus(char const* szReason);
void logSASGameRecordPlayerEliminated(PlayerTypes ePlayer);
void logSASGameRecordPlayerAliveChanged(PlayerTypes ePlayer, bool bRevived);
// <!-- custom: Diplomatic-vote lifecycle rows preserve the exact proposal/election boundary and the final weighted ballot/result before Civ4 clears stored votes or applies the resolution. (ChatGPT-5.6-Sol) -->
struct VoteTriggeredData;
void logSASGameRecordVoteTriggered(VoteTriggeredData const* pVoteTriggered);
void logSASGameRecordVoteResult(VoteTriggeredData const* pVoteTriggered, bool bThresholdPassed, bool bPassed, bool bCancelled, qword uiDefaultedAbstain, qword uiDefiers, qword uiEndorsers);
// <!-- custom: Resolved diplomacy rows capture factual help/demand/religion/civic/war/trade-request outcomes and their immediate relationship-state deltas; direct gold transfers use the existing EventReporter boundary. (ChatGPT-5.6-Sol) -->
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
void logSASGameRecordPlayerGoldTrade(PlayerTypes eFromPlayer, PlayerTypes eToPlayer, int iAmount);
// <!-- custom: Recorder-only control-session actions make AI Auto Play, active-player handoffs and successful Debug-mode toggles explicit without changing Base AdvCiv's autoplay API/signatures. (ChatGPT-5.6-Sol) -->
void logSASGameRecordAutoPlayChanged(int iOldValue, int iNewValue, bool bChangePlayerStatus);
void logSASGameRecordActivePlayerChanged(PlayerTypes eOldPlayer, PlayerTypes eNewPlayer);
void logSASGameRecordDebugModeChanged(bool bOldDebugMode, bool bNewDebugMode);
// <!-- custom: Team-relationship lifecycle actions make first contact, permanent-alliance merges and vassal-state changes explicit instead of forcing snapshot consumers to infer their exact turn. (ChatGPT-5.6-Sol) -->
void logSASGameRecordTeamMerged(TeamTypes eSurvivingTeam, TeamTypes eAbsorbedTeam);
void logSASGameRecordTeamMet(TeamTypes eTeam, TeamTypes eOtherTeam, bool bNewDiplo, int iX1, int iY1, int iX2, int iY2, CvPlot const* pTeamContactPlot, CvPlot const* pOtherContactPlot);
void logSASGameRecordVassalState(TeamTypes eMaster, TeamTypes eVassal, bool bVassal);
#define gGameRecordLogLevel getSASGameRecordLogLevel() // <!-- custom: Structured game-state/action record for autoplay comparison and external review, independent from the classic BBAI master switch. (ChatGPT-5.5 + GPT-5.5) -->
#define gGameRecordTurnInterval getSASGameRecordTurnInterval() // <!-- custom: Periodic game-record snapshot interval in game turns. (ChatGPT-5.5) -->

void logSASGameRecord(TCHAR* format, ... );

#endif // SAS_GAME_RECORD_LOG_H
