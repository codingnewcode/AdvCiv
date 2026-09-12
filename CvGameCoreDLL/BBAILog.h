#pragma once

#ifndef BBAI_LOG_H
#define BBAI_LOG_H

// AI decision making logging

// Log levels:
// 0 - None
// 1 - Important decisions only
// 2 - Many decisions
// 3 - All logging
// <!-- custom: Base AdvCiv required recompiling the DLL to toggle BBAI logging through LOG_AI / LOG_FOUND_VALUE.
// AdvCiv-SAS exposes the existing log levels through cached XML defines instead, keeping the existing g*LogLevel call-site checks
// while allowing users/modders to enable logs without a special logging DLL. Values are clamped in BBAILog.cpp. (GPT-5.5?) -->
bool isSASBBAILogEnabled();
bool isSASBBAILogMasterEnabled();
int getSASBBAIPlayerLogLevel();
int getSASBBAITeamLogLevel();
int getSASBBAIWarLogLevel();
int getSASBBAICityLogLevel();
int getSASBBAIMilitaryProductionLogLevel();
int getSASBBAISpaceProductionLogLevel();
int getSASBBAICitizenLogLevel();
int getSASBBAIUnitLogLevel();
int getSASBBAIOverseasTransportLogLevel();
int getSASBBAIGreatGeneralLogLevel();
int getSASBBAISettlerLogLevel();
int getSASBBAIFoundLogLevel();
int getSASBBAIEvacuationLogLevel();
int getSASBBAIWorkerLogLevel();
int getSASBBAIWorkerSeaLogLevel();
int getSASBBAIMapLogLevel();
int getSASBBAIDealCancelLogLevel();
int getSASBBAICultureLogLevel();
int getSASBBAIScoreLogInterval();
void startSASBBAILogForNewGame(); // <!-- custom: Roll to a new timestamped BBAI diagnostic file before new-game map generation can log. (GPT-5.5 + GPT-5.5) -->
void logSASBBAINewGameStarted(); // <!-- custom: Log complete new-game identification after map and player initialization. (GPT-5.5) -->
void startSASBBAILogForLoadedSave(); // <!-- custom: Roll and identify a loaded save after its complete game state is read. (GPT-5.5) -->

#define gLogBBAI isSASBBAILogEnabled() // advc.007: So that BBAI logging can be checked in FAssert
#define gPlayerLogLevel getSASBBAIPlayerLogLevel()
#define gScoreLogInterval getSASBBAIScoreLogInterval() // advc.007: was hardcoded to 25 in CvPlayer::onTurnLogging
#define gTeamLogLevel getSASBBAITeamLogLevel()
#define gWarLogLevel getSASBBAIWarLogLevel() // <!-- custom: Separate war-plan and war-target diagnostics from broad TEAM logging. (GPT-5.5) -->
#define gCityLogLevel getSASBBAICityLogLevel()
#define gMilitaryProductionLogLevel getSASBBAIMilitaryProductionLogLevel() // <!-- custom: Dedicated AI city military-production diagnostics without enabling broad CITY logging. (ChatGPT-5.6-Sol) -->
#define gSpaceProductionLogLevel getSASBBAISpaceProductionLogLevel() // <!-- custom: Dedicated spaceship-project valuation, selection, and production-switch diagnostics without broad PLAYER/CITY logging. (GPT-5.6-Sol) -->
#define gCitizenLogLevel getSASBBAICitizenLogLevel() // <!-- custom: Separate citizen-assignment and plot-allocation diagnostics from general CITY logging. (GPT-5.5) -->
#define gUnitLogLevel getSASBBAIUnitLogLevel()
#define gOverseasTransportLogLevel getSASBBAIOverseasTransportLogLevel() // <!-- custom: Separate overseas military-cargo and Settler-transport diagnostics from broad CITY and UNIT logging. (GPT-5.6-Sol) -->
#define gGreatGeneralLogLevel getSASBBAIGreatGeneralLogLevel() // <!-- custom: Separate Great General action, Military Instructor, and Warlord attachment diagnostics from general UNIT logging. (GPT-5.5) -->
#define gSettlerLogLevel getSASBBAISettlerLogLevel() // <!-- custom: Separate Settler diagnostics from general UNIT logging so each can be inspected independently. (GPT-5.5) -->
#define gFoundLogLevel getSASBBAIFoundLogLevel() // advc.031c
#define gEvacuationLogLevel getSASBBAIEvacuationLogLevel() // <!-- custom: Separate evacuation/retreat diagnostics from general CITY and UNIT logging. (GPT-5.5) -->
#define gWorkerLogLevel getSASBBAIWorkerLogLevel() // <!-- custom: Separate land-Worker diagnostics from general UNIT logging. (ChatGPT-5.5 + GPT-5.5 review) -->
#define gWorkerSeaLogLevel getSASBBAIWorkerSeaLogLevel() // <!-- custom: Separate Work Boat / WORKER_SEA diagnostics from general UNIT and land-Worker logging. (ChatGPT-5.5 + GPT-5.5 review) -->
#define gMapLogLevel getSASBBAIMapLogLevel() // K-Mod
#define gDealCancelLogLevel getSASBBAIDealCancelLogLevel() // advc.133
#define gCultureLogLevel getSASBBAICultureLogLevel() // <!-- custom: Separate culture-victory diagnostics from general PLAYER and CITY logging. (ChatGPT-5.5) -->

void logBBAI(TCHAR* format, ... );
// <advc.133>
class CvDeal;
void logBBAICancel(CvDeal const& d, PlayerTypes eCancelPlayer, wchar const* szReason);
// </advc.133>

#endif  //BBAI_LOG_H
