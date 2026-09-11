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
// <!-- custom: The incremental AdvCiv 1.14 port currently uses this player-turn helper only for finalized research-target observation; mature AdvCiv-SAS also accumulates session Golden-Age/anarchy counters here, which can be added with their later action-history slice. (ChatGPT-5.6-Sol) -->
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
void logSASGameRecordGreatGeneralAttached(CvUnit const* pGreatGeneral, CvUnit const* pTargetUnit, PromotionTypes ePromotion);
void logSASGameRecordUnitScrapped(CvUnit const* pUnit);
void logSASGameRecordUnitUpgraded(CvUnit const* pOldUnit, CvUnit const* pNewUnit, int iCost);
void logSASGameRecordUnitCaptured(PlayerTypes eOldOwner, UnitTypes eOldUnitType, CvUnit const* pNewUnit);
// <!-- custom: War lifecycle hooks preserve factual declaration/cascade and peace context at the authoritative CvTeam boundaries. The incremental 1.14 port intentionally leaves mature per-war aggregate summaries for a later slice. (ChatGPT-5.6-Sol) -->
void logSASGameRecordWarStarted(TeamTypes eDeclarer, TeamTypes eTarget, WarPlanTypes eWarPlan, bool bPrimaryDoW, bool bNewDiplo, PlayerTypes eSponsor, bool bRandomEvent, WarDeclarationCause eCause);
void logSASGameRecordWarEnded(TeamTypes eTeam, TeamTypes eOtherTeam, int iTeamAWarSuccess, int iTeamBWarSuccess, bool bCapitulate, TeamTypes eBroker, bool bRandomEvent, bool bReparations);
void logSASGameRecordWarPlanChanged(TeamTypes eTeam, TeamTypes eTarget, WarPlanTypes eOldWarPlan, WarPlanTypes eNewWarPlan, bool bWar, int iOldStateCounter);
// <!-- custom: Authoritative religion/corporation founding and realized city membership changes. Spread-attempt/failure provenance remains a separate later CvUnit layer. (ChatGPT-5.6-Sol) -->
void logSASGameRecordReligionFounded(ReligionTypes eReligion, PlayerTypes ePlayer);
void logSASGameRecordCorporationFounded(CorporationTypes eCorporation, PlayerTypes ePlayer);
void logSASGameRecordReligionChanged(ReligionTypes eReligion, PlayerTypes ePlayer, CvCity const* pCity, bool bAdded);
void logSASGameRecordCorporationChanged(CorporationTypes eCorporation, PlayerTypes ePlayer, CvCity const* pCity, bool bAdded);
// <!-- custom: Team-relationship lifecycle actions make first contact, permanent-alliance merges and vassal-state changes explicit instead of forcing snapshot consumers to infer their exact turn. (ChatGPT-5.6-Sol) -->
void logSASGameRecordTeamMerged(TeamTypes eSurvivingTeam, TeamTypes eAbsorbedTeam);
void logSASGameRecordTeamMet(TeamTypes eTeam, TeamTypes eOtherTeam, bool bNewDiplo, int iX1, int iY1, int iX2, int iY2, CvPlot const* pTeamContactPlot, CvPlot const* pOtherContactPlot);
void logSASGameRecordVassalState(TeamTypes eMaster, TeamTypes eVassal, bool bVassal);
#define gGameRecordLogLevel getSASGameRecordLogLevel() // <!-- custom: Structured game-state/action record for autoplay comparison and external review, independent from the classic BBAI master switch. (ChatGPT-5.5 + GPT-5.5) -->
#define gGameRecordTurnInterval getSASGameRecordTurnInterval() // <!-- custom: Periodic game-record snapshot interval in game turns. (ChatGPT-5.5) -->

void logSASGameRecord(TCHAR* format, ... );

#endif // SAS_GAME_RECORD_LOG_H
