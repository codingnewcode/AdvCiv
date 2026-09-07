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
// <!-- custom: Exact level-3 combat chronology keeps transient attacker/target context across Civ4's combat callbacks. Aggregate battle-quality, XP-flow and per-war statistics remain a later slice so this batch cannot emit partially populated summary counters. (ChatGPT-5.6-Sol) -->
void noteSASGameRecordCombatStarted(CvUnit const* pAttacker, CvUnit const* pDefender, CvPlot const* pBattlePlot);
void logSASGameRecordNonlethalCombat(CvUnit const* pAttacker, CvUnit const* pDefender, CvPlot const* pBattlePlot, bool bCombatLimitReached);
void logSASGameRecordCombatResult(CvUnit const* pWinner, CvUnit const* pLoser, CvPlot const* pBattlePlot);
// <!-- custom: War lifecycle hooks preserve factual declaration/cascade and peace context at the authoritative CvTeam boundaries. The incremental 1.14 port intentionally leaves mature per-war aggregate summaries for a later slice. (ChatGPT-5.6-Sol) -->
void logSASGameRecordWarStarted(TeamTypes eDeclarer, TeamTypes eTarget, WarPlanTypes eWarPlan, bool bPrimaryDoW, bool bNewDiplo, PlayerTypes eSponsor, bool bRandomEvent, WarDeclarationCause eCause);
void logSASGameRecordWarEnded(TeamTypes eTeam, TeamTypes eOtherTeam, int iTeamAWarSuccess, int iTeamBWarSuccess, bool bCapitulate, TeamTypes eBroker, bool bRandomEvent, bool bReparations);
void logSASGameRecordWarPlanChanged(TeamTypes eTeam, TeamTypes eTarget, WarPlanTypes eOldWarPlan, WarPlanTypes eNewWarPlan, bool bWar, int iOldStateCounter);
#define gGameRecordLogLevel getSASGameRecordLogLevel() // <!-- custom: Structured game-state/action record for autoplay comparison and external review, independent from the classic BBAI master switch. (ChatGPT-5.5 + GPT-5.5) -->
#define gGameRecordTurnInterval getSASGameRecordTurnInterval() // <!-- custom: Periodic game-record snapshot interval in game turns. (ChatGPT-5.5) -->

void logSASGameRecord(TCHAR* format, ... );

#endif // SAS_GAME_RECORD_LOG_H
