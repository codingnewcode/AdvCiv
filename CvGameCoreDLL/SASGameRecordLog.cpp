#include "CvGameCoreDLL.h"
#include "SASGameRecordLog.h"
#include "CvGame.h" // <!-- custom: Needed for game-record turn, game-state, victory, RNG, and map-classification context rows. (GPT-5.5) -->
#include "CvCity.h" // <!-- custom: Needed by game-record city action/BFC rows; SASGameRecordLog.h only forward-declares CvCity. (GPT-5.5) -->
#include "CvCityAI.h" // <!-- custom: Needed only to read the existing Avoid Growth AI emphasis flag in city snapshots/aggregates; CvCity.h only forward-declares CvCityAI. This is a compile-time type dependency and does not alter AI state or gameplay. (ChatGPT-5.6-Sol) -->
#include "CvUnit.h" // <!-- custom: Needed by game-record battle rows; SASGameRecordLog.h only forward-declares CvUnit. (GPT-5.5) -->
#include "CombatOdds.h" // <!-- custom: Needed only for exact pre-combat odds on real level-2+ battle outcomes; AI candidate valuation remains untouched. (ChatGPT-5.6-Sol) -->
#include "CvUnitAI.h" // <!-- custom: Needed to inspect the head unit of large city groups and its UnitAI role; the base unit header only forward-declares CvUnitAI. (GPT-5.6-Sol) -->
#include "CityPlotIterator.h" // <!-- custom: Needed by compact game-record BFC composition rows. (ChatGPT-5.5) -->
#include "CvPlot.h" // <!-- custom: Needed by game-record BFC and unit posture rows. (ChatGPT-5.5) -->
#include "CvInfo_Build.h" // <!-- custom: Needed for worker build-type names and build target classification in game-record rows. (ChatGPT-5.5) -->
#include "CvInfo_Command.h" // <!-- custom: Needed for mission-type names in worker/settler game-record rows. (ChatGPT-5.5) -->
#include "CvInfo_Building.h" // <!-- custom: Needed to classify city production in game-record city rows. (ChatGPT-5.5) -->
#include "CvInfo_Tech.h" // <!-- custom: Needed to bucket owned-tech counts by era in game-record rows. (ChatGPT-5.5) -->
#include "CvInfo_RandomEvent.h" // <!-- custom: Needed for stable EventTrigger/EventInfo identities and gameplay-owned unit-local random-event semantics in dedicated random-event lifecycle rows. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
#include "CvInfo_Terrain.h" // <!-- custom: Needed for terrain/feature/bonus type names in game-record context rows. (ChatGPT-5.5) -->
#include "CvInfo_Organization.h" // <!-- custom: Needed for religion/corporation type names in game-record action rows. (ChatGPT-5.5) -->
#include "CvInfo_Unit.h" // <!-- custom: Needed to classify unit composition and city production in game-record rows. (ChatGPT-5.5) -->
#include "CvInfo_Symbol.h" // <!-- custom: Needed for commerce-slider type names in game-record economy rows. (GPT-5.5) -->
#include "CvInfo_City.h" // <!-- custom: Needed for specialist and process type names in game-record city rows. (ChatGPT-5.5) -->
#include "CvInfo_Civics.h" // <!-- custom: Needed for policy/civic names in game-record advisor rows. (ChatGPT-5.5) -->
#include "CvInfo_Civilization.h" // <!-- custom: Needed to attribute player-wide extra happiness/health to traits instead of leaving effects from loaded-mod rules under an opaque `extra` label. (GPT-5.6-Sol) -->
#include "CvCivilization.h" // <!-- custom: Needed to resolve civilization-specific BuildingClass types in realized random-event building/city result rows; CvPlayer/CvCity only forward-declare the runtime CvCivilization wrapper. This is a compile-time dependency only. (ChatGPT-5.6-Sol) -->
#include "CvInfo_GameOption.h" // <!-- custom: Needed to log enabled game-option type names; CvGlobals only forward-declares CvGameOptionInfo. (GPT-5.5) -->
#include "CvInfo_Misc.h" // <!-- custom: Needed to log enabled graphics-option type names; CvGlobals only forward-declares CvGraphicOptionInfo. (GPT-5.6-Sol) -->
#include "CvInfo_Symbol.h" // <!-- custom: Needed to log actual assigned player-color and primary-color context; CvGlobals only forward-declares their info classes. (GPT-5.6-Sol) -->
#include "CvMap.h" // <!-- custom: Needed to log map dimensions; CvGlobals only forward-declares CvMap. (GPT-5.5) -->
#include "CvSelectionGroup.h" // <!-- custom: Needed to inspect worker/settler mission queues in game-record rows. (ChatGPT-5.5) -->
#include "CvSelectionGroupAI.h" // <!-- custom: Needed for large city-group mission targets and MissionAI state; the base group header only forward-declares CvSelectionGroupAI. (GPT-5.6-Sol) -->
#include "CvPlotGroup.h" // <!-- custom: Needed to identify connected city networks in game-record city rows. (ChatGPT-5.5) -->
#include "CvArea.h" // <!-- custom: Needed for area-wide city happiness/health detail rows. (ChatGPT-5.5) -->
#include "CvPlayerAI.h" // <!-- custom: Needed for attitude/glance values in game-record advisor rows. (ChatGPT-5.5) -->
#include "AgentIterator.h" // <!-- custom: Needed directly for MemberIter in compact team-aware research-redirection context; do not rely on unrelated gameplay headers to provide the iterator transitively. (ChatGPT-5.6-Sol) -->
#include "CvTeamAI.h" // <!-- custom: Needed for team-level worst-enemy state in game-record diplomacy-status rows. (ChatGPT-5.5) -->
#include "CvGameAI.h" // <!-- custom: Complete CvGameAI is needed for getUWAI(). (ChatGPT-5.6-Sol) -->
#include "CvPythonCaller.h" // <!-- custom: Resolve map-option defaults and descriptions. (ChatGPT-5.6-Sol) -->
#include "CvStatistics.h" // <!-- custom: Needed for persistent player-record statistics in game-record benchmark rows. (GPT-5.5) -->
#include "CvGameCoreUtils.h" // <!-- custom: Needed for the shared DLL-process UTC identity used by BBAI and SASGameRecord. See KI#629. (GPT-5.6-Sol) -->
#include <time.h>
#include <psapi.h> // <!-- custom: Reuse AdvCiv's existing Windows process-memory API for compact game-record performance context. (GPT-5.6-Sol) -->
#include <algorithm> // <!-- custom: Needed to deduplicate buffered plot-change/map-revelation coordinates within each turn. (GPT-5.6-Sol) -->
#include <utility> // <!-- custom: Needed for Great Person odds pairs in game-record city rows. (ChatGPT-5.5) -->
#include <vector> // <!-- custom: Used for compact dynamic buckets in game-record known-area, BFC development, advisor, tech-era, worker/settler, and unit-composition rows. (ChatGPT-5.5) -->

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
// Use ACTION rows rather than generic EVENT rows to avoid confusion with Civ4 random events; dedicated GAME_RECORD_RANDOM_EVENT_* rows are reserved specifically for actual EventTrigger/EventInfo lifecycle boundaries.
// Keep the recorder portable across Civ4 mods by enumerating loaded XML and using generic field meanings instead of hardcoding AdvCiv-SAS types or copying the full XML.
// Mod-specific rules can still be named in comments as concrete examples: TECH_DEPOPULATION currently applies negative player-wide health and happiness in AdvCiv-SAS, but the recorder attributes health/happiness from every loaded trait, civic and technology dynamically.
// The record describes the current format; do not add schema-version maintenance unless independently evolving consumers later require it. (ChatGPT-5.5 + GPT-5.5 + ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
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

static bool isSASGameRecordPerformanceMetricsEnabled()
{
	static const bool bEnabled = (GC.getDefineINT("SAS_GAME_RECORD_PERFORMANCE_METRICS_ENABLE") > 0);
	return bEnabled;
}

// <!-- custom: Foreign-Advisor market snapshots are intentionally level-3-only and can be disabled independently because enumerating target-specific resource/technology offerability and denial reasons is more expensive than ordinary factual player snapshots.
// Cache the switch so disabled runs perform no repeated XML lookup or market scan. (ChatGPT-5.6-Sol) -->
static bool isSASGameRecordTradeMarketEnabled()
{
	static const bool bEnabled = (GC.getDefineINT("SAS_GAME_RECORD_TRADE_MARKET_ENABLE") > 0);
	return bEnabled;
}

static bool isSASGameRecordTradeMarketBonusGPTQuotesEnabled()
{
	static const bool bEnabled = (GC.getDefineINT("SAS_GAME_RECORD_TRADE_MARKET_BONUS_GPT_QUOTES_ENABLE") > 0);
	return bEnabled;
}

static bool isSASGameRecordTradeMarketAITechValuesEnabled()
{
	static const bool bEnabled = (GC.getDefineINT("SAS_GAME_RECORD_TRADE_MARKET_AI_TECH_VALUES_ENABLE") > 0);
	return bEnabled;
}

static int getSASGameRecordSystemContextLevel()
{
	static const int iLevel = std::min(3, std::max(0, GC.getDefineINT("SAS_GAME_RECORD_SYSTEM_CONTEXT_LEVEL")));
	return iLevel;
}

int getSASGameRecordTurnInterval()
{
	// <!-- custom: Separate snapshot frequency from detail level. Level 0 disables the game-record rows; the interval is still clamped so modulo callers are safe. (ChatGPT-5.5) -->
	static const int iInterval = std::max(1, GC.getDefineINT("SAS_GAME_RECORD_INTERVAL_TURNS_UNSCALED_GAMESPEED"));
	return iInterval;
}

// <!-- custom: Cache the tunable preview bounds and horizontal character ratio like the other SASGameRecord settings; 0 for either bound lets level 3 keep its other detail while omitting all text-map layers. (GPT-5.6-Sol) -->
static int getSASGameRecordMapAsciiMaxWidth()
{
	static const int iMaxWidth = std::max(0, GC.getDefineINT("SAS_GAME_RECORD_MAP_ASCII_MAX_WIDTH"));
	return iMaxWidth;
}

static int getSASGameRecordMapAsciiMaxHeight()
{
	static const int iMaxHeight = std::max(0, GC.getDefineINT("SAS_GAME_RECORD_MAP_ASCII_MAX_HEIGHT"));
	return iMaxHeight;
}

static int getSASGameRecordMapAsciiHorizontalCharsPerCell()
{
	static const int iChars = std::min(4, std::max(1, GC.getDefineINT("SAS_GAME_RECORD_MAP_ASCII_HORIZONTAL_CHARS_PER_CELL")));
	return iChars;
}

// <!-- custom: Each text-map layer can be disabled independently to reduce SASGameRecord size while retaining the layers useful for a particular analysis. Cache these XML switches like the other recorder settings; width or height 0 remains a convenient master switch for all ASCII maps. (ChatGPT-5.6-Sol) -->
static bool isSASGameRecordMapAsciiGeographyEnabled()
{
	static const bool bEnabled = (GC.getDefineINT("SAS_GAME_RECORD_MAP_ASCII_GEOGRAPHY_ENABLE") > 0);
	return bEnabled;
}

static bool isSASGameRecordMapAsciiTerrainEnabled()
{
	static const bool bEnabled = (GC.getDefineINT("SAS_GAME_RECORD_MAP_ASCII_TERRAIN_ENABLE") > 0);
	return bEnabled;
}

static bool isSASGameRecordMapAsciiRiversEnabled()
{
	static const bool bEnabled = (GC.getDefineINT("SAS_GAME_RECORD_MAP_ASCII_RIVER_ENABLE") > 0);
	return bEnabled;
}

static bool isSASGameRecordMapAsciiBonusesEnabled()
{
	static const bool bEnabled = (GC.getDefineINT("SAS_GAME_RECORD_MAP_ASCII_BONUS_ENABLE") > 0);
	return bEnabled;
}

static bool isSASGameRecordMapAsciiFeaturesEnabled()
{
	static const bool bEnabled = (GC.getDefineINT("SAS_GAME_RECORD_MAP_ASCII_FEATURE_ENABLE") > 0);
	return bEnabled;
}

static bool isSASGameRecordMapAsciiPoliticalEnabled()
{
	static const bool bEnabled = (GC.getDefineINT("SAS_GAME_RECORD_MAP_ASCII_POLITICAL_ENABLE") > 0);
	return bEnabled;
}

static CvString g_szSASGameRecordLogTimestamp;
static int g_iSASGameRecordLogSequence = 0;
static CvString g_szSASGameRecordLogContext;
// <!-- custom: Record snapshot UTC plus cumulative and per-interval wall time directly so benchmark duration is visible without external timestamp subtraction.
// Use the monotonic millisecond timer for useful precision and immunity to system-clock adjustments; wall time intentionally includes pauses and user interaction. (GPT-5.6-Sol) -->
static uint g_uiSASGameRecordSessionStartTime = 0;
static uint g_uiSASGameRecordPreviousSnapshotTime = 0;
static bool g_bSASGameRecordDisplayContextLogged = false;
// <!-- custom: New-game setup can emit many real ACTION rows before the EXE reports gameStart.
// Buffer only those actions so stable game/mod/map context stays at the top, then flush them in original order once initialization is complete. (ChatGPT-5.6-Sol) -->
static bool g_bSASGameRecordBufferInitializingActions = false;
static std::vector<std::pair<CvString, std::string> > g_aszSASGameRecordInitializingActions;
static void flushSASGameRecordPendingCityBombard();
static bool g_bSASGameRecordFlushingCityBombard = false;

// <!-- custom: Windows exposes focus separately from minimization. Find Civ4's visible, unowned top-level process window so background-visible and minimized snapshots remain distinguishable.
// Return -1 if no suitable window exists. (GPT-5.6-Sol) -->
struct SASGameRecordProcessWindowState
{
	SASGameRecordProcessWindowState() : bFound(false), bMinimized(false) {}
	bool bFound;
	bool bMinimized;
};

static BOOL CALLBACK findSASGameRecordProcessMainWindow(HWND hWindow, LPARAM lParam)
{
	DWORD uiProcessId = 0;
	GetWindowThreadProcessId(hWindow, &uiProcessId);
	if (uiProcessId != GetCurrentProcessId() || !IsWindowVisible(hWindow) || GetWindow(hWindow, GW_OWNER) != NULL)
		return TRUE;
	SASGameRecordProcessWindowState& kState = *(SASGameRecordProcessWindowState*)lParam;
	kState.bFound = true;
	kState.bMinimized = (IsIconic(hWindow) != FALSE);
	return FALSE;
}

static int getSASGameRecordProcessWindowMinimized()
{
	SASGameRecordProcessWindowState kState;
	EnumWindows(findSASGameRecordProcessMainWindow, (LPARAM)&kState);
	return kState.bFound ? (kState.bMinimized ? 1 : 0) : -1;
}

// <!-- custom: Native Win32 and Wine/Proton expose the same DLL APIs, but the distinction helps qualify performance results.
// Wine does not reliably expose whether its host is Linux or macOS, so do not guess beyond the compatibility layer. (GPT-5.6-Sol) -->
static char const* getSASGameRecordWin32Runtime()
{
	HMODULE const hNtdll = GetModuleHandleA("ntdll.dll");
	return (hNtdll != NULL && GetProcAddress(hNtdll, "wine_get_version") != NULL ? "WINE_OR_PROTON" : "NATIVE_WINDOWS");
}

// <!-- custom: Resolve PSAPI only when enabled performance metrics first sample memory, so disabling them also avoids a mandatory runtime dependency. (GPT-5.6-Sol) -->
static bool getSASGameRecordProcessMemory(PROCESS_MEMORY_COUNTERS& kProcessMemory)
{
	typedef BOOL (WINAPI* GetProcessMemoryInfoFunction)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
	static HMODULE const hPsapi = LoadLibraryA("psapi.dll");
	static GetProcessMemoryInfoFunction const pGetProcessMemoryInfo = (hPsapi == NULL ? NULL : (GetProcessMemoryInfoFunction)GetProcAddress(hPsapi, "GetProcessMemoryInfo"));
	return (pGetProcessMemoryInfo != NULL && pGetProcessMemoryInfo(GetCurrentProcess(), &kProcessMemory, sizeof(kProcessMemory)) != FALSE);
}

struct SASGameRecordSystemSnapshot
{
	SASGameRecordSystemSnapshot() : iProcessForeground(-1), iProcessWindowMinimized(-1), iProcessWorkingSetKB(-1), iProcessPeakWorkingSetKB(-1), iProcessPagefileUsageKB(-1), iSystemMemoryLoadPercent(-1), iProcessAvailableVirtualMB(-1) {}
	int iProcessForeground;
	int iProcessWindowMinimized;
	int iProcessWorkingSetKB;
	int iProcessPeakWorkingSetKB;
	int iProcessPagefileUsageKB;
	int iSystemMemoryLoadPercent;
	int iProcessAvailableVirtualMB;
};

static SASGameRecordSystemSnapshot getSASGameRecordSystemSnapshot()
{
	SASGameRecordSystemSnapshot kSnapshot;
	if (!isSASGameRecordPerformanceMetricsEnabled())
		return kSnapshot;
	DWORD uiForegroundProcessId = 0;
	HWND const hForegroundWindow = GetForegroundWindow();
	if (hForegroundWindow != NULL) GetWindowThreadProcessId(hForegroundWindow, &uiForegroundProcessId);
	kSnapshot.iProcessForeground = (uiForegroundProcessId == GetCurrentProcessId() ? 1 : 0);
	kSnapshot.iProcessWindowMinimized = getSASGameRecordProcessWindowMinimized();
	PROCESS_MEMORY_COUNTERS kProcessMemory;
	ZeroMemory(&kProcessMemory, sizeof(kProcessMemory));
	kProcessMemory.cb = sizeof(kProcessMemory);
	if (getSASGameRecordProcessMemory(kProcessMemory))
	{
		kSnapshot.iProcessWorkingSetKB = (int)(kProcessMemory.WorkingSetSize / 1024);
		kSnapshot.iProcessPeakWorkingSetKB = (int)(kProcessMemory.PeakWorkingSetSize / 1024);
		kSnapshot.iProcessPagefileUsageKB = (int)(kProcessMemory.PagefileUsage / 1024);
	}
	MEMORYSTATUSEX kSystemMemory;
	ZeroMemory(&kSystemMemory, sizeof(kSystemMemory));
	kSystemMemory.dwLength = sizeof(kSystemMemory);
	if (GlobalMemoryStatusEx(&kSystemMemory) != FALSE)
	{
		kSnapshot.iSystemMemoryLoadPercent = (int)kSystemMemory.dwMemoryLoad;
		kSnapshot.iProcessAvailableVirtualMB = (int)(kSystemMemory.ullAvailVirtual / (1024 * 1024));
	}
	return kSnapshot;
}

static CvString getSASGameRecordLogTimestamp()
{
	if (g_szSASGameRecordLogTimestamp.empty())
		g_szSASGameRecordLogTimestamp = createSASUtcTimestamp();
	return g_szSASGameRecordLogTimestamp;
}

static bool isSASGameRecordTimestampedFilenameEnabled()
{
	static const bool bUseTimestampedFilename = (GC.getDefineINT("SAS_GAME_RECORD_LOG_USE_TIMESTAMPED_FILENAME") > 0);
	return bUseTimestampedFilename;
}

static CvString getSASGameRecordLogName()
{
	return getSASDiagnosticLogName("SASGameRecord", getSASGameRecordLogTimestamp(), g_szSASGameRecordLogContext, isSASGameRecordTimestampedFilenameEnabled());
}

static void rollSASGameRecordLog(const char* szContext)
{
	time_t kSessionStartTime;
	time(&kSessionStartTime);
	g_uiSASGameRecordSessionStartTime = getSASMonotonicMilliseconds();
	g_uiSASGameRecordPreviousSnapshotTime = g_uiSASGameRecordSessionStartTime;
	g_bSASGameRecordDisplayContextLogged = false;
	g_szSASGameRecordLogTimestamp = createSASUtcTimestamp(kSessionStartTime);
	if (isSASGameRecordTimestampedFilenameEnabled())
	{
		g_iSASGameRecordLogSequence++;
		g_szSASGameRecordLogContext.Format("%s%d", szContext, g_iSASGameRecordLogSequence);
	}
}

static void logSASGameRecordFormattedLine(CvString const& szLogName, TCHAR* format, va_list args)
{
	std::string szLine;
	// <!-- custom: KI#161.2's explicit terminator stopped MSVC 7.1 truncation from leaving unsafe unterminated output, but the fixed 2048-byte buffer still silently discarded long structured rows such as late-game building, unit-type and promotion inventories.
	// Reuse CvString's grow-and-retry formatter so the complete machine-readable row reaches the log; abort the row if even that bounded formatter fails. See KI#375. (ChatGPT-5.5 + GPT-5.5; ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
	bool const bFormatted = CvString::formatv(szLine, format, args);
	FAssertMsg(bFormatted, "SASGameRecord row formatting failed");
	if (!bFormatted)
		return;
	if (g_bSASGameRecordBufferInitializingActions && szLine.find("GAME_RECORD_ACTION ") == 0)
	{
		// <!-- custom: Successful initialization replaces this procedural transcript with compact finalized state. If initialization aborts, preserve event timing only on the raw fallback actions instead of adding a redundant timestamp to every normal row. (GPT-5.6-Sol) -->
		CvString szSessionWall;
		szSessionWall.Format(" sessionWallMilliseconds=%u", getSASElapsedMilliseconds(g_uiSASGameRecordSessionStartTime, getSASMonotonicMilliseconds()));
		szLine += szSessionWall.GetCString();
		g_aszSASGameRecordInitializingActions.push_back(std::make_pair(szLogName, szLine));
		return;
	}
	gDLL->logMsg(szLogName.GetCString(), szLine.c_str(), false, false);
}

static uint getSASGameRecordSessionWallMilliseconds(uint uiNow)
{
	return getSASElapsedMilliseconds(g_uiSASGameRecordSessionStartTime, uiNow);
}

static uint getSASGameRecordSessionWallMilliseconds()
{
	return getSASGameRecordSessionWallMilliseconds(getSASMonotonicMilliseconds());
}

static void appendSASGameRecordType(CvString& szTypes, char const* szType)
{
	if (!szTypes.empty()) szTypes += ",";
	szTypes += szType;
}

static void flushSASGameRecordInitializingActions(bool bContextComplete)
{
	if (g_aszSASGameRecordInitializingActions.empty())
		return;
	logSASGameRecord("GAME_RECORD_INITIALIZATION_ACTIONS count=%d contextComplete=%d sessionWallMilliseconds=%u", (int)g_aszSASGameRecordInitializingActions.size(), bContextComplete, getSASGameRecordSessionWallMilliseconds());
	for (size_t iI = 0; iI < g_aszSASGameRecordInitializingActions.size(); iI++)
	{
		std::pair<CvString, std::string> const& kBuffered = g_aszSASGameRecordInitializingActions[iI];
		gDLL->logMsg(kBuffered.first.GetCString(), kBuffered.second.c_str(), false, false);
	}
	g_aszSASGameRecordInitializingActions.clear();
}

void logSASGameRecord(TCHAR* format, ... )
{
	static const bool bEnabled = isSASGameRecordLogEnabled();
	if (!bEnabled)
		return;
	// <!-- custom: CITY_BOMBARD is buffered only across consecutive equivalent actions. Flush it before the next ordinary GameRecord row so repeated siege clicks become one synthetic line without losing same-turn ordering relative to battles or other actions. (GPT-5.6 Thinking) -->
	if (!g_bSASGameRecordFlushingCityBombard)
		flushSASGameRecordPendingCityBombard();

	va_list args;
	va_start(args, format);
	logSASGameRecordFormattedLine(getSASGameRecordLogName(), format, args);
	va_end(args);
}

// <!-- custom: Free-text escaping is shared with other diagnostic logs in CvGameCoreUtils; keep only this city-specific missing-value wrapper local. (ChatGPT-5.6-Sol) -->
static CvWString getSASGameRecordQuotedCityName(CvCity const* pCity)
{
	return pCity == NULL ? L"-" : getSASDiagnosticQuoted(pCity->getName().GetCString());
}

// <!-- custom: Save-load context is recorded before Civ4 initializes graphics, which produced a misleading 0x0 resolution row.
// Defer the optional display context until graphics are ready, but write the display-disabled placeholder immediately because it needs no graphics queries. (GPT-5.6-Sol) -->
static void logSASGameRecordDisplayContext()
{
	if (g_bSASGameRecordDisplayContextLogged)
		return;
	int const iSystemContextLevel = getSASGameRecordSystemContextLevel();
	if (iSystemContextLevel < 1)
	{
		logSASGameRecord("GAME_RECORD_DISPLAY_CONTEXT systemContextLevel=0 resolution=-1x-1 graphicsInitialized=-1 fullscreen=-1 graphicOptions=-");
		g_bSASGameRecordDisplayContextLogged = true;
		return;
	}
	if (!GC.IsGraphicsInitialized())
		return;
	CvString szGraphicOptions;
	FOR_EACH_ENUM(GraphicOption)
	{
		if (!gDLL->getGraphicOption(eLoopGraphicOption))
			continue;
		if (!szGraphicOptions.empty())
			szGraphicOptions += ",";
		szGraphicOptions += GC.getInfo(eLoopGraphicOption).getType();
	}
	if (szGraphicOptions.empty())
		szGraphicOptions = "-";
	CvGame const& kGame = GC.getGame();
	logSASGameRecord("GAME_RECORD_DISPLAY_CONTEXT systemContextLevel=%d resolution=%dx%d graphicsInitialized=1 fullscreen=%d graphicOptions=%s", iSystemContextLevel, kGame.getScreenWidth(), kGame.getScreenHeight(), gDLL->getGraphicOption(GRAPHICOPTION_FULLSCREEN), szGraphicOptions.GetCString());
	g_bSASGameRecordDisplayContextLogged = true;
}

// <!-- custom: Record every stored map-script option, including hidden values and current script defaults.
// Keep numeric values durable; readable descriptions can remain unresolved when the script is unavailable. (ChatGPT-5.6-Sol) -->
static void logSASGameRecordMapOptions(CvInitCore const& kInitCore)
{
	const int iNumOptions = kInitCore.getNumCustomMapOptions();
	const int iNumHiddenOptions = std::min(iNumOptions, std::max(0, kInitCore.getNumHiddenCustomMapOptions()));
	CvString const szMapScriptName(kInitCore.getMapScriptName());
	const bool bMapScriptAvailable = (!kInitCore.getWBMapScript() && gDLL->pythonMapExists(szMapScriptName.GetCString()));
	logSASGameRecord("GAME_RECORD_MAP_OPTIONS count=%d hidden=%d scriptAvailable=%d", iNumOptions, iNumHiddenOptions, bMapScriptAvailable);
	if (iNumOptions <= 0)
		return;
	CvPythonCaller const& kPython = *GC.getPythonCaller();
	for (int iOption = 0; iOption < iNumOptions; iOption++)
	{
		CustomMapOptionTypes const eValue = kInitCore.getCustomMapOption(iOption);
		CustomMapOptionTypes eDefault = NO_CUSTOM_MAPOPTION;
		CvWString szDescription;
		CvWString szDefaultDescription;
		if (bMapScriptAvailable)
		{
			eDefault = kPython.customMapOptionDefault(szMapScriptName.GetCString(), iOption);
			szDescription = kPython.customMapOptionDescription(szMapScriptName.GetCString(), iOption, eValue);
			if (eDefault >= 0)
				szDefaultDescription = kPython.customMapOptionDescription(szMapScriptName.GetCString(), iOption, eDefault);
		}
		CvWString const szQuotedDescription = (szDescription.empty() ? CvWString(L"-") : getSASDiagnosticQuoted(szDescription.GetCString()));
		CvWString const szQuotedDefaultDescription = (szDefaultDescription.empty() ? CvWString(L"-") : getSASDiagnosticQuoted(szDefaultDescription.GetCString()));
		const bool bHidden = (iOption >= iNumOptions - iNumHiddenOptions);
		const int iIsScriptDefault = (eDefault < 0 ? -1 : (eValue == eDefault ? 1 : 0));
		logSASGameRecord("GAME_RECORD_MAP_OPTION index=%d hidden=%d value=%d scriptDefault=%d isScriptDefault=%d description=%S scriptDefaultDescription=%S",
				iOption, bHidden, eValue, eDefault, iIsScriptDefault, szQuotedDescription.GetCString(), szQuotedDefaultDescription.GetCString());
	}
}

// <!-- custom: GAMEOPTION_AGGRESSIVE_AI is repurposed while AdvCiv selects UWAI versus legacy K-Mod logic.
// Record the resolved mode plus only the three defines that can change that interpretation. (ChatGPT-5.6-Sol) -->
static void logSASGameRecordWarAISettings(CvGame const& kGame)
{
	const bool bUWAI = getUWAI().isEnabled();
	const bool bUWAIBackground = getUWAI().isEnabled(true);
	const char* szUWAIMode = (bUWAI ? "FULL" : (bUWAIBackground ? "BACKGROUND" : "DISABLED"));

	// <!-- custom: These are cold setup/load reads; caching would add static state for negligible benefit. (ChatGPT-5.6-Sol) -->
	const int iUseKModAINonAggressive = GC.getDefineINT("USE_KMOD_AI_NONAGGRESSIVE");
	const int iDisableUWAI = GC.getDefineINT("DISABLE_UWAI");
	const int iUWAIInBackground = GC.getDefineINT("UWAI_IN_BACKGROUND");
	logSASGameRecord("GAME_RECORD_WAR_AI_SETTINGS warPeaceAI=%s uwaiMode=%s engineAggressiveAI=%d USE_KMOD_AI_NONAGGRESSIVE=%d DISABLE_UWAI=%d UWAI_IN_BACKGROUND=%d",
			bUWAI ? "UWAI" : "KMOD_LEGACY", szUWAIMode, kGame.isOption(GAMEOPTION_AGGRESSIVE_AI),
			iUseKModAINonAggressive, iDisableUWAI, iUWAIInBackground);
}

// <!-- custom: Use "row" wording for generic SASGameRecord row prefixes because Civ4 also has EventInfo/random events. Keep GAME_RECORD_ACTION only for chronological gameplay action rows. (GPT-5.5) -->
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
	CvString szMPOptions;
	FOR_EACH_ENUM(MPOption)
	{
		if (!kGame.isMPOption(eLoopMPOption))
			continue;
		if (!szMPOptions.empty())
			szMPOptions += ",";
		szMPOptions += GC.getInfo(eLoopMPOption).getType();
	}
	if (szMPOptions.empty())
		szMPOptions = "-";
	const CvString szLogName = getSASGameRecordLogName();
	// <!-- custom: This session boundary is a useful timing anchor: on a new game it measures initialization, and on load it measures recorder setup before the finalized save context is available. (GPT-5.6-Sol) -->
	logSASGameRecord("%s processUtc=%s utc=%s logFile=%s turn=%d elapsed=%d year=%d scenario=%d activePlayer=%d activeCivilization=%s activeHandicap=%s playersDefined=%d playersAlive=%d playersEverAlive=%d humans=%d sessionWallMilliseconds=%u",
			szRowType, getSASProcessUtcTimestamp().GetCString(), getSASGameRecordLogTimestamp().GetCString(), getSASDiagnosticQuoted(szLogName.GetCString()).GetCString(), kGame.getGameTurn(), kGame.getElapsedGameTurns(), kGame.getGameTurnYear(), kGame.isScenario(), eActivePlayer, szActiveCivilization, szActiveHandicap, kInitCore.getNumDefinedPlayers(), kGame.countCivPlayersAlive(), kGame.countCivPlayersEverAlive(), kGame.getNumHumanPlayers(), getSASGameRecordSessionWallMilliseconds());
	// <!-- custom: Record the engine's durable launch/load and multiplayer identities so standalone logs distinguish ordinary games, scenarios, saves, replays, and supported network/turn modes. The explicit booleans avoid requiring consumers to reproduce Civ4's non-obvious GameType groupings.
	// Simple Game versus Custom Game is not retained reliably after launch, so do not infer it from mutable player slots or options. (GPT-5.6-Sol) -->
	logSASGameRecord("GAME_RECORD_SESSION_CONTEXT gameType=%s gameMode=%s newGame=%d savedGame=%d scenario=%d gameMultiplayer=%d networkMultiplayer=%d hotseat=%d pbem=%d pitboss=%d simultaneousTeamTurns=%d mpOptions=%s",
			getSASGameType(kInitCore.getType()), getSASGameMode(kInitCore.getMode()), kInitCore.getNewGame(), kInitCore.getSavedGame(), kGame.isScenario(), kInitCore.getGameMultiplayer(), kGame.isNetworkMultiPlayer(), kGame.isHotSeat(), kGame.isPbem(), kGame.isPitboss(), kGame.isSimultaneousTeamTurns(), szMPOptions.GetCString());
	// <!-- custom: Enabled victories and their fixed turn/score limits determine which later victory-progress and AI-strategy rows are relevant. Record this compact setup context instead of requiring external XML or save inspection. (GPT-5.6-Sol) -->
	// <!-- custom: Calendar, starting turn and starting year complete the time scale for scenarios and nonstandard calendars; ReplayInfo preserves the same context. (GPT-5.6-Sol) -->
	// <!-- custom: Record configured Advanced Start points separately from the boolean option, plus the current XML default for comparison.
	// Use -1 when ordinary Advanced Start is inactive or SPaH repurposes the setup value. (ChatGPT-5.6-Sol) -->
	const bool bAdvancedStart = kGame.isOption(GAMEOPTION_ADVANCED_START);
	const bool bStartPointsAsHandicap = kGame.isOption(GAMEOPTION_SPAH);
	const int iAdvancedStartConfiguredPoints = (bAdvancedStart && !bStartPointsAsHandicap ? kGame.getNumAdvancedStartPoints() : -1);
	int iAdvancedStartXmlDefaultPoints = -1;
	if (bAdvancedStart && !bStartPointsAsHandicap && kGame.getStartEra() != NO_ERA && kInitCore.getWorldSize() != NO_WORLDSIZE)
	{
		iAdvancedStartXmlDefaultPoints = GC.getInfo(kGame.getStartEra()).getAdvancedStartPoints();
		iAdvancedStartXmlDefaultPoints *= GC.getInfo(kInitCore.getWorldSize()).getAdvancedStartPointsMod();
		iAdvancedStartXmlDefaultPoints /= 100;
	}
	logSASGameRecord("GAME_RECORD_GAME_SETTINGS mapScript=%S map=%dx%d landHeavy=%d navalHeavy=%d world=%s climate=%s seaLevel=%s gameSpeed=%s startEra=%s calendar=%s startTurn=%d startYear=%d gameHandicap=%s maxTurns=%d targetScore=%d advancedStartConfiguredPoints=%d advancedStartXmlDefaultPoints=%d victories=%s options=%s",
			getSASDiagnosticQuoted(kInitCore.getMapScriptName().GetCString()).GetCString(), GC.getMap().getGridWidth(), GC.getMap().getGridHeight(), kGame.isLandHeavyMapnameCached(), kGame.isNavalHeavyMapnameCached(),
			GC.getInfo(kInitCore.getWorldSize()).getType(), GC.getInfo(kInitCore.getClimate()).getType(), GC.getInfo(kInitCore.getSeaLevel()).getType(), GC.getInfo(kGame.getGameSpeedType()).getType(), GC.getInfo(kGame.getStartEra()).getType(),
			getSASCalendarType(kGame.getCalendar()), kGame.getStartTurn(), kGame.getStartYear(), GC.getInfo(kGame.getHandicapType()).getType(), kGame.getMaxTurns(), kGame.getTargetScore(), iAdvancedStartConfiguredPoints, iAdvancedStartXmlDefaultPoints, szVictories.GetCString(), szGameOptions.GetCString());
	logSASGameRecordMapOptions(kInitCore);
	logSASGameRecordWarAISettings(kGame);
	// <!-- custom: Display settings can affect measured autoplay wall time. Record the compact Civ4 context once, after graphics initialization, rather than copying unrelated CivilizationIV.ini settings. (GPT-5.6-Sol) -->
	logSASGameRecordDisplayContext();
	// <!-- custom: A Civ4 custom DLL is a Win32 binary even when Wine or Proton runs it on another host OS.
	// Record that compatibility layer honestly instead of guessing the host system, plus only compact hardware context useful for performance comparisons. (GPT-5.6-Sol) -->
	int const iSystemContextLevel = getSASGameRecordSystemContextLevel();
	if (iSystemContextLevel >= 2)
	{
		int iLogicalProcessors = -1;
		int iTotalPhysicalMemoryMB = -1;
		if (iSystemContextLevel >= 3)
		{
			SYSTEM_INFO kSystemInfo;
			GetSystemInfo(&kSystemInfo);
			iLogicalProcessors = (int)kSystemInfo.dwNumberOfProcessors;
			MEMORYSTATUSEX kSystemMemory;
			ZeroMemory(&kSystemMemory, sizeof(kSystemMemory));
			kSystemMemory.dwLength = sizeof(kSystemMemory);
			if (GlobalMemoryStatusEx(&kSystemMemory)) iTotalPhysicalMemoryMB = (int)(kSystemMemory.ullTotalPhys / (1024 * 1024));
		}
		logSASGameRecord("GAME_RECORD_RUNTIME_CONTEXT systemContextLevel=%d win32Runtime=%s pointerBits=%d logicalProcessors=%d totalPhysicalMemoryMB=%d", iSystemContextLevel, getSASGameRecordWin32Runtime(), (int)(8 * sizeof(void*)), iLogicalProcessors, iTotalPhysicalMemoryMB);
	}
	else logSASGameRecord("GAME_RECORD_RUNTIME_CONTEXT systemContextLevel=%d win32Runtime=- pointerBits=-1 logicalProcessors=-1 totalPhysicalMemoryMB=-1", iSystemContextLevel);
	logSASGameRecord("GAME_RECORD_GAME_RNG mapRandState=%u syncRandState=%u", kGame.getMapRand().getSeed(), kGame.getSorenRand().getSeed());
}

static void logSASGameRecordLogSettings()
{
	// <!-- custom: Read the popup define directly because this settings row is emitted only once per log; unlike repeatedly queried map settings, caching it would add state without avoiding repeat work. (GPT-5.6-Sol) -->
	logSASGameRecord("GAME_RECORD_LOG_SETTINGS SAS_GAME_RECORD_LOG_LEVEL=%d SAS_GAME_RECORD_INTERVAL_TURNS_UNSCALED_GAMESPEED=%d SAS_GAME_RECORD_LOG_USE_TIMESTAMPED_FILENAME=%d SAS_AIAUTOPLAY_AUTO_DISMISS_INFORMATIONAL_POPUPS_ENABLE=%d SAS_GAME_RECORD_MAP_ASCII_MAX_WIDTH=%d SAS_GAME_RECORD_MAP_ASCII_MAX_HEIGHT=%d SAS_GAME_RECORD_MAP_ASCII_HORIZONTAL_CHARS_PER_CELL=%d SAS_GAME_RECORD_MAP_ASCII_GEOGRAPHY_ENABLE=%d SAS_GAME_RECORD_MAP_ASCII_TERRAIN_ENABLE=%d SAS_GAME_RECORD_MAP_ASCII_RIVER_ENABLE=%d SAS_GAME_RECORD_MAP_ASCII_BONUS_ENABLE=%d SAS_GAME_RECORD_MAP_ASCII_FEATURE_ENABLE=%d SAS_GAME_RECORD_MAP_ASCII_POLITICAL_ENABLE=%d SAS_GAME_RECORD_TRADE_MARKET_ENABLE=%d SAS_GAME_RECORD_TRADE_MARKET_BONUS_GPT_QUOTES_ENABLE=%d SAS_GAME_RECORD_TRADE_MARKET_AI_TECH_VALUES_ENABLE=%d SAS_GAME_RECORD_PERFORMANCE_METRICS_ENABLE=%d SAS_GAME_RECORD_SYSTEM_CONTEXT_LEVEL=%d",
			getSASGameRecordLogLevel(), getSASGameRecordTurnInterval(), isSASGameRecordTimestampedFilenameEnabled(), GC.getDefineINT("SAS_AIAUTOPLAY_AUTO_DISMISS_INFORMATIONAL_POPUPS_ENABLE"), getSASGameRecordMapAsciiMaxWidth(), getSASGameRecordMapAsciiMaxHeight(), getSASGameRecordMapAsciiHorizontalCharsPerCell(),
			isSASGameRecordMapAsciiGeographyEnabled(), isSASGameRecordMapAsciiTerrainEnabled(), isSASGameRecordMapAsciiRiversEnabled(), isSASGameRecordMapAsciiBonusesEnabled(), isSASGameRecordMapAsciiFeaturesEnabled(), isSASGameRecordMapAsciiPoliticalEnabled(), isSASGameRecordTradeMarketEnabled(), isSASGameRecordTradeMarketBonusGPTQuotesEnabled(), isSASGameRecordTradeMarketAITechValuesEnabled(), isSASGameRecordPerformanceMetricsEnabled(), getSASGameRecordSystemContextLevel());
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

static void resetSASGameRecordState();
static void logSASGameRecordInitialContext(bool bNewGame);
static void logSASGameRecordInitialPlayerIdentities();
static void logSASGameRecordFinalizedInitialState(int& iTeamStateRows, int& iTechRows, int& iDeals);
static void initializeSASGameRecordWarsFromLoadedSave();

// <!-- custom: Keep shared provenance behind the recorder's runtime gate so source resolution/DLL hashing never happens merely because an otherwise-disabled lifecycle hook was reached. (ChatGPT-5.6-Sol) -->
static void logSASGameRecordProvenanceContext()
{
	if (!isSASGameRecordLogEnabled())
		return;
	logSASGameRecord("GAME_RECORD_MOD_CONTEXT %s", getSASModContextFields().GetCString());
	logSASGameRecord("GAME_RECORD_SOURCE_CONTEXT %s", getSASSourceContextFields().GetCString());
	logSASGameRecord("GAME_RECORD_DLL_CONTEXT %s", getSASDllContextFields().GetCString());
}

// <!-- custom: Persisted game-source history is separate from the current SOURCE_CONTEXT: it records where this save lineage began and each later runtime-source transition without storing noisy dirty-file lists in the save itself. (ChatGPT-5.6-Sol) -->
static void logSASGameRecordVersionHistory()
{
	CvGame const& kGame = GC.getGame();
	int const iEntries = kGame.getNumSASVersionHistoryEntries();
	logSASGameRecord("GAME_RECORD_SAVE_VERSION_HISTORY entries=%d", iEntries);
	for (int i = 0; i < iEntries; i++)
	{
		char const* szRole = (i == 0 ? "CREATION" : "TRANSITION");
		logSASGameRecord("GAME_RECORD_SAVE_VERSION_HISTORY_ENTRY index=%d role=%s turn=%d version=%s commit=%s dirty=%d",
			i, szRole, kGame.getSASVersionHistoryTurn(i), getSASDiagnosticQuoted(kGame.getSASVersionHistoryVersion(i)).GetCString(),
			getSASDiagnosticQuoted(kGame.getSASVersionHistoryCommitHash(i)).GetCString(), kGame.getSASVersionHistoryDirtyState(i));
	}
}

void startSASGameRecordLogForNewGame()
{
	rollSASGameRecordLog("new");
	resetSASGameRecordState();
	logSASGameRecord("GAME_RECORD_NEW_GAME_INITIALIZING processUtc=%s utc=%s logFile=%s sessionWallMilliseconds=0", getSASProcessUtcTimestamp().GetCString(), getSASGameRecordLogTimestamp().GetCString(), getSASDiagnosticQuoted(getSASGameRecordLogName().GetCString()).GetCString());
	// <!-- custom: Static mod/source/binary provenance is already final once this DLL is running, so keep it immediately after the lifecycle marker instead of burying it behind generated game context. (ChatGPT-5.6-Sol) -->
	logSASGameRecordProvenanceContext();
	logSASGameRecordLogSettings();
	logSASGameRecordTechCapabilitySources();
	// <!-- custom: Settings below are not final until map/player initialization finishes.
	// Buffer setup-generated actions only as a failure diagnostic: successful gameStart replaces the procedural transcript with authoritative finalized initial state, while an aborted initialization flushes the raw actions with contextComplete=0. (ChatGPT-5.6-Sol) -->
	g_bSASGameRecordBufferInitializingActions = true;
}

void logSASGameRecordNewGameStarted()
{
	logSASGameRecordGameState("GAME_RECORD_NEW_GAME_STARTED");
	// <!-- custom: gameStart has now established the persisted creation entry, so expose it immediately in the successful new-game record. (ChatGPT-5.6-Sol) -->
	logSASGameRecordVersionHistory();
	logSASGameRecordInitialPlayerIdentities();
	int iTeamStateRows = 0;
	int iTechRows = 0;
	int iDeals = 0;
	// <!-- custom: Finalized team/tech/deal expansion is level-2-only. Own that gate here so the helper never starts or validates work that its caller already knows is disabled. (ChatGPT-5.6-Sol) -->
	bool const bLogInitialDetails = (gGameRecordLogLevel >= 2);
	if (bLogInitialDetails) logSASGameRecordFinalizedInitialState(iTeamStateRows, iTechRows, iDeals);
	logSASGameRecordInitialContext(true);
	// <!-- custom: A successful initialization is represented by authoritative finalized state above, not by replaying the engine's procedural setup transcript.
	// Keep only the count as an audit clue; failed initialization still flushes every raw buffered action from finalizeSASGameRecordLogSession. (ChatGPT-5.6-Sol) -->
	int const iBufferedActionsNotReplayed = (int)g_aszSASGameRecordInitializingActions.size();
	g_bSASGameRecordBufferInitializingActions = false;
	g_aszSASGameRecordInitializingActions.clear();
	if (bLogInitialDetails || iBufferedActionsNotReplayed > 0)
		logSASGameRecord("GAME_RECORD_INITIAL_STATE_SUMMARY teamStateRows=%d techGroupRows=%d techTeamsCovered=%d %s bufferedActionsNotReplayed=%d source=FINALIZED_STATE sessionWallMilliseconds=%u",
			iTeamStateRows, iTechRows, iTeamStateRows, getSASInitialDealSummaryFields(bLogInitialDetails, iDeals).GetCString(), iBufferedActionsNotReplayed, getSASGameRecordSessionWallMilliseconds());
}

void startSASGameRecordLogForLoadedSave()
{
	rollSASGameRecordLog("load");
	resetSASGameRecordState();
	// <!-- custom: The save contains no recorder-local war history. Begin partial observations for wars already in progress, with the loaded turn and current war success recorded explicitly as their observable baseline. (GPT-5.6-Sol) -->
	if (gGameRecordLogLevel >= 2) initializeSASGameRecordWarsFromLoadedSave();
	logSASGameRecordGameState("GAME_RECORD_SAVE_LOADED");
	// <!-- custom: Keep static mod/source/binary identity immediately after the load-session marker; it does not depend on the loaded save's generated/game state. (ChatGPT-5.6-Sol) -->
	logSASGameRecordProvenanceContext();
	// <!-- custom: onAllGameDataRead reconciles the persisted lineage before starting this log, so a load records any newly encountered source transition here. (ChatGPT-5.6-Sol) -->
	logSASGameRecordVersionHistory();
	logSASGameRecordLogSettings();
	logSASGameRecordTechCapabilitySources();
	logSASGameRecordInitialPlayerIdentities();
	logSASGameRecordInitialContext(false);
}

// <!-- custom: Game-record helpers keep output compact, stable, and machine-readable. They intentionally use XML type names instead of localized text where possible, so external tools can diff and parse autoplay runs reliably. The static state below is tiny and is only reset/updated through game-record call sites when the XML log level enables this feature; dynamic XML logging cannot be compiled out cleanly without losing normal runtime XML tuning. (ChatGPT-5.5) -->
static int g_aiSASGameRecordBattleWins[MAX_PLAYERS];
static int g_aiSASGameRecordBattleLosses[MAX_PLAYERS];
static int g_aiSASGameRecordCityBattleWins[MAX_PLAYERS];
static int g_aiSASGameRecordCityBattleLosses[MAX_PLAYERS];
static int g_aiSASGameRecordTotalBattleWins[MAX_PLAYERS];
static int g_aiSASGameRecordTotalBattleLosses[MAX_PLAYERS];
static int g_aiSASGameRecordTotalCityBattleWins[MAX_PLAYERS];
static int g_aiSASGameRecordTotalCityBattleLosses[MAX_PLAYERS];

// <!-- custom: Ordinary win/loss counts do not describe withdrawals, combat-limit attacks or how surprising binary outcomes were.
// Keep compact interval and recorder-session aggregates using Civ4's exact pre-combat odds; no individual level-2 battle rows are added. (ChatGPT-5.6-Sol) -->
struct SASGameRecordBattleQuality
{
	int iWithdrawals;
	int iEnemyWithdrawals;
	int iCombatLimitAttacks;
	int iCombatLimitDefenses;
	int iLuckEligibleBattles;
	int iLuckEligibleWins;
	int iExpectedWinsX1000;
	int iUpsetWins;
	int iUpsetLosses;
	int iLowestOddsWinPermille;
	int iHighestOddsLossPermille;
	void reset()
	{
		iWithdrawals = 0;
		iEnemyWithdrawals = 0;
		iCombatLimitAttacks = 0;
		iCombatLimitDefenses = 0;
		iLuckEligibleBattles = 0;
		iLuckEligibleWins = 0;
		iExpectedWinsX1000 = 0;
		iUpsetWins = 0;
		iUpsetLosses = 0;
		iLowestOddsWinPermille = -1;
		iHighestOddsLossPermille = -1;
	}
	bool hasAny() const
	{
		return (iWithdrawals > 0 || iEnemyWithdrawals > 0 || iCombatLimitAttacks > 0 || iCombatLimitDefenses > 0 || iLuckEligibleBattles > 0);
	}
};
static SASGameRecordBattleQuality g_akSASGameRecordBattleQuality[MAX_PLAYERS];
static SASGameRecordBattleQuality g_akSASGameRecordTotalBattleQuality[MAX_PLAYERS];

// <!-- custom: Session totals complement current-unit XP snapshots: veteran deaths, upgrades and captures no longer erase evidence of XP generated, promotion decisions made or veteran quality exchanged in combat.
// Promotion-type detail stays interval-only to avoid repeating a growing lifetime list. (ChatGPT-5.6-Sol) -->
struct SASGameRecordMilitaryQualityTotals
{
	int iExperienceGained;
	int iCombatExperienceGained;
	int iNonCombatExperienceGained;
	int iExperiencePreventedByCap;
	int iExperienceLostAdjustments;
	int iPromotionsChosen;
	int iLeaderPromotionApplications;
	int iEnemyExperienceDestroyed;
	int iOwnExperienceLost;
	void reset()
	{
		iExperienceGained = 0;
		iCombatExperienceGained = 0;
		iNonCombatExperienceGained = 0;
		iExperiencePreventedByCap = 0;
		iExperienceLostAdjustments = 0;
		iPromotionsChosen = 0;
		iLeaderPromotionApplications = 0;
		iEnemyExperienceDestroyed = 0;
		iOwnExperienceLost = 0;
	}
};
static SASGameRecordMilitaryQualityTotals g_akSASGameRecordMilitaryQualityTotals[MAX_PLAYERS];

// <!-- custom: Visible combat can delay combatResult for several frames after resolveCombat has already determined the fight.
// Keep only transient recorder context needed to join that later callback to the true attacker and pre-combat odds; this is never serialized into the save. (ChatGPT-5.6-Sol) -->
struct SASGameRecordCombatPending
{
	PlayerTypes eAttacker;
	PlayerTypes eDefender;
	int iAttackerUnitId;
	int iDefenderUnitId;
	int iX;
	int iY;
	int iAttackerCombatOddsPermille;
	bool bLuckEligible;
};
static std::vector<SASGameRecordCombatPending> g_aSASGameRecordCombatPending;

// <!-- custom: Naval blockades persist across turns but CivUnit stores only the current boolean state, not when the present blockade began.
// Keep tiny recorder-local start context so one rare end row can report observed duration and accumulated plunder without adding savegame fields or per-turn blockade spam. Mid-save sessions gracefully fall back to startKnown=0. (ChatGPT-5.6-Sol) -->
struct SASGameRecordBlockadeContext
{
	PlayerTypes ePlayer;
	int iUnitId;
	int iStartTurn;
	int iStartElapsedTurn;
	int iStartX;
	int iStartY;
	int iRangePlots;
	int iAffectedTeams;
	int iAffectedCities;
	CvString szRangePlots;
	CvString szAffectedTeams;
	CvString szAffectedCities;
	int iPlunderEvents;
	int iGoldPlundered;
	std::vector<std::pair<PlayerTypes,int> > aPlunderedCities;
};
static std::vector<SASGameRecordBlockadeContext> g_aSASGameRecordBlockades;

// <!-- custom: These counters reset whenever a new GameRecord log session begins, including after loading a save.
// Name them as logged observations rather than misleading lifetime totals. See KI#379. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
static int g_aiSASGameRecordLoggedGoldenAgeTurns[MAX_PLAYERS];
static int g_aiSASGameRecordLoggedAnarchyTurns[MAX_PLAYERS];
// <!-- custom: Observe each player's finalized research target once per player turn.
// This recorder-local state detects real incomplete-tech redirections without instrumenting every queue-mutating gameplay path or inventing a cause that the observation cannot prove.
// Repeat-tech counts distinguish a completed repeat from a true redirect. (ChatGPT-5.6-Sol) -->
struct SASGameRecordResearchPrevious
{
	bool bValid;
	TeamTypes eTeam;
	TechTypes eTech;
	int iTechCount;
	ResearchTargetChangeCause ePendingCause;
};
static SASGameRecordResearchPrevious g_akSASGameRecordResearchPrevious[MAX_PLAYERS];
// <!-- custom: CvPlayer::doResearch knows the exact split between this turn's modified research and previously stored unmodified overflow before both are combined into team progress.
// Retain that tiny level-2-only application context until an actual same-turn completion consumes it; this keeps RESEARCH_COMPLETED exact without widening generic gameplay research APIs for logging. (ChatGPT-5.6-Sol) -->
struct SASGameRecordResearchApplication
{
	bool bValid;
	int iGameTurn;
	TechTypes eTech;
	int iModifiedResearchRate;
	int iIncomingOverflowUnmodified;
	int iIncomingOverflowModified;
};
static SASGameRecordResearchApplication g_akSASGameRecordResearchApplication[MAX_PLAYERS];
// <!-- custom: Raze diagnostics use a tiny LIFO context stack rather than exposing recorder-specific state to CvPlayer.
// Python cityRazed callbacks can theoretically trigger nested gameplay, so a stack remains correct where one global pending slot would not. (ChatGPT-5.6-Sol) -->
struct SASGameRecordCityRazeContext
{
	PlayerTypes eRazer;
	TeamTypes eRazerTeam;
	PlayerTypes ePreviousOwner;
	TeamTypes ePreviousTeam;
	PlayerTypes eOriginalOwner;
	TeamTypes eOriginalTeam;
	int iGameTurn;
	int iCityId;
	CvWString szCityName;
	int iX;
	int iY;
	int iArea;
	CvString szRazeMode;
	int iPopulation;
	int iHighestPopulation;
	int iFoundedTurn;
	int iAcquiredTurn;
	int iOccupationTurns;
	int iRazerCulturePercent;
	int iPreviousCulturePercent;
	PlayerTypes eHighestCulturePlayer;
	int iHighestCulturePercent;
	int iMaintenanceTimes100;
	int iConnectedToCapital;
	int iCapitalDistance;
	int iCapitalSameArea;
	int iNearestRazerCityDistance;
	int iSameAreaRazerCitiesOther;
	int iNearestPreviousOwnerCityDistance;
	int iSameAreaPreviousOwnerCities;
	int iBuildings;
	int iRegularBuildings;
	int iNationalWonders;
	int iTeamWonders;
	int iWorldWonders;
	CvString szBuildings;
	CvString szReligions;
	CvString szHolyReligions;
	CvString szCorporations;
	CvString szHeadquarters;
	int iPlayerCitiesBefore;
	int iPlayerLandBefore;
	int iPlayerPopulationBefore;
	int iTeamCitiesBefore;
	int iTeamLandBefore;
	int iTeamPopulationBefore;
	int iWorldPopulationBefore;
	int iLandPctX100Before;
	int iPopPctX100Before;
	int iAIMaxVictoryStage;
	int iAIConquestStage;
	int iAIDominationStage;
	CvString szLandPopVictoryProgressBefore;
};
static std::vector<SASGameRecordCityRazeContext> g_aSASGameRecordCityRazeContexts;
static int g_aiSASGameRecordCitiesAcquired[MAX_PLAYERS];
static int g_aiSASGameRecordCitiesLost[MAX_PLAYERS];
static int g_aiSASGameRecordCitiesConquered[MAX_PLAYERS];
static int g_aiSASGameRecordCitiesLostByConquest[MAX_PLAYERS];
static int g_aiSASGameRecordCitiesTradedIn[MAX_PLAYERS];
static int g_aiSASGameRecordCitiesTradedOut[MAX_PLAYERS];
// <!-- custom: Recorder-local autoplay state makes each start/end row self-contained and counts active-player transfers without adding savegame fields. AI Auto Play is stopped when a save is loaded, so resetting this state with each log session matches the actual automation boundary. See KI#203. (GPT-5.6-Sol) -->
static int g_iSASGameRecordAutoPlayRequestId = 0;
static int g_iSASGameRecordAutoPlayRequestedTurns = 0;
static int g_iSASGameRecordAutoPlayStartTurn = -1;
static int g_iSASGameRecordAutoPlayStartElapsedTurn = -1;
static PlayerTypes g_eSASGameRecordAutoPlayStartPlayer = NO_PLAYER;
// <!-- custom: Track real per-request autoplay wall time with the same monotonic timer used by snapshot timing. (ChatGPT-5.6-Sol) -->
static uint g_uiSASGameRecordAutoPlayStartTime = 0;
static int g_iSASGameRecordAutoPlayPlayerChanges = 0;
static int g_iSASGameRecordTotalActivePlayerChanges = 0;
static int g_iSASGameRecordLastFullSnapshotTurn = -1;
// <!-- custom: Battle counters reset at actual snapshot boundaries, which need not match the configured periodic interval after loading or a victory flush. Track their real inclusive start like the newer flow buckets. See KI#378. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
static int g_iSASGameRecordBattleStartTurn = 0;
static int g_iSASGameRecordFlowStartTurn = 0;

// <!-- custom: Recorder-only production categories are shared by the interval transition matrix and exact target-change formatting.
// Keep the enum beside the flow state so its size is available before any serializer uses it. (ChatGPT-5.6-Sol) -->
enum SASGameRecordProductionKindIndex
{
	SAS_PRODUCTION_UNIT = 0,
	SAS_PRODUCTION_BUILDING,
	SAS_PRODUCTION_WONDER,
	SAS_PRODUCTION_PROJECT,
	SAS_PRODUCTION_PROCESS,
	NUM_SAS_PRODUCTION_KINDS
};

// <!-- custom: Level 3 preserves exact production, military and natural city-population evidence, but thousands of routine rows can distract broad analysis. Accumulate the same strategic totals into compact per-player interval rows for level 2+; dynamic type buckets use the loaded mod's XML rather than fixed BTS categories.
// `productionNeeded` is the loaded ruleset's production threshold at the action time, providing a consistent material-cost comparison; it is not a claim about raw hammers historically invested after production modifiers. (GPT-5.6-Sol + ChatGPT-5.6-Sol) -->
struct SASGameRecordPlayerFlow
{
	int iUnitsCompleted;
	int iUnitsConscripted;
	int iUnitProductionNeeded;
	int iConscriptProductionNeeded;
	int iBuildingsCompleted;
	int iBuildingProductionNeeded;
	int iProjectsCompleted;
	int iProjectProductionNeeded;
	int iOverflowActions;
	int iRawModifiedOverflow;
	int iUnmodifiedOverflow;
	int iKeptOverflow;
	int iLostProduction;
	int iUnusedOverflowCapacity;
	int iOverflowGold;
	int iFailedInvestedProduction;
	int iFailGold;
	// <!-- custom: Natural city population flow is compacted into interval totals at level 2; level 3 additionally keeps exact city transitions.
	// Hurries, conscription, events, conquest/razing and other non-growth population changes retain their own existing/provenance-specific boundaries rather than being mixed into these natural-growth counters. (ChatGPT-5.6-Sol) -->
	int iCityGrowthEvents;
	int iPopulationGainedFromGrowth;
	int iCityGrowthPreventedEvents;
	int iFoodDiscardedByAvoidGrowth;
	int iCityStarvationEvents;
	int iPopulationLostToStarvation;
	// <!-- custom: AI production-target flow distinguishes ordinary completion->next selection from real active-target switching.
	// Stored production is parked rather than assumed wasted; separate decay/fail/overflow counters own actual mechanical loss. (ChatGPT-5.6-Sol) -->
	int iAIProductionTargetSwitches;
	int iAIProductionTargetClears;
	int iAIProductionInvestedTargetChanges;
	int iAIProductionParked;
	int iAIProductionTargetResumes;
	int iAIProductionResumed;
	int iProductionDecayActions;
	int iProductionDecayLost;
	int iProductionInvalidatedActions;
	int iProductionInvalidatedLost;
	int iProductionUpgradeTransfers;
	int iProductionUpgradeTransferred;
	int iProductionUpgradeOverwriteActions;
	int iProductionUpgradeOverwritten;
	int aiAIProductionTransitions[NUM_SAS_PRODUCTION_KINDS * NUM_SAS_PRODUCTION_KINDS];
	std::vector<std::pair<int,int> > aAIProductionTargetChangesByCity;
	int iUpgrades;
	int iUpgradeGold;
	int iScrapped;
	int iScrappedProductionNeeded;
	int iCaptured;
	int iCapturedProductionNeeded;
	int iCombatWins;
	int iCombatLosses;
	int iCityPlotWins;
	int iCityPlotLosses;
	int iEnemyProductionNeededDestroyed;
	int iOwnProductionNeededLost;
	int iExperienceGained;
	int iCombatExperienceGained;
	int iNonCombatExperienceGained;
	int iExperiencePreventedByCap;
	int iExperienceLostAdjustments;
	int iPromotionsChosen;
	int iLeaderPromotionApplications;
	int iEnemyExperienceDestroyed;
	int iOwnExperienceLost;
	std::vector<int> aiUnitTypes;
	std::vector<int> aiConscriptedUnitTypes;
	std::vector<int> aiBuildingTypes;
	std::vector<int> aiProjectTypes;
	std::vector<int> aiPromotionChoices;

	void reset()
	{
		iUnitsCompleted = 0;
		iUnitsConscripted = 0;
		iUnitProductionNeeded = 0;
		iConscriptProductionNeeded = 0;
		iBuildingsCompleted = 0;
		iBuildingProductionNeeded = 0;
		iProjectsCompleted = 0;
		iProjectProductionNeeded = 0;
		iOverflowActions = 0;
		iRawModifiedOverflow = 0;
		iUnmodifiedOverflow = 0;
		iKeptOverflow = 0;
		iLostProduction = 0;
		iUnusedOverflowCapacity = 0;
		iOverflowGold = 0;
		iFailedInvestedProduction = 0;
		iFailGold = 0;
		iCityGrowthEvents = 0;
		iPopulationGainedFromGrowth = 0;
		iCityGrowthPreventedEvents = 0;
		iFoodDiscardedByAvoidGrowth = 0;
		iCityStarvationEvents = 0;
		iPopulationLostToStarvation = 0;
		iAIProductionTargetSwitches = 0;
		iAIProductionTargetClears = 0;
		iAIProductionInvestedTargetChanges = 0;
		iAIProductionParked = 0;
		iAIProductionTargetResumes = 0;
		iAIProductionResumed = 0;
		iProductionDecayActions = 0;
		iProductionDecayLost = 0;
		iProductionInvalidatedActions = 0;
		iProductionInvalidatedLost = 0;
		iProductionUpgradeTransfers = 0;
		iProductionUpgradeTransferred = 0;
		iProductionUpgradeOverwriteActions = 0;
		iProductionUpgradeOverwritten = 0;
		for (int iI = 0; iI < NUM_SAS_PRODUCTION_KINDS * NUM_SAS_PRODUCTION_KINDS; iI++) aiAIProductionTransitions[iI] = 0;
		aAIProductionTargetChangesByCity.clear();
		iUpgrades = 0;
		iUpgradeGold = 0;
		iScrapped = 0;
		iScrappedProductionNeeded = 0;
		iCaptured = 0;
		iCapturedProductionNeeded = 0;
		iCombatWins = 0;
		iCombatLosses = 0;
		iCityPlotWins = 0;
		iCityPlotLosses = 0;
		iEnemyProductionNeededDestroyed = 0;
		iOwnProductionNeededLost = 0;
		iExperienceGained = 0;
		iCombatExperienceGained = 0;
		iNonCombatExperienceGained = 0;
		iExperiencePreventedByCap = 0;
		iExperienceLostAdjustments = 0;
		iPromotionsChosen = 0;
		iLeaderPromotionApplications = 0;
		iEnemyExperienceDestroyed = 0;
		iOwnExperienceLost = 0;
		aiUnitTypes.assign(GC.getNumUnitInfos(), 0);
		aiConscriptedUnitTypes.assign(GC.getNumUnitInfos(), 0);
		aiBuildingTypes.assign(GC.getNumBuildingInfos(), 0);
		aiProjectTypes.assign(GC.getNumProjectInfos(), 0);
		aiPromotionChoices.assign(GC.getNumPromotionInfos(), 0);
	}

	bool hasProduction() const
	{
		return (iUnitsCompleted > 0 || iUnitsConscripted > 0 || iBuildingsCompleted > 0 || iProjectsCompleted > 0 || iOverflowActions > 0 || iFailedInvestedProduction > 0 || iFailGold > 0 ||
			iAIProductionTargetSwitches > 0 || iAIProductionTargetClears > 0 || iAIProductionTargetResumes > 0 || iProductionDecayActions > 0 || iProductionInvalidatedActions > 0 || iProductionUpgradeTransfers > 0 || iProductionUpgradeOverwritten > 0);
	}

	bool hasMilitary() const
	{
		return (iUpgrades > 0 || iScrapped > 0 || iCaptured > 0 || iCombatWins > 0 || iCombatLosses > 0 || iExperienceGained > 0 || iExperiencePreventedByCap > 0 || iExperienceLostAdjustments > 0 || iPromotionsChosen > 0 || iLeaderPromotionApplications > 0 || iEnemyExperienceDestroyed > 0 || iOwnExperienceLost > 0);
	}

	bool hasCityPopulationFlow() const
	{
		return (iCityGrowthEvents > 0 || iCityGrowthPreventedEvents > 0 || iCityStarvationEvents > 0);
	}
};

static SASGameRecordPlayerFlow g_akSASGameRecordPlayerFlow[MAX_PLAYERS];

struct SASGameRecordPlotChangeGroup
{
	CvString szCategory;
	std::vector<std::pair<int,int> > aCoordinates;
};

static int g_iSASGameRecordPendingPlotTurn = -1;

// <!-- custom: Keep the city-bombard member named szMode. During the 6385 city-raze logging work it was accidentally renamed to szRazeMode while the existing bombard code still referenced szMode, causing MSVC C2039 compile errors.
// The separate city-raze context intentionally owns szRazeMode instead. (ChatGPT-5.6-Sol) -->
struct SASGameRecordCityBombardPending
{
	bool bValid;
	int iTurn;
	CvString szMode;
	PlayerTypes ePlayer;
	PlayerTypes eTargetPlayer;
	int iCityId;
	CvWString szCity;
	int iX;
	int iY;
	int iActions;
	int iBombardRateTotal;
	int iIgnoreBuildingDefenseActions;
	int iDefenseModifierBefore;
	int iDefenseModifierAfter;
	int iTotalDefense;
	int iDefenseDamageBefore;
	int iDefenseDamageAfter;
	int iDefenseDamageMax;
	std::vector<std::pair<CvString,int> > aUnitTypes;
	std::vector<std::pair<CvString,int> > aUnitAIs;
	SASGameRecordCityBombardPending() : bValid(false), iTurn(-1), ePlayer(NO_PLAYER), eTargetPlayer(NO_PLAYER), iCityId(-1), iX(-1), iY(-1), iActions(0), iBombardRateTotal(0), iIgnoreBuildingDefenseActions(0), iDefenseModifierBefore(-1), iDefenseModifierAfter(-1), iTotalDefense(-1), iDefenseDamageBefore(-1), iDefenseDamageAfter(-1), iDefenseDamageMax(-1) {}
};

static SASGameRecordCityBombardPending g_kSASGameRecordPendingCityBombard;
static std::vector<SASGameRecordPlotChangeGroup> g_aSASGameRecordPlotChanges;
static std::vector<std::pair<int,int> > g_aaSASGameRecordRevealedPlots[MAX_TEAMS];
static TeamTypes g_eSASGameRecordFullMapRevelationTeam = NO_TEAM;
static int g_iSASGameRecordFullMapRevealedBefore = 0;

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
	int iTerritoryImprovedLand;
	int iTerritoryImprovedWater;
	int iTerritoryRoaded;
	int iTerritoryFarms;
	int iTerritoryIrrigatedFarms;
	int iTerritoryDryFarms;
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
	int iCityAngryPopulation;
	int iCityWorkingPopulation;
	int iCityAssignedSpecialists;
	int iCityFreeSpecialistInstances;
	int iCityFreeSpecialistAllowance;
	int iCityGarrison;
};

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

struct SASGameRecordGlobalPrevious
{
	bool bValid;
	int iGlobalWarmingIndex;
	int iGlobalWarmingChances;
	int iOwnedLand;
	int iUnownedLand;
};

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

struct SASGameRecordTerritoryDevelopment
{
	SASGameRecordPlotComposition kOwned;
	std::vector<int> aiImprovedBonuses;
	std::vector<int> aiUnimprovedBonuses;
	int iBFCPlots;
	int iSuburbPlots;
	int iDevelopmentLand;
	int iDevelopmentWater;
	int iImprovedLand;
	int iImprovedWater;
	int iBFCDevelopmentLand;
	int iBFCImprovedLand;
	int iSuburbDevelopmentLand;
	int iSuburbImprovedLand;
	int iFarms;
	int iIrrigatedFarms;
	int iDryFarms;
	int iBonusFarms;
	int iIrrigatedBonusFarms;
	int iDryBonusFarms;
	int iBFCFarms;
	int iBFCIrrigatedFarms;
	int iBFCDryFarms;
	SASGameRecordTerritoryDevelopment() : aiImprovedBonuses(GC.getNumBonusInfos(), 0), aiUnimprovedBonuses(GC.getNumBonusInfos(), 0), iBFCPlots(0), iSuburbPlots(0), iDevelopmentLand(0), iDevelopmentWater(0), iImprovedLand(0), iImprovedWater(0), iBFCDevelopmentLand(0), iBFCImprovedLand(0), iSuburbDevelopmentLand(0), iSuburbImprovedLand(0), iFarms(0), iIrrigatedFarms(0), iDryFarms(0), iBonusFarms(0), iIrrigatedBonusFarms(0), iDryBonusFarms(0), iBFCFarms(0), iBFCIrrigatedFarms(0), iBFCDryFarms(0) {}
};

static SASGameRecordPlayerPrevious g_akSASGameRecordPlayerPrevious[MAX_PLAYERS];
static SASGameRecordTeamPrevious g_akSASGameRecordTeamPrevious[MAX_TEAMS];
static SASGameRecordGlobalPrevious g_kSASGameRecordGlobalPrevious;

// <!-- custom: A team-pair war is a natural historical unit that action rows otherwise force external tools to reconstruct across many turns.
// Keep this transient and recorder-local: loaded saves start an explicitly partial observation, while declarations in the current log retain their exact start/cause.
// Battles and conquered cities are attributed only to the two teams directly involved, so simultaneous wars do not contaminate one another. (GPT-5.6-Sol + GPT-5.6 Thinking) -->
struct SASGameRecordWarSummary
{
	TeamTypes eTeamA;
	TeamTypes eTeamB;
	TeamTypes eDeclarer;
	TeamTypes eTarget;
	WarPlanTypes eInitialWarPlan;
	CvString szStartCause;
	bool bStartKnown;
	bool bPrimary;
	int iStartTurn;
	int iObservedStartTurn;
	int iWarSuccessStartA;
	int iWarSuccessStartB;
	int iWarSuccessA;
	int iWarSuccessB;
	int iUnitsDestroyedByA;
	int iUnitsDestroyedByB;
	int iProductionDestroyedByA;
	int iProductionDestroyedByB;
	int iCityPlotWinsA;
	int iCityPlotWinsB;
	int iCitiesCapturedByA;
	int iCitiesCapturedByB;
	int iPopulationCapturedByA;
	int iPopulationCapturedByB;
	int iLastOngoingSummaryTurn;

	SASGameRecordWarSummary() : eTeamA(NO_TEAM), eTeamB(NO_TEAM), eDeclarer(NO_TEAM), eTarget(NO_TEAM), eInitialWarPlan(NO_WARPLAN), bStartKnown(false), bPrimary(false),
			iStartTurn(-1), iObservedStartTurn(-1), iWarSuccessStartA(0), iWarSuccessStartB(0), iWarSuccessA(0), iWarSuccessB(0),
			iUnitsDestroyedByA(0), iUnitsDestroyedByB(0), iProductionDestroyedByA(0), iProductionDestroyedByB(0), iCityPlotWinsA(0), iCityPlotWinsB(0),
			iCitiesCapturedByA(0), iCitiesCapturedByB(0), iPopulationCapturedByA(0), iPopulationCapturedByB(0), iLastOngoingSummaryTurn(-1) {}
};

static std::vector<SASGameRecordWarSummary> g_aSASGameRecordWars;

static bool getSASGameRecordWarPair(TeamTypes eFirst, TeamTypes eSecond, TeamTypes& eTeamA, TeamTypes& eTeamB)
{
	if (eFirst < 0 || eFirst >= MAX_CIV_TEAMS || eSecond < 0 || eSecond >= MAX_CIV_TEAMS || eFirst == eSecond)
		return false;
	eTeamA = (eFirst < eSecond ? eFirst : eSecond);
	eTeamB = (eFirst < eSecond ? eSecond : eFirst);
	return true;
}

static SASGameRecordWarSummary* findSASGameRecordWar(TeamTypes eFirst, TeamTypes eSecond)
{
	TeamTypes eTeamA;
	TeamTypes eTeamB;
	if (!getSASGameRecordWarPair(eFirst, eSecond, eTeamA, eTeamB))
		return NULL;
	for (size_t iI = 0; iI < g_aSASGameRecordWars.size(); iI++)
	{
		SASGameRecordWarSummary& kWar = g_aSASGameRecordWars[iI];
		if (kWar.eTeamA == eTeamA && kWar.eTeamB == eTeamB)
			return &kWar;
	}
	return NULL;
}

static void refreshSASGameRecordWarSuccess(SASGameRecordWarSummary& kWar)
{
	if (kWar.eTeamA < 0 || kWar.eTeamB < 0)
		return;
	kWar.iWarSuccessA = GET_TEAM(kWar.eTeamA).AI_getWarSuccess(kWar.eTeamB).round();
	kWar.iWarSuccessB = GET_TEAM(kWar.eTeamB).AI_getWarSuccess(kWar.eTeamA).round();
}

// <!-- custom: Release builds remove FAssert expressions entirely. Calling getSASGameRecordWarPair only from an assertion left both normalized teams at NO_TEAM and reproducibly crashed turn-110 reconciliation; validate in executed code and return NULL instead. (GPT-5.6-Sol) -->
static SASGameRecordWarSummary* addSASGameRecordWar(TeamTypes eFirst, TeamTypes eSecond, bool bStartKnown, TeamTypes eDeclarer, TeamTypes eTarget, WarPlanTypes eWarPlan, char const* szStartCause, bool bPrimary)
{
	TeamTypes eTeamA;
	TeamTypes eTeamB;
	if (!getSASGameRecordWarPair(eFirst, eSecond, eTeamA, eTeamB))
		return NULL;
	SASGameRecordWarSummary* pExisting = findSASGameRecordWar(eTeamA, eTeamB);
	if (pExisting != NULL)
		return pExisting;
	g_aSASGameRecordWars.push_back(SASGameRecordWarSummary());
	SASGameRecordWarSummary& kWar = g_aSASGameRecordWars.back();
	kWar.eTeamA = eTeamA;
	kWar.eTeamB = eTeamB;
	kWar.eDeclarer = eDeclarer;
	kWar.eTarget = eTarget;
	kWar.eInitialWarPlan = eWarPlan;
	kWar.szStartCause = szStartCause;
	kWar.bStartKnown = bStartKnown;
	kWar.bPrimary = bPrimary;
	kWar.iStartTurn = (bStartKnown ? GC.getGame().getGameTurn() : -1);
	kWar.iObservedStartTurn = GC.getGame().getGameTurn();
	refreshSASGameRecordWarSuccess(kWar);
	kWar.iWarSuccessStartA = kWar.iWarSuccessA;
	kWar.iWarSuccessStartB = kWar.iWarSuccessB;
	return &kWar;
}

static void initializeSASGameRecordWarsFromLoadedSave()
{
	for (int iA = 0; iA < MAX_CIV_TEAMS; iA++)
	{
		TeamTypes const eTeamA = (TeamTypes)iA;
		if (!GET_TEAM(eTeamA).isAlive())
			continue;
		for (int iB = iA + 1; iB < MAX_CIV_TEAMS; iB++)
		{
			TeamTypes const eTeamB = (TeamTypes)iB;
			if (GET_TEAM(eTeamB).isAlive() && GET_TEAM(eTeamA).isAtWar(eTeamB))
				addSASGameRecordWar(eTeamA, eTeamB, false, NO_TEAM, NO_TEAM, NO_WARPLAN, "PREEXISTING_ON_LOAD", false);
		}
	}
}

static void logSASGameRecordWarSummary(SASGameRecordWarSummary& kWar, char const* szStatus, char const* szTrigger, int iSummaryTurn, int iWarSuccessA, int iWarSuccessB, bool bCapitulate, TeamTypes eBroker, bool bRandomEvent, bool bReparations)
{
	kWar.iWarSuccessA = iWarSuccessA;
	kWar.iWarSuccessB = iWarSuccessB;
	bool const bEnded = (strcmp(szStatus, "ENDED") == 0);
	bool const bStateReconciliation = (strcmp(szTrigger, "STATE_RECONCILIATION") == 0);
	char const* szEndCause = (!bEnded ? "-" : (bStateReconciliation ? "STATE_CHANGE" : (bCapitulate ? "CAPITULATION" : (bRandomEvent ? "RANDOM_EVENT" : (eBroker != NO_TEAM ? "BROKERED_PEACE" : "PEACE")))));
	int const iElapsedTurns = (kWar.bStartKnown ? iSummaryTurn - kWar.iStartTurn : -1);
	int const iObservedTurns = iSummaryTurn - kWar.iObservedStartTurn;
	logSASGameRecord("GAME_RECORD_WAR_SUMMARY turn=%d status=%s trigger=%s teamA=%d teamB=%d startKnown=%d startTurn=%d observedStartTurn=%d endTurn=%d elapsedTurns=%d observedTurns=%d declarerTeam=%d targetTeam=%d startCause=%s initialWarPlan=%s primary=%d endCause=%s brokerTeam=%d reparations=%d warSuccessObservedStartA=%d warSuccessObservedStartB=%d warSuccessEndA=%d warSuccessEndB=%d warSuccessObservedGainA=%+d warSuccessObservedGainB=%+d unitsDestroyedByA=%d unitsDestroyedByB=%d unitProductionCostDestroyedByA=%d unitProductionCostDestroyedByB=%d cityPlotWinsA=%d cityPlotWinsB=%d citiesCapturedByA=%d citiesCapturedByB=%d populationCapturedByA=%d populationCapturedByB=%d",
			iSummaryTurn, szStatus, szTrigger, kWar.eTeamA, kWar.eTeamB,
			kWar.bStartKnown, kWar.iStartTurn, kWar.iObservedStartTurn, bEnded ? iSummaryTurn : -1, iElapsedTurns, iObservedTurns,
			kWar.eDeclarer, kWar.eTarget, kWar.szStartCause.GetCString(), getSASWarPlanType(kWar.eInitialWarPlan), kWar.bPrimary,
			szEndCause, eBroker, bReparations, kWar.iWarSuccessStartA, kWar.iWarSuccessStartB, kWar.iWarSuccessA, kWar.iWarSuccessB, kWar.iWarSuccessA - kWar.iWarSuccessStartA, kWar.iWarSuccessB - kWar.iWarSuccessStartB,
			kWar.iUnitsDestroyedByA, kWar.iUnitsDestroyedByB, kWar.iProductionDestroyedByA, kWar.iProductionDestroyedByB, kWar.iCityPlotWinsA, kWar.iCityPlotWinsB,
			kWar.iCitiesCapturedByA, kWar.iCitiesCapturedByB, kWar.iPopulationCapturedByA, kWar.iPopulationCapturedByB);
}

static void logSASGameRecordOngoingWarSummaries(char const* szReason)
{
	int const iGameTurn = GC.getGame().getGameTurn();
	for (size_t iI = 0; iI < g_aSASGameRecordWars.size(); iI++)
	{
		SASGameRecordWarSummary& kWar = g_aSASGameRecordWars[iI];
		if (kWar.iLastOngoingSummaryTurn == iGameTurn)
			continue;
		refreshSASGameRecordWarSuccess(kWar);
		logSASGameRecordWarSummary(kWar, "ONGOING", szReason, iGameTurn, kWar.iWarSuccessA, kWar.iWarSuccessB, false, NO_TEAM, false, false);
		kWar.iLastOngoingSummaryTurn = iGameTurn;
	}
}

static void reconcileSASGameRecordWars()
{
	for (size_t iI = 0; iI < g_aSASGameRecordWars.size();)
	{
		SASGameRecordWarSummary& kWar = g_aSASGameRecordWars[iI];
		if (GET_TEAM(kWar.eTeamA).isAtWar(kWar.eTeamB))
		{
			iI++;
			continue;
		}
		refreshSASGameRecordWarSuccess(kWar);
		logSASGameRecordWarSummary(kWar, "ENDED", "STATE_RECONCILIATION", GC.getGame().getGameTurn(), kWar.iWarSuccessA, kWar.iWarSuccessB, false, NO_TEAM, false, false);
		g_aSASGameRecordWars.erase(g_aSASGameRecordWars.begin() + iI);
	}
}

static int getSASGameRecordDelta(bool bValid, int iCurrent, int iPrevious)
{
	return bValid ? iCurrent - iPrevious : 0;
}

static void resetSASGameRecordState()
{
	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		g_aiSASGameRecordBattleWins[iI] = 0;
		g_aiSASGameRecordBattleLosses[iI] = 0;
		g_aiSASGameRecordCityBattleWins[iI] = 0;
		g_aiSASGameRecordCityBattleLosses[iI] = 0;
		g_aiSASGameRecordTotalBattleWins[iI] = 0;
		g_aiSASGameRecordTotalBattleLosses[iI] = 0;
		g_aiSASGameRecordTotalCityBattleWins[iI] = 0;
		g_aiSASGameRecordTotalCityBattleLosses[iI] = 0;
		g_akSASGameRecordBattleQuality[iI].reset();
		g_akSASGameRecordTotalBattleQuality[iI].reset();
		g_akSASGameRecordMilitaryQualityTotals[iI].reset();
		g_aiSASGameRecordLoggedGoldenAgeTurns[iI] = 0;
		g_aiSASGameRecordLoggedAnarchyTurns[iI] = 0;
		g_akSASGameRecordResearchPrevious[iI].bValid = false;
		g_akSASGameRecordResearchPrevious[iI].eTeam = NO_TEAM;
		g_akSASGameRecordResearchPrevious[iI].eTech = NO_TECH;
		g_akSASGameRecordResearchPrevious[iI].iTechCount = 0;
		g_akSASGameRecordResearchPrevious[iI].ePendingCause = RESEARCH_TARGET_CHANGE_UNKNOWN;
		g_akSASGameRecordResearchApplication[iI].bValid = false;
		g_akSASGameRecordResearchApplication[iI].iGameTurn = -1;
		g_akSASGameRecordResearchApplication[iI].eTech = NO_TECH;
		g_akSASGameRecordResearchApplication[iI].iModifiedResearchRate = 0;
		g_akSASGameRecordResearchApplication[iI].iIncomingOverflowUnmodified = 0;
		g_akSASGameRecordResearchApplication[iI].iIncomingOverflowModified = 0;
		g_aiSASGameRecordCitiesAcquired[iI] = 0;
		g_aiSASGameRecordCitiesLost[iI] = 0;
		g_aiSASGameRecordCitiesConquered[iI] = 0;
		g_aiSASGameRecordCitiesLostByConquest[iI] = 0;
		g_aiSASGameRecordCitiesTradedIn[iI] = 0;
		g_aiSASGameRecordCitiesTradedOut[iI] = 0;
		g_akSASGameRecordPlayerFlow[iI].reset();
		g_akSASGameRecordPlayerPrevious[iI].bValid = false;
	}
	for (int iI = 0; iI < MAX_TEAMS; iI++)
	{
		g_akSASGameRecordTeamPrevious[iI].bValid = false;
		g_akSASGameRecordTeamPrevious[iI].bContactsValid = false;
	}
	g_kSASGameRecordGlobalPrevious.bValid = false;
	g_aSASGameRecordWars.clear();
	g_aSASGameRecordCityRazeContexts.clear();
	g_aSASGameRecordCombatPending.clear();
	g_aSASGameRecordBlockades.clear();
	g_iSASGameRecordLastFullSnapshotTurn = -1;
	g_iSASGameRecordBattleStartTurn = GC.getGame().getGameTurn();
	g_iSASGameRecordFlowStartTurn = GC.getGame().getGameTurn();
	g_iSASGameRecordPendingPlotTurn = -1;
	g_kSASGameRecordPendingCityBombard = SASGameRecordCityBombardPending();
	g_aSASGameRecordPlotChanges.clear();
	g_eSASGameRecordFullMapRevelationTeam = NO_TEAM;
	g_iSASGameRecordFullMapRevealedBefore = 0;
	// <!-- custom: AI Auto Play stops when a save is loaded, so clear its recorder-local request boundary and active-player transfer counters with the rest of the new log-session state. See KI#203. (GPT-5.6-Sol) -->
	g_iSASGameRecordAutoPlayRequestId = 0;
	g_iSASGameRecordAutoPlayRequestedTurns = 0;
	g_iSASGameRecordAutoPlayStartTurn = -1;
	g_iSASGameRecordAutoPlayStartElapsedTurn = -1;
	g_eSASGameRecordAutoPlayStartPlayer = NO_PLAYER;
	g_uiSASGameRecordAutoPlayStartTime = 0;
	g_iSASGameRecordAutoPlayPlayerChanges = 0;
	g_iSASGameRecordTotalActivePlayerChanges = 0;
	for (int iI = 0; iI < MAX_TEAMS; iI++)
		g_aaSASGameRecordRevealedPlots[iI].clear();
}

// <!-- custom: Team snapshots intentionally list living members, but CvTeam::addTeam reassigns every player slot on the absorbed team.
// Keep a separate exact helper for that rare structural boundary. (ChatGPT-5.6-Sol) -->
static CvString getSASGameRecordTeamAssignedPlayers(TeamTypes eTeam, int& iCount)
{
	iCount = 0;
	CvString szList;
	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		PlayerTypes const eLoopPlayer = (PlayerTypes)iI;
		if (GET_PLAYER(eLoopPlayer).getTeam() != eTeam)
			continue;
		appendSASDiagnosticIntListValue(szList, eLoopPlayer);
		iCount++;
	}
	return getSASDiagnosticOrDash(szList);
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

static const char* getSASGameRecordTechType(TechTypes eTech)
{
	return (eTech == NO_TECH ? "-" : GC.getInfo(eTech).getType());
}

static const char* getSASGameRecordEventTriggerType(EventTriggerTypes eTrigger)
{
	return (eTrigger == NO_EVENTTRIGGER ? "-" : GC.getInfo(eTrigger).getType());
}

static const char* getSASGameRecordEventType(EventTypes eEvent)
{
	return (eEvent == NO_EVENT ? "-" : GC.getInfo(eEvent).getType());
}

static const char* getSASGameRecordGoodyType(GoodyTypes eGoody)
{
	return (eGoody == NO_GOODY ? "-" : GC.getInfo(eGoody).getType());
}

static const char* getSASGameRecordReligionType(ReligionTypes eReligion)
{
	return (eReligion == NO_RELIGION ? "-" : GC.getInfo(eReligion).getType());
}

static const char* getSASGameRecordCorporationType(CorporationTypes eCorporation)
{
	return (eCorporation == NO_CORPORATION ? "-" : GC.getInfo(eCorporation).getType());
}

static const char* getSASGameRecordBuildingType(BuildingTypes eBuilding)
{
	return (eBuilding == NO_BUILDING ? "-" : GC.getInfo(eBuilding).getType());
}

static const char* getSASGameRecordProjectType(ProjectTypes eProject)
{
	return (eProject == NO_PROJECT ? "-" : GC.getInfo(eProject).getType());
}

static const char* getSASGameRecordUnitType(UnitTypes eUnit)
{
	return (eUnit == NO_UNIT ? "-" : GC.getInfo(eUnit).getType());
}

static const char* getSASGameRecordBonusType(BonusTypes eBonus)
{
	return (eBonus == NO_BONUS ? "-" : GC.getInfo(eBonus).getType());
}

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

SASGameRecordGoodyResult::SASGameRecordGoodyResult() :
	bFollowupOutcome(false), bUpgradeRoll(false), bUpgradeApplied(false), bAdditionalOutcomeAttempted(false),
	iGold(0), iNewlyRevealedPlots(0), iExperienceGained(0), iDamageHealed(0), eTech(NO_TECH), iTechRewardValue(0),
	iTechProgressBefore(-1), iTechProgressAfter(-1), iTechCost(-1), bTechCompleted(false), iFreePromotionsGranted(0)
{}

SASGameRecordPlotState::SASGameRecordPlotState() : eTerrain(NO_TERRAIN), eFeature(NO_FEATURE), eBonus(NO_BONUS), eImprovement(NO_IMPROVEMENT), eRoute(NO_ROUTE)
{
	for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
		aiExtraYield[iI] = 0;
}

SASGameRecordPlotState::SASGameRecordPlotState(CvPlot const& kPlot) : eTerrain(kPlot.getTerrainType()), eFeature(kPlot.getFeatureType()), eBonus(kPlot.getBonusType()), eImprovement(kPlot.getImprovementType()), eRoute(kPlot.getRouteType())
{
	for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
		aiExtraYield[iI] = GC.getMap().getPlotExtraYield(kPlot, (YieldTypes)iI);
}

static bool isSASGameRecordPlotStateChanged(SASGameRecordPlotState const& kOldState, CvPlot const& kPlot)
{
	if (kOldState.eTerrain != kPlot.getTerrainType() || kOldState.eFeature != kPlot.getFeatureType() || kOldState.eBonus != kPlot.getBonusType() || kOldState.eImprovement != kPlot.getImprovementType() || kOldState.eRoute != kPlot.getRouteType())
		return true;
	for (int iI = 0; iI < NUM_YIELD_TYPES; iI++)
	{
		if (kOldState.aiExtraYield[iI] != GC.getMap().getPlotExtraYield(kPlot, (YieldTypes)iI))
			return true;
	}
	return false;
}

static void addSASGameRecordCoordinate(std::vector<std::pair<int,int> >& aCoordinates, CvPlot const& kPlot)
{
	std::pair<int,int> const kCoordinate(kPlot.getX(), kPlot.getY());
	if (std::find(aCoordinates.begin(), aCoordinates.end(), kCoordinate) == aCoordinates.end())
		aCoordinates.push_back(kCoordinate);
}

static void appendSASGameRecordCoordinateChunks(std::vector<CvString>& aszChunks, CvString& szChunk, char const* szCategory, std::vector<std::pair<int,int> > const& aCoordinates)
{
	for (size_t iI = 0; iI < aCoordinates.size(); iI++)
	{
		CvString szItem;
		if (iI == 0)
			szItem.Format("%s%s=(%d,%d)", szChunk.empty() ? "" : " ", szCategory, aCoordinates[iI].first, aCoordinates[iI].second);
		else szItem.Format(",(%d,%d)", aCoordinates[iI].first, aCoordinates[iI].second);
		if (!szChunk.empty() && szChunk.length() + szItem.length() > 1500)
		{
			aszChunks.push_back(szChunk);
			szChunk.clear();
			szItem.Format("%s=(%d,%d)", szCategory, aCoordinates[iI].first, aCoordinates[iI].second);
		}
		szChunk += szItem;
	}
}

static int getSASGameRecordRevealedPlotCount(TeamTypes eTeam)
{
	int iRevealed = 0;
	int iLoop = 0;
	for (CvArea const* pLoopArea = GC.getMap().firstArea(&iLoop); pLoopArea != NULL; pLoopArea = GC.getMap().nextArea(&iLoop))
		iRevealed += pLoopArea->getNumRevealedTiles(eTeam);
	return iRevealed;
}

void beginSASGameRecordFullMapRevelation(TeamTypes eTeam, TechTypes eTech)
{
	FAssert(g_eSASGameRecordFullMapRevelationTeam == NO_TEAM);
	FAssert(eTeam >= 0 && eTeam < MAX_CIV_TEAMS);
	FAssert(eTech != NO_TECH);
	g_eSASGameRecordFullMapRevelationTeam = eTeam;
	g_iSASGameRecordFullMapRevealedBefore = getSASGameRecordRevealedPlotCount(eTeam);
}

void endSASGameRecordFullMapRevelation(TeamTypes eTeam, TechTypes eTech)
{
	FAssert(g_eSASGameRecordFullMapRevelationTeam == eTeam);
	int const iRevealed = getSASGameRecordRevealedPlotCount(eTeam);
	int const iNewlyRevealed = iRevealed - g_iSASGameRecordFullMapRevealedBefore;
	int const iRevealedPctX100 = (10000 * iRevealed) / std::max(1, (int)GC.getMap().numPlots());
	logSASGameRecord("GAME_RECORD_MAP_REVELATION turn=%d team=%d cause=MAP_VISIBLE_TECH tech=%s revealMode=FULL_MAP newlyRevealedCount=%d revealedPlots=%d revealedPctX100=%d", GC.getGame().getGameTurn(), eTeam, getSASGameRecordTechType(eTech), iNewlyRevealed, iRevealed, iRevealedPctX100);
	g_eSASGameRecordFullMapRevelationTeam = NO_TEAM;
	g_iSASGameRecordFullMapRevealedBefore = 0;
}

void flushSASGameRecordTurnChanges(int iGameTurn)
{
	flushSASGameRecordPendingCityBombard();
	if (g_iSASGameRecordPendingPlotTurn < 0)
		return;
	FAssert(iGameTurn == g_iSASGameRecordPendingPlotTurn);
	int const iLoggedTurn = iGameTurn;
	std::vector<CvString> aszPlotChunks;
	CvString szPlotChunk;
	for (size_t iI = 0; iI < g_aSASGameRecordPlotChanges.size(); iI++)
		appendSASGameRecordCoordinateChunks(aszPlotChunks, szPlotChunk, g_aSASGameRecordPlotChanges[iI].szCategory.GetCString(), g_aSASGameRecordPlotChanges[iI].aCoordinates);
	if (!szPlotChunk.empty())
		aszPlotChunks.push_back(szPlotChunk);
	for (size_t iI = 0; iI < aszPlotChunks.size(); iI++)
		logSASGameRecord("GAME_RECORD_PLOT_CHANGES turn=%d part=%d parts=%d changes=%s", iLoggedTurn, (int)iI + 1, (int)aszPlotChunks.size(), aszPlotChunks[iI].GetCString());
	for (int iI = 0; iI < MAX_CIV_TEAMS; iI++)
	{
		std::vector<std::pair<int,int> > const& aCoordinates = g_aaSASGameRecordRevealedPlots[iI];
		if (aCoordinates.empty())
			continue;
		std::vector<CvString> aszRevelationChunks;
		CvString szRevelationChunk;
		appendSASGameRecordCoordinateChunks(aszRevelationChunks, szRevelationChunk, "newlyRevealed", aCoordinates);
		if (!szRevelationChunk.empty())
			aszRevelationChunks.push_back(szRevelationChunk);
		// <!-- custom: CvMap::numPlots returns PlotNumTypes; casting it to int keeps MSVC 7.1 std::max template deduction unambiguous and fixed the compile error from adding map-revelation percentages. (GPT-5.6-Sol) -->
		int const iRevealedPctX100 = (10000 * getSASGameRecordRevealedPlotCount((TeamTypes)iI)) / std::max(1, (int)GC.getMap().numPlots());
		for (size_t iJ = 0; iJ < aszRevelationChunks.size(); iJ++)
			logSASGameRecord("GAME_RECORD_MAP_REVELATION turn=%d team=%d cause=INCREMENTAL revealMode=COORDINATES newlyRevealedCount=%d part=%d parts=%d revealedPctX100=%d %s", iLoggedTurn, iI, (int)aCoordinates.size(), (int)iJ + 1, (int)aszRevelationChunks.size(), iRevealedPctX100, aszRevelationChunks[iJ].GetCString());
	}
	g_iSASGameRecordPendingPlotTurn = -1;
	g_aSASGameRecordPlotChanges.clear();
	for (int iI = 0; iI < MAX_TEAMS; iI++)
		g_aaSASGameRecordRevealedPlots[iI].clear();
}

// <!-- custom: Session rollover previously reset pending city-bombard, plot-change and incremental-revelation observations without writing them.
// Flush while the old game/map still supply the matching turn and revelation totals, before CvGame::init or CvMap::read resets the corresponding state. See KI#382. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
void finalizeSASGameRecordLogSession()
{
	// <!-- custom: If initialization aborts before gameStart, keep its setup actions even though authoritative context could not be completed. (ChatGPT-5.6-Sol) -->
	if (g_bSASGameRecordBufferInitializingActions)
	{
		g_bSASGameRecordBufferInitializingActions = false;
		flushSASGameRecordInitializingActions(false);
	}
	if (g_iSASGameRecordPendingPlotTurn >= 0)
		flushSASGameRecordTurnChanges(g_iSASGameRecordPendingPlotTurn);
	else flushSASGameRecordPendingCityBombard();
}

static void prepareSASGameRecordTurnChanges()
{
	int const iGameTurn = GC.getGame().getGameTurn();
	if (g_iSASGameRecordPendingPlotTurn >= 0 && g_iSASGameRecordPendingPlotTurn != iGameTurn)
		flushSASGameRecordTurnChanges(g_iSASGameRecordPendingPlotTurn);
	if (g_iSASGameRecordPendingPlotTurn < 0)
		g_iSASGameRecordPendingPlotTurn = iGameTurn;
}

static void bufferSASGameRecordPlotChangeCoordinate(CvPlot const& kPlot, char const* szCategory)
{
	prepareSASGameRecordTurnChanges();
	SASGameRecordPlotChangeGroup* pGroup = NULL;
	for (size_t iI = 0; iI < g_aSASGameRecordPlotChanges.size(); iI++)
	{
		if (g_aSASGameRecordPlotChanges[iI].szCategory == szCategory)
		{
			pGroup = &g_aSASGameRecordPlotChanges[iI];
			break;
		}
	}
	if (pGroup == NULL)
	{
		SASGameRecordPlotChangeGroup kGroup;
		kGroup.szCategory = szCategory;
		g_aSASGameRecordPlotChanges.push_back(kGroup);
		pGroup = &g_aSASGameRecordPlotChanges.back();
	}
	addSASGameRecordCoordinate(pGroup->aCoordinates, kPlot);
}

void recordSASGameRecordPlotChange(CvPlot const& kPlot, SASGameRecordPlotState const& kOldState, char const* szCategory, char const* szCause, bool bDetailed)
{
	if (GC.getGame().getElapsedGameTurns() <= 0 || !isSASGameRecordPlotStateChanged(kOldState, kPlot))
		return;
	bufferSASGameRecordPlotChangeCoordinate(kPlot, szCategory);
	if (bDetailed)
	{
		logSASGameRecord("GAME_RECORD_PLOT_CHANGE turn=%d cause=%s category=%s x=%d y=%d owner=%d terrainOld=%s terrainNew=%s featureOld=%s featureNew=%s bonusOld=%s bonusNew=%s improvementOld=%s improvementNew=%s routeOld=%s routeNew=%s extraFoodOld=%d extraFoodNew=%d extraProductionOld=%d extraProductionNew=%d extraCommerceOld=%d extraCommerceNew=%d",
				GC.getGame().getGameTurn(), szCause, szCategory, kPlot.getX(), kPlot.getY(), kPlot.getOwner(),
				getSASGameRecordTerrainType(kOldState.eTerrain), getSASGameRecordTerrainType(kPlot.getTerrainType()),
				getSASGameRecordFeatureType(kOldState.eFeature), getSASGameRecordFeatureType(kPlot.getFeatureType()),
				getSASGameRecordBonusType(kOldState.eBonus), getSASGameRecordBonusType(kPlot.getBonusType()),
				getSASGameRecordImprovementType(kOldState.eImprovement), getSASGameRecordImprovementType(kPlot.getImprovementType()),
				getSASGameRecordRouteType(kOldState.eRoute), getSASGameRecordRouteType(kPlot.getRouteType()),
				kOldState.aiExtraYield[YIELD_FOOD], GC.getMap().getPlotExtraYield(kPlot, YIELD_FOOD),
				kOldState.aiExtraYield[YIELD_PRODUCTION], GC.getMap().getPlotExtraYield(kPlot, YIELD_PRODUCTION),
				kOldState.aiExtraYield[YIELD_COMMERCE], GC.getMap().getPlotExtraYield(kPlot, YIELD_COMMERCE));
	}
}

void logSASGameRecordRiverEdgeChanged(CvPlot const& kPlot, bool bOldSouthBoundary, bool bOldEastBoundary)
{
	bool const bNewSouthBoundary = kPlot.isNOfRiver();
	bool const bNewEastBoundary = kPlot.isWOfRiver();
	if (bOldSouthBoundary == bNewSouthBoundary && bOldEastBoundary == bNewEastBoundary) return;
	bufferSASGameRecordPlotChangeCoordinate(kPlot, "riverChanges");
	logSASGameRecord("GAME_RECORD_RIVER_EDGE_CHANGE turn=%d x=%d y=%d owner=%d southBoundaryOld=%d southBoundaryNew=%d eastBoundaryOld=%d eastBoundaryNew=%d",
			GC.getGame().getGameTurn(), kPlot.getX(), kPlot.getY(), kPlot.getOwner(), bOldSouthBoundary, bNewSouthBoundary, bOldEastBoundary, bNewEastBoundary);
}

void recordSASGameRecordPlotRevealed(CvPlot const& kPlot, TeamTypes eTeam)
{
	if (GC.getGame().getElapsedGameTurns() <= 0 || eTeam < 0 || eTeam >= MAX_CIV_TEAMS)
		return;
	if (eTeam == g_eSASGameRecordFullMapRevelationTeam)
		return;
	prepareSASGameRecordTurnChanges();
	// <!-- custom: setRevealed calls this only on false-to-true transitions, so the same team cannot add this plot twice without first losing permanent revelation; append directly instead of repeatedly searching a potentially large map-trade list. (GPT-5.6-Sol) -->
	g_aaSASGameRecordRevealedPlots[eTeam].push_back(std::make_pair(kPlot.getX(), kPlot.getY()));
}

void logSASGameRecordEnvironmentTurn(int iPollution, int iSustainabilityThreshold, int iLandDefense, int iIndexBefore, int iIndexBeforeRestoration, int iIndexEnd, int iWarmingChances, int iEventTally)
{
	logSASGameRecord("GAME_RECORD_ENVIRONMENT_TURN turn=%d pollution=%d sustainabilityThreshold=%d landDefense=%d totalDefense=%d indexBefore=%d indexBeforeRestoration=%d indexEnd=%d indexDelta=%+d warmingChances=%d eventTally=%d severityPercent=%d active=%d",
			GC.getGame().getGameTurn(), iPollution, iSustainabilityThreshold, iLandDefense, iSustainabilityThreshold + iLandDefense, iIndexBefore, iIndexBeforeRestoration, iIndexEnd, iIndexEnd - iIndexBefore, iWarmingChances, iEventTally, GC.getGame().calculateGwSeverityRating(), iIndexEnd > 0);
}

void logSASGameRecordBonusChanged(CvPlot const* pPlot, BonusTypes eOldBonus, BonusTypes eNewBonus)
{
	if (pPlot == NULL || eOldBonus == eNewBonus)
		return;
	SASGameRecordPlotState kOldState(*pPlot);
	kOldState.eBonus = eOldBonus;
	recordSASGameRecordPlotChange(*pPlot, kOldState, "resourceChanges", "RESOURCE_CHANGE", false);
	const char* szAction = (eOldBonus == NO_BONUS ? "appeared" : (eNewBonus == NO_BONUS ? "disappeared" : "changed"));
	CvCity const* pWorkingCity = pPlot->getWorkingCity();
	CvCity const* pPlotCity = pPlot->getPlotCity();
	// <!-- custom: Reproducible T129 crash dumps after adding this row failed in msvcr71!_output/_vsnprintf with an invalid read at 0x000003fc. The original argument for area=%d was pPlot->getArea(), but CvPlot::getArea returns CvArea&, not an integer; passing that object reference through varargs corrupted the following formatter reads. Logging the area ID explicitly fixed the crash in the next test run. (GPT-5.5) -->
	logSASGameRecord("GAME_RECORD_BONUS_CHANGE turn=%d elapsed=%d action=%s x=%d y=%d area=%d owner=%d oldBonus=%s newBonus=%s terrain=%s feature=%s improvement=%s route=%s water=%d hills=%d peak=%d riverSide=%d cityRadius=%d workingCity=%S workingCityId=%d plotCity=%S plotCityId=%d",
			GC.getGame().getGameTurn(), GC.getGame().getElapsedGameTurns(), szAction, pPlot->getX(), pPlot->getY(), pPlot->getArea().getID(), pPlot->getOwner(), getSASGameRecordBonusType(eOldBonus), getSASGameRecordBonusType(eNewBonus), getSASGameRecordTerrainType(pPlot->getTerrainType()), getSASGameRecordFeatureType(pPlot->getFeatureType()), getSASGameRecordImprovementType(pPlot->getImprovementType()), getSASGameRecordRouteType(pPlot->getRouteType()), pPlot->isWater(), pPlot->isHills(), pPlot->isPeak(), pPlot->isRiverSide(), pPlot->isCityRadius(), getSASGameRecordQuotedCityName(pWorkingCity).GetCString(), (pWorkingCity == NULL ? -1 : pWorkingCity->getID()), getSASGameRecordQuotedCityName(pPlotCity).GetCString(), (pPlotCity == NULL ? -1 : pPlotCity->getID()));
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

static const char* getSASGameRecordEspionageMissionType(EspionageMissionTypes eMission)
{
	return (eMission == NO_ESPIONAGEMISSION ? "-" : GC.getInfo(eMission).getType());
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

static const char* getSASGameRecordProcessType(ProcessTypes eProcess)
{
	return (eProcess == NO_PROCESS ? "-" : GC.getInfo(eProcess).getType());
}

static const char* getSASGameRecordCivicType(CivicTypes eCivic)
{
	return (eCivic == NO_CIVIC ? "-" : GC.getInfo(eCivic).getType());
}

static const char* getSASGameRecordVoteSourceType(VoteSourceTypes eVoteSource)
{
	return (eVoteSource == NO_VOTESOURCE ? "-" : GC.getInfo(eVoteSource).getType());
}

static const char* getSASGameRecordVoteType(VoteTypes eVote)
{
	return (eVote == NO_VOTE ? "-" : GC.getInfo(eVote).getType());
}

static const char* getSASGameRecordEraType(EraTypes eEra)
{
	return (eEra == NO_ERA ? "-" : GC.getInfo(eEra).getType());
}

static void appendSASGameRecordTypeCount(CvString& szList, const char* szType, int iCount)
{
	if (iCount <= 0)
		return;
	CvString szItem;
	szItem.Format(szList.empty() ? "%s:%d" : ",%s:%d", szType, iCount);
	szList += szItem;
}

static void addSASGameRecordCityBombardTypeCount(std::vector<std::pair<CvString,int> >& aCounts, char const* szType)
{
	for (size_t iI = 0; iI < aCounts.size(); iI++)
	{
		if (aCounts[iI].first == szType)
		{
			aCounts[iI].second++;
			return;
		}
	}
	aCounts.push_back(std::make_pair(CvString(szType), 1));
}

static void flushSASGameRecordPendingCityBombard()
{
	if (!g_kSASGameRecordPendingCityBombard.bValid)
		return;
	CvString szUnitTypes;
	CvString szUnitAIs;
	for (size_t iI = 0; iI < g_kSASGameRecordPendingCityBombard.aUnitTypes.size(); iI++)
		appendSASGameRecordTypeCount(szUnitTypes, g_kSASGameRecordPendingCityBombard.aUnitTypes[iI].first.GetCString(), g_kSASGameRecordPendingCityBombard.aUnitTypes[iI].second);
	for (size_t iI = 0; iI < g_kSASGameRecordPendingCityBombard.aUnitAIs.size(); iI++)
		appendSASGameRecordTypeCount(szUnitAIs, g_kSASGameRecordPendingCityBombard.aUnitAIs[iI].first.GetCString(), g_kSASGameRecordPendingCityBombard.aUnitAIs[iI].second);
	if (szUnitTypes.empty()) szUnitTypes = "-";
	if (szUnitAIs.empty()) szUnitAIs = "-";
	g_bSASGameRecordFlushingCityBombard = true;
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=CITY_BOMBARD mode=%s player=%d targetPlayer=%d cityId=%d city=%S x=%d y=%d actions=%d unitTypes=%s unitAI=%s bombardRateTotal=%d ignoreBuildingDefenseActions=%d defenseModifierBefore=%d defenseModifierAfter=%d defenseReduction=%d totalDefense=%d defenseDamageBefore=%d defenseDamageAfter=%d defenseDamageMax=%d",
			g_kSASGameRecordPendingCityBombard.iTurn, g_kSASGameRecordPendingCityBombard.szMode.GetCString(), g_kSASGameRecordPendingCityBombard.ePlayer, g_kSASGameRecordPendingCityBombard.eTargetPlayer, g_kSASGameRecordPendingCityBombard.iCityId, g_kSASGameRecordPendingCityBombard.szCity.GetCString(), g_kSASGameRecordPendingCityBombard.iX, g_kSASGameRecordPendingCityBombard.iY, g_kSASGameRecordPendingCityBombard.iActions, szUnitTypes.GetCString(), szUnitAIs.GetCString(),
			g_kSASGameRecordPendingCityBombard.iBombardRateTotal, g_kSASGameRecordPendingCityBombard.iIgnoreBuildingDefenseActions, g_kSASGameRecordPendingCityBombard.iDefenseModifierBefore, g_kSASGameRecordPendingCityBombard.iDefenseModifierAfter, std::max(0, g_kSASGameRecordPendingCityBombard.iDefenseModifierBefore - g_kSASGameRecordPendingCityBombard.iDefenseModifierAfter), g_kSASGameRecordPendingCityBombard.iTotalDefense, g_kSASGameRecordPendingCityBombard.iDefenseDamageBefore, g_kSASGameRecordPendingCityBombard.iDefenseDamageAfter, g_kSASGameRecordPendingCityBombard.iDefenseDamageMax);
	g_bSASGameRecordFlushingCityBombard = false;
	g_kSASGameRecordPendingCityBombard = SASGameRecordCityBombardPending();
}

static void appendSASGameRecordPositiveValue(CvString& szList, const char* szName, int iValue)
{
	if (iValue <= 0)
		return;
	CvString szItem;
	szItem.Format(szList.empty() ? "%s:%d" : ",%s:%d", szName, iValue);
	szList += szItem;
}

static void appendSASGameRecordValue(CvString& szList, const char* szName, int iValue)
{
	CvString szItem;
	szItem.Format(szList.empty() ? "%s:%d" : ",%s:%d", szName, iValue);
	szList += szItem;
}

static void appendSASGameRecordSignedValue(CvString& szList, const char* szName, int iValue)
{
	if (iValue == 0)
		return;
	CvString szItem;
	szItem.Format(szList.empty() ? "%s:%+d" : ",%s:%+d", szName, iValue);
	szList += szItem;
}

static void addSASGameRecordPlotComposition(SASGameRecordPlotComposition& kComposition, CvPlot const& kPlot, TeamTypes eTeam)
{
	kComposition.iPlots++;
	if (kPlot.isWater())
		kComposition.iWater++;
	else kComposition.iLand++;
	if (kPlot.isHills())
		kComposition.iHills++;
	if (kPlot.isPeak())
		kComposition.iPeaks++;
	if (kPlot.isRiverSide())
		kComposition.iRiverSide++;
	if (kPlot.isFreshWater())
		kComposition.iFreshWater++;
	if (kPlot.isCoastalLand())
		kComposition.iCoastal++;
	if (kPlot.getTerrainType() != NO_TERRAIN)
		kComposition.aiTerrains[kPlot.getTerrainType()]++;
	if (kPlot.getFeatureType() != NO_FEATURE)
		kComposition.aiFeatures[kPlot.getFeatureType()]++;
	ImprovementTypes eImprovement = kPlot.getImprovementType();
	if (eImprovement != NO_IMPROVEMENT)
	{
		kComposition.iImproved++;
		kComposition.aiImprovements[eImprovement]++;
	}
	else if (!kPlot.isWater())
	{
		kComposition.iUnimprovedLand++;
	}
	RouteTypes eRoute = kPlot.getRouteType();
	if (eRoute != NO_ROUTE)
	{
		kComposition.iRoaded++;
		kComposition.aiRoutes[eRoute]++;
	}
	BonusTypes eBonus = kPlot.getBonusType(eTeam);
	if (eBonus != NO_BONUS)
	{
		kComposition.aiBonuses[eBonus]++;
		if (eImprovement != NO_IMPROVEMENT)
			kComposition.iBonusImproved++;
		else kComposition.iBonusUnimproved++;
	}
	if (kPlot.isBeingWorked())
	{
		kComposition.iWorked++;
		if (eImprovement != NO_IMPROVEMENT)
			kComposition.iWorkedImproved++;
		else kComposition.iWorkedUnimproved++;
	}
	kComposition.iNatureFood += kPlot.calculateBestNatureYield(YIELD_FOOD, eTeam);
	kComposition.iNatureProduction += kPlot.calculateBestNatureYield(YIELD_PRODUCTION, eTeam);
	kComposition.iNatureCommerce += kPlot.calculateBestNatureYield(YIELD_COMMERCE, eTeam);
	kComposition.iCurrentFood += kPlot.calculateYield(YIELD_FOOD);
	kComposition.iCurrentProduction += kPlot.calculateYield(YIELD_PRODUCTION);
	kComposition.iCurrentCommerce += kPlot.calculateYield(YIELD_COMMERCE);
}

static int getSASGameRecordPercentX100(int iValue, int iTotal)
{
	return (iTotal <= 0 ? -1 : (10000 * iValue) / iTotal);
}

// <!-- custom: Add lightweight owned-territory counts to the map scan already used by the expansion record, rather than scanning every plot again or calculating unused plot yields. BFC means the plot is assigned to one of this player's cities; development land excludes city centers and peaks because Workers cannot add ordinary improvements there. (GPT-5.6-Sol) -->
static void addSASGameRecordTerritoryDevelopment(SASGameRecordTerritoryDevelopment& kDevelopment, CvPlot const& kPlot, PlayerTypes ePlayer, TeamTypes eTeam, ImprovementTypes eFarm)
{
	SASGameRecordPlotComposition& kOwned = kDevelopment.kOwned;
	kOwned.iPlots++;
	if (kPlot.isWater())
		kOwned.iWater++;
	else kOwned.iLand++;
	if (kPlot.getTerrainType() != NO_TERRAIN)
		kOwned.aiTerrains[kPlot.getTerrainType()]++;
	if (kPlot.getFeatureType() != NO_FEATURE)
		kOwned.aiFeatures[kPlot.getFeatureType()]++;
	CvCity const* pWorkingCity = kPlot.getWorkingCity();
	bool const bBFC = (pWorkingCity != NULL && pWorkingCity->getOwner() == ePlayer);
	if (bBFC)
		kDevelopment.iBFCPlots++;
	else kDevelopment.iSuburbPlots++;
	ImprovementTypes const eImprovement = kPlot.getImprovementType();
	bool const bImproved = (eImprovement != NO_IMPROVEMENT);
	if (bImproved)
	{
		kOwned.iImproved++;
		kOwned.aiImprovements[eImprovement]++;
	}
	RouteTypes const eRoute = kPlot.getRouteType();
	if (eRoute != NO_ROUTE)
	{
		kOwned.iRoaded++;
		kOwned.aiRoutes[eRoute]++;
	}
	BonusTypes const eBonus = kPlot.getBonusType(eTeam);
	if (eBonus != NO_BONUS)
	{
		kOwned.aiBonuses[eBonus]++;
		if (bImproved)
		{
			kOwned.iBonusImproved++;
			kDevelopment.aiImprovedBonuses[eBonus]++;
		}
		else
		{
			kOwned.iBonusUnimproved++;
			kDevelopment.aiUnimprovedBonuses[eBonus]++;
		}
	}
	bool const bDevelopmentLand = (!kPlot.isWater() && !kPlot.isPeak() && !kPlot.isCity());
	if (bDevelopmentLand)
	{
		kDevelopment.iDevelopmentLand++;
		if (bImproved)
			kDevelopment.iImprovedLand++;
		if (bBFC)
		{
			kDevelopment.iBFCDevelopmentLand++;
			if (bImproved)
				kDevelopment.iBFCImprovedLand++;
		}
		else
		{
			kDevelopment.iSuburbDevelopmentLand++;
			if (bImproved)
				kDevelopment.iSuburbImprovedLand++;
		}
	}
	else if (kPlot.isWater() && (eBonus != NO_BONUS || bImproved))
	{
		// <!-- custom: Ordinary water cannot receive an improvement. Count only visible bonus water or an already improved water plot in the development denominator, so seafood coverage is not diluted by unusable ocean. (GPT-5.6-Sol) -->
		kDevelopment.iDevelopmentWater++;
		if (bImproved)
			kDevelopment.iImprovedWater++;
	}
	if (eImprovement != eFarm)
		return;
	kDevelopment.iFarms++;
	bool const bIrrigated = kPlot.isIrrigated();
	if (bIrrigated)
		kDevelopment.iIrrigatedFarms++;
	else kDevelopment.iDryFarms++;
	if (eBonus != NO_BONUS)
	{
		kDevelopment.iBonusFarms++;
		if (bIrrigated)
			kDevelopment.iIrrigatedBonusFarms++;
		else kDevelopment.iDryBonusFarms++;
	}
	if (bBFC)
	{
		kDevelopment.iBFCFarms++;
		if (bIrrigated)
			kDevelopment.iBFCIrrigatedFarms++;
		else kDevelopment.iBFCDryFarms++;
	}
}

static void getSASGameRecordImprovementRouteTypes(SASGameRecordPlotComposition const& kComposition, CvString& szImprovements, CvString& szRoutes)
{
	for (int iI = 0; iI < GC.getNumImprovementInfos(); iI++)
		appendSASGameRecordTypeCount(szImprovements, getSASGameRecordImprovementType((ImprovementTypes)iI), kComposition.aiImprovements[iI]);
	for (int iI = 0; iI < GC.getNumRouteInfos(); iI++)
		appendSASGameRecordTypeCount(szRoutes, getSASGameRecordRouteType((RouteTypes)iI), kComposition.aiRoutes[iI]);
}

static void getSASGameRecordLandscapeTypes(SASGameRecordPlotComposition const& kComposition, CvString& szTerrains, CvString& szFeatures, CvString& szBonuses)
{
	for (int iI = 0; iI < GC.getNumTerrainInfos(); iI++)
		appendSASGameRecordTypeCount(szTerrains, getSASGameRecordTerrainType((TerrainTypes)iI), kComposition.aiTerrains[iI]);
	for (int iI = 0; iI < GC.getNumFeatureInfos(); iI++)
		appendSASGameRecordTypeCount(szFeatures, getSASGameRecordFeatureType((FeatureTypes)iI), kComposition.aiFeatures[iI]);
	for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
		appendSASGameRecordTypeCount(szBonuses, getSASGameRecordBonusType((BonusTypes)iI), kComposition.aiBonuses[iI]);
}

static void logSASGameRecordTerritoryDevelopment(PlayerTypes ePlayer, int iGameTurn, SASGameRecordTerritoryDevelopment const& kDevelopment)
{
	SASGameRecordPlotComposition const& kOwned = kDevelopment.kOwned;
	SASGameRecordPlayerPrevious& kPrevious = g_akSASGameRecordPlayerPrevious[ePlayer];
	CvString szImprovements;
	CvString szRoutes;
	getSASGameRecordImprovementRouteTypes(kOwned, szImprovements, szRoutes);
	int const iDevelopmentPlots = kDevelopment.iDevelopmentLand + kDevelopment.iDevelopmentWater;
	int const iImprovedPlots = kDevelopment.iImprovedLand + kDevelopment.iImprovedWater;
	int const iSuburbFarms = kDevelopment.iFarms - kDevelopment.iBFCFarms;
	int const iSuburbIrrigatedFarms = kDevelopment.iIrrigatedFarms - kDevelopment.iBFCIrrigatedFarms;
	int const iSuburbDryFarms = kDevelopment.iDryFarms - kDevelopment.iBFCDryFarms;
	logSASGameRecord("GAME_RECORD_TERRITORY_DEVELOPMENT turn=%d player=%d deltaValid=%d ownedPlots=%d ownedLand=%d ownedWater=%d bfcPlots=%d suburbPlots=%d developmentPlots=%d improvedPlots=%d improvedPercentX100=%d developmentLand=%d improvedLand=%d improvedLandDelta=%+d improvedLandPercentX100=%d developmentWater=%d improvedWater=%d improvedWaterDelta=%+d improvedWaterPercentX100=%d"
			" bfcDevelopmentLand=%d bfcImprovedLand=%d bfcImprovedLandPercentX100=%d suburbDevelopmentLand=%d suburbImprovedLand=%d suburbImprovedLandPercentX100=%d roaded=%d roadedDelta=%+d bonusImproved=%d bonusUnimproved=%d"
			" farms=%d farmsDelta=%+d irrigatedFarms=%d irrigatedFarmsDelta=%+d dryFarms=%d dryFarmsDelta=%+d irrigatedFarmPercentX100=%d dryFarmPercentX100=%d bonusFarms=%d irrigatedBonusFarms=%d dryBonusFarms=%d bfcFarms=%d bfcIrrigatedFarms=%d bfcDryFarms=%d bfcIrrigatedFarmPercentX100=%d suburbFarms=%d suburbIrrigatedFarms=%d suburbDryFarms=%d suburbIrrigatedFarmPercentX100=%d improvements=%s routes=%s",
			iGameTurn, ePlayer, kPrevious.bValid, kOwned.iPlots, kOwned.iLand, kOwned.iWater, kDevelopment.iBFCPlots, kDevelopment.iSuburbPlots, iDevelopmentPlots, iImprovedPlots, getSASGameRecordPercentX100(iImprovedPlots, iDevelopmentPlots),
			kDevelopment.iDevelopmentLand, kDevelopment.iImprovedLand, getSASGameRecordDelta(kPrevious.bValid, kDevelopment.iImprovedLand, kPrevious.iTerritoryImprovedLand), getSASGameRecordPercentX100(kDevelopment.iImprovedLand, kDevelopment.iDevelopmentLand), kDevelopment.iDevelopmentWater, kDevelopment.iImprovedWater, getSASGameRecordDelta(kPrevious.bValid, kDevelopment.iImprovedWater, kPrevious.iTerritoryImprovedWater), getSASGameRecordPercentX100(kDevelopment.iImprovedWater, kDevelopment.iDevelopmentWater),
			kDevelopment.iBFCDevelopmentLand, kDevelopment.iBFCImprovedLand, getSASGameRecordPercentX100(kDevelopment.iBFCImprovedLand, kDevelopment.iBFCDevelopmentLand), kDevelopment.iSuburbDevelopmentLand, kDevelopment.iSuburbImprovedLand, getSASGameRecordPercentX100(kDevelopment.iSuburbImprovedLand, kDevelopment.iSuburbDevelopmentLand), kOwned.iRoaded, getSASGameRecordDelta(kPrevious.bValid, kOwned.iRoaded, kPrevious.iTerritoryRoaded), kOwned.iBonusImproved, kOwned.iBonusUnimproved,
			kDevelopment.iFarms, getSASGameRecordDelta(kPrevious.bValid, kDevelopment.iFarms, kPrevious.iTerritoryFarms), kDevelopment.iIrrigatedFarms, getSASGameRecordDelta(kPrevious.bValid, kDevelopment.iIrrigatedFarms, kPrevious.iTerritoryIrrigatedFarms), kDevelopment.iDryFarms, getSASGameRecordDelta(kPrevious.bValid, kDevelopment.iDryFarms, kPrevious.iTerritoryDryFarms), getSASGameRecordPercentX100(kDevelopment.iIrrigatedFarms, kDevelopment.iFarms), getSASGameRecordPercentX100(kDevelopment.iDryFarms, kDevelopment.iFarms),
			kDevelopment.iBonusFarms, kDevelopment.iIrrigatedBonusFarms, kDevelopment.iDryBonusFarms, kDevelopment.iBFCFarms, kDevelopment.iBFCIrrigatedFarms, kDevelopment.iBFCDryFarms, getSASGameRecordPercentX100(kDevelopment.iBFCIrrigatedFarms, kDevelopment.iBFCFarms), iSuburbFarms, iSuburbIrrigatedFarms, iSuburbDryFarms, getSASGameRecordPercentX100(iSuburbIrrigatedFarms, iSuburbFarms), getSASDiagnosticOrDash(szImprovements).GetCString(), getSASDiagnosticOrDash(szRoutes).GetCString());
	if (gGameRecordLogLevel >= 3)
	{
		CvString szTerrains;
		CvString szFeatures;
		CvString szBonuses;
		CvString szImprovedBonuses;
		CvString szUnimprovedBonuses;
		getSASGameRecordLandscapeTypes(kOwned, szTerrains, szFeatures, szBonuses);
		for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
		{
			appendSASGameRecordTypeCount(szImprovedBonuses, getSASGameRecordBonusType((BonusTypes)iI), kDevelopment.aiImprovedBonuses[iI]);
			appendSASGameRecordTypeCount(szUnimprovedBonuses, getSASGameRecordBonusType((BonusTypes)iI), kDevelopment.aiUnimprovedBonuses[iI]);
		}
		logSASGameRecord("GAME_RECORD_TERRITORY_LANDSCAPE turn=%d player=%d terrains=%s features=%s bonuses=%s improvedBonuses=%s unimprovedBonuses=%s", iGameTurn, ePlayer, getSASDiagnosticOrDash(szTerrains).GetCString(), getSASDiagnosticOrDash(szFeatures).GetCString(), getSASDiagnosticOrDash(szBonuses).GetCString(), getSASDiagnosticOrDash(szImprovedBonuses).GetCString(), getSASDiagnosticOrDash(szUnimprovedBonuses).GetCString());
	}
	kPrevious.iTerritoryImprovedLand = kDevelopment.iImprovedLand;
	kPrevious.iTerritoryImprovedWater = kDevelopment.iImprovedWater;
	kPrevious.iTerritoryRoaded = kOwned.iRoaded;
	kPrevious.iTerritoryFarms = kDevelopment.iFarms;
	kPrevious.iTerritoryIrrigatedFarms = kDevelopment.iIrrigatedFarms;
	kPrevious.iTerritoryDryFarms = kDevelopment.iDryFarms;
}

static void getSASGameRecordPlotCompositionTypes(SASGameRecordPlotComposition const& kComposition, CvString& szTerrains, CvString& szFeatures, CvString& szBonuses, CvString& szImprovements, CvString& szRoutes)
{
	getSASGameRecordLandscapeTypes(kComposition, szTerrains, szFeatures, szBonuses);
	getSASGameRecordImprovementRouteTypes(kComposition, szImprovements, szRoutes);
}

static void logSASGameRecordKnownArea(PlayerTypes ePlayer, const char* szReason)
{
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	TeamTypes eTeam = kPlayer.getTeam();
	if (eTeam == NO_TEAM)
		return;
	SASGameRecordPlotComposition kRevealed;
	SASGameRecordPlotComposition kVisible;
	CvMap const& kMap = GC.getMap();
	for (int iI = 0; iI < kMap.numPlots(); iI++)
	{
		CvPlot const& kPlot = kMap.getPlotByIndex(iI);
		if (kPlot.isRevealed(eTeam, false))
			addSASGameRecordPlotComposition(kRevealed, kPlot, eTeam);
		if (kPlot.isVisible(eTeam, false))
			addSASGameRecordPlotComposition(kVisible, kPlot, eTeam);
	}
	logSASGameRecord("GAME_RECORD_KNOWN_AREA turn=%d reason=%s player=%d team=%d revealedPlots=%d visiblePlots=%d revealedLand=%d visibleLand=%d revealedWater=%d visibleWater=%d revealedHills=%d visibleHills=%d revealedPeaks=%d visiblePeaks=%d revealedRiverSide=%d visibleRiverSide=%d revealedFreshWater=%d visibleFreshWater=%d revealedCoastal=%d visibleCoastal=%d revealedImproved=%d visibleImproved=%d revealedUnimprovedLand=%d visibleUnimprovedLand=%d revealedRoaded=%d visibleRoaded=%d revealedBonusImproved=%d visibleBonusImproved=%d revealedBonusUnimproved=%d visibleBonusUnimproved=%d revealedNatureFood=%d visibleNatureFood=%d revealedNatureProd=%d visibleNatureProd=%d revealedNatureCommerce=%d visibleNatureCommerce=%d revealedCurrentFood=%d visibleCurrentFood=%d revealedCurrentProd=%d visibleCurrentProd=%d revealedCurrentCommerce=%d visibleCurrentCommerce=%d",
			GC.getGame().getGameTurn(), szReason, ePlayer, eTeam,
			kRevealed.iPlots, kVisible.iPlots, kRevealed.iLand, kVisible.iLand, kRevealed.iWater, kVisible.iWater,
			kRevealed.iHills, kVisible.iHills, kRevealed.iPeaks, kVisible.iPeaks, kRevealed.iRiverSide, kVisible.iRiverSide,
			kRevealed.iFreshWater, kVisible.iFreshWater, kRevealed.iCoastal, kVisible.iCoastal, kRevealed.iImproved, kVisible.iImproved,
			kRevealed.iUnimprovedLand, kVisible.iUnimprovedLand, kRevealed.iRoaded, kVisible.iRoaded, kRevealed.iBonusImproved, kVisible.iBonusImproved,
			kRevealed.iBonusUnimproved, kVisible.iBonusUnimproved, kRevealed.iNatureFood, kVisible.iNatureFood, kRevealed.iNatureProduction, kVisible.iNatureProduction,
			kRevealed.iNatureCommerce, kVisible.iNatureCommerce, kRevealed.iCurrentFood, kVisible.iCurrentFood,
			kRevealed.iCurrentProduction, kVisible.iCurrentProduction, kRevealed.iCurrentCommerce, kVisible.iCurrentCommerce);
	CvString szRevealedTerrains;
	CvString szVisibleTerrains;
	CvString szRevealedFeatures;
	CvString szVisibleFeatures;
	CvString szRevealedBonuses;
	CvString szVisibleBonuses;
	CvString szRevealedImprovements;
	CvString szVisibleImprovements;
	CvString szRevealedRoutes;
	CvString szVisibleRoutes;
	getSASGameRecordPlotCompositionTypes(kRevealed, szRevealedTerrains, szRevealedFeatures, szRevealedBonuses, szRevealedImprovements, szRevealedRoutes);
	getSASGameRecordPlotCompositionTypes(kVisible, szVisibleTerrains, szVisibleFeatures, szVisibleBonuses, szVisibleImprovements, szVisibleRoutes);
	logSASGameRecord("GAME_RECORD_KNOWN_AREA_TYPES turn=%d reason=%s player=%d team=%d revealedTerrains=%s visibleTerrains=%s revealedFeatures=%s visibleFeatures=%s revealedBonuses=%s visibleBonuses=%s revealedImprovements=%s visibleImprovements=%s revealedRoutes=%s visibleRoutes=%s",
			GC.getGame().getGameTurn(), szReason, ePlayer, eTeam,
			getSASDiagnosticOrDash(szRevealedTerrains).GetCString(), getSASDiagnosticOrDash(szVisibleTerrains).GetCString(),
			getSASDiagnosticOrDash(szRevealedFeatures).GetCString(), getSASDiagnosticOrDash(szVisibleFeatures).GetCString(),
			getSASDiagnosticOrDash(szRevealedBonuses).GetCString(), getSASDiagnosticOrDash(szVisibleBonuses).GetCString(),
			getSASDiagnosticOrDash(szRevealedImprovements).GetCString(), getSASDiagnosticOrDash(szVisibleImprovements).GetCString(),
			getSASDiagnosticOrDash(szRevealedRoutes).GetCString(), getSASDiagnosticOrDash(szVisibleRoutes).GetCString());
}

static void logSASGameRecordStartingUnits(PlayerTypes ePlayer, const char* szReason)
{
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	if (GC.getGame().getElapsedGameTurns() > 1)
		return;
	int iLoop = 0;
	for (CvUnit const* pLoopUnit = kPlayer.firstUnit(&iLoop); pLoopUnit != NULL; pLoopUnit = kPlayer.nextUnit(&iLoop))
	{
		CvPlot const& kPlot = pLoopUnit->getPlot();
		logSASGameRecord("GAME_RECORD_STARTING_UNIT turn=%d reason=%s player=%d unitId=%d unit=%s unitAI=%s unitCombat=%s x=%d y=%d damage=%d xp=%d level=%d movesLeft=%d plotOwner=%d plotTerrain=%s plotFeature=%s plotBonus=%s plotImprovement=%s plotRoute=%s",
				GC.getGame().getGameTurn(), szReason, ePlayer, pLoopUnit->getID(), getSASGameRecordUnitType(pLoopUnit->getUnitType()), getSASGameRecordUnitAIType(pLoopUnit->AI_getUnitAIType()), getSASGameRecordUnitCombatType(pLoopUnit->getUnitInfo().getUnitCombatType()), pLoopUnit->getX(), pLoopUnit->getY(), pLoopUnit->getDamage(), pLoopUnit->getExperience(), pLoopUnit->getLevel(), pLoopUnit->movesLeft(), kPlot.getOwner(), getSASGameRecordTerrainType(kPlot.getTerrainType()), getSASGameRecordFeatureType(kPlot.getFeatureType()), getSASGameRecordBonusType(kPlot.getBonusType(pLoopUnit->getTeam())), getSASGameRecordImprovementType(kPlot.getImprovementType()), getSASGameRecordRouteType(kPlot.getRouteType()));
	}
}

static void logSASGameRecordCityBFC(CvCity const& kCity, const char* szReason)
{
	CvString szTerrains;
	CvString szFeatures;
	CvString szBonuses;
	CvString szImprovements;
	CvString szRoutes;
	SASGameRecordPlotComposition kComposition;
	int iOwned = 0;
	TeamTypes eTeam = GET_PLAYER(kCity.getOwner()).getTeam();
	for (CityPlotIter it(kCity); it.hasNext(); ++it)
	{
		CvPlot const& kPlot = *it;
		if (kPlot.getOwner() == kCity.getOwner())
			iOwned++;
		addSASGameRecordPlotComposition(kComposition, kPlot, eTeam);
	}
	getSASGameRecordPlotCompositionTypes(kComposition, szTerrains, szFeatures, szBonuses, szImprovements, szRoutes);
	logSASGameRecord("GAME_RECORD_CITY_BFC turn=%d reason=%s player=%d cityId=%d city=%S x=%d y=%d plots=%d owned=%d land=%d water=%d hills=%d peaks=%d riverSide=%d freshWater=%d coastal=%d improved=%d unimprovedLand=%d roaded=%d bonusImproved=%d bonusUnimproved=%d worked=%d workedImproved=%d workedUnimproved=%d natureFood=%d natureProd=%d natureCommerce=%d currentFood=%d currentProd=%d currentCommerce=%d terrains=%s features=%s bonuses=%s improvements=%s routes=%s",
			GC.getGame().getGameTurn(), szReason, kCity.getOwner(), kCity.getID(), getSASGameRecordQuotedCityName(&kCity).GetCString(), kCity.getX(), kCity.getY(),
			kComposition.iPlots, iOwned, kComposition.iLand, kComposition.iWater, kComposition.iHills, kComposition.iPeaks, kComposition.iRiverSide,
			kComposition.iFreshWater, kComposition.iCoastal, kComposition.iImproved, kComposition.iUnimprovedLand, kComposition.iRoaded, kComposition.iBonusImproved, kComposition.iBonusUnimproved,
			kComposition.iWorked, kComposition.iWorkedImproved, kComposition.iWorkedUnimproved, kComposition.iNatureFood, kComposition.iNatureProduction, kComposition.iNatureCommerce, kComposition.iCurrentFood,
			kComposition.iCurrentProduction, kComposition.iCurrentCommerce, getSASDiagnosticOrDash(szTerrains).GetCString(), getSASDiagnosticOrDash(szFeatures).GetCString(), getSASDiagnosticOrDash(szBonuses).GetCString(), getSASDiagnosticOrDash(szImprovements).GetCString(), getSASDiagnosticOrDash(szRoutes).GetCString());
}

static SASGameRecordPlotComposition getSASGameRecordWorkedPlotComposition(CvCity const& kCity)
{
	SASGameRecordPlotComposition kComposition;
	const TeamTypes eTeam = GET_PLAYER(kCity.getOwner()).getTeam();
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
	for (int iI = 0; iI < GC.getNumTerrainInfos(); iI++)
		kTarget.aiTerrains[iI] += kSource.aiTerrains[iI];
	for (int iI = 0; iI < GC.getNumFeatureInfos(); iI++)
		kTarget.aiFeatures[iI] += kSource.aiFeatures[iI];
	for (int iI = 0; iI < GC.getNumBonusInfos(); iI++)
		kTarget.aiBonuses[iI] += kSource.aiBonuses[iI];
	for (int iI = 0; iI < GC.getNumImprovementInfos(); iI++)
		kTarget.aiImprovements[iI] += kSource.aiImprovements[iI];
	for (int iI = 0; iI < GC.getNumRouteInfos(); iI++)
		kTarget.aiRoutes[iI] += kSource.aiRoutes[iI];
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

struct SASGameRecordLandmassGeography
{
	SASGameRecordLandmassGeography() : iAreaId(-1), iAnchorX(-1), iAnchorY(-1), iPlots(0), iHabitablePlots(0), iImpassablePlots(0), iZeroNatureYieldPlots(0), iWaterBorderPlots(0), iSeaBorderPlots(0), iLakeBorderPlots(0), iIceBorderPlots(0), iSeaBorderEdges(0), iLakeBorderEdges(0), iIceBorderEdges(0), iHillsPlots(0), iPeakPlots(0), iRiverSidePlots(0), iFreshWaterPlots(0), iSumWrappedDX(0), iSumWrappedDY(0), iStartingPlayers(0), iNatureFood(0), iNatureProduction(0), iNatureCommerce(0), iBonusCount(0) {}
	int iAreaId;
	int iAnchorX;
	int iAnchorY;
	int iPlots;
	int iHabitablePlots;
	int iImpassablePlots;
	int iZeroNatureYieldPlots;
	int iWaterBorderPlots;
	int iSeaBorderPlots;
	int iLakeBorderPlots;
	int iIceBorderPlots;
	int iSeaBorderEdges;
	int iLakeBorderEdges;
	int iIceBorderEdges;
	int iHillsPlots;
	int iPeakPlots;
	int iRiverSidePlots;
	int iFreshWaterPlots;
	int iSumWrappedDX;
	int iSumWrappedDY;
	int iStartingPlayers;
	int iNatureFood;
	int iNatureProduction;
	int iNatureCommerce;
	int iBonusCount;
	CvString szStartingPlayers;
	std::vector<int> aiNoOceanConnectedAreas;
	std::vector<int> aiNoOceanNavigableConnectedAreas;
	std::vector<int> aiAdjacentLakeAreas;
	std::vector<int> aiTerrainCounts;
	std::vector<int> aiFeatureCounts;
	std::vector<int> aiBonusCounts;
	std::vector<CvString> aszBonusCoordinates;
	std::vector<int> aiEraRevealedRawFood;
	std::vector<int> aiEraRevealedRawProduction;
	std::vector<int> aiEraRevealedRawCommerce;
	std::vector<int> aiEraRevealedRaw321;
	std::vector<int> aiEraBonusPotentialFood;
	std::vector<int> aiEraBonusPotentialProduction;
	std::vector<int> aiEraBonusPotentialCommerce;
	std::vector<int> aiEraBonusPotential321;
};

static SASGameRecordLandmassGeography* getSASGameRecordLandmassGeography(std::vector<SASGameRecordLandmassGeography>& aLandmasses, int iAreaId)
{
	for (size_t iI = 0; iI < aLandmasses.size(); iI++)
	{
		if (aLandmasses[iI].iAreaId == iAreaId)
			return &aLandmasses[iI];
	}
	return NULL;
}

static void addSASGameRecordUniqueArea(std::vector<int>& aiAreas, int iAreaId)
{
	if (iAreaId < 0)
		return;
	if (std::find(aiAreas.begin(), aiAreas.end(), iAreaId) == aiAreas.end())
		aiAreas.push_back(iAreaId);
}

static CvString getSASGameRecordLandmassName(SASGameRecordLandmassGeography const& kLandmass)
{
	CvString szName;
	szName.Format("LAND_%d_%d_A%d", kLandmass.iAnchorX, kLandmass.iAnchorY, kLandmass.iAreaId);
	return szName;
}

static CvString getSASGameRecordLandmassList(std::vector<SASGameRecordLandmassGeography> const& aLandmasses, std::vector<int> const& aiAreas)
{
	CvString szList;
	for (size_t iI = 0; iI < aiAreas.size(); iI++)
	{
		for (size_t iJ = 0; iJ < aLandmasses.size(); iJ++)
		{
			if (aLandmasses[iJ].iAreaId != aiAreas[iI])
				continue;
			CvString szItem;
			CvString const szName = getSASGameRecordLandmassName(aLandmasses[iJ]);
			szItem.Format(szList.empty() ? "%s" : ",%s", szName.GetCString());
			szList += szItem;
			break;
		}
	}
	return getSASDiagnosticOrDash(szList);
}

static CvString getSASGameRecordLandmassTerrainPercentages(SASGameRecordLandmassGeography const& kLandmass)
{
	CvString szList;
	for (int iTerrain = 0; iTerrain < (int)kLandmass.aiTerrainCounts.size(); iTerrain++)
	{
		if (kLandmass.aiTerrainCounts[iTerrain] <= 0)
			continue;
		CvString szItem;
		szItem.Format(szList.empty() ? "%s:%d" : ",%s:%d", GC.getInfo((TerrainTypes)iTerrain).getType(), getSASGameRecordPercentX100(kLandmass.aiTerrainCounts[iTerrain], kLandmass.iPlots));
		szList += szItem;
	}
	return getSASDiagnosticOrDash(szList);
}

static CvString getSASGameRecordLandmassFeaturePercentages(SASGameRecordLandmassGeography const& kLandmass)
{
	CvString szList;
	for (int iFeature = 0; iFeature < (int)kLandmass.aiFeatureCounts.size(); iFeature++)
	{
		if (kLandmass.aiFeatureCounts[iFeature] <= 0)
			continue;
		CvString szItem;
		szItem.Format(szList.empty() ? "%s:%d" : ",%s:%d", GC.getInfo((FeatureTypes)iFeature).getType(), getSASGameRecordPercentX100(kLandmass.aiFeatureCounts[iFeature], kLandmass.iPlots));
		szList += szItem;
	}
	return getSASDiagnosticOrDash(szList);
}

static int getSASGameRecordAdjacentLakePlots(SASGameRecordLandmassGeography const& kLandmass)
{
	int iPlots = 0;
	for (size_t iI = 0; iI < kLandmass.aiAdjacentLakeAreas.size(); iI++)
	{
		CvArea const* pArea = GC.getMap().getArea(kLandmass.aiAdjacentLakeAreas[iI]);
		if (pArea != NULL && pArea->isLake())
			iPlots += pArea->getNumTiles();
	}
	return iPlots;
}

static bool isSASGameRecordTechAvailableByEra(TechTypes eTech, EraTypes eEra)
{
	return (eTech == NO_TECH || GC.getInfo(eTech).getEra() <= eEra);
}

static bool isSASGameRecordBuildAvailableByEra(CvPlot const& kPlot, BuildTypes eBuild, EraTypes eEra)
{
	CvBuildInfo const& kBuild = GC.getInfo(eBuild);
	if (kBuild.getImprovement() == NO_IMPROVEMENT || !isSASGameRecordTechAvailableByEra(kBuild.getTechPrereq(), eEra))
		return false;
	FeatureTypes const eFeature = kPlot.getFeatureType();
	if (eFeature != NO_FEATURE && kBuild.isFeatureRemove(eFeature) && !isSASGameRecordTechAvailableByEra(kBuild.getFeatureTech(eFeature), eEra))
		return false;
	return kPlot.canHaveImprovement(kBuild.getImprovement(), NO_TEAM, true, eBuild, false);
}

static bool isSASGameRecordIrrigationAvailableByEra(EraTypes eEra)
{
	static std::vector<char> abAvailable;
	if (abAvailable.empty())
	{
		abAvailable.assign(GC.getNumEraInfos(), 0);
		FOR_EACH_ENUM(Tech)
		{
			if (!GC.getInfo(eLoopTech).isIrrigation())
				continue;
			for (int iEra = GC.getInfo(eLoopTech).getEra(); iEra < GC.getNumEraInfos(); iEra++)
				abAvailable[iEra] = 1;
		}
	}
	return (eEra >= 0 && eEra < (int)abAvailable.size() && abAvailable[eEra] != 0);
}

static int getSASGameRecordImprovementTechYieldByEra(ImprovementTypes eImprovement, YieldTypes eYield, EraTypes eEra)
{
	static std::vector<int> aiYield;
	int const iNumEras = GC.getNumEraInfos();
	int const iNumImprovements = GC.getNumImprovementInfos();
	if (aiYield.empty())
	{
		aiYield.assign(iNumEras * iNumImprovements * NUM_YIELD_TYPES, 0);
		for (int iEra = 0; iEra < iNumEras; iEra++)
		{
			for (int iImprovement = 0; iImprovement < iNumImprovements; iImprovement++)
			{
				FOR_EACH_ENUM(Yield)
				{
					int iValue = 0;
					FOR_EACH_ENUM(Tech)
					{
						if (GC.getInfo(eLoopTech).getEra() <= iEra)
							iValue += GC.getInfo((ImprovementTypes)iImprovement).getTechYieldChanges(eLoopTech, eLoopYield);
					}
					aiYield[(iEra * iNumImprovements + iImprovement) * NUM_YIELD_TYPES + eLoopYield] = iValue;
				}
			}
		}
	}
	if (eEra < 0 || eEra >= iNumEras || eImprovement < 0 || eImprovement >= iNumImprovements || eYield < 0 || eYield >= NUM_YIELD_TYPES)
		return 0;
	return aiYield[(eEra * iNumImprovements + eImprovement) * NUM_YIELD_TYPES + eYield];
}

static void getSASGameRecordLandmassPlotPotential(CvPlot const& kPlot, EraTypes eEra, int& iRawFood, int& iRawProduction, int& iRawCommerce, int& iRaw321, int& iPotentialFood, int& iPotentialProduction, int& iPotentialCommerce, int& iPotential321)
{
	BonusTypes const eBonus = kPlot.getBonusType(NO_TEAM);
	bool const bBonusRevealed = (eBonus != NO_BONUS && isSASGameRecordTechAvailableByEra(GC.getInfo(eBonus).getTechReveal(), eEra));
	int aiRaw[NUM_YIELD_TYPES] = { 0 };
	FOR_EACH_ENUM(Yield)
	{
		aiRaw[eLoopYield] = kPlot.calculateNatureYield(eLoopYield, NO_TEAM);
		if (bBonusRevealed)
			aiRaw[eLoopYield] += GC.getInfo(eBonus).getYieldChange(eLoopYield);
	}
	iRawFood = aiRaw[YIELD_FOOD];
	iRawProduction = aiRaw[YIELD_PRODUCTION];
	iRawCommerce = aiRaw[YIELD_COMMERCE];
	iRaw321 = 3 * iRawFood + 2 * iRawProduction + iRawCommerce;
	iPotentialFood = iRawFood;
	iPotentialProduction = iRawProduction;
	iPotentialCommerce = iRawCommerce;
	iPotential321 = iRaw321;
	if (!bBonusRevealed || !isSASGameRecordTechAvailableByEra(GC.getInfo(eBonus).getTechCityTrade(), eEra))
		return;
	bool const bIrrigationPotential = (isSASGameRecordIrrigationAvailableByEra(eEra) && kPlot.canHavePotentialIrrigation());
	FOR_EACH_ENUM(Build)
	{
		if (!isSASGameRecordBuildAvailableByEra(kPlot, eLoopBuild, eEra))
			continue;
		CvBuildInfo const& kBuild = GC.getInfo(eLoopBuild);
		ImprovementTypes const eImprovement = kBuild.getImprovement();
		CvImprovementInfo const& kImprovement = GC.getInfo(eImprovement);
		// <!-- custom: Ordinary Farm/Cottage/Mine/etc. choices encode city specialization rather than intrinsic geography. Only project an improvement when XML explicitly says that it connects this bonus, making the resource improvement a comparatively unambiguous part of the landmass's potential. (ChatGPT-5.6-Sol) -->
		if (!kImprovement.isImprovementBonusMakesValid(eBonus))
			continue;
		int aiYield[NUM_YIELD_TYPES] = { 0 };
		FOR_EACH_ENUM(Yield)
		{
			int iYield = kPlot.calculateNatureYield(eLoopYield, NO_TEAM, kPlot.getFeatureType() != NO_FEATURE && kBuild.isFeatureRemove(kPlot.getFeatureType()));
			iYield += GC.getInfo(eBonus).getYieldChange(eLoopYield);
			iYield += kImprovement.getYieldChange(eLoopYield);
			if (kPlot.isRiverSide())
				iYield += kImprovement.getRiverSideYieldChange(eLoopYield);
			if (kPlot.isHills())
				iYield += kImprovement.getHillsYieldChange(eLoopYield);
			if (bIrrigationPotential)
				iYield += kImprovement.getIrrigatedYieldChange(eLoopYield);
			iYield += getSASGameRecordImprovementTechYieldByEra(eImprovement, eLoopYield, eEra);
			iYield += kImprovement.getImprovementBonusYield(eBonus, eLoopYield);
			aiYield[eLoopYield] = std::max(0, iYield);
		}
		iPotentialFood = std::max(iPotentialFood, aiYield[YIELD_FOOD]);
		iPotentialProduction = std::max(iPotentialProduction, aiYield[YIELD_PRODUCTION]);
		iPotentialCommerce = std::max(iPotentialCommerce, aiYield[YIELD_COMMERCE]);
		iPotential321 = std::max(iPotential321, 3 * aiYield[YIELD_FOOD] + 2 * aiYield[YIELD_PRODUCTION] + aiYield[YIELD_COMMERCE]);
	}
}

static CvString getSASGameRecordEraLandmassYieldList(SASGameRecordLandmassGeography const& kLandmass, bool bBonusPotential)
{
	CvString szList;
	for (int iEra = 0; iEra < GC.getNumEraInfos(); iEra++)
	{
		if (kLandmass.iPlots <= 0)
			continue;
		std::vector<int> const& aiFood = (bBonusPotential ? kLandmass.aiEraBonusPotentialFood : kLandmass.aiEraRevealedRawFood);
		std::vector<int> const& aiProduction = (bBonusPotential ? kLandmass.aiEraBonusPotentialProduction : kLandmass.aiEraRevealedRawProduction);
		std::vector<int> const& aiCommerce = (bBonusPotential ? kLandmass.aiEraBonusPotentialCommerce : kLandmass.aiEraRevealedRawCommerce);
		std::vector<int> const& ai321 = (bBonusPotential ? kLandmass.aiEraBonusPotential321 : kLandmass.aiEraRevealedRaw321);
		CvString szItem;
		szItem.Format(szList.empty() ? "%s:%d/%d/%d/%d" : ",%s:%d/%d/%d/%d", GC.getInfo((EraTypes)iEra).getType(), (100 * aiFood[iEra]) / kLandmass.iPlots, (100 * aiProduction[iEra]) / kLandmass.iPlots, (100 * aiCommerce[iEra]) / kLandmass.iPlots, (100 * ai321[iEra]) / kLandmass.iPlots);
		szList += szItem;
	}
	return getSASDiagnosticOrDash(szList);
}

static CvString getSASGameRecordLandmassBonusTypes(SASGameRecordLandmassGeography const& kLandmass)
{
	CvString szTypes;
	for (int iBonus = 0; iBonus < (int)kLandmass.aiBonusCounts.size(); iBonus++)
	{
		if (kLandmass.aiBonusCounts[iBonus] <= 0)
			continue;
		appendSASGameRecordTypeCount(szTypes, getSASGameRecordBonusType((BonusTypes)iBonus), kLandmass.aiBonusCounts[iBonus]);
	}
	return getSASDiagnosticOrDash(szTypes);
}

static void logSASGameRecordLandmassBonusCoordinates(SASGameRecordLandmassGeography const& kLandmass, CvString const& szLandmassName)
{
	std::vector<CvString> aszParts;
	CvString szPart;
	for (size_t iI = 0; iI < kLandmass.aszBonusCoordinates.size(); iI++)
	{
		CvString const& szItem = kLandmass.aszBonusCoordinates[iI];
		if (!szPart.empty() && szPart.length() + szItem.length() + 1 > 1200)
		{
			aszParts.push_back(szPart);
			szPart = "";
		}
		if (!szPart.empty())
			szPart += ";";
		szPart += szItem;
	}
	if (!szPart.empty())
		aszParts.push_back(szPart);
	if (aszParts.empty())
	{
		logSASGameRecord("GAME_RECORD_LANDMASS_BONUS_COORDS turn=%d landmass=%s part=1 parts=1 bonuses=-", GC.getGame().getGameTurn(), szLandmassName.GetCString());
		return;
	}
	for (size_t iI = 0; iI < aszParts.size(); iI++)
		logSASGameRecord("GAME_RECORD_LANDMASS_BONUS_COORDS turn=%d landmass=%s part=%d parts=%d bonuses=%s", GC.getGame().getGameTurn(), szLandmassName.GetCString(), (int)iI + 1, (int)aszParts.size(), aszParts[iI].GetCString());
}

// <!-- custom: Record a map-geography snapshot once per new/load context so autoplay analysis can distinguish crowded continents, isolated islands, coast-connected landmasses, terrain/feature composition, lake/sea structure, map-resource distribution, and underlying land quality without reconstructing the map from later city history.
// Landmass names use a deterministic anchor coordinate plus the Civ4 area ID; center coordinates are wrap-aware approximations. "habitable" uses CvPlot::isHabitable, while impassable and zero-natural-yield plots remain separate.
// Nature yields exclude bonuses. Per-era raw value adds a bonus only from its reveal era; conservative potential improves only revealed/connectable bonus plots through an XML-valid resource improvement, leaving ordinary non-bonus specialization choices untouched.
// Routes, civics and improvement maturation remain excluded because they are not intrinsic geography. (ChatGPT-5.6-Sol); or because they are available everywhere, so counting them or not has no extra strategic information value so do not count them. -->
static void addSASGameRecordNoOceanConnections(CvMap const& kMap, TerrainTypes eOcean, bool bSkipImpassable, std::vector<SASGameRecordLandmassGeography>& aLandmasses)
{
	std::vector<char> abVisited(kMap.numPlots(), 0);
	for (int iI = 0; iI < kMap.numPlots(); iI++)
	{
		CvPlot const& kStart = kMap.getPlotByIndex(iI);
		if (abVisited[iI] || !kStart.isWater() || kStart.getTerrainType() == eOcean || (bSkipImpassable && kStart.isImpassable()))
			continue;
		std::vector<int> aiQueue;
		std::vector<int> aiBorderLandAreas;
		abVisited[iI] = 1;
		aiQueue.push_back(iI);
		for (size_t iQ = 0; iQ < aiQueue.size(); iQ++)
		{
			CvPlot const& kWater = kMap.getPlotByIndex(aiQueue[iQ]);
			FOR_EACH_ADJ_PLOT(kWater)
			{
				if (!pAdj->isWater())
				{
					addSASGameRecordUniqueArea(aiBorderLandAreas, pAdj->getArea().getID());
					continue;
				}
				if (pAdj->getTerrainType() == eOcean || (bSkipImpassable && pAdj->isImpassable()))
					continue;
				int const iAdjIndex = kMap.plotNum(pAdj->getX(), pAdj->getY());
				if (iAdjIndex < 0 || abVisited[iAdjIndex])
					continue;
				abVisited[iAdjIndex] = 1;
				aiQueue.push_back(iAdjIndex);
			}
		}
		for (size_t iA = 0; iA < aiBorderLandAreas.size(); iA++)
		{
			SASGameRecordLandmassGeography* pLandmass = getSASGameRecordLandmassGeography(aLandmasses, aiBorderLandAreas[iA]);
			if (pLandmass == NULL)
				continue;
			for (size_t iB = 0; iB < aiBorderLandAreas.size(); iB++)
			{
				if (iA == iB)
					continue;
				if (bSkipImpassable)
					addSASGameRecordUniqueArea(pLandmass->aiNoOceanNavigableConnectedAreas, aiBorderLandAreas[iB]);
				else addSASGameRecordUniqueArea(pLandmass->aiNoOceanConnectedAreas, aiBorderLandAreas[iB]);
			}
		}
	}
}

static void logSASGameRecordGeography()
{
	CvMap const& kMap = GC.getMap();
	std::vector<SASGameRecordLandmassGeography> aLandmasses;
	int iLandPlots = 0;
	int iWaterPlots = 0;
	int iSeaPlots = 0;
	int iLakePlots = 0;
	int iCoastSeaPlots = 0;
	int iOceanSeaPlots = 0;
	int iOtherSeaPlots = 0;
	int iIcePlots = 0;
	int iIceSeaPlots = 0;
	int iIceLakePlots = 0;
	int iIceCoastSeaPlots = 0;
	int iIceOceanSeaPlots = 0;
	int iWaterAreas = 0;
	int iSeaAreas = 0;
	int iLakeAreas = 0;
	TerrainTypes const eCoast = (TerrainTypes)GC.getInfoTypeForString("TERRAIN_COAST");
	TerrainTypes const eOcean = (TerrainTypes)GC.getInfoTypeForString("TERRAIN_OCEAN");
	FeatureTypes const eIce = (FeatureTypes)GC.getInfoTypeForString("FEATURE_ICE");
	int iLoop = 0;
	for (CvArea const* pLoopArea = kMap.firstArea(&iLoop); pLoopArea != NULL; pLoopArea = kMap.nextArea(&iLoop))
	{
		if (pLoopArea->isWater())
		{
			iWaterAreas++;
			if (pLoopArea->isLake())
				iLakeAreas++;
			else iSeaAreas++;
			continue;
		}
		SASGameRecordLandmassGeography kLandmass;
		kLandmass.iAreaId = pLoopArea->getID();
		kLandmass.aiTerrainCounts.assign(GC.getNumTerrainInfos(), 0);
		kLandmass.aiFeatureCounts.assign(GC.getNumFeatureInfos(), 0);
		kLandmass.aiBonusCounts.assign(GC.getNumBonusInfos(), 0);
		kLandmass.aiEraRevealedRawFood.assign(GC.getNumEraInfos(), 0);
		kLandmass.aiEraRevealedRawProduction.assign(GC.getNumEraInfos(), 0);
		kLandmass.aiEraRevealedRawCommerce.assign(GC.getNumEraInfos(), 0);
		kLandmass.aiEraRevealedRaw321.assign(GC.getNumEraInfos(), 0);
		kLandmass.aiEraBonusPotentialFood.assign(GC.getNumEraInfos(), 0);
		kLandmass.aiEraBonusPotentialProduction.assign(GC.getNumEraInfos(), 0);
		kLandmass.aiEraBonusPotentialCommerce.assign(GC.getNumEraInfos(), 0);
		kLandmass.aiEraBonusPotential321.assign(GC.getNumEraInfos(), 0);
		aLandmasses.push_back(kLandmass);
	}
	for (int iI = 0; iI < kMap.numPlots(); iI++)
	{
		CvPlot const& kPlot = kMap.getPlotByIndex(iI);
		if (kPlot.isWater())
		{
			iWaterPlots++;
			bool const bIce = (eIce != NO_FEATURE && kPlot.getFeatureType() == eIce);
			if (bIce)
				iIcePlots++;
			if (kPlot.isLake())
			{
				iLakePlots++;
				if (bIce)
					iIceLakePlots++;
				FOR_EACH_ADJ_PLOT(kPlot)
				{
					if (pAdj->isWater())
						continue;
					SASGameRecordLandmassGeography* pAdjacentLandmass = getSASGameRecordLandmassGeography(aLandmasses, pAdj->getArea().getID());
					if (pAdjacentLandmass != NULL)
						addSASGameRecordUniqueArea(pAdjacentLandmass->aiAdjacentLakeAreas, kPlot.getArea().getID());
				}
			}
			else
			{
				iSeaPlots++;
				if (bIce)
					iIceSeaPlots++;
				if (kPlot.getTerrainType() == eCoast)
				{
					iCoastSeaPlots++;
					if (bIce)
						iIceCoastSeaPlots++;
				}
				else if (kPlot.getTerrainType() == eOcean)
				{
					iOceanSeaPlots++;
					if (bIce)
						iIceOceanSeaPlots++;
				}
				else iOtherSeaPlots++;
			}
			continue;
		}
		iLandPlots++;
		SASGameRecordLandmassGeography* pLandmass = getSASGameRecordLandmassGeography(aLandmasses, kPlot.getArea().getID());
		if (pLandmass == NULL)
			continue;
		if (pLandmass->iPlots == 0)
		{
			pLandmass->iAnchorX = kPlot.getX();
			pLandmass->iAnchorY = kPlot.getY();
		}
		pLandmass->iPlots++;
		if (kPlot.getTerrainType() != NO_TERRAIN)
			pLandmass->aiTerrainCounts[kPlot.getTerrainType()]++;
		if (kPlot.getFeatureType() != NO_FEATURE)
			pLandmass->aiFeatureCounts[kPlot.getFeatureType()]++;
		if (kPlot.isHills())
			pLandmass->iHillsPlots++;
		if (kPlot.isPeak())
			pLandmass->iPeakPlots++;
		if (kPlot.isRiverSide())
			pLandmass->iRiverSidePlots++;
		if (kPlot.isFreshWater())
			pLandmass->iFreshWaterPlots++;
		pLandmass->iSumWrappedDX += kMap.dxWrap(kPlot.getX() - pLandmass->iAnchorX);
		pLandmass->iSumWrappedDY += kMap.dyWrap(kPlot.getY() - pLandmass->iAnchorY);
		if (kPlot.isHabitable())
			pLandmass->iHabitablePlots++;
		if (kPlot.isImpassable())
			pLandmass->iImpassablePlots++;
		int const iNatureFood = kPlot.calculateNatureYield(YIELD_FOOD, NO_TEAM);
		int const iNatureProduction = kPlot.calculateNatureYield(YIELD_PRODUCTION, NO_TEAM);
		int const iNatureCommerce = kPlot.calculateNatureYield(YIELD_COMMERCE, NO_TEAM);
		if (iNatureFood + iNatureProduction + iNatureCommerce <= 0)
			pLandmass->iZeroNatureYieldPlots++;
		pLandmass->iNatureFood += iNatureFood;
		pLandmass->iNatureProduction += iNatureProduction;
		pLandmass->iNatureCommerce += iNatureCommerce;
		BonusTypes const eBonus = kPlot.getBonusType(NO_TEAM);
		if (eBonus != NO_BONUS)
		{
			pLandmass->iBonusCount++;
			if (eBonus >= 0 && eBonus < (int)pLandmass->aiBonusCounts.size())
				pLandmass->aiBonusCounts[eBonus]++;
			CvString szBonus;
			szBonus.Format("%s@%d,%d", getSASGameRecordBonusType(eBonus), kPlot.getX(), kPlot.getY());
			pLandmass->aszBonusCoordinates.push_back(szBonus);
		}
		for (int iEra = 0; iEra < GC.getNumEraInfos(); iEra++)
		{
			int iRawFood = 0;
			int iRawProduction = 0;
			int iRawCommerce = 0;
			int iRaw321 = 0;
			int iPotentialFood = 0;
			int iPotentialProduction = 0;
			int iPotentialCommerce = 0;
			int iPotential321 = 0;
			getSASGameRecordLandmassPlotPotential(kPlot, (EraTypes)iEra, iRawFood, iRawProduction, iRawCommerce, iRaw321, iPotentialFood, iPotentialProduction, iPotentialCommerce, iPotential321);
			pLandmass->aiEraRevealedRawFood[iEra] += iRawFood;
			pLandmass->aiEraRevealedRawProduction[iEra] += iRawProduction;
			pLandmass->aiEraRevealedRawCommerce[iEra] += iRawCommerce;
			pLandmass->aiEraRevealedRaw321[iEra] += iRaw321;
			pLandmass->aiEraBonusPotentialFood[iEra] += iPotentialFood;
			pLandmass->aiEraBonusPotentialProduction[iEra] += iPotentialProduction;
			pLandmass->aiEraBonusPotentialCommerce[iEra] += iPotentialCommerce;
			pLandmass->aiEraBonusPotential321[iEra] += iPotential321;
		}
		bool bBordersWater = false;
		bool bBordersSea = false;
		bool bBordersLake = false;
		bool bBordersIce = false;
		FOR_EACH_ADJ_PLOT(kPlot)
		{
			if (!pAdj->isWater())
				continue;
			bBordersWater = true;
			if (pAdj->isLake())
			{
				bBordersLake = true;
				pLandmass->iLakeBorderEdges++;
			}
			else
			{
				bBordersSea = true;
				pLandmass->iSeaBorderEdges++;
			}
			if (eIce != NO_FEATURE && pAdj->getFeatureType() == eIce)
			{
				bBordersIce = true;
				pLandmass->iIceBorderEdges++;
			}
		}
		if (bBordersWater) pLandmass->iWaterBorderPlots++;
		if (bBordersSea) pLandmass->iSeaBorderPlots++;
		if (bBordersLake) pLandmass->iLakeBorderPlots++;
		if (bBordersIce) pLandmass->iIceBorderPlots++;
	}
	int iStartingPlayers = 0;
	for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		PlayerTypes eLoopPlayer = (PlayerTypes)iI;
		CvPlayer const& kPlayer = GET_PLAYER(eLoopPlayer);
		CvPlot const* pStart = kPlayer.getStartingPlot();
		if (!kPlayer.isEverAlive() || kPlayer.isBarbarian() || pStart == NULL || pStart->isWater())
			continue;
		SASGameRecordLandmassGeography* pLandmass = getSASGameRecordLandmassGeography(aLandmasses, pStart->getArea().getID());
		if (pLandmass == NULL)
			continue;
		iStartingPlayers++;
		pLandmass->iStartingPlayers++;
		CvString szItem;
		szItem.Format(pLandmass->szStartingPlayers.empty() ? "%d@%d,%d" : ";%d@%d,%d", eLoopPlayer, pStart->getX(), pStart->getY());
		pLandmass->szStartingPlayers += szItem;
	}
	if (eOcean != NO_TERRAIN)
	{
		// <!-- custom: Keep terrain-only coastal connectivity for shape/topology, then separately exclude impassable water (currently Ice) to capture what ordinary pre-ocean sea units can actually traverse. (ChatGPT-5.6-Sol) -->
		addSASGameRecordNoOceanConnections(kMap, eOcean, false, aLandmasses);
		addSASGameRecordNoOceanConnections(kMap, eOcean, true, aLandmasses);
	}

	int iLargestAreaId = -1;
	int iLargestAreaPlots = 0;
	for (size_t iI = 0; iI < aLandmasses.size(); iI++)
	{
		if (aLandmasses[iI].iPlots > iLargestAreaPlots)
		{
			iLargestAreaPlots = aLandmasses[iI].iPlots;
			iLargestAreaId = aLandmasses[iI].iAreaId;
		}
	}
	CvString szLargestLandmass = "-";
	SASGameRecordLandmassGeography* pLargestLandmass = getSASGameRecordLandmassGeography(aLandmasses, iLargestAreaId);
	if (pLargestLandmass != NULL)
		szLargestLandmass = getSASGameRecordLandmassName(*pLargestLandmass);
	int const iMapPlots = iLandPlots + iWaterPlots;

	// <!-- custom: Standardize the initial crowding context that otherwise has to be reconstructed from each landmass/start list for every map comparison.
	// Start spacing uses wrap-aware plotDistance and intentionally treats every starting civilization as map occupancy; current foreign/rival context is recorded separately in periodic expansion rows so later team merges cannot rewrite the meaning of these static start metrics. (ChatGPT-5.6-Sol) -->
	int iHabitableLandPlots = 0;
	int iInhabitedLandmasses = 0;
	int iInhabitedLandPlots = 0;
	int iInhabitedHabitablePlots = 0;
	int iUninhabitedLandmasses = 0;
	int iUninhabitedLandPlots = 0;
	int iUninhabitedHabitablePlots = 0;
	int iIsolatedLandmassStarts = 0;
	for (size_t iI = 0; iI < aLandmasses.size(); iI++)
	{
		SASGameRecordLandmassGeography const& kLandmass = aLandmasses[iI];
		iHabitableLandPlots += kLandmass.iHabitablePlots;
		if (kLandmass.iStartingPlayers > 0)
		{
			iInhabitedLandmasses++;
			iInhabitedLandPlots += kLandmass.iPlots;
			iInhabitedHabitablePlots += kLandmass.iHabitablePlots;
			if (kLandmass.iStartingPlayers == 1)
				iIsolatedLandmassStarts++;
		}
		else
		{
			iUninhabitedLandmasses++;
			iUninhabitedLandPlots += kLandmass.iPlots;
			iUninhabitedHabitablePlots += kLandmass.iHabitablePlots;
		}
	}
	int iStartPairs = 0;
	int iStartPairDistance = 0;
	int iSameLandmassPairs = 0;
	int iSameLandmassPairDistance = 0;
	int iNearestStartDistance = -1;
	int iNearestStartDistanceTotal = 0;
	int iNearestStartDistancePlayers = 0;
	int iMaxNearestStartDistance = -1;
	int iNearestSameLandmassStartDistance = -1;
	int iNearestSameLandmassStartDistanceTotal = 0;
	int iNearestSameLandmassStartDistancePlayers = 0;
	int iMaxNearestSameLandmassStartDistance = -1;
	for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		PlayerTypes const ePlayer = (PlayerTypes)iI;
		CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
		CvPlot const* pStart = kPlayer.getStartingPlot();
		if (!kPlayer.isEverAlive() || kPlayer.isBarbarian() || pStart == NULL || pStart->isWater())
			continue;
		int iPlayerNearest = -1;
		int iPlayerNearestSameLandmass = -1;
		for (int iJ = 0; iJ < MAX_CIV_PLAYERS; iJ++)
		{
			if (iJ == iI)
				continue;
			PlayerTypes const eOtherPlayer = (PlayerTypes)iJ;
			CvPlayer const& kOtherPlayer = GET_PLAYER(eOtherPlayer);
			CvPlot const* pOtherStart = kOtherPlayer.getStartingPlot();
			if (!kOtherPlayer.isEverAlive() || kOtherPlayer.isBarbarian() || pOtherStart == NULL || pOtherStart->isWater())
				continue;
			int const iDistance = plotDistance(pStart->getX(), pStart->getY(), pOtherStart->getX(), pOtherStart->getY());
			iPlayerNearest = (iPlayerNearest < 0 ? iDistance : std::min(iPlayerNearest, iDistance));
			bool const bSameLandmass = (pOtherStart->getArea().getID() == pStart->getArea().getID());
			if (bSameLandmass)
				iPlayerNearestSameLandmass = (iPlayerNearestSameLandmass < 0 ? iDistance : std::min(iPlayerNearestSameLandmass, iDistance));
			if (iJ <= iI)
				continue;
			iStartPairs++;
			iStartPairDistance += iDistance;
			if (bSameLandmass)
			{
				iSameLandmassPairs++;
				iSameLandmassPairDistance += iDistance;
			}
		}
		if (iPlayerNearest >= 0)
		{
			iNearestStartDistance = (iNearestStartDistance < 0 ? iPlayerNearest : std::min(iNearestStartDistance, iPlayerNearest));
			iNearestStartDistanceTotal += iPlayerNearest;
			iNearestStartDistancePlayers++;
			iMaxNearestStartDistance = std::max(iMaxNearestStartDistance, iPlayerNearest);
		}
		if (iPlayerNearestSameLandmass >= 0)
		{
			iNearestSameLandmassStartDistance = (iNearestSameLandmassStartDistance < 0 ? iPlayerNearestSameLandmass : std::min(iNearestSameLandmassStartDistance, iPlayerNearestSameLandmass));
			iNearestSameLandmassStartDistanceTotal += iPlayerNearestSameLandmass;
			iNearestSameLandmassStartDistancePlayers++;
			iMaxNearestSameLandmassStartDistance = std::max(iMaxNearestSameLandmassStartDistance, iPlayerNearestSameLandmass);
		}
	}
	logSASGameRecord("GAME_RECORD_START_DENSITY_SUMMARY turn=%d startingPlayers=%d inhabitedLandmasses=%d isolatedLandmassStarts=%d uninhabitedLandmasses=%d landPlotsPerStartX100=%d habitableLandPlots=%d habitableLandPlotsPerStartX100=%d inhabitedLandPlots=%d inhabitedLandPlotsPerStartX100=%d inhabitedHabitablePlots=%d inhabitedHabitablePlotsPerStartX100=%d uninhabitedLandPlots=%d uninhabitedHabitablePlots=%d startPairs=%d avgStartPairDistanceX100=%d sameLandmassPairs=%d avgSameLandmassPairDistanceX100=%d nearestStartDistance=%d avgNearestStartDistanceX100=%d maxNearestStartDistance=%d startsWithSameLandmassNeighbor=%d nearestSameLandmassStartDistance=%d avgNearestSameLandmassStartDistanceX100=%d maxNearestSameLandmassStartDistance=%d",
			GC.getGame().getGameTurn(), iStartingPlayers, iInhabitedLandmasses, iIsolatedLandmassStarts, iUninhabitedLandmasses,
			iStartingPlayers <= 0 ? -1 : (100 * iLandPlots) / iStartingPlayers, iHabitableLandPlots, iStartingPlayers <= 0 ? -1 : (100 * iHabitableLandPlots) / iStartingPlayers,
			iInhabitedLandPlots, iStartingPlayers <= 0 ? -1 : (100 * iInhabitedLandPlots) / iStartingPlayers, iInhabitedHabitablePlots, iStartingPlayers <= 0 ? -1 : (100 * iInhabitedHabitablePlots) / iStartingPlayers,
			iUninhabitedLandPlots, iUninhabitedHabitablePlots, iStartPairs, iStartPairs <= 0 ? -1 : (100 * iStartPairDistance) / iStartPairs, iSameLandmassPairs, iSameLandmassPairs <= 0 ? -1 : (100 * iSameLandmassPairDistance) / iSameLandmassPairs,
			iNearestStartDistance, iNearestStartDistancePlayers <= 0 ? -1 : (100 * iNearestStartDistanceTotal) / iNearestStartDistancePlayers, iMaxNearestStartDistance,
			iNearestSameLandmassStartDistancePlayers, iNearestSameLandmassStartDistance, iNearestSameLandmassStartDistancePlayers <= 0 ? -1 : (100 * iNearestSameLandmassStartDistanceTotal) / iNearestSameLandmassStartDistancePlayers, iMaxNearestSameLandmassStartDistance);
	for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		PlayerTypes const ePlayer = (PlayerTypes)iI;
		CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
		CvPlot const* pStart = kPlayer.getStartingPlot();
		if (!kPlayer.isEverAlive() || kPlayer.isBarbarian() || pStart == NULL || pStart->isWater())
			continue;
		SASGameRecordLandmassGeography const* pLandmass = getSASGameRecordLandmassGeography(aLandmasses, pStart->getArea().getID());
		if (pLandmass == NULL)
			continue;
		PlayerTypes eNearestPlayer = NO_PLAYER;
		int iNearestDistance = -1;
		PlayerTypes eNearestSameLandmassPlayer = NO_PLAYER;
		int iNearestSameLandmassDistance = -1;
		int iSameLandmassDistance = 0;
		int iSameLandmassDistancePlayers = 0;
		for (int iJ = 0; iJ < MAX_CIV_PLAYERS; iJ++)
		{
			if (iJ == iI)
				continue;
			PlayerTypes const eOtherPlayer = (PlayerTypes)iJ;
			CvPlayer const& kOtherPlayer = GET_PLAYER(eOtherPlayer);
			CvPlot const* pOtherStart = kOtherPlayer.getStartingPlot();
			if (!kOtherPlayer.isEverAlive() || kOtherPlayer.isBarbarian() || pOtherStart == NULL || pOtherStart->isWater())
				continue;
			int const iDistance = plotDistance(pStart->getX(), pStart->getY(), pOtherStart->getX(), pOtherStart->getY());
			if (iNearestDistance < 0 || iDistance < iNearestDistance)
			{
				iNearestDistance = iDistance;
				eNearestPlayer = eOtherPlayer;
			}
			if (pOtherStart->getArea().getID() != pStart->getArea().getID())
				continue;
			iSameLandmassDistance += iDistance;
			iSameLandmassDistancePlayers++;
			if (iNearestSameLandmassDistance < 0 || iDistance < iNearestSameLandmassDistance)
			{
				iNearestSameLandmassDistance = iDistance;
				eNearestSameLandmassPlayer = eOtherPlayer;
			}
		}
		CvString const szLandmass = getSASGameRecordLandmassName(*pLandmass);
		logSASGameRecord("GAME_RECORD_PLAYER_START_DENSITY turn=%d player=%d currentTeam=%d start=%d,%d landmass=%s landmassStartingPlayers=%d landmassPlots=%d landmassHabitablePlots=%d landmassPlotsPerStartX100=%d landmassHabitablePlotsPerStartX100=%d nearestStartPlayer=%d nearestStartDistance=%d nearestSameLandmassStartPlayer=%d nearestSameLandmassStartDistance=%d avgSameLandmassStartDistanceX100=%d",
				GC.getGame().getGameTurn(), ePlayer, kPlayer.getTeam(), pStart->getX(), pStart->getY(), szLandmass.GetCString(), pLandmass->iStartingPlayers, pLandmass->iPlots, pLandmass->iHabitablePlots,
				pLandmass->iStartingPlayers <= 0 ? -1 : (100 * pLandmass->iPlots) / pLandmass->iStartingPlayers, pLandmass->iStartingPlayers <= 0 ? -1 : (100 * pLandmass->iHabitablePlots) / pLandmass->iStartingPlayers,
				eNearestPlayer, iNearestDistance, eNearestSameLandmassPlayer, iNearestSameLandmassDistance,
				iSameLandmassDistancePlayers <= 0 ? -1 : (100 * iSameLandmassDistance) / iSameLandmassDistancePlayers);
	}

	logSASGameRecord("GAME_RECORD_GEOGRAPHY_SUMMARY turn=%d landmasses=%d waterAreas=%d seaAreas=%d lakeAreas=%d mapPlots=%d landPlots=%d landPercentOfMapX100=%d waterPlots=%d waterPercentOfMapX100=%d seaPlots=%d seaPercentOfMapX100=%d coastSeaPlots=%d coastSeaPercentOfMapX100=%d oceanSeaPlots=%d oceanSeaPercentOfMapX100=%d otherSeaPlots=%d lakePlots=%d lakePercentOfMapX100=%d icePlots=%d icePercentOfMapX100=%d icePercentOfWaterX100=%d iceSeaPlots=%d icePercentOfSeaX100=%d iceCoastSeaPlots=%d iceOceanSeaPlots=%d iceLakePlots=%d largestLandmass=%s largestLandmassPlots=%d wrapX=%d wrapY=%d coastTerrain=%s oceanTerrain=%s iceFeature=%s",
			GC.getGame().getGameTurn(), (int)aLandmasses.size(), iWaterAreas, iSeaAreas, iLakeAreas, iMapPlots, iLandPlots, getSASGameRecordPercentX100(iLandPlots, iMapPlots), iWaterPlots, getSASGameRecordPercentX100(iWaterPlots, iMapPlots), iSeaPlots, getSASGameRecordPercentX100(iSeaPlots, iMapPlots), iCoastSeaPlots, getSASGameRecordPercentX100(iCoastSeaPlots, iMapPlots), iOceanSeaPlots, getSASGameRecordPercentX100(iOceanSeaPlots, iMapPlots), iOtherSeaPlots, iLakePlots, getSASGameRecordPercentX100(iLakePlots, iMapPlots), iIcePlots, getSASGameRecordPercentX100(iIcePlots, iMapPlots), getSASGameRecordPercentX100(iIcePlots, iWaterPlots), iIceSeaPlots, getSASGameRecordPercentX100(iIceSeaPlots, iSeaPlots), iIceCoastSeaPlots, iIceOceanSeaPlots, iIceLakePlots,
			szLargestLandmass.GetCString(), iLargestAreaPlots, kMap.isWrapX(), kMap.isWrapY(), eCoast == NO_TERRAIN ? "-" : GC.getInfo(eCoast).getType(), eOcean == NO_TERRAIN ? "-" : GC.getInfo(eOcean).getType(), eIce == NO_FEATURE ? "-" : GC.getInfo(eIce).getType());
	logSASGameRecord("GAME_RECORD_GEOGRAPHY_YIELD_LEGEND nature=terrain_feature_hills_river_without_bonus revealedRaw=nature_plus_bonus_base_yield_from_reveal_era bonusImprovedPotential=revealedRaw_plus_only_XML_valid_bonus_improvement_after_reveal_connection_and_build_tech nonBonusPlots=never_improvement_optimized routesCivicsMaturation=excluded potentialIrrigation=allowed_on_valid_bonus_improvement improvementTechYieldChanges=included score321=3F+2H+C averageScale=100 eraFormat=ERA:F/H/C/321");
	logSASGameRecord("GAME_RECORD_GEOGRAPHY_COMPOSITION_LEGEND percentScale=100 terrainPercentDenominator=landmassPlots featurePercentDenominator=landmassPlots featurelessPercent=plots_without_feature waterBorderLand=land_plots_adjacent_to_any_water seaBorderLand=land_plots_adjacent_to_nonlake_water lakeBorderLand=land_plots_adjacent_to_lake_water borderEdgesPerLandX100=adjacent_land_water_edges_per_land_plot_times_100 iceBorderLand=land_plots_adjacent_to_FEATURE_ICE adjacentLakePlots=unique_lake_area_tiles_touching_landmass adjacentLakePerLandPercentDenominator=landmassPlots noOceanConnected=non_ocean_water_topology_ignoring_impassable_features noOceanNavigable=non_ocean_water_excluding_impassable_plots sea=water_excluding_lakes coastSea=nonlake_TERRAIN_COAST oceanSea=nonlake_TERRAIN_OCEAN");
	for (size_t iI = 0; iI < aLandmasses.size(); iI++)
	{
		SASGameRecordLandmassGeography const& kLandmass = aLandmasses[iI];
		if (kLandmass.iPlots <= 0)
			continue;
		int iCenterX100 = kLandmass.iAnchorX * 100 + (100 * kLandmass.iSumWrappedDX) / kLandmass.iPlots;
		int iCenterY100 = kLandmass.iAnchorY * 100 + (100 * kLandmass.iSumWrappedDY) / kLandmass.iPlots;
		if (kMap.isWrapX())
		{
			int const iWidthX100 = kMap.getGridWidth() * 100;
			while (iCenterX100 < 0) iCenterX100 += iWidthX100;
			while (iCenterX100 >= iWidthX100) iCenterX100 -= iWidthX100;
		}
		if (kMap.isWrapY())
		{
			int const iHeightX100 = kMap.getGridHeight() * 100;
			while (iCenterY100 < 0) iCenterY100 += iHeightX100;
			while (iCenterY100 >= iHeightX100) iCenterY100 -= iHeightX100;
		}
		CvString const szName = getSASGameRecordLandmassName(kLandmass);
		logSASGameRecord("GAME_RECORD_LANDMASS turn=%d landmass=%s area=%d anchor=%d,%d centerX100=%d centerY100=%d plots=%d landSharePercentX100=%d habitablePlots=%d nonHabitablePlots=%d impassablePlots=%d zeroNatureYieldPlots=%d waterBorderPlots=%d seaBorderPlots=%d lakeBorderPlots=%d startingPlayers=%d starts=%s noOceanConnectedCount=%d noOceanConnectedTo=%s",
			GC.getGame().getGameTurn(), szName.GetCString(), kLandmass.iAreaId, kLandmass.iAnchorX, kLandmass.iAnchorY, iCenterX100, iCenterY100,
			kLandmass.iPlots, getSASGameRecordPercentX100(kLandmass.iPlots, iLandPlots), kLandmass.iHabitablePlots, kLandmass.iPlots - kLandmass.iHabitablePlots, kLandmass.iImpassablePlots,
			kLandmass.iZeroNatureYieldPlots, kLandmass.iWaterBorderPlots, kLandmass.iSeaBorderPlots, kLandmass.iLakeBorderPlots,
			kLandmass.iStartingPlayers, getSASDiagnosticOrDash(kLandmass.szStartingPlayers).GetCString(), (int)kLandmass.aiNoOceanConnectedAreas.size(), getSASGameRecordLandmassList(aLandmasses, kLandmass.aiNoOceanConnectedAreas).GetCString());
		std::vector<int> aiIceBlockedNoOceanAreas;
		for (size_t iConnection = 0; iConnection < kLandmass.aiNoOceanConnectedAreas.size(); iConnection++)
		{
			int const iAreaId = kLandmass.aiNoOceanConnectedAreas[iConnection];
			if (std::find(kLandmass.aiNoOceanNavigableConnectedAreas.begin(), kLandmass.aiNoOceanNavigableConnectedAreas.end(), iAreaId) == kLandmass.aiNoOceanNavigableConnectedAreas.end())
				aiIceBlockedNoOceanAreas.push_back(iAreaId);
		}
		logSASGameRecord("GAME_RECORD_LANDMASS_NAVIGATION turn=%d landmass=%s noOceanNavigableConnectedCount=%d iceBlockedNoOceanCount=%d iceBlockedNoOceanTo=%s",
				GC.getGame().getGameTurn(), szName.GetCString(), (int)kLandmass.aiNoOceanNavigableConnectedAreas.size(), (int)aiIceBlockedNoOceanAreas.size(), getSASGameRecordLandmassList(aLandmasses, aiIceBlockedNoOceanAreas).GetCString());
		int iFeaturePlots = 0;
		for (size_t iFeature = 0; iFeature < kLandmass.aiFeatureCounts.size(); iFeature++)
			iFeaturePlots += kLandmass.aiFeatureCounts[iFeature];
		int const iAdjacentLakePlots = getSASGameRecordAdjacentLakePlots(kLandmass);
		logSASGameRecord("GAME_RECORD_LANDMASS_COMPOSITION turn=%d landmass=%s hillsPercentX100=%d peakPercentX100=%d riverSidePercentX100=%d freshWaterPercentX100=%d waterBorderLandPercentX100=%d seaBorderLandPercentX100=%d lakeBorderLandPercentX100=%d iceBorderLandPercentX100=%d seaBorderEdgesPerLandX100=%d lakeBorderEdgesPerLandX100=%d iceBorderEdgesPerLandX100=%d featurelessPercentX100=%d adjacentLakePlots=%d adjacentLakePerLandPercentX100=%d terrainPercentX100=%s featurePercentX100=%s",
				GC.getGame().getGameTurn(), szName.GetCString(), getSASGameRecordPercentX100(kLandmass.iHillsPlots, kLandmass.iPlots), getSASGameRecordPercentX100(kLandmass.iPeakPlots, kLandmass.iPlots), getSASGameRecordPercentX100(kLandmass.iRiverSidePlots, kLandmass.iPlots), getSASGameRecordPercentX100(kLandmass.iFreshWaterPlots, kLandmass.iPlots),
				getSASGameRecordPercentX100(kLandmass.iWaterBorderPlots, kLandmass.iPlots), getSASGameRecordPercentX100(kLandmass.iSeaBorderPlots, kLandmass.iPlots), getSASGameRecordPercentX100(kLandmass.iLakeBorderPlots, kLandmass.iPlots), getSASGameRecordPercentX100(kLandmass.iIceBorderPlots, kLandmass.iPlots), (100 * kLandmass.iSeaBorderEdges) / kLandmass.iPlots, (100 * kLandmass.iLakeBorderEdges) / kLandmass.iPlots, (100 * kLandmass.iIceBorderEdges) / kLandmass.iPlots, getSASGameRecordPercentX100(kLandmass.iPlots - iFeaturePlots, kLandmass.iPlots), iAdjacentLakePlots, getSASGameRecordPercentX100(iAdjacentLakePlots, kLandmass.iPlots), getSASGameRecordLandmassTerrainPercentages(kLandmass).GetCString(), getSASGameRecordLandmassFeaturePercentages(kLandmass).GetCString());
		int const iFinalEra = GC.getNumEraInfos() - 1;
		int const iNature321 = 3 * kLandmass.iNatureFood + 2 * kLandmass.iNatureProduction + kLandmass.iNatureCommerce;
		logSASGameRecord("GAME_RECORD_LANDMASS_YIELDS turn=%d landmass=%s natureAvgF100=%d natureAvgH100=%d natureAvgC100=%d natureAvg321X100=%d revealedRawFinalAvgF100=%d revealedRawFinalAvgH100=%d revealedRawFinalAvgC100=%d revealedRawFinalAvg321X100=%d bonusImprovedPotentialFinalAvgF100=%d bonusImprovedPotentialFinalAvgH100=%d bonusImprovedPotentialFinalAvgC100=%d bonusImprovedPotentialFinalAvg321X100=%d eraRevealedRawAvgFHC321X100=%s eraBonusImprovedPotentialAvgFHC321X100=%s",
				GC.getGame().getGameTurn(), szName.GetCString(), (100 * kLandmass.iNatureFood) / kLandmass.iPlots, (100 * kLandmass.iNatureProduction) / kLandmass.iPlots, (100 * kLandmass.iNatureCommerce) / kLandmass.iPlots, (100 * iNature321) / kLandmass.iPlots,
				iFinalEra < 0 ? 0 : (100 * kLandmass.aiEraRevealedRawFood[iFinalEra]) / kLandmass.iPlots, iFinalEra < 0 ? 0 : (100 * kLandmass.aiEraRevealedRawProduction[iFinalEra]) / kLandmass.iPlots, iFinalEra < 0 ? 0 : (100 * kLandmass.aiEraRevealedRawCommerce[iFinalEra]) / kLandmass.iPlots, iFinalEra < 0 ? 0 : (100 * kLandmass.aiEraRevealedRaw321[iFinalEra]) / kLandmass.iPlots,
				iFinalEra < 0 ? 0 : (100 * kLandmass.aiEraBonusPotentialFood[iFinalEra]) / kLandmass.iPlots, iFinalEra < 0 ? 0 : (100 * kLandmass.aiEraBonusPotentialProduction[iFinalEra]) / kLandmass.iPlots, iFinalEra < 0 ? 0 : (100 * kLandmass.aiEraBonusPotentialCommerce[iFinalEra]) / kLandmass.iPlots, iFinalEra < 0 ? 0 : (100 * kLandmass.aiEraBonusPotential321[iFinalEra]) / kLandmass.iPlots, getSASGameRecordEraLandmassYieldList(kLandmass, false).GetCString(), getSASGameRecordEraLandmassYieldList(kLandmass, true).GetCString());
		logSASGameRecord("GAME_RECORD_LANDMASS_BONUS_SUMMARY turn=%d landmass=%s bonusCount=%d types=%s", GC.getGame().getGameTurn(), szName.GetCString(), kLandmass.iBonusCount, getSASGameRecordLandmassBonusTypes(kLandmass).GetCString());
		logSASGameRecordLandmassBonusCoordinates(kLandmass, szName);
	}
}

enum
{
	SAS_MAP_ASCII_GEOGRAPHY_SYMBOL_COUNT = 9,
	SAS_MAP_ASCII_RIVER_SYMBOL_COUNT = 6,
	SAS_MAP_ASCII_POLITICAL_SYMBOL_COUNT = 8
};

// <!-- custom: Parse and cache all ASCII-map symbols and related terrain-case settings together once per DLL session; callers therefore do not need separate static define caches.
// Keep each layer's palette independent. Ice deliberately has separately configurable Geography, Terrain-override, and Features symbols so a mod-mod can choose how it reads in each picture rather than inheriting one presentation everywhere. (GPT-5.6-Sol) -->
struct SASGameRecordMapAsciiPalette
{
	bool bValid;
	char acGeography[SAS_MAP_ASCII_GEOGRAPHY_SYMBOL_COUNT];
	char acRivers[SAS_MAP_ASCII_RIVER_SYMBOL_COUNT];
	char acPolitical[SAS_MAP_ASCII_POLITICAL_SYMBOL_COUNT];
	std::vector<char> acTerrain;
	std::vector<char> acFeatures;
	std::vector<char> acBonuses;
	int iTerrainUppercaseMinNatureFoodSurplus;
	int iFoodPerPopulation;
	CvString szPlayers;
	CvString szGeographyDefine;
	CvString szRiverDefine;
	CvString szPoliticalDefine;
	CvString szTerrainDefine;
	CvString szFeatureDefine;
	CvString szBonusDefine;
	CvString szError;
};

static bool parseSASGameRecordMapAsciiSymbol(CvString const& szToken, char& cSymbol)
{
	if (szToken == "SPACE") cSymbol = ' ';
	else if (szToken == "COMMA") cSymbol = ',';
	else if (szToken == "COLON") cSymbol = ':';
	else if (szToken == "SEMICOLON") cSymbol = ';';
	else if (szToken == "EQUALS") cSymbol = '=';
	else if (szToken.length() == 1) cSymbol = szToken[0];
	else return false;
	return (cSymbol >= 32 && cSymbol <= 126 && cSymbol != '"' && cSymbol != '\\' && cSymbol != '|');
}

static bool parseSASGameRecordMapAsciiPaletteDefine(char const* szDefineName, int iExpectedSymbols, char* acSymbols, CvString& szRaw, CvString& szError)
{
	szRaw = GC.getDefineSTRING(szDefineName);
	std::vector<CvString> aszTokens;
	szRaw.getTokens(",", aszTokens);
	if ((int)aszTokens.size() != iExpectedSymbols)
	{
		szError.Format("%s requires %d comma-separated symbols but has %d", szDefineName, iExpectedSymbols, (int)aszTokens.size());
		return false;
	}
	bool abUsed[127] = { false };
	for (int iSymbol = 0; iSymbol < iExpectedSymbols; iSymbol++)
	{
		if (!parseSASGameRecordMapAsciiSymbol(aszTokens[iSymbol], acSymbols[iSymbol]))
		{
			szError.Format("%s symbol %d must be one safe printable character or SPACE/COMMA/COLON/SEMICOLON/EQUALS", szDefineName, iSymbol);
			return false;
		}
		unsigned char const ucSymbol = (unsigned char)acSymbols[iSymbol];
		if (abUsed[ucSymbol])
		{
			szError.Format("%s repeats symbol ASCII %d", szDefineName, (int)ucSymbol);
			return false;
		}
		abUsed[ucSymbol] = true;
	}
	return true;
}

static char getSASGameRecordMapAsciiUppercaseSymbol(char cSymbol)
{
	return (cSymbol >= 'a' && cSymbol <= 'z' ? cSymbol - 'a' + 'A' : cSymbol);
}

static SASGameRecordMapAsciiPalette const& getSASGameRecordMapAsciiPalette()
{
	static SASGameRecordMapAsciiPalette kPalette;
	static bool bInitialized = false;
	if (bInitialized)
		return kPalette;
	bInitialized = true;
	kPalette.bValid = false;
	kPalette.acTerrain.resize(GC.getNumTerrainInfos() + 1);
	kPalette.acFeatures.resize(GC.getNumFeatureInfos() + 2);
	kPalette.acBonuses.resize(GC.getNumBonusInfos() + 3);
	kPalette.iTerrainUppercaseMinNatureFoodSurplus = GC.getDefineINT("SAS_GAME_RECORD_MAP_ASCII_TERRAIN_UPPERCASE_MIN_NATURE_FOOD_SURPLUS");
	kPalette.iFoodPerPopulation = GC.getFOOD_CONSUMPTION_PER_POPULATION();
	if (!parseSASGameRecordMapAsciiPaletteDefine("SAS_GAME_RECORD_MAP_ASCII_GEOGRAPHY_SYMBOLS", SAS_MAP_ASCII_GEOGRAPHY_SYMBOL_COUNT, kPalette.acGeography, kPalette.szGeographyDefine, kPalette.szError) ||
			!parseSASGameRecordMapAsciiPaletteDefine("SAS_GAME_RECORD_MAP_ASCII_RIVER_SYMBOLS", SAS_MAP_ASCII_RIVER_SYMBOL_COUNT, kPalette.acRivers, kPalette.szRiverDefine, kPalette.szError) ||
			!parseSASGameRecordMapAsciiPaletteDefine("SAS_GAME_RECORD_MAP_ASCII_POLITICAL_SYMBOLS", SAS_MAP_ASCII_POLITICAL_SYMBOL_COUNT, kPalette.acPolitical, kPalette.szPoliticalDefine, kPalette.szError) ||
			!parseSASGameRecordMapAsciiPaletteDefine("SAS_GAME_RECORD_MAP_ASCII_TERRAIN_SYMBOLS", (int)kPalette.acTerrain.size(), &kPalette.acTerrain[0], kPalette.szTerrainDefine, kPalette.szError) ||
			!parseSASGameRecordMapAsciiPaletteDefine("SAS_GAME_RECORD_MAP_ASCII_FEATURE_SYMBOLS", (int)kPalette.acFeatures.size(), &kPalette.acFeatures[0], kPalette.szFeatureDefine, kPalette.szError) ||
			!parseSASGameRecordMapAsciiPaletteDefine("SAS_GAME_RECORD_MAP_ASCII_BONUS_SYMBOLS", (int)kPalette.acBonuses.size(), &kPalette.acBonuses[0], kPalette.szBonusDefine, kPalette.szError))
		return kPalette;
	// <!-- custom: Uppercase is derived rather than configured separately. Reject a base palette that already uses one of those derived symbols for another terrain, which would make the food distinction ambiguous. (GPT-5.6-Sol) -->
	for (int iTerrain = 0; iTerrain < GC.getNumTerrainInfos(); iTerrain++)
	{
		char const cUppercase = getSASGameRecordMapAsciiUppercaseSymbol(kPalette.acTerrain[iTerrain]);
		if (cUppercase == kPalette.acTerrain[iTerrain]) continue;
		for (int iSymbol = 0; iSymbol < (int)kPalette.acTerrain.size(); iSymbol++)
		{
			if (iSymbol == iTerrain || cUppercase != kPalette.acTerrain[iSymbol]) continue;
			kPalette.szError.Format("SAS_GAME_RECORD_MAP_ASCII_TERRAIN_SYMBOLS derived uppercase symbol %c for terrain %d reuses symbol %d", cUppercase, iTerrain, iSymbol);
			return kPalette;
		}
	}
	kPalette.szPlayers = GC.getDefineSTRING("SAS_GAME_RECORD_MAP_ASCII_PLAYER_SYMBOLS");
	if ((int)kPalette.szPlayers.length() != MAX_CIV_PLAYERS)
	{
		kPalette.szError.Format("SAS_GAME_RECORD_MAP_ASCII_PLAYER_SYMBOLS requires exactly MAX_CIV_PLAYERS=%d characters but has %d", MAX_CIV_PLAYERS, (int)kPalette.szPlayers.length());
		return kPalette;
	}
	bool abPlayerSymbols[127] = { false };
	for (int iPlayer = 0; iPlayer < MAX_CIV_PLAYERS; iPlayer++)
	{
		unsigned char const ucSymbol = (unsigned char)kPalette.szPlayers[iPlayer];
		if (ucSymbol < 33 || ucSymbol > 126 || ucSymbol == '"' || ucSymbol == '\\' || ucSymbol == '|' || ucSymbol == ',' || ucSymbol == ';' || ucSymbol == '=')
		{
			kPalette.szError.Format("SAS_GAME_RECORD_MAP_ASCII_PLAYER_SYMBOLS player %d uses unsafe ASCII %d", iPlayer, (int)ucSymbol);
			return kPalette;
		}
		if (abPlayerSymbols[ucSymbol])
		{
			kPalette.szError.Format("SAS_GAME_RECORD_MAP_ASCII_PLAYER_SYMBOLS repeats symbol ASCII %d", (int)ucSymbol);
			return kPalette;
		}
		// <!-- custom: Separate layers may reuse intuitive symbols; only political symbols share a picture with player symbols. (GPT-5.6-Sol) -->
		for (int iSymbol = 0; iSymbol < SAS_MAP_ASCII_POLITICAL_SYMBOL_COUNT; iSymbol++)
		{
			if (ucSymbol == (unsigned char)kPalette.acPolitical[iSymbol])
			{
				kPalette.szError.Format("SAS_GAME_RECORD_MAP_ASCII_PLAYER_SYMBOLS player %d reuses political symbol ASCII %d", iPlayer, (int)ucSymbol);
				return kPalette;
			}
		}
		abPlayerSymbols[ucSymbol] = true;
	}
	kPalette.bValid = true;
	return kPalette;
}

static char getSASGameRecordMapAsciiPlayerSymbol(PlayerTypes ePlayer, SASGameRecordMapAsciiPalette const& kPalette)
{
	int const iPlayer = (int)ePlayer;
	return (iPlayer >= 0 && iPlayer < MAX_CIV_PLAYERS ? kPalette.szPlayers[iPlayer] : '?');
}

static void appendSASGameRecordMapAsciiSymbol(CvString& szRow, char cSymbol)
{
	char acSymbol[2] = { cSymbol, '\0' };
	szRow += acSymbol;
}

static CvString getSASGameRecordMapAsciiSymbolToken(char cSymbol)
{
	if (cSymbol == ' ') return "SPACE";
	if (cSymbol == ',') return "COMMA";
	if (cSymbol == ':') return "COLON";
	if (cSymbol == ';') return "SEMICOLON";
	if (cSymbol == '=') return "EQUALS";
	CvString szToken;
	szToken.Format("%c", cSymbol);
	return szToken;
}

static void appendSASGameRecordMapAsciiLegendEntry(CvString& szLegend, char cSymbol, char const* szType)
{
	CvString szEntry;
	szEntry.Format("%s=%s", getSASGameRecordMapAsciiSymbolToken(cSymbol).GetCString(), szType);
	if (!szLegend.empty()) szLegend += ";";
	szLegend += szEntry;
}

static void appendSASGameRecordMapAsciiRuntimeTypeLegendEntry(CvString& szLegend, char cSymbol, int iType, char const* szType)
{
	// <!-- custom: Runtime XML order can differ in another mod, so record the numeric ID with the stable type name and readable map symbol. (GPT-5.6-Sol) -->
	CvString szIdAndType;
	szIdAndType.Format("%d:%s", iType, szType);
	appendSASGameRecordMapAsciiLegendEntry(szLegend, cSymbol, szIdAndType.GetCString());
}

static CvString getSASGameRecordMapAsciiSymbolCounts(int const aiCounts[127])
{
	CvString szCounts;
	for (int iSymbol = 32; iSymbol <= 126; iSymbol++)
	{
		if (aiCounts[iSymbol] <= 0) continue;
		CvString szEntry;
		szEntry.Format("%s=%d", getSASGameRecordMapAsciiSymbolToken((char)iSymbol).GetCString(), aiCounts[iSymbol]);
		if (!szCounts.empty()) szCounts += ";";
		szCounts += szEntry;
	}
	return szCounts;
}

static CvString getSASGameRecordMapAsciiTerrainLegend(SASGameRecordMapAsciiPalette const& kPalette)
{
	CvString szLegend;
	for (int iTerrain = 0; iTerrain < GC.getNumTerrainInfos(); iTerrain++)
		appendSASGameRecordMapAsciiRuntimeTypeLegendEntry(szLegend, kPalette.acTerrain[iTerrain], iTerrain, getSASGameRecordTerrainType((TerrainTypes)iTerrain));
	appendSASGameRecordMapAsciiLegendEntry(szLegend, kPalette.acTerrain[GC.getNumTerrainInfos()], "FEATURE_ICE_OVERRIDE");
	return szLegend;
}

static CvString getSASGameRecordMapAsciiGeographyLegend(SASGameRecordMapAsciiPalette const& kPalette)
{
	static char const* const aszTypes[SAS_MAP_ASCII_GEOGRAPHY_SYMBOL_COUNT] = { "WATER", "LAKE", "FLAT_OR_MOSTLY_FLAT_LAND", "MOSTLY_HILLS", "MOSTLY_PEAKS", "MOSTLY_WATER_MIXED_LAND", "BALANCED_WATER_LAND", "MOSTLY_LAND_MIXED_WATER", "ICE" };
	CvString szLegend;
	for (int iType = 0; iType < SAS_MAP_ASCII_GEOGRAPHY_SYMBOL_COUNT; iType++)
		appendSASGameRecordMapAsciiLegendEntry(szLegend, kPalette.acGeography[iType], aszTypes[iType]);
	return szLegend;
}

static CvString getSASGameRecordMapAsciiTerrainUppercaseLegend(SASGameRecordMapAsciiPalette const& kPalette)
{
	CvString szLegend;
	for (int iTerrain = 0; iTerrain < GC.getNumTerrainInfos(); iTerrain++)
	{
		char const cUppercase = getSASGameRecordMapAsciiUppercaseSymbol(kPalette.acTerrain[iTerrain]);
		if (cUppercase != kPalette.acTerrain[iTerrain])
			appendSASGameRecordMapAsciiRuntimeTypeLegendEntry(szLegend, cUppercase, iTerrain, getSASGameRecordTerrainType((TerrainTypes)iTerrain));
	}
	if (szLegend.empty()) return "-";
	return szLegend;
}

static CvString getSASGameRecordMapAsciiRiverLegend(SASGameRecordMapAsciiPalette const& kPalette)
{
	static char const* const aszTypes[SAS_MAP_ASCII_RIVER_SYMBOL_COUNT] = { "NO_RIVER_EDGE_WATER", "NO_RIVER_EDGE_LAKE", "NO_RIVER_EDGE_LAND", "SOUTH_BOUNDARY_RIVER_EDGE", "EAST_BOUNDARY_RIVER_EDGE", "SOUTH_AND_EAST_BOUNDARY_RIVER_EDGES" };
	CvString szLegend;
	for (int iType = 0; iType < SAS_MAP_ASCII_RIVER_SYMBOL_COUNT; iType++)
		appendSASGameRecordMapAsciiLegendEntry(szLegend, kPalette.acRivers[iType], aszTypes[iType]);
	return szLegend;
}

static CvString getSASGameRecordMapAsciiFeatureLegend(SASGameRecordMapAsciiPalette const& kPalette)
{
	CvString szLegend;
	appendSASGameRecordMapAsciiLegendEntry(szLegend, kPalette.acFeatures[0], "NO_FEATURE_WATER");
	appendSASGameRecordMapAsciiLegendEntry(szLegend, kPalette.acFeatures[1], "NO_FEATURE_LAND");
	for (int iFeature = 0; iFeature < GC.getNumFeatureInfos(); iFeature++)
		appendSASGameRecordMapAsciiRuntimeTypeLegendEntry(szLegend, kPalette.acFeatures[iFeature + 2], iFeature, getSASGameRecordFeatureType((FeatureTypes)iFeature));
	return szLegend;
}

static CvString getSASGameRecordMapAsciiBonusLegend(SASGameRecordMapAsciiPalette const& kPalette)
{
	CvString szLegend;
	appendSASGameRecordMapAsciiLegendEntry(szLegend, kPalette.acBonuses[0], "NO_BONUS_WATER");
	appendSASGameRecordMapAsciiLegendEntry(szLegend, kPalette.acBonuses[1], "NO_BONUS_LAND");
	for (int iBonus = 0; iBonus < GC.getNumBonusInfos(); iBonus++)
		appendSASGameRecordMapAsciiRuntimeTypeLegendEntry(szLegend, kPalette.acBonuses[iBonus + 2], iBonus, getSASGameRecordBonusType((BonusTypes)iBonus));
	appendSASGameRecordMapAsciiLegendEntry(szLegend, kPalette.acBonuses[GC.getNumBonusInfos() + 2], "MULTIPLE_BONUS_TYPES");
	return szLegend;
}

static char getSASGameRecordMapAsciiGeographySymbol(CvMap const& kMap, SASGameRecordMapAsciiPalette const& kPalette, int iMinX, int iMaxX, int iMinY, int iMaxY)
{
	static FeatureTypes const eIce = (FeatureTypes)GC.getDefineINT("COLD_FEATURE");
	int iWater = 0;
	int iLake = 0;
	int iIce = 0;
	int iLand = 0;
	int iHills = 0;
	int iPeaks = 0;
	for (int iY = iMinY; iY < iMaxY; iY++)
	{
		for (int iX = iMinX; iX < iMaxX; iX++)
		{
			CvPlot const& kPlot = *kMap.plot(iX, iY);
			if (kPlot.isWater())
			{
				iWater++;
				if (kPlot.isLake()) iLake++;
				if (kPlot.getFeatureType() == eIce) iIce++;
			}
			else
			{
				iLand++;
				if (kPlot.isPeak()) iPeaks++;
				else if (kPlot.isHills()) iHills++;
			}
		}
	}
	// <!-- custom: Ice can form strategically important sea barriers in ordinary polar bands and in unconventional shapes on maps such as Peirce. Exact previews mark every ice plot; when resampling, require an ice-majority water cell so one plot does not hide a much larger mixed area. (GPT-5.6-Sol) -->
	if (iIce > 0 && 2 * iIce >= iWater && iWater >= iLand) return kPalette.acGeography[8];
	if (iLand <= 0)
		return (iLake == iWater ? kPalette.acGeography[1] : kPalette.acGeography[0]);
	if (iWater > 0)
	{
		int const iPlots = iLand + iWater;
		if (3 * iLand < iPlots) return kPalette.acGeography[5];
		if (3 * iLand > 2 * iPlots) return kPalette.acGeography[7];
		return kPalette.acGeography[6];
	}
	if (2 * iPeaks >= iLand) return kPalette.acGeography[4];
	if (2 * iHills >= iLand) return kPalette.acGeography[3];
	return kPalette.acGeography[2];
}

static char getSASGameRecordMapAsciiTerrainSymbol(CvMap const& kMap, SASGameRecordMapAsciiPalette const& kPalette, int iMinX, int iMaxX, int iMinY, int iMaxY)
{
	static FeatureTypes const eIce = (FeatureTypes)GC.getDefineINT("COLD_FEATURE");
	static std::vector<int> aiCounts;
	static std::vector<int> aiNatureFood;
	aiCounts.assign(GC.getNumTerrainInfos(), 0);
	aiNatureFood.assign(GC.getNumTerrainInfos(), 0);
	int iIce = 0;
	for (int iY = iMinY; iY < iMaxY; iY++)
	{
		for (int iX = iMinX; iX < iMaxX; iX++)
		{
			CvPlot const& kPlot = *kMap.plot(iX, iY);
			if (kPlot.getFeatureType() == eIce) iIce++;
			TerrainTypes const eTerrain = kPlot.getTerrainType();
			if (eTerrain >= 0 && eTerrain < GC.getNumTerrainInfos())
			{
				aiCounts[eTerrain]++;
				aiNatureFood[eTerrain] += kPlot.calculateNatureYield(YIELD_FOOD, NO_TEAM);
			}
		}
	}
	// <!-- custom: Ice is technically a feature, but it is not ordinarily removable and blocks naval paths, so it reads as part of the strategic surface alongside Coast, land and peaks.
	// One character cannot show both ice and its underlying Coast/Ocean terrain; prefer ice because blocked access is the strategically decisive state, while the obscured Coast shape is normally inferable from adjacent Coast and land.
	// Show exact ice at native resolution and an ice-majority cell after resampling; geography and features retain it for their separate shape/history purposes. (GPT-5.6-Sol) -->
	int const iPlotCount = (iMaxX - iMinX) * (iMaxY - iMinY);
	if (iIce > 0 && 2 * iIce >= iPlotCount) return kPalette.acTerrain[GC.getNumTerrainInfos()];
	TerrainTypes eDominant = NO_TERRAIN;
	int iDominantCount = 0;
	for (int iTerrain = 0; iTerrain < GC.getNumTerrainInfos(); iTerrain++)
	{
		if (aiCounts[iTerrain] > iDominantCount)
		{
			eDominant = (TerrainTypes)iTerrain;
			iDominantCount = aiCounts[iTerrain];
		}
	}
	if (eDominant == NO_TERRAIN) return '?';
	char const cTerrain = kPalette.acTerrain[eDominant];
	// <!-- custom: Case adds the strategic food information that terrain identity alone loses: e.g. flat Grass versus a Grass hill, or bare Desert versus Flood Plains. At reduced resolution, use the dominant terrain's average rather than letting one fertile plot capitalize a much larger poor cell. (GPT-5.6-Sol) -->
	int const iUppercaseMinNatureFood = kPalette.iFoodPerPopulation + kPalette.iTerrainUppercaseMinNatureFoodSurplus;
	if (aiNatureFood[eDominant] >= iUppercaseMinNatureFood * iDominantCount)
		return getSASGameRecordMapAsciiUppercaseSymbol(cTerrain);
	return cTerrain;
}

static char getSASGameRecordMapAsciiRiverSymbol(CvMap const& kMap, SASGameRecordMapAsciiPalette const& kPalette, int iMinX, int iMaxX, int iMinY, int iMaxY)
{
	// <!-- custom: Civ4's isNOfRiver flag means that this plot is north of a river on its southern boundary; isWOfRiver analogously records its eastern boundary. These two bits therefore identify the orthogonal movement edges that cross rivers, unlike a riverside-plot marker. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
	// <!-- custom: When no river edge replaces the background, blank ocean, visible lakes and dotted land keep each river attached to a recognizable landmass. A distinct lake symbol avoids mistaking an inland body near the coast for an ocean inlet; the same majority rule preserves the broad outline after resampling. (GPT-5.6-Sol) -->
	bool bSouthBoundary = false;
	bool bEastBoundary = false;
	int iWater = 0;
	int iLake = 0;
	int iLand = 0;
	for (int iY = iMinY; iY < iMaxY; iY++)
	{
		for (int iX = iMinX; iX < iMaxX; iX++)
		{
			CvPlot const& kPlot = *kMap.plot(iX, iY);
			if (kPlot.isNOfRiver()) bSouthBoundary = true;
			if (kPlot.isWOfRiver()) bEastBoundary = true;
			if (kPlot.isWater())
			{
				iWater++;
				if (kPlot.isLake()) iLake++;
			}
			else iLand++;
		}
	}
	if (bSouthBoundary || bEastBoundary) return kPalette.acRivers[(bSouthBoundary && bEastBoundary) ? 5 : (bSouthBoundary ? 3 : 4)];
	if (iLand > iWater) return kPalette.acRivers[2];
	return kPalette.acRivers[iLake == iWater ? 1 : 0];
}

static char getSASGameRecordMapAsciiFeatureSymbol(CvMap const& kMap, SASGameRecordMapAsciiPalette const& kPalette, int iMinX, int iMaxX, int iMinY, int iMaxY)
{
	static std::vector<int> aiCounts;
	aiCounts.assign(GC.getNumFeatureInfos() + 2, 0);
	for (int iY = iMinY; iY < iMaxY; iY++)
	{
		for (int iX = iMinX; iX < iMaxX; iX++)
		{
			CvPlot const& kPlot = *kMap.plot(iX, iY);
			FeatureTypes const eFeature = kPlot.getFeatureType();
			aiCounts[eFeature == NO_FEATURE ? (kPlot.isWater() ? 0 : 1) : eFeature + 2]++;
		}
	}
	// <!-- custom: Distinguishing featureless land from featureless water preserves the continent silhouette, so this changing feature layer remains readable without mentally overlaying the static geography map. (GPT-5.6-Sol) -->
	int iDominant = 0;
	for (int iFeature = 1; iFeature < (int)aiCounts.size(); iFeature++)
	{
		if (aiCounts[iFeature] > aiCounts[iDominant]) iDominant = iFeature;
	}
	return kPalette.acFeatures[iDominant];
}

static char getSASGameRecordMapAsciiBonusSymbol(CvMap const& kMap, SASGameRecordMapAsciiPalette const& kPalette, int iMinX, int iMaxX, int iMinY, int iMaxY)
{
	int iNoBonusWater = 0;
	int iNoBonusLand = 0;
	BonusTypes eOnlyBonus = NO_BONUS;
	for (int iY = iMinY; iY < iMaxY; iY++)
	{
		for (int iX = iMinX; iX < iMaxX; iX++)
		{
			CvPlot const& kPlot = *kMap.plot(iX, iY);
			BonusTypes const eBonus = kPlot.getBonusType(NO_TEAM);
			if (eBonus == NO_BONUS)
			{
				if (kPlot.isWater()) iNoBonusWater++;
				else iNoBonusLand++;
			}
			else if (eOnlyBonus == NO_BONUS) eOnlyBonus = eBonus;
			else if (eOnlyBonus != eBonus) return kPalette.acBonuses[GC.getNumBonusInfos() + 2];
		}
	}
	// <!-- custom: Bonuses are sparse, so any bonus is more informative than the surrounding empty plots in a resampled cell. Different bonus types collapse to the dedicated multiple marker; exact setup coordinates and later BONUS_CHANGE rows preserve every identity and location. (GPT-5.6-Sol) -->
	if (eOnlyBonus != NO_BONUS) return kPalette.acBonuses[eOnlyBonus + 2];
	return kPalette.acBonuses[iNoBonusWater >= iNoBonusLand ? 0 : 1];
}

static char getSASGameRecordMapAsciiPoliticalSymbol(CvMap const& kMap, SASGameRecordMapAsciiPalette const& kPalette, std::vector<PlayerTypes> const& aeStartingPlayers, int iMinX, int iMaxX, int iMinY, int iMaxY)
{
	// <!-- custom: Keep this layer purely political. Coast or ice overriding a player symbol would hide owned water, while showing them only on unowned plots would make physical geography inconsistent as borders expand; Terrain/Geography/Features preserve that context separately. (GPT-5.6-Sol) -->
	int aiOwnerPlots[MAX_PLAYERS] = { 0 };
	PlayerTypes eStartingPlayer = NO_PLAYER;
	bool bMultipleStartingPlayers = false;
	bool bCivilizationCity = false;
	bool bBarbarianCity = false;
	int iWater = 0;
	int iLand = 0;
	for (int iY = iMinY; iY < iMaxY; iY++)
	{
		for (int iX = iMinX; iX < iMaxX; iX++)
		{
			CvPlot const& kPlot = *kMap.plot(iX, iY);
			if (kPlot.isWater()) iWater++;
			else iLand++;
			PlayerTypes const eOwner = kPlot.getOwner();
			if (eOwner >= 0 && eOwner < MAX_PLAYERS)
				aiOwnerPlots[eOwner]++;
			CvCity const* pCity = kPlot.getPlotCity();
			if (pCity != NULL)
			{
				if (pCity->isBarbarian()) bBarbarianCity = true;
				else bCivilizationCity = true;
			}
			PlayerTypes const ePlotStartingPlayer = (aeStartingPlayers.empty() ? NO_PLAYER : aeStartingPlayers[kMap.plotNum(iX, iY)]);
			if (ePlotStartingPlayer != NO_PLAYER)
			{
				if (eStartingPlayer == NO_PLAYER) eStartingPlayer = ePlotStartingPlayer;
				else if (eStartingPlayer != ePlotStartingPlayer) bMultipleStartingPlayers = true;
			}
		}
	}
	if (bCivilizationCity && bBarbarianCity) return kPalette.acPolitical[5];
	if (bBarbarianCity) return kPalette.acPolitical[4];
	if (bCivilizationCity) return kPalette.acPolitical[3];
	if (bMultipleStartingPlayers) return kPalette.acPolitical[6];
	if (eStartingPlayer != NO_PLAYER) return getSASGameRecordMapAsciiPlayerSymbol(eStartingPlayer, kPalette);
	PlayerTypes eDominantOwner = NO_PLAYER;
	int iDominantOwnerPlots = 0;
	for (int iPlayer = 0; iPlayer < MAX_PLAYERS; iPlayer++)
	{
		if (aiOwnerPlots[iPlayer] > iDominantOwnerPlots)
		{
			iDominantOwnerPlots = aiOwnerPlots[iPlayer];
			eDominantOwner = (PlayerTypes)iPlayer;
		}
	}
	if (eDominantOwner == BARBARIAN_PLAYER) return kPalette.acPolitical[7];
	if (eDominantOwner != NO_PLAYER) return getSASGameRecordMapAsciiPlayerSymbol(eDominantOwner, kPalette);
	if (iWater > 0 && iLand > 0) return kPalette.acPolitical[2];
	return (iWater > 0 ? kPalette.acPolitical[0] : kPalette.acPolitical[1]);
}

// <!-- custom: The bounded river picture can merge edges on oversized maps. Preserve every setup edge in compact coordinate chunks too, using S/E for the plot's south/east boundaries so an external tool can reconstruct exact orthogonal river crossings. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
static void logSASGameRecordRiverEdgeCoordinates()
{
	CvMap const& kMap = GC.getMap();
	std::vector<CvString> aszChunks;
	CvString szChunk;
	int iEdgePlots = 0;
	int iRiverEdges = 0;
	for (int iPlot = 0; iPlot < kMap.numPlots(); iPlot++)
	{
		CvPlot const& kPlot = *kMap.plotByIndex(iPlot);
		bool const bSouth = kPlot.isNOfRiver();
		bool const bEast = kPlot.isWOfRiver();
		if (!bSouth && !bEast) continue;
		char const* const szEdgeDirections = (bSouth && bEast ? "SE" : bSouth ? "S" : "E");
		iEdgePlots++;
		iRiverEdges += (bSouth ? 1 : 0) + (bEast ? 1 : 0);
		CvString szItem;
		szItem.Format("%s(%d,%d):%s", szChunk.empty() ? "" : ";", kPlot.getX(), kPlot.getY(), szEdgeDirections);
		if (!szChunk.empty() && szChunk.length() + szItem.length() > 1500)
		{
			aszChunks.push_back(szChunk);
			szChunk.clear();
			szItem.Format("(%d,%d):%s", kPlot.getX(), kPlot.getY(), szEdgeDirections);
		}
		szChunk += szItem;
	}
	if (!szChunk.empty()) aszChunks.push_back(szChunk);
	if (aszChunks.empty()) aszChunks.push_back("-");
	for (size_t iPart = 0; iPart < aszChunks.size(); iPart++)
		logSASGameRecord("GAME_RECORD_MAP_RIVER_EDGES turn=%d edgePlots=%d riverEdges=%d part=%d parts=%d edgeFormat=(x,y):D directionCodes=S/E/SE edges=%s", GC.getGame().getGameTurn(), iEdgePlots, iRiverEdges, (int)iPart + 1, (int)aszChunks.size(), aszChunks[iPart].GetCString());
}

// <!-- custom: A bounded text map gives external LLMs and text-only reviewers the broad spatial relationships that aggregate landmass statistics cannot show. Fit the generated map, rather than the selected XML world size, into the tunable box with one scale so Tiny maps remain exact while horizontal SAS_Longworld and possible vertical/tower maps preserve their shapes.
// Monospace characters are usually much taller than wide, so repeat each map cell horizontally by a tunable amount. Keep metadata outside the pipe-framed drawing rows so humans and LLMs can parse each layer as one uninterrupted picture.
// Geography, terrain, directional river edges, and bonuses are normally stable enough to record once at setup/load; structured map-change rows preserve later exceptions. Record features initially because jungle, forest, flood plains, oases, and ice affect settling, movement, health, and yields; repeat features and political borders at level-3 snapshots to show Forest/Jungle clearing and regrowth, fallout, expansion, conquest, and collapse. (GPT-5.6-Sol + ChatGPT-5.6-Sol) -->
static void logSASGameRecordMapAscii(bool bIncludeStaticLayers, char const* szReason)
{
	uint const uiMapAsciiStartTime = getSASMonotonicMilliseconds();
	bool const abLayerEnabled[6] =
	{
		bIncludeStaticLayers && isSASGameRecordMapAsciiGeographyEnabled(),
		bIncludeStaticLayers && isSASGameRecordMapAsciiTerrainEnabled(),
		bIncludeStaticLayers && isSASGameRecordMapAsciiRiversEnabled(),
		bIncludeStaticLayers && isSASGameRecordMapAsciiBonusesEnabled(),
		isSASGameRecordMapAsciiFeaturesEnabled(),
		isSASGameRecordMapAsciiPoliticalEnabled()
	};
	int iLayerCount = 0;
	for (int iLayer = 0; iLayer < 6; iLayer++)
		if (abLayerEnabled[iLayer]) iLayerCount++;
	if (iLayerCount <= 0)
		return;
	int const iMaxWidth = getSASGameRecordMapAsciiMaxWidth();
	int const iMaxHeight = getSASGameRecordMapAsciiMaxHeight();
	if (iMaxWidth <= 0 || iMaxHeight <= 0)
		return;
	SASGameRecordMapAsciiPalette const& kPalette = getSASGameRecordMapAsciiPalette();
	if (!kPalette.bValid)
	{
		logSASGameRecord("GAME_RECORD_MAP_ASCII_CONFIG_ERROR error=%s", getSASDiagnosticQuoted(kPalette.szError.GetCString()).GetCString());
		FAssertMsg(false, kPalette.szError.GetCString());
		return;
	}
	CvMap const& kMap = GC.getMap();
	int const iSourceWidth = kMap.getGridWidth();
	int const iSourceHeight = kMap.getGridHeight();
	if (iSourceWidth <= 0 || iSourceHeight <= 0)
		return;
	int const iHorizontalCharsPerCell = getSASGameRecordMapAsciiHorizontalCharsPerCell();
	int const iMaxCellWidth = std::max(1, iMaxWidth / iHorizontalCharsPerCell);
	int iPreviewWidth = iSourceWidth;
	int iPreviewHeight = iSourceHeight;
	if (iSourceWidth > iMaxCellWidth || iSourceHeight > iMaxHeight)
	{
		if (iSourceWidth * iMaxHeight >= iSourceHeight * iMaxCellWidth)
		{
			iPreviewWidth = iMaxCellWidth;
			iPreviewHeight = std::max(1, (iSourceHeight * iMaxCellWidth + iSourceWidth / 2) / iSourceWidth);
		}
		else
		{
			iPreviewHeight = iMaxHeight;
			iPreviewWidth = std::max(1, (iSourceWidth * iMaxHeight + iSourceHeight / 2) / iSourceHeight);
		}
	}
	int const iOutputWidth = iPreviewWidth * iHorizontalCharsPerCell;
	int iSourceSouthBoundaryRiverEdges = 0;
	int iSourceEastBoundaryRiverEdges = 0;
	if (abLayerEnabled[2])
	{
		for (int iPlot = 0; iPlot < kMap.numPlots(); iPlot++)
		{
			CvPlot const& kPlot = *kMap.plotByIndex(iPlot);
			if (kPlot.isNOfRiver()) iSourceSouthBoundaryRiverEdges++;
			if (kPlot.isWOfRiver()) iSourceEastBoundaryRiverEdges++;
		}
	}
	bool const bMarkStartingPlots = (abLayerEnabled[5] && GC.getGame().getElapsedGameTurns() == 0);
	std::vector<PlayerTypes> aeStartingPlayers;
	CvString szPlayerSymbols;
	if (bMarkStartingPlots) aeStartingPlayers.assign(kMap.numPlots(), NO_PLAYER);
	if (abLayerEnabled[5])
	{
		for (int iPlayer = 0; iPlayer < MAX_CIV_PLAYERS; iPlayer++)
		{
			PlayerTypes const ePlayer = (PlayerTypes)iPlayer;
			CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
			if (!kPlayer.isEverAlive())
				continue;
			if (bMarkStartingPlots && kPlayer.getStartingPlot() != NULL)
				aeStartingPlayers[kMap.plotNum(kPlayer.getStartingPlot()->getX(), kPlayer.getStartingPlot()->getY())] = ePlayer;
			CvString szPlayerSymbol;
			szPlayerSymbol.Format("%c=%d", getSASGameRecordMapAsciiPlayerSymbol(ePlayer, kPalette), iPlayer);
			if (!szPlayerSymbols.empty()) szPlayerSymbols += ";";
			szPlayerSymbols += szPlayerSymbol;
		}
	}
	CvString const szPlayerSymbolsQuoted = getSASDiagnosticQuoted(getSASDiagnosticOrDash(szPlayerSymbols).GetCString());
	// <!-- custom: Keep one timing anchor at each text-map boundary. Appending the same session time to every fixed-width drawing row obscured the frame, repeated no useful chronology and materially enlarged the log. (GPT-5.6-Sol) -->
	logSASGameRecord("GAME_RECORD_MAP_ASCII_BEGIN turn=%d reason=%s layers=%d source=%dx%d previewCells=%dx%d outputCharacters=%dx%d maxCharacters=%dx%d horizontalCharactersPerCell=%d aspectRatioPreserved=1 resampled=%d wrapX=%d wrapY=%d topRowFirst=1 rowFrame=PIPE informationScope=omniscient_actual_map sessionWallMilliseconds=%u",
			GC.getGame().getGameTurn(), szReason, iLayerCount, iSourceWidth, iSourceHeight, iPreviewWidth, iPreviewHeight, iOutputWidth, iPreviewHeight, iMaxWidth, iMaxHeight, iHorizontalCharsPerCell, iPreviewWidth != iSourceWidth || iPreviewHeight != iSourceHeight, kMap.isWrapX(), kMap.isWrapY(), getSASGameRecordSessionWallMilliseconds(uiMapAsciiStartTime));
	if (abLayerEnabled[0])
		logSASGameRecord("GAME_RECORD_MAP_ASCII_LEGEND layer=GEOGRAPHY symbolTypeCount=%d symbolTypes=%s sourcePlotTypeCount=%d sourcePlotTypes=\"%d:PLOT_PEAK;%d:PLOT_HILLS;%d:PLOT_LAND;%d:PLOT_OCEAN\" resampledCell=derived_plot_mix",
				SAS_MAP_ASCII_GEOGRAPHY_SYMBOL_COUNT, getSASDiagnosticQuoted(getSASGameRecordMapAsciiGeographyLegend(kPalette).GetCString()).GetCString(), NUM_PLOT_TYPES, PLOT_PEAK, PLOT_HILLS, PLOT_LAND, PLOT_OCEAN);
	if (abLayerEnabled[1])
		logSASGameRecord("GAME_RECORD_MAP_ASCII_LEGEND layer=TERRAIN runtimeTypeCount=%d runtimeTypeFormat=SYMBOL=ID:TYPE symbolTypes=%s uppercaseFoodSymbols=%s uppercaseMinNatureFoodSurplus=%d foodPerPopulation=%d foodIncludes=hills_features_lakes_rivers_permanent_plot_yields foodExcludes=improvements_bonuses resampledCell=dominant_type_with_average_nature_food_case",
				GC.getNumTerrainInfos(), getSASDiagnosticQuoted(getSASGameRecordMapAsciiTerrainLegend(kPalette).GetCString()).GetCString(), getSASDiagnosticQuoted(getSASGameRecordMapAsciiTerrainUppercaseLegend(kPalette).GetCString()).GetCString(), kPalette.iTerrainUppercaseMinNatureFoodSurplus, kPalette.iFoodPerPopulation);
	if (abLayerEnabled[2])
		logSASGameRecord("GAME_RECORD_MAP_ASCII_LEGEND layer=RIVERS palette=%s symbolTypes=%s storedEdgeSemantics=plot_south_and_east_boundaries resampledCell=any_source_edge_by_orientation",
				getSASDiagnosticQuoted(kPalette.szRiverDefine.GetCString()).GetCString(), getSASDiagnosticQuoted(getSASGameRecordMapAsciiRiverLegend(kPalette).GetCString()).GetCString());
	// <!-- custom: getBonusType(NO_TEAM) intentionally records the actual map, including bonuses that no civilization has the technology to reveal yet.
	// State this explicitly so an analyst does not mistake diagnostic knowledge for contemporary AI knowledge. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
	if (abLayerEnabled[3])
		logSASGameRecord("GAME_RECORD_MAP_ASCII_LEGEND layer=BONUSES runtimeTypeCount=%d runtimeTypeFormat=SYMBOL=ID:TYPE symbolTypes=%s resampledCell=bonus_type_or_multiple includesUnrevealedBonuses=1", GC.getNumBonusInfos(), getSASDiagnosticQuoted(getSASGameRecordMapAsciiBonusLegend(kPalette).GetCString()).GetCString());
	if (abLayerEnabled[4])
		logSASGameRecord("GAME_RECORD_MAP_ASCII_LEGEND layer=FEATURES runtimeTypeCount=%d runtimeTypeFormat=SYMBOL=ID:TYPE symbolTypes=%s resampledCell=dominant_type", GC.getNumFeatureInfos(), getSASDiagnosticQuoted(getSASGameRecordMapAsciiFeatureLegend(kPalette).GetCString()).GetCString());
	// <!-- custom: GAME_RECORD_PLAYER_SETUP already stores each player ID's quoted civilization and display name.
	// Keep this repeated map legend to unambiguous SYMBOL=PLAYER_ID pairs so user-controlled punctuation in names cannot break an embedded mini-format or duplicate long names at every snapshot. (ChatGPT-5.5 + GPT-5.6-Sol) -->
	if (abLayerEnabled[5])
		logSASGameRecord("GAME_RECORD_MAP_ASCII_LEGEND layer=POLITICAL palette=%s order=unowned_water,unowned_land,mixed_unowned_water_land,civilization_city,Barbarian_city,civilization_and_Barbarian_cities,multiple_starting_players,Barbarian_territory playerSymbolFormat=SYMBOL=PLAYER_ID playerSymbols=%s playerDetailsRows=GAME_RECORD_PLAYER_SETUP startingPlotsMarked=%d",
				getSASDiagnosticQuoted(kPalette.szPoliticalDefine.GetCString()).GetCString(), szPlayerSymbolsQuoted.GetCString(), bMarkStartingPlots);
	for (int iLayer = 0; iLayer < 6; iLayer++)
	{
		if (!abLayerEnabled[iLayer]) continue;
		char const* szLayer = (iLayer == 0 ? "GEOGRAPHY" : iLayer == 1 ? "TERRAIN" : iLayer == 2 ? "RIVERS" : iLayer == 3 ? "BONUSES" : iLayer == 4 ? "FEATURES" : "POLITICAL");
		int aiSymbolCounts[127] = { 0 };
		logSASGameRecord("GAME_RECORD_MAP_ASCII_LAYER_BEGIN layer=%s rows=%d columns=%d sourceYTop=%d sourceYBottom=0", szLayer, iPreviewHeight, iOutputWidth, iSourceHeight - 1);
		for (int iRow = 0; iRow < iPreviewHeight; iRow++)
		{
			int const iPreviewY = iPreviewHeight - iRow - 1;
			int const iMinY = (iPreviewY * iSourceHeight) / iPreviewHeight;
			int const iMaxY = ((iPreviewY + 1) * iSourceHeight) / iPreviewHeight;
			CvString szPlots = "|";
			for (int iPreviewX = 0; iPreviewX < iPreviewWidth; iPreviewX++)
			{
				int const iMinX = (iPreviewX * iSourceWidth) / iPreviewWidth;
				int const iMaxX = ((iPreviewX + 1) * iSourceWidth) / iPreviewWidth;
				char cSymbol;
				if (iLayer == 0) cSymbol = getSASGameRecordMapAsciiGeographySymbol(kMap, kPalette, iMinX, iMaxX, iMinY, iMaxY);
				else if (iLayer == 1) cSymbol = getSASGameRecordMapAsciiTerrainSymbol(kMap, kPalette, iMinX, iMaxX, iMinY, iMaxY);
				else if (iLayer == 2) cSymbol = getSASGameRecordMapAsciiRiverSymbol(kMap, kPalette, iMinX, iMaxX, iMinY, iMaxY);
				else if (iLayer == 3) cSymbol = getSASGameRecordMapAsciiBonusSymbol(kMap, kPalette, iMinX, iMaxX, iMinY, iMaxY);
				else if (iLayer == 4) cSymbol = getSASGameRecordMapAsciiFeatureSymbol(kMap, kPalette, iMinX, iMaxX, iMinY, iMaxY);
				else cSymbol = getSASGameRecordMapAsciiPoliticalSymbol(kMap, kPalette, aeStartingPlayers, iMinX, iMaxX, iMinY, iMaxY);
				aiSymbolCounts[(unsigned char)cSymbol]++;
				for (int iRepeat = 0; iRepeat < iHorizontalCharsPerCell; iRepeat++)
					appendSASGameRecordMapAsciiSymbol(szPlots, cSymbol);
			}
			szPlots += "|";
			logSASGameRecord("%s", szPlots.GetCString());
		}
		int const iPreviewCells = iPreviewWidth * iPreviewHeight;
		int const iDrawingCharacters = (iOutputWidth + 2) * iPreviewHeight;
		if (iLayer == 2)
			logSASGameRecord("GAME_RECORD_MAP_ASCII_LAYER_END layer=%s previewCells=%d drawingCharacters=%d previewCellCounts=%s sourceSouthBoundaryRiverEdges=%d sourceEastBoundaryRiverEdges=%d sourceRiverEdges=%d",
					szLayer, iPreviewCells, iDrawingCharacters, getSASDiagnosticQuoted(getSASGameRecordMapAsciiSymbolCounts(aiSymbolCounts).GetCString()).GetCString(),
					iSourceSouthBoundaryRiverEdges, iSourceEastBoundaryRiverEdges, iSourceSouthBoundaryRiverEdges + iSourceEastBoundaryRiverEdges);
		else logSASGameRecord("GAME_RECORD_MAP_ASCII_LAYER_END layer=%s previewCells=%d drawingCharacters=%d previewCellCounts=%s", szLayer, iPreviewCells, iDrawingCharacters, getSASDiagnosticQuoted(getSASGameRecordMapAsciiSymbolCounts(aiSymbolCounts).GetCString()).GetCString());
	}
	uint const uiMapAsciiEndTime = getSASMonotonicMilliseconds();
	logSASGameRecord("GAME_RECORD_MAP_ASCII_END turn=%d reason=%s layers=%d rowsPerLayer=%d sessionWallMilliseconds=%u mapBlockWallMilliseconds=%u", GC.getGame().getGameTurn(), szReason, iLayerCount, iPreviewHeight, getSASGameRecordSessionWallMilliseconds(uiMapAsciiEndTime), getSASElapsedMilliseconds(uiMapAsciiStartTime, uiMapAsciiEndTime));
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

struct SASGameRecordInitialTechGroup
{
	CvString szTechFields;
	CvString szTeams;
	int iTeams;
};

// <!-- custom: Successful new-game initialization is best described by its authoritative result, not by the order in which Civ4 happened to call meet/declareWar/setHasTech/startTrade while constructing that result.
// Share the field semantics with BBAI and seed recorder deltas from the same finalized baseline.
// Group identical technology sets so a late-era start does not repeat the same long payload for every team; the explicit team lists keep arbitrary scenarios and mixed/modded setups exact. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
static void logSASGameRecordFinalizedInitialState(int& iTeamStateRows, int& iTechRows, int& iDeals)
{
	iTeamStateRows = 0;
	iTechRows = 0;
	iDeals = 0;
	// <!-- custom: Caller contract: logSASGameRecordNewGameStarted invokes this helper only at level 2+.
	// Keeping the gate outside prevents redundant checks and makes any future caller responsible for avoiding log-only setup work. (ChatGPT-5.6-Sol) -->
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

static void logSASGameRecordInitialContext(bool bNewGame)
{
	logSASGameRecordGeography();
	bool const bLogInitialDetails = (gGameRecordLogLevel >= 2);
	bool const bLogInitialVerboseDetails = (bLogInitialDetails && gGameRecordLogLevel >= 3);
	if (bLogInitialVerboseDetails)
	{
		logSASGameRecordMapAscii(true, "initialContext");
		logSASGameRecordRiverEdgeCoordinates();
	}
	if (!bLogInitialDetails)
		return;
	logSASGameRecordAttitudeLegend();
	for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		PlayerTypes eLoopPlayer = (PlayerTypes)iI;
		CvPlayer const& kLoopPlayer = GET_PLAYER(eLoopPlayer);
		if (!kLoopPlayer.isAlive() || kLoopPlayer.isBarbarian())
			continue;
		logSASGameRecordKnownArea(eLoopPlayer, "setup");
		logSASGameRecordStartingUnits(eLoopPlayer, "setup");
		int iLoop = 0;
		for (CvCity const* pLoopCity = kLoopPlayer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kLoopPlayer.nextCity(&iLoop))
			logSASGameRecordCityBFC(*pLoopCity, "setup");
	}
	// <!-- custom: Level-2+ new games already emitted authoritative INITIAL_TEAM_STATE metTeams and seeded the contact baseline.
	// Loaded saves have no finalized initial-team block in this session, so retain explicit setup contact rows for them. (ChatGPT-5.6-Sol) -->
	if (!bNewGame)
	{
		for (int iI = 0; iI < MAX_CIV_TEAMS; iI++)
		{
			TeamTypes eLoopTeam = (TeamTypes)iI;
			if (GET_TEAM(eLoopTeam).isAlive() && !GET_TEAM(eLoopTeam).isBarbarian())
				logSASGameRecordTeamContacts(eLoopTeam, GC.getGame().getGameTurn(), "setup");
		}
	}
}

static void logSASGameRecordMapBonusTotals(int iGameTurn)
{
	CvString szBonuses;
	FOR_EACH_ENUM(Bonus)
		appendSASGameRecordTypeCount(szBonuses, getSASGameRecordBonusType(eLoopBonus), GC.getMap().getNumBonuses(eLoopBonus));
	logSASGameRecord("GAME_RECORD_MAP_BONUSES turn=%d total=%s", iGameTurn, getSASDiagnosticOrDash(szBonuses).GetCString());
}

static void logSASGameRecordTeamProjects(TeamTypes eTeam, int iGameTurn);

static void logSASGameRecordBattleBuckets(int iGameTurn)
{
	for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		PlayerTypes eLoopPlayer = (PlayerTypes)iI;
		SASGameRecordBattleQuality& kQuality = g_akSASGameRecordBattleQuality[iI];
		if (g_aiSASGameRecordBattleWins[iI] != 0 || g_aiSASGameRecordBattleLosses[iI] != 0 || g_aiSASGameRecordCityBattleWins[iI] != 0 || g_aiSASGameRecordCityBattleLosses[iI] != 0 || kQuality.hasAny())
		{
			// <!-- custom: Expected wins are the sum of exact own pre-combat odds for the same binary battles counted by luckEligibleWins. luckDeltaX1000 is therefore observed minus expected wins in thousandths of a win.
			// Withdrawals/combat-limit outcomes remain separate. (ChatGPT-5.6-Sol) -->
			logSASGameRecord("GAME_RECORD_BATTLE_SUMMARY turn=%d range=%d-%d player=%d wins=%d losses=%d cityPlotWins=%d cityPlotLosses=%d withdrawals=%d enemyWithdrawals=%d combatLimitAttacks=%d combatLimitDefenses=%d luckEligibleBattles=%d luckEligibleWins=%d expectedWinsX1000=%d luckDeltaX1000=%+d upsetWins=%d upsetLosses=%d lowestOddsWinPermille=%d highestOddsLossPermille=%d",
				iGameTurn, g_iSASGameRecordBattleStartTurn, iGameTurn, eLoopPlayer, g_aiSASGameRecordBattleWins[iI], g_aiSASGameRecordBattleLosses[iI], g_aiSASGameRecordCityBattleWins[iI], g_aiSASGameRecordCityBattleLosses[iI],
				kQuality.iWithdrawals, kQuality.iEnemyWithdrawals, kQuality.iCombatLimitAttacks, kQuality.iCombatLimitDefenses, kQuality.iLuckEligibleBattles, kQuality.iLuckEligibleWins, kQuality.iExpectedWinsX1000, 1000 * kQuality.iLuckEligibleWins - kQuality.iExpectedWinsX1000, kQuality.iUpsetWins, kQuality.iUpsetLosses, kQuality.iLowestOddsWinPermille, kQuality.iHighestOddsLossPermille);
		}
		g_aiSASGameRecordBattleWins[iI] = 0;
		g_aiSASGameRecordBattleLosses[iI] = 0;
		g_aiSASGameRecordCityBattleWins[iI] = 0;
		g_aiSASGameRecordCityBattleLosses[iI] = 0;
		kQuality.reset();
	}
	// <!-- custom: Advance from the actual reset boundary instead of fabricating the next range from the configured interval.
	// This preserves turn-0 combat and mid-interval load/victory flushes. See KI#378. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
	g_iSASGameRecordBattleStartTurn = iGameTurn + 1;
}

static CvString getSASGameRecordAIProductionTransitions(SASGameRecordPlayerFlow const& kFlow)
{
	CvString szTransitions;
	static char const* const aszKinds[NUM_SAS_PRODUCTION_KINDS] = {"UNIT", "BUILDING", "WONDER", "PROJECT", "PROCESS"};
	for (int iOld = 0; iOld < NUM_SAS_PRODUCTION_KINDS; iOld++)
	{
		for (int iNew = 0; iNew < NUM_SAS_PRODUCTION_KINDS; iNew++)
		{
			int const iCount = kFlow.aiAIProductionTransitions[iOld * NUM_SAS_PRODUCTION_KINDS + iNew];
			if (iCount <= 0)
				continue;
			CvString szItem;
			szItem.Format(szTransitions.empty() ? "%s>%s:%d" : ",%s>%s:%d", aszKinds[iOld], aszKinds[iNew], iCount);
			szTransitions += szItem;
		}
	}
	return getSASDiagnosticOrDash(szTransitions);
}

static int getSASGameRecordMaxAIProductionTargetChangesOneCity(SASGameRecordPlayerFlow const& kFlow)
{
	int iMax = 0;
	for (size_t iI = 0; iI < kFlow.aAIProductionTargetChangesByCity.size(); iI++)
		iMax = std::max(iMax, kFlow.aAIProductionTargetChangesByCity[iI].second);
	return iMax;
}

static void logSASGameRecordFlowBuckets(int iGameTurn)
{
	for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		PlayerTypes const ePlayer = (PlayerTypes)iI;
		SASGameRecordPlayerFlow& kFlow = g_akSASGameRecordPlayerFlow[iI];
		if (kFlow.hasProduction())
		{
			CvString szUnitTypes;
			CvString szConscriptedUnitTypes;
			CvString szBuildingTypes;
			CvString szProjectTypes;
			FOR_EACH_ENUM(Unit)
			{
				appendSASGameRecordTypeCount(szUnitTypes, getSASGameRecordUnitType(eLoopUnit), kFlow.aiUnitTypes[eLoopUnit]);
				appendSASGameRecordTypeCount(szConscriptedUnitTypes, getSASGameRecordUnitType(eLoopUnit), kFlow.aiConscriptedUnitTypes[eLoopUnit]);
			}
			FOR_EACH_ENUM(Building)
				appendSASGameRecordTypeCount(szBuildingTypes, getSASGameRecordBuildingType(eLoopBuilding), kFlow.aiBuildingTypes[eLoopBuilding]);
			FOR_EACH_ENUM(Project)
				appendSASGameRecordTypeCount(szProjectTypes, getSASGameRecordProjectType(eLoopProject), kFlow.aiProjectTypes[eLoopProject]);
			logSASGameRecord("GAME_RECORD_PRODUCTION_FLOW turn=%d range=%d-%d player=%d unitsProduced=%d unitProductionNeeded=%d unitTypes=%s unitsConscripted=%d conscriptProductionNeeded=%d conscriptedUnitTypes=%s buildingsCompleted=%d buildingProductionNeeded=%d buildingTypes=%s projectsCompleted=%d projectProductionNeeded=%d projectTypes=%s overflowActions=%d rawModifiedOverflow=%d unmodifiedOverflow=%d keptOverflow=%d lostProduction=%d unusedOverflowCapacity=%d overflowGold=%d failedInvestedProduction=%d failGold=%d aiTargetSwitches=%d aiTargetClears=%d aiInvestedTargetChanges=%d aiProductionParked=%d aiTargetResumes=%d aiProductionResumed=%d aiTargetChangedCities=%d aiMaxTargetChangesOneCity=%d aiTargetTransitions=%s productionDecayActions=%d productionDecayLost=%d productionInvalidatedActions=%d productionInvalidatedLost=%d productionUpgradeTransfers=%d productionUpgradeTransferred=%d productionUpgradeOverwriteActions=%d productionUpgradeOverwritten=%d",
				iGameTurn, g_iSASGameRecordFlowStartTurn, iGameTurn, ePlayer, kFlow.iUnitsCompleted, kFlow.iUnitProductionNeeded, getSASDiagnosticOrDash(szUnitTypes).GetCString(), kFlow.iUnitsConscripted, kFlow.iConscriptProductionNeeded, getSASDiagnosticOrDash(szConscriptedUnitTypes).GetCString(),
				kFlow.iBuildingsCompleted, kFlow.iBuildingProductionNeeded, getSASDiagnosticOrDash(szBuildingTypes).GetCString(), kFlow.iProjectsCompleted, kFlow.iProjectProductionNeeded, getSASDiagnosticOrDash(szProjectTypes).GetCString(),
				kFlow.iOverflowActions, kFlow.iRawModifiedOverflow, kFlow.iUnmodifiedOverflow, kFlow.iKeptOverflow, kFlow.iLostProduction, kFlow.iUnusedOverflowCapacity, kFlow.iOverflowGold, kFlow.iFailedInvestedProduction, kFlow.iFailGold,
				kFlow.iAIProductionTargetSwitches, kFlow.iAIProductionTargetClears, kFlow.iAIProductionInvestedTargetChanges, kFlow.iAIProductionParked, kFlow.iAIProductionTargetResumes, kFlow.iAIProductionResumed, (int)kFlow.aAIProductionTargetChangesByCity.size(), getSASGameRecordMaxAIProductionTargetChangesOneCity(kFlow), getSASGameRecordAIProductionTransitions(kFlow).GetCString(),
				kFlow.iProductionDecayActions, kFlow.iProductionDecayLost, kFlow.iProductionInvalidatedActions, kFlow.iProductionInvalidatedLost, kFlow.iProductionUpgradeTransfers, kFlow.iProductionUpgradeTransferred, kFlow.iProductionUpgradeOverwriteActions, kFlow.iProductionUpgradeOverwritten);
		}
		if (kFlow.hasMilitary())
		{
			CvString szPromotionChoices;
			FOR_EACH_ENUM(Promotion)
				appendSASGameRecordTypeCount(szPromotionChoices, getSASGameRecordPromotionType(eLoopPromotion), kFlow.aiPromotionChoices[eLoopPromotion]);
			logSASGameRecord("GAME_RECORD_MILITARY_FLOW turn=%d range=%d-%d player=%d combatWins=%d combatLosses=%d cityPlotWins=%d cityPlotLosses=%d enemyProductionNeededDestroyed=%d ownProductionNeededLost=%d enemyXpDestroyed=%d ownXpLost=%d xpGained=%d combatXpGained=%d nonCombatXpGained=%d xpPreventedByCap=%d xpLostAdjustments=%d promotionsChosen=%d leaderPromotionApplications=%d promotionChoices=%s upgrades=%d upgradeGold=%d scrapped=%d scrappedProductionNeeded=%d captured=%d capturedProductionNeeded=%d",
				iGameTurn, g_iSASGameRecordFlowStartTurn, iGameTurn, ePlayer, kFlow.iCombatWins, kFlow.iCombatLosses, kFlow.iCityPlotWins, kFlow.iCityPlotLosses, kFlow.iEnemyProductionNeededDestroyed, kFlow.iOwnProductionNeededLost, kFlow.iEnemyExperienceDestroyed, kFlow.iOwnExperienceLost,
				kFlow.iExperienceGained, kFlow.iCombatExperienceGained, kFlow.iNonCombatExperienceGained, kFlow.iExperiencePreventedByCap, kFlow.iExperienceLostAdjustments, kFlow.iPromotionsChosen, kFlow.iLeaderPromotionApplications, getSASDiagnosticOrDash(szPromotionChoices).GetCString(),
				kFlow.iUpgrades, kFlow.iUpgradeGold, kFlow.iScrapped, kFlow.iScrappedProductionNeeded, kFlow.iCaptured, kFlow.iCapturedProductionNeeded);
		}
		if (kFlow.hasCityPopulationFlow())
		{
			logSASGameRecord("GAME_RECORD_CITY_POPULATION_FLOW turn=%d range=%d-%d player=%d growthEvents=%d populationGained=%d growthPreventedEvents=%d foodDiscardedByAvoidGrowth=%d starvationEvents=%d populationLost=%d netNaturalPopulationChange=%+d",
				iGameTurn, g_iSASGameRecordFlowStartTurn, iGameTurn, ePlayer, kFlow.iCityGrowthEvents, kFlow.iPopulationGainedFromGrowth, kFlow.iCityGrowthPreventedEvents, kFlow.iFoodDiscardedByAvoidGrowth,
				kFlow.iCityStarvationEvents, kFlow.iPopulationLostToStarvation, kFlow.iPopulationGainedFromGrowth - kFlow.iPopulationLostToStarvation);
		}
		kFlow.reset();
	}
	for (int iI = MAX_CIV_PLAYERS; iI < MAX_PLAYERS; iI++)
		g_akSASGameRecordPlayerFlow[iI].reset();
	g_iSASGameRecordFlowStartTurn = iGameTurn + 1;
}

// <!-- custom: Project completion rows did not show whether a project-based victory had its minimum/full component set or an active launch countdown. Build one compact shared state for periodic progress and the explicit launch action. (GPT-5.6-Sol) -->
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

static void logSASGameRecordTeamSnapshot(TeamTypes eTeam, int iGameTurn)
{
	CvGame const& kGame = GC.getGame();
	CvTeam const& kTeam = GET_TEAM(eTeam);
	bool const bLogTeamDetails = (gGameRecordLogLevel >= 2);
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
	// <!-- custom: Domination and Space already have detailed per-victory rows, and diplomatic vote-source rows already contain exact vote thresholds.
	// Add one compact general row per team rather than one new row per missing victory, so Score/Time, Conquest, and Cultural progress become explicit without multiplying snapshot noise.
	// Culture lists only the required number of leading cities. (GPT-5.6-Sol) -->
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

// <!-- custom: City health/happiness rows previously combined player-wide modifiers under `extra`, hiding whether a loaded-mod rule caused a demographic change; for example, AdvCiv-SAS's TECH_DEPOPULATION currently applies negative health and happiness.
// Reconstruct all currently defined trait, civic and technology contributions once per player snapshot; preserve any event or other DLL adjustment as OTHER. (GPT-5.6-Sol) -->
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

static CvString getSASGameRecordCommercePercents(CvPlayer const& kPlayer)
{
	CvString szList;
	FOR_EACH_ENUM(Commerce)
		appendSASGameRecordValue(szList, getSASGameRecordCommerceType(eLoopCommerce), kPlayer.getCommercePercent(eLoopCommerce));
	return getSASDiagnosticOrDash(szList);
}

static CvString getSASGameRecordCommerceRates(CvPlayer const& kPlayer)
{
	CvString szList;
	FOR_EACH_ENUM(Commerce)
		appendSASGameRecordValue(szList, getSASGameRecordCommerceType(eLoopCommerce), kPlayer.getCommerceRate(eLoopCommerce));
	return getSASDiagnosticOrDash(szList);
}

static CvString getSASGameRecordCommerceFlexible(CvPlayer const& kPlayer)
{
	CvString szList;
	FOR_EACH_ENUM(Commerce)
		appendSASGameRecordValue(szList, getSASGameRecordCommerceType(eLoopCommerce), kPlayer.isCommerceFlexible(eLoopCommerce));
	return getSASDiagnosticOrDash(szList);
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

static void logSASGameRecordStatistics(PlayerTypes ePlayer, int iGameTurn)
{
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	CvPlayerRecord const* pRecord = kPlayer.getPlayerRecord();
	const int iCitiesBuilt = (pRecord == NULL ? 0 : pRecord->getNumCitiesBuilt());
	const int iCitiesRazed = (pRecord == NULL ? 0 : pRecord->getNumCitiesRazed());
	// <!-- custom: Built/razed are persistent CyStatistics player-record values used by the Statistics tab.
	// Acquired/lost and all `logged*` military values are recorder-session observations, reset on load; keeping them local avoids save-format churn while exposing cumulative quality generation and combat luck alongside ordinary wins/losses. (GPT-5.5 + ChatGPT-5.6-Sol) -->
	SASGameRecordMilitaryQualityTotals const& kMilitary = g_akSASGameRecordMilitaryQualityTotals[ePlayer];
	SASGameRecordBattleQuality const& kBattleQuality = g_akSASGameRecordTotalBattleQuality[ePlayer];
	logSASGameRecord("GAME_RECORD_STATISTICS turn=%d player=%d currentCities=%d persistentCitiesBuilt=%d persistentCitiesRazed=%d loggedCitiesAcquired=%d loggedCitiesLost=%d loggedCitiesConquered=%d loggedCitiesLostByConquest=%d loggedCitiesTradedIn=%d loggedCitiesTradedOut=%d loggedCityNet=%+d loggedBattleWins=%d loggedBattleLosses=%d loggedCityBattleWins=%d loggedCityBattleLosses=%d loggedBattleNet=%+d loggedWithdrawals=%d loggedEnemyWithdrawals=%d loggedCombatLimitAttacks=%d loggedCombatLimitDefenses=%d loggedLuckEligibleBattles=%d loggedLuckEligibleWins=%d loggedExpectedWinsX1000=%d loggedLuckDeltaX1000=%+d loggedUpsetWins=%d loggedUpsetLosses=%d loggedLowestOddsWinPermille=%d loggedHighestOddsLossPermille=%d loggedXpGained=%d loggedCombatXpGained=%d loggedNonCombatXpGained=%d loggedXpPreventedByCap=%d loggedXpLostAdjustments=%d loggedPromotionsChosen=%d loggedLeaderPromotionApplications=%d loggedEnemyXpDestroyed=%d loggedOwnXpLost=%d",
			iGameTurn, ePlayer, kPlayer.getNumCities(), iCitiesBuilt, iCitiesRazed, g_aiSASGameRecordCitiesAcquired[ePlayer], g_aiSASGameRecordCitiesLost[ePlayer], g_aiSASGameRecordCitiesConquered[ePlayer], g_aiSASGameRecordCitiesLostByConquest[ePlayer], g_aiSASGameRecordCitiesTradedIn[ePlayer], g_aiSASGameRecordCitiesTradedOut[ePlayer], g_aiSASGameRecordCitiesAcquired[ePlayer] - g_aiSASGameRecordCitiesLost[ePlayer],
			g_aiSASGameRecordTotalBattleWins[ePlayer], g_aiSASGameRecordTotalBattleLosses[ePlayer], g_aiSASGameRecordTotalCityBattleWins[ePlayer], g_aiSASGameRecordTotalCityBattleLosses[ePlayer], g_aiSASGameRecordTotalBattleWins[ePlayer] - g_aiSASGameRecordTotalBattleLosses[ePlayer],
			kBattleQuality.iWithdrawals, kBattleQuality.iEnemyWithdrawals, kBattleQuality.iCombatLimitAttacks, kBattleQuality.iCombatLimitDefenses, kBattleQuality.iLuckEligibleBattles, kBattleQuality.iLuckEligibleWins, kBattleQuality.iExpectedWinsX1000, 1000 * kBattleQuality.iLuckEligibleWins - kBattleQuality.iExpectedWinsX1000, kBattleQuality.iUpsetWins, kBattleQuality.iUpsetLosses, kBattleQuality.iLowestOddsWinPermille, kBattleQuality.iHighestOddsLossPermille,
			kMilitary.iExperienceGained, kMilitary.iCombatExperienceGained, kMilitary.iNonCombatExperienceGained, kMilitary.iExperiencePreventedByCap, kMilitary.iExperienceLostAdjustments, kMilitary.iPromotionsChosen, kMilitary.iLeaderPromotionApplications, kMilitary.iEnemyExperienceDestroyed, kMilitary.iOwnExperienceLost);
}

static CvString getSASGameRecordEliminatedPlayers()
{
	CvString szList;
	for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		PlayerTypes const eLoopPlayer = (PlayerTypes)iI;
		CvPlayer const& kLoopPlayer = GET_PLAYER(eLoopPlayer);
		if (kLoopPlayer.isEverAlive() && !kLoopPlayer.isAlive() && !kLoopPlayer.isBarbarian())
			appendSASDiagnosticIntListValue(szList, eLoopPlayer);
	}
	return getSASDiagnosticOrDash(szList);
}

static PlayerTypes getSASGameRecordTopScorePlayer()
{
	PlayerTypes eBestPlayer = NO_PLAYER;
	int iBestScore = MIN_INT;
	for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		PlayerTypes const eLoopPlayer = (PlayerTypes)iI;
		CvPlayer const& kLoopPlayer = GET_PLAYER(eLoopPlayer);
		if (!kLoopPlayer.isAlive() || kLoopPlayer.isBarbarian())
			continue;
		int const iScore = kLoopPlayer.calculateScore();
		if (eBestPlayer == NO_PLAYER || iScore > iBestScore)
		{
			eBestPlayer = eLoopPlayer;
			iBestScore = iScore;
		}
	}
	return eBestPlayer;
}

static PlayerTypes getSASGameRecordTopPowerPlayer()
{
	PlayerTypes eBestPlayer = NO_PLAYER;
	int iBestPower = MIN_INT;
	for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		PlayerTypes const eLoopPlayer = (PlayerTypes)iI;
		CvPlayer const& kLoopPlayer = GET_PLAYER(eLoopPlayer);
		if (!kLoopPlayer.isAlive() || kLoopPlayer.isBarbarian())
			continue;
		int const iPower = kLoopPlayer.getPower();
		if (eBestPlayer == NO_PLAYER || iPower > iBestPower)
		{
			eBestPlayer = eLoopPlayer;
			iBestPower = iPower;
		}
	}
	return eBestPlayer;
}

void logSASGameRecordRunStatus(char const* szReason)
{
	// <!-- custom: CvGame::getNumHumanPlayers is not const in the Civ4 SDK headers, so this local game reference cannot be const. (GPT-5.5) -->
	CvGame& kGame = GC.getGame();
	PlayerTypes const eTopScorePlayer = getSASGameRecordTopScorePlayer();
	PlayerTypes const eTopPowerPlayer = getSASGameRecordTopPowerPlayer();
	// <!-- custom: Compact run-status row gives autoplay/LLM review a single parse-friendly checkpoint for who is alive, eliminated, leading by score, and leading by power. Victory already has its own action row; this row also works for ordinary stopped autoplays where no victory event fires. (GPT-5.5) -->
	logSASGameRecord("GAME_RECORD_RUN_STATUS turn=%d reason=%s elapsed=%d year=%d winnerTeam=%d victory=%s playersAlive=%d teamsAlive=%d playersEverAlive=%d humans=%d eliminatedPlayers=%s topScorePlayer=%d topScore=%d topPowerPlayer=%d topPower=%d totalCities=%d totalPopulation=%d",
			kGame.getGameTurn(), szReason == NULL ? "-" : szReason, kGame.getElapsedGameTurns(), kGame.getGameTurnYear(), kGame.getWinner(), kGame.getVictory() == NO_VICTORY ? "-" : GC.getInfo(kGame.getVictory()).getType(), kGame.countCivPlayersAlive(), kGame.countCivTeamsAlive(), kGame.countCivPlayersEverAlive(), kGame.getNumHumanPlayers(), getSASGameRecordEliminatedPlayers().GetCString(), eTopScorePlayer, eTopScorePlayer == NO_PLAYER ? 0 : GET_PLAYER(eTopScorePlayer).calculateScore(), eTopPowerPlayer, eTopPowerPlayer == NO_PLAYER ? 0 : GET_PLAYER(eTopPowerPlayer).getPower(), kGame.getNumCities(), kGame.getTotalPopulation());
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

// <!-- custom: Pairwise trade-market rows primarily mirror resolved information available through the active player's Foreign Advisor/diplomacy interface rather than speculative AI candidate reasoning.
// `NO_TALK` is kept separate from DenialTypes. Optional bonus GPT quotes and AI_techTradeVal fields add compact resolved valuation context under independent gates; they remain const observations and never submit an offer or alter trade state.
// All market observations use const query APIs, do not consume synchronized RNG, and do not mutate gameplay state. (ChatGPT-5.6-Sol) -->
static char const* getSASGameRecordDenialType(DenialTypes eDenial)
{
	return (eDenial == NO_DENIAL ? "NO_DENIAL" : GC.getInfo(eDenial).getType());
}

static void appendSASGameRecordMarketReason(CvString& szList, char const* szItem, char const* szReason)
{
	CvString szEntry;
	szEntry.Format(szList.empty() ? "%s=%s" : ",%s=%s", szItem, szReason);
	szList += szEntry;
}

static void appendSASGameRecordMarketValue(CvString& szList, char const* szItem, int iValue)
{
	CvString szEntry;
	szEntry.Format(szList.empty() ? "%s=%d" : ",%s=%d", szItem, iValue);
	szList += szEntry;
}

// <!-- custom: AI Auto Play temporarily changes the original human slot from isHuman() to isHumanDisabled(), so AI_goldForBonus's human-vs-AI assertion deliberately no longer applies even though a useful controller-to-controller market quote still exists.
// Reproduce AI_goldForBonus's AI-buyer conversion locally from its public valuation primitives instead of mutating human-control state or weakening the gameplay assertion.
// Ordinary human-vs-AI snapshots still call AI_goldForBonus directly and therefore retain exact Foreign Advisor hover semantics. (ChatGPT-5.6-Sol) -->
static int getSASGameRecordBonusGPTQuote(CvPlayerAI const& kBuyer, BonusTypes eBonus, PlayerTypes eSeller)
{
	CvPlayerAI const& kSeller = GET_PLAYER(eSeller);
	if (kBuyer.isHuman() != kSeller.isHuman())
		return kBuyer.AI_goldForBonus(eBonus, eSeller);
	if (kBuyer.isHuman())
		return 0; // <!-- custom: No AI quote is defined for human-vs-human trade. (ChatGPT-5.6-Sol) -->

	int const iTradeVal = kBuyer.AI_bonusTradeVal(eBonus, eSeller, 1);
	int const iMaxGPT = kBuyer.AI_maxGoldPerTurnTrade(eSeller);
	int iGPT = 0;
	while (iGPT < iMaxGPT && kBuyer.AI_goldPerTurnTradeVal(iGPT + 1) <= iTradeVal)
		iGPT++;
	return iGPT;
}

static char const* getSASGameRecordBonusUnavailableReason(CvPlayerAI const& kFrom, CvPlayerAI const& kTo, BonusTypes eBonus)
{
	CvTeam const& kFromTeam = GET_TEAM(kFrom.getTeam());
	CvTeam const& kToTeam = GET_TEAM(kTo.getTeam());
	if (kToTeam.isBonusObsolete(eBonus))
		return "RECIPIENT_OBSOLETE";
	if (kFromTeam.isBonusObsolete(eBonus))
		return "DONOR_OBSOLETE";
	if (kFrom.getNumTradeableBonuses(eBonus) <= 0)
		return "NO_TRADEABLE_COPY";
	return "OTHER";
}

static char const* getSASGameRecordTechUnavailableReason(CvPlayerAI const& kFrom, CvPlayerAI const& kTo, TechTypes eTech)
{
	if (!GC.getInfo(eTech).isTrade())
		return "XML_NOT_TRADEABLE";
	if (GET_TEAM(kFrom.getTeam()).isNoTradeTech(eTech))
		return "NO_TRADE_TECH";
	if (!kTo.canResearch(eTech, true))
		return "RECIPIENT_CANNOT_RESEARCH_VIA_TRADE";
	return "OTHER";
}

static void logSASGameRecordTradeMarket(int iGameTurn)
{
	if (!isSASGameRecordTradeMarketEnabled())
		return;
	CvGame const& kGame = GC.getGame();
	PlayerTypes const eViewer = kGame.getActivePlayer();
	if (eViewer == NO_PLAYER)
		return;
	CvPlayerAI const& kViewer = GET_PLAYER(eViewer);
	if (!kViewer.isAlive() || kViewer.isBarbarian() || kViewer.isMinorCiv())
		return;
	CvTeam const& kViewerTeam = GET_TEAM(kViewer.getTeam());
	bool const bBonusGPTQuotesEnabled = isSASGameRecordTradeMarketBonusGPTQuotesEnabled();
	bool const bAITechValuesEnabled = isSASGameRecordTradeMarketAITechValuesEnabled();

	for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		PlayerTypes const eOther = (PlayerTypes)iI;
		if (eOther == eViewer)
			continue;
		CvPlayerAI const& kOther = GET_PLAYER(eOther);
		if (!kOther.isAlive() || kOther.isBarbarian() || kOther.isMinorCiv())
			continue;
		CvTeam const& kOtherTeam = GET_TEAM(kOther.getTeam());
		if (!kViewerTeam.isHasMet(kOther.getTeam()))
			continue;

		bool const bOtherWillingToTalk = kOther.AI_isWillingToTalk(eViewer);
		bool const bTradeNetwork = kViewer.canTradeNetworkWith(eOther);
		bool const bGoldTrading = (kViewerTeam.isGoldTrading() || kOtherTeam.isGoldTrading());
		bool const bTechVisible = kViewer.canSeeTech(eOther);
		bool const bTechTradingPossible = kOther.canPossiblyTradeItem(eViewer, TRADE_TECHNOLOGIES);
		int const iOtherMaxGold = (bGoldTrading && bOtherWillingToTalk ? kOther.AI_maxGoldTrade(eViewer) : -1);
		int const iOtherMaxGPT = (bGoldTrading && bOtherWillingToTalk ? kOther.AI_maxGoldPerTurnTrade(eViewer) : -1);
		int const iViewerGPTBalance = kViewer.getGoldPerTurnByPlayer(eOther);
		char const* szBonusGPTQuoteMode = "DISABLED";
		if (bBonusGPTQuotesEnabled)
		{
			if (kViewer.isHuman() && !kOther.isHuman()) szBonusGPTQuoteMode = "FOREIGN_ADVISOR";
			else if (!kViewer.isHuman() && !kOther.isHuman()) szBonusGPTQuoteMode = (kViewer.isHumanDisabled() ? "AUTOPLAY_AI" : "AI_MARKET");
			else if (kViewer.isHuman() != kOther.isHuman()) szBonusGPTQuoteMode = "MIXED_AI_HUMAN";
			else szBonusGPTQuoteMode = "UNAVAILABLE";
		}

		CvString szViewerCanOfferBonuses;
		CvString szOtherBonusesForTrade;
		CvString szOtherBonusDenials;
		CvString szOtherBonusUnavailable;
		CvString szOtherBonusAskGPT;
		CvString szOtherBonusWillPayGPT;
		// <!-- custom: Resources cannot trade at all without a network, so keep that pair-wide fact in tradeNetwork instead of repeating NO_TRADE_NETWORK after every owned bonus. (ChatGPT-5.6-Sol) -->
		if (bTradeNetwork)
		{
			TradeData kBonusTrade(TRADE_RESOURCES, 0);
			TradeData const kOneGPTTrade(TRADE_GOLD_PER_TURN, 1);
			FOR_EACH_ENUM(Bonus)
			{
				int const iViewerAvailable = kViewer.getNumAvailableBonuses(eLoopBonus);
				int const iOtherAvailable = kOther.getNumAvailableBonuses(eLoopBonus);
				if (iViewerAvailable <= 0 && iOtherAvailable <= 0)
					continue;
				kBonusTrade.m_iData = eLoopBonus;
				if (iViewerAvailable > 0 && kViewer.canTradeItem(eOther, kBonusTrade, true))
				{
					appendSASGameRecordType(szViewerCanOfferBonuses, getSASGameRecordBonusType(eLoopBonus));
					// <!-- custom: Keep quote reconstruction independently optional because bonus trade valuation can perform substantial bonus/building work.
					// getSASGameRecordBonusGPTQuote preserves exact Foreign Advisor semantics in ordinary human play and switches to the current AI controller's equivalent quote during AI Auto Play. (ChatGPT-5.6-Sol) -->
					if (bBonusGPTQuotesEnabled && bOtherWillingToTalk && kOther.canTradeItem(eViewer, kOneGPTTrade, false))
					{
						int const iWillPayGPT = getSASGameRecordBonusGPTQuote(kOther, eLoopBonus, eViewer);
						if (iWillPayGPT > 0)
							appendSASGameRecordMarketValue(szOtherBonusWillPayGPT, getSASGameRecordBonusType(eLoopBonus), iWillPayGPT);
					}
				}

				if (iOtherAvailable <= 0)
					continue;
				if (!kOther.canTradeItem(eViewer, kBonusTrade, false))
				{
					appendSASGameRecordMarketReason(szOtherBonusUnavailable, getSASGameRecordBonusType(eLoopBonus), getSASGameRecordBonusUnavailableReason(kOther, kViewer, eLoopBonus));
					continue;
				}
				DenialTypes const eDenial = kOther.getTradeDenial(eViewer, kBonusTrade);
				if (eDenial != NO_DENIAL)
				{
					appendSASGameRecordMarketReason(szOtherBonusDenials, getSASGameRecordBonusType(eLoopBonus), getSASGameRecordDenialType(eDenial));
					continue;
				}
				if (!bOtherWillingToTalk)
				{
					appendSASGameRecordMarketReason(szOtherBonusDenials, getSASGameRecordBonusType(eLoopBonus), "NO_TALK");
					continue;
				}
				appendSASGameRecordType(szOtherBonusesForTrade, getSASGameRecordBonusType(eLoopBonus));
				if (bBonusGPTQuotesEnabled && kViewer.canTradeItem(eOther, kOneGPTTrade, false))
				{
					int const iAskGPT = getSASGameRecordBonusGPTQuote(kViewer, eLoopBonus, eOther);
					if (iAskGPT > 0)
						appendSASGameRecordMarketValue(szOtherBonusAskGPT, getSASGameRecordBonusType(eLoopBonus), iAskGPT);
				}
			}
		}

		CvString szViewerTechsForTrade;
		CvString szOtherTechsForTrade;
		CvString szOtherTechDenials;
		CvString szOtherTechUnavailable;
		CvString szViewerTechReceiveValues;
		CvString szOtherTechReceiveValues;
		// <!-- custom: canPossiblyTradeItem is pair-wide for technology trading; when false, techTradingPossible explains the whole tab and avoids per-tech work. (ChatGPT-5.6-Sol) -->
		if (bTechVisible && bTechTradingPossible)
		{
			TradeData kTechTrade(TRADE_TECHNOLOGIES, 0);
			FOR_EACH_ENUM(Tech)
			{
				bool const bViewerHasTech = kViewerTeam.isHasTech(eLoopTech);
				bool const bOtherHasTech = kOtherTeam.isHasTech(eLoopTech);
				if (bViewerHasTech == bOtherHasTech)
					continue;
				kTechTrade.m_iData = eLoopTech;
				if (bViewerHasTech)
				{
					if (kViewer.canTradeItem(eOther, kTechTrade, false))
					{
						if (bAITechValuesEnabled)
						{
							int const iReceiveValue = GET_TEAM(kOther.getTeam()).AI_techTradeVal( eLoopTech, kViewer.getTeam());
							appendSASGameRecordMarketValue(szOtherTechReceiveValues, getSASGameRecordTechType(eLoopTech), iReceiveValue);
						}
						if (kViewer.getTradeDenial(eOther, kTechTrade) == NO_DENIAL)
							appendSASGameRecordType(szViewerTechsForTrade, getSASGameRecordTechType(eLoopTech));
					}
					continue;
				}

				if (kOther.canTradeItem(eViewer, kTechTrade, false))
				{
					if (bAITechValuesEnabled)
					{
						int const iReceiveValue = GET_TEAM(kViewer.getTeam()).AI_techTradeVal( eLoopTech, kOther.getTeam());
						appendSASGameRecordMarketValue(szViewerTechReceiveValues, getSASGameRecordTechType(eLoopTech), iReceiveValue);
					}
					DenialTypes const eDenial = kOther.getTradeDenial(eViewer, kTechTrade);
					if (eDenial != NO_DENIAL)
						appendSASGameRecordMarketReason(szOtherTechDenials, getSASGameRecordTechType(eLoopTech), getSASGameRecordDenialType(eDenial));
					else if (!bOtherWillingToTalk)
						appendSASGameRecordMarketReason(szOtherTechDenials, getSASGameRecordTechType(eLoopTech), "NO_TALK");
					else appendSASGameRecordType(szOtherTechsForTrade, getSASGameRecordTechType(eLoopTech));
				}
				else if (kViewer.canResearch(eLoopTech, false))
				{
					appendSASGameRecordMarketReason(szOtherTechUnavailable, getSASGameRecordTechType(eLoopTech), getSASGameRecordTechUnavailableReason(kOther, kViewer, eLoopTech));
				}
			}
		}

		// <!-- custom: `viewerGPTBalanceWithOther` uses CvPlayer's signed pair balance: positive means the viewer currently receives GPT from this player, negative means the viewer pays them.
		// Accepted/ended deal rows remain the canonical item lifecycles; this snapshot supplies the current Foreign-Advisor-style market without duplicating every active deal. (ChatGPT-5.6-Sol) -->
		logSASGameRecord("GAME_RECORD_TRADE_MARKET turn=%d viewer=%d other=%d viewerTeam=%d otherTeam=%d otherWillingToTalk=%d tradeNetwork=%d goldTrading=%d otherMaxGold=%d otherMaxGPT=%d viewerGPTBalanceWithOther=%+d techVisible=%d techTradingPossible=%d bonusGPTQuotesEnabled=%d bonusGPTQuoteMode=%s aiTechValuesEnabled=%d viewerCanOfferBonuses=%s otherBonusesForTrade=%s otherBonusDenials=%s otherBonusUnavailable=%s otherBonusAskGPT=%s otherBonusWillPayGPT=%s viewerTechsForTrade=%s otherTechsForTrade=%s otherTechDenials=%s otherTechUnavailable=%s viewerTechReceiveValues=%s otherTechReceiveValues=%s",
				iGameTurn, eViewer, eOther, kViewer.getTeam(), kOther.getTeam(), bOtherWillingToTalk, bTradeNetwork, bGoldTrading, iOtherMaxGold, iOtherMaxGPT, iViewerGPTBalance, bTechVisible, bTechTradingPossible, bBonusGPTQuotesEnabled, szBonusGPTQuoteMode, bAITechValuesEnabled,
				getSASDiagnosticOrDash(szViewerCanOfferBonuses).GetCString(), getSASDiagnosticOrDash(szOtherBonusesForTrade).GetCString(), getSASDiagnosticOrDash(szOtherBonusDenials).GetCString(), getSASDiagnosticOrDash(szOtherBonusUnavailable).GetCString(),
				getSASDiagnosticOrDash(szOtherBonusAskGPT).GetCString(), getSASDiagnosticOrDash(szOtherBonusWillPayGPT).GetCString(), getSASDiagnosticOrDash(szViewerTechsForTrade).GetCString(), getSASDiagnosticOrDash(szOtherTechsForTrade).GetCString(), getSASDiagnosticOrDash(szOtherTechDenials).GetCString(), getSASDiagnosticOrDash(szOtherTechUnavailable).GetCString(),
				getSASDiagnosticOrDash(szViewerTechReceiveValues).GetCString(), getSASDiagnosticOrDash(szOtherTechReceiveValues).GetCString());
	}
}

static void logSASGameRecordEnvironment(int iGameTurn)
{
	CvMap const& kMap = GC.getMap();
	std::vector<int> aiFeatures(GC.getNumFeatureInfos(), 0);
	std::vector<int> aiNegativeHealthFeatures(GC.getNumFeatureInfos(), 0);
	int iOwnedLand = 0;
	int iUnownedLand = 0;
	for (int iI = 0; iI < kMap.numPlots(); iI++)
	{
		CvPlot const& kPlot = kMap.getPlotByIndex(iI);
		if (!kPlot.isWater())
		{
			if (kPlot.isOwned())
				iOwnedLand++;
			else iUnownedLand++;
		}
		FeatureTypes eFeature = kPlot.getFeatureType();
		if (eFeature != NO_FEATURE)
		{
			aiFeatures[eFeature]++;
			if (GC.getInfo(eFeature).getHealthPercent() < 0)
				aiNegativeHealthFeatures[eFeature]++;
		}
	}
	CvString szFeatures;
	CvString szNegativeHealthFeatures;
	FOR_EACH_ENUM(Feature)
	{
		appendSASGameRecordTypeCount(szFeatures, getSASGameRecordFeatureType(eLoopFeature), aiFeatures[eLoopFeature]);
		appendSASGameRecordTypeCount(szNegativeHealthFeatures, getSASGameRecordFeatureType(eLoopFeature), aiNegativeHealthFeatures[eLoopFeature]);
	}
	const int iGlobalWarmingIndex = GC.getGame().getGlobalWarmingIndex();
	const int iGlobalWarmingChances = GC.getGame().getGlobalWarmingChances();
	logSASGameRecord("GAME_RECORD_ENVIRONMENT turn=%d globalWarmingIndex=%d globalWarmingChances=%d land=%d water=%d ownedLand=%d unownedLand=%d negativeHealthFeatures=%s features=%s",
			iGameTurn, iGlobalWarmingIndex, iGlobalWarmingChances, kMap.getLandPlots(), kMap.getWaterPlots(), iOwnedLand, iUnownedLand, getSASDiagnosticOrDash(szNegativeHealthFeatures).GetCString(), getSASDiagnosticOrDash(szFeatures).GetCString());
	logSASGameRecord("GAME_RECORD_ENVIRONMENT_DELTAS turn=%d deltaValid=%d globalWarmingIndexDelta=%+d globalWarmingChancesDelta=%+d ownedLandDelta=%+d unownedLandDelta=%+d",
			iGameTurn, g_kSASGameRecordGlobalPrevious.bValid,
			getSASGameRecordDelta(g_kSASGameRecordGlobalPrevious.bValid, iGlobalWarmingIndex, g_kSASGameRecordGlobalPrevious.iGlobalWarmingIndex),
			getSASGameRecordDelta(g_kSASGameRecordGlobalPrevious.bValid, iGlobalWarmingChances, g_kSASGameRecordGlobalPrevious.iGlobalWarmingChances),
			getSASGameRecordDelta(g_kSASGameRecordGlobalPrevious.bValid, iOwnedLand, g_kSASGameRecordGlobalPrevious.iOwnedLand),
			getSASGameRecordDelta(g_kSASGameRecordGlobalPrevious.bValid, iUnownedLand, g_kSASGameRecordGlobalPrevious.iUnownedLand));
	g_kSASGameRecordGlobalPrevious.bValid = true;
	g_kSASGameRecordGlobalPrevious.iGlobalWarmingIndex = iGlobalWarmingIndex;
	g_kSASGameRecordGlobalPrevious.iGlobalWarmingChances = iGlobalWarmingChances;
	g_kSASGameRecordGlobalPrevious.iOwnedLand = iOwnedLand;
	g_kSASGameRecordGlobalPrevious.iUnownedLand = iUnownedLand;
}

static void logSASGameRecordVoteSources(int iGameTurn)
{
	CvGame const& kGame = GC.getGame();
	FOR_EACH_ENUM(VoteSource)
	{
		CvCity const* pSourceCity = kGame.getVoteSourceCity(eLoopVoteSource, NO_TEAM, true);
		ReligionTypes eReligion = kGame.getVoteSourceReligion(eLoopVoteSource);
		TeamTypes eSecretary = kGame.getSecretaryGeneral(eLoopVoteSource);
		CvString szVotingTeams;
		CvString szFullTeams;
		CvString szVotes;
		for (int iI = 0; iI < MAX_CIV_TEAMS; iI++)
		{
			TeamTypes eLoopTeam = (TeamTypes)iI;
			CvTeam const& kLoopTeam = GET_TEAM(eLoopTeam);
			if (!kLoopTeam.isAlive() || kLoopTeam.isBarbarian())
				continue;
			if (kLoopTeam.isVotingMember(eLoopVoteSource))
				appendSASDiagnosticIntListValue(szVotingTeams, eLoopTeam);
			if (kLoopTeam.isFullMember(eLoopVoteSource))
				appendSASDiagnosticIntListValue(szFullTeams, eLoopTeam);
			const int iVotes = kLoopTeam.getVotes(NO_VOTE, eLoopVoteSource);
			if (iVotes > 0)
			{
				CvString szItem;
				szItem.Format(szVotes.empty() ? "%d:%d" : ",%d:%d", eLoopTeam, iVotes);
				szVotes += szItem;
			}
		}
		CvString szVictoryVotes;
		FOR_EACH_ENUM(Vote)
		{
			if (!GC.getInfo(eLoopVote).isVoteSourceType(eLoopVoteSource) || !GC.getInfo(eLoopVote).isVictory())
				continue;
			CvString szItem;
			szItem.Format(szVictoryVotes.empty() ? "%s:required=%d,possible=%d" : ",%s:required=%d,possible=%d", getSASGameRecordVoteType(eLoopVote), kGame.getVoteRequired(eLoopVote, eLoopVoteSource), kGame.countPossibleVote(eLoopVote, eLoopVoteSource));
			szVictoryVotes += szItem;
		}
		if (pSourceCity == NULL && eReligion == NO_RELIGION && eSecretary == NO_TEAM && szVotingTeams.empty() && szVictoryVotes.empty())
			continue;
		logSASGameRecord("GAME_RECORD_DIPLO_VOTE_SOURCE turn=%d source=%s secretaryTeam=%d secretaryTimer=%d voteTimer=%d religion=%s sourceOwner=%d sourceCityId=%d sourceCity=%S sourceX=%d sourceY=%d votingTeams=%s fullTeams=%s votes=%s victoryVotes=%s",
			iGameTurn, getSASGameRecordVoteSourceType(eLoopVoteSource), eSecretary, kGame.getSecretaryGeneralTimer(eLoopVoteSource), kGame.getVoteTimer(eLoopVoteSource), getSASGameRecordReligionType(eReligion),
			pSourceCity == NULL ? -1 : pSourceCity->getOwner(), pSourceCity == NULL ? -1 : pSourceCity->getID(), getSASGameRecordQuotedCityName(pSourceCity).GetCString(), pSourceCity == NULL ? -1 : pSourceCity->getX(), pSourceCity == NULL ? -1 : pSourceCity->getY(),
			getSASDiagnosticOrDash(szVotingTeams).GetCString(), getSASDiagnosticOrDash(szFullTeams).GetCString(), getSASDiagnosticOrDash(szVotes).GetCString(), getSASDiagnosticOrDash(szVictoryVotes).GetCString());
	}
}

static void logSASGameRecordTeamProjects(TeamTypes eTeam, int iGameTurn)
{
	CvString szProjects;
	FOR_EACH_ENUM(Project)
		appendSASGameRecordTypeCount(szProjects, getSASGameRecordProjectType(eLoopProject), GET_TEAM(eTeam).getProjectCount(eLoopProject));
	if (!szProjects.empty())
		logSASGameRecord("GAME_RECORD_TEAM_PROJECTS turn=%d team=%d projects=%s", iGameTurn, eTeam, szProjects.GetCString());
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

static void logSASGameRecordExpansion(PlayerTypes ePlayer, int iGameTurn)
{
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	const TeamTypes eTeam = kPlayer.getTeam();
	static const ImprovementTypes eFarm = (ImprovementTypes)GC.getInfoTypeForString("IMPROVEMENT_FARM");
	int iRevealedLand = 0;
	int iVisibleLand = 0;
	int iRevealedUnownedLand = 0;
	int iVisibleUnownedLand = 0;
	int iRevealedForeignLand = 0;
	int iVisibleForeignLand = 0;
	int iRevealedOtherTeamLand = 0;
	int iVisibleOtherTeamLand = 0;
	CvCity const* pCapital = kPlayer.getCapitalCity();
	int const iCapitalArea = (pCapital == NULL ? -1 : pCapital->getArea().getID());
	int iNearestRevealedOtherTeamLandDistance = -1;
	int iNearestRevealedEnemyLandDistance = -1;
	SASGameRecordTerritoryDevelopment kTerritoryDevelopment;
	CvMap const& kMap = GC.getMap();
	for (int iI = 0; iI < kMap.numPlots(); iI++)
	{
		CvPlot const& kPlot = kMap.getPlotByIndex(iI);
		if (kPlot.getOwner() == ePlayer)
			addSASGameRecordTerritoryDevelopment(kTerritoryDevelopment, kPlot, ePlayer, eTeam, eFarm);
		if (kPlot.isWater())
			continue;
		if (kPlot.isRevealed(eTeam, false))
		{
			iRevealedLand++;
			if (kPlot.getOwner() == NO_PLAYER)
				iRevealedUnownedLand++;
			else if (kPlot.getOwner() != ePlayer)
				iRevealedForeignLand++;
			if (kPlot.getTeam() != NO_TEAM && kPlot.getTeam() != eTeam)
			{
				iRevealedOtherTeamLand++;
				if (pCapital != NULL)
				{
					int const iDistance = plotDistance(pCapital->getX(), pCapital->getY(), kPlot.getX(), kPlot.getY());
					iNearestRevealedOtherTeamLandDistance = (iNearestRevealedOtherTeamLandDistance < 0 ? iDistance : std::min(iNearestRevealedOtherTeamLandDistance, iDistance));
					if (GET_TEAM(eTeam).isAtWar(kPlot.getTeam()))
						iNearestRevealedEnemyLandDistance = (iNearestRevealedEnemyLandDistance < 0 ? iDistance : std::min(iNearestRevealedEnemyLandDistance, iDistance));
				}
			}
		}
		if (kPlot.isVisible(eTeam, false))
		{
			iVisibleLand++;
			if (kPlot.getOwner() == NO_PLAYER)
				iVisibleUnownedLand++;
			else if (kPlot.getOwner() != ePlayer)
				iVisibleForeignLand++;
			if (kPlot.getTeam() != NO_TEAM && kPlot.getTeam() != eTeam)
				iVisibleOtherTeamLand++;
		}
	}
	int iCitiesProducingSettlers = 0;
	int iCityLoop = 0;
	for (CvCity const* pLoopCity = kPlayer.firstCity(&iCityLoop); pLoopCity != NULL; pLoopCity = kPlayer.nextCity(&iCityLoop))
	{
		UnitTypes eProductionUnit = pLoopCity->getProductionUnit();
		if (eProductionUnit != NO_UNIT && GC.getInfo(eProductionUnit).getDefaultUnitAIType() == UNITAI_SETTLE)
			iCitiesProducingSettlers++;
	}
	int iSettlers = 0;
	int iFoundMission = 0;
	int iNearestSettlerCityDistance = -1;
	int iTotalSettlerCityDistance = 0;
	int iSettlersWithCityDistance = 0;
	int iUnitLoop = 0;
	for (CvUnit const* pLoopUnit = kPlayer.firstUnit(&iUnitLoop); pLoopUnit != NULL; pLoopUnit = kPlayer.nextUnit(&iUnitLoop))
	{
		if (!isSASGameRecordSettlerUnit(*pLoopUnit))
			continue;
		iSettlers++;
		if (getSASGameRecordUnitMissionType(*pLoopUnit) == MISSION_FOUND)
			iFoundMission++;
		CvCity const* pNearestCity = kMap.findCity(pLoopUnit->getX(), pLoopUnit->getY(), ePlayer, NO_TEAM, false);
		if (pNearestCity == NULL)
			continue;
		const int iDistance = plotDistance(pLoopUnit->getX(), pLoopUnit->getY(), pNearestCity->getX(), pNearestCity->getY());
		iNearestSettlerCityDistance = (iNearestSettlerCityDistance < 0 ? iDistance : std::min(iNearestSettlerCityDistance, iDistance));
		iTotalSettlerCityDistance += iDistance;
		iSettlersWithCityDistance++;
	}
	const int iAvgSettlerCityDistanceX100 = (iSettlersWithCityDistance == 0 ? -1 : (100 * iTotalSettlerCityDistance) / iSettlersWithCityDistance);

	// <!-- custom: Add a knowledge-limited rival-distance complement to the already map-wide revealed land counts.
	// Reuse the existing plot scan for nearest revealed other-team/enemy territory and measure cities only from the current capital, avoiding an own-city x foreign-city scan; capital-area counts separately expose same-landmass competition. (ChatGPT-5.6-Sol) -->
	int iMetForeignPlayers = 0;
	int iForeignPlayersWithKnownCities = 0;
	int iKnownForeignCities = 0;
	int iCapitalAreaForeignPlayersWithKnownCities = 0;
	int iCapitalAreaKnownForeignCities = 0;
	PlayerTypes eNearestKnownForeignCityPlayer = NO_PLAYER;
	int iNearestKnownForeignCityId = -1;
	int iNearestKnownForeignCityDistance = -1;
	PlayerTypes eNearestCapitalAreaKnownForeignCityPlayer = NO_PLAYER;
	int iNearestCapitalAreaKnownForeignCityId = -1;
	int iNearestCapitalAreaKnownForeignCityDistance = -1;
	int iWarEnemyPlayers = 0;
	int iEnemyPlayersWithKnownCities = 0;
	int iKnownEnemyCities = 0;
	PlayerTypes eNearestKnownEnemyCityPlayer = NO_PLAYER;
	int iNearestKnownEnemyCityId = -1;
	int iNearestKnownEnemyCityDistance = -1;
	for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		PlayerTypes const eOtherPlayer = (PlayerTypes)iI;
		CvPlayer const& kOtherPlayer = GET_PLAYER(eOtherPlayer);
		if (!kOtherPlayer.isAlive() || kOtherPlayer.isBarbarian() || kOtherPlayer.getTeam() == eTeam)
			continue;
		bool const bMet = GET_TEAM(eTeam).isHasMet(kOtherPlayer.getTeam());
		bool const bEnemy = GET_TEAM(eTeam).isAtWar(kOtherPlayer.getTeam());
		if (bMet)
			iMetForeignPlayers++;
		if (bEnemy)
			iWarEnemyPlayers++;
		bool bHasKnownCity = false;
		bool bHasKnownCapitalAreaCity = false;
		bool bEnemyHasKnownCity = false;
		int iOtherCityLoop = 0;
		for (CvCity const* pOtherCity = kOtherPlayer.firstCity(&iOtherCityLoop); pOtherCity != NULL; pOtherCity = kOtherPlayer.nextCity(&iOtherCityLoop))
		{
			if (!pOtherCity->isRevealed(eTeam))
				continue;
			bHasKnownCity = true;
			iKnownForeignCities++;
			if (iCapitalArea >= 0 && pOtherCity->getArea().getID() == iCapitalArea)
			{
				bHasKnownCapitalAreaCity = true;
				iCapitalAreaKnownForeignCities++;
				if (pCapital != NULL)
				{
					int const iCapitalAreaDistance = plotDistance(pCapital->getX(), pCapital->getY(), pOtherCity->getX(), pOtherCity->getY());
					if (iNearestCapitalAreaKnownForeignCityDistance < 0 || iCapitalAreaDistance < iNearestCapitalAreaKnownForeignCityDistance)
					{
						iNearestCapitalAreaKnownForeignCityDistance = iCapitalAreaDistance;
						eNearestCapitalAreaKnownForeignCityPlayer = eOtherPlayer;
						iNearestCapitalAreaKnownForeignCityId = pOtherCity->getID();
					}
				}
			}
			if (bEnemy)
			{
				bEnemyHasKnownCity = true;
				iKnownEnemyCities++;
			}
			if (pCapital == NULL)
				continue;
			int const iDistance = plotDistance(pCapital->getX(), pCapital->getY(), pOtherCity->getX(), pOtherCity->getY());
			if (iNearestKnownForeignCityDistance < 0 || iDistance < iNearestKnownForeignCityDistance)
			{
				iNearestKnownForeignCityDistance = iDistance;
				eNearestKnownForeignCityPlayer = eOtherPlayer;
				iNearestKnownForeignCityId = pOtherCity->getID();
			}
			if (bEnemy && (iNearestKnownEnemyCityDistance < 0 || iDistance < iNearestKnownEnemyCityDistance))
			{
				iNearestKnownEnemyCityDistance = iDistance;
				eNearestKnownEnemyCityPlayer = eOtherPlayer;
				iNearestKnownEnemyCityId = pOtherCity->getID();
			}
		}
		if (bHasKnownCity)
			iForeignPlayersWithKnownCities++;
		if (bHasKnownCapitalAreaCity)
			iCapitalAreaForeignPlayersWithKnownCities++;
		if (bEnemyHasKnownCity)
			iEnemyPlayersWithKnownCities++;
	}
	logSASGameRecord("GAME_RECORD_EXPANSION turn=%d player=%d cities=%d targetCities=%d ownedLand=%d revealedLand=%d visibleLand=%d revealedUnownedLand=%d visibleUnownedLand=%d revealedForeignLand=%d visibleForeignLand=%d revealedOtherTeamLand=%d visibleOtherTeamLand=%d settlers=%d foundMission=%d citiesProducingSettlers=%d nearestSettlerCityDistance=%d avgSettlerCityDistanceX100=%d capitalArea=%d nearestRevealedOtherTeamLandFromCapitalDistance=%d nearestRevealedEnemyLandFromCapitalDistance=%d metForeignPlayers=%d foreignPlayersWithKnownCities=%d knownForeignCities=%d capitalAreaForeignPlayersWithKnownCities=%d capitalAreaKnownForeignCities=%d nearestKnownForeignCityPlayer=%d nearestKnownForeignCityId=%d nearestKnownForeignCityFromCapitalDistance=%d nearestCapitalAreaKnownForeignCityPlayer=%d nearestCapitalAreaKnownForeignCityId=%d nearestCapitalAreaKnownForeignCityFromCapitalDistance=%d warEnemyPlayers=%d enemyPlayersWithKnownCities=%d knownEnemyCities=%d nearestKnownEnemyCityPlayer=%d nearestKnownEnemyCityId=%d nearestKnownEnemyCityFromCapitalDistance=%d",
			iGameTurn, ePlayer, kPlayer.getNumCities(), GC.getInfo(kMap.getWorldSize()).getTargetNumCities(), kPlayer.getTotalLand(), iRevealedLand, iVisibleLand, iRevealedUnownedLand, iVisibleUnownedLand, iRevealedForeignLand, iVisibleForeignLand, iRevealedOtherTeamLand, iVisibleOtherTeamLand,
			iSettlers, iFoundMission, iCitiesProducingSettlers, iNearestSettlerCityDistance, iAvgSettlerCityDistanceX100, iCapitalArea, iNearestRevealedOtherTeamLandDistance, iNearestRevealedEnemyLandDistance,
			iMetForeignPlayers, iForeignPlayersWithKnownCities, iKnownForeignCities, iCapitalAreaForeignPlayersWithKnownCities, iCapitalAreaKnownForeignCities,
			eNearestKnownForeignCityPlayer, iNearestKnownForeignCityId, iNearestKnownForeignCityDistance, eNearestCapitalAreaKnownForeignCityPlayer, iNearestCapitalAreaKnownForeignCityId, iNearestCapitalAreaKnownForeignCityDistance,
			iWarEnemyPlayers, iEnemyPlayersWithKnownCities, iKnownEnemyCities, eNearestKnownEnemyCityPlayer, iNearestKnownEnemyCityId, iNearestKnownEnemyCityDistance);
	logSASGameRecordTerritoryDevelopment(ePlayer, iGameTurn, kTerritoryDevelopment);
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

// <!-- custom: Production churn is about the active head target, not every queued order mutation.
// Keep a compact recorder-only representation that survives AI_chooseProduction clearing/rebuilding its queue and distinguishes Wonders from ordinary buildings. (ChatGPT-5.6-Sol) -->
static int getSASGameRecordProductionKindIndex(OrderTypes eOrder, int iData1)
{
	switch (eOrder)
	{
	case ORDER_TRAIN: return SAS_PRODUCTION_UNIT;
	case ORDER_CONSTRUCT:
		return (iData1 >= 0 && iData1 < GC.getNumBuildingInfos() && GC.getInfo((BuildingTypes)iData1).isLimited() ? SAS_PRODUCTION_WONDER : SAS_PRODUCTION_BUILDING);
	case ORDER_CREATE: return SAS_PRODUCTION_PROJECT;
	case ORDER_MAINTAIN: return SAS_PRODUCTION_PROCESS;
	default: return -1;
	}
}

static const char* getSASGameRecordProductionKind(OrderTypes eOrder, int iData1)
{
	static char const* const aszKinds[NUM_SAS_PRODUCTION_KINDS] = {"UNIT", "BUILDING", "WONDER", "PROJECT", "PROCESS"};
	int const iKind = getSASGameRecordProductionKindIndex(eOrder, iData1);
	return (iKind < 0 ? "-" : aszKinds[iKind]);
}

static const char* getSASGameRecordProductionType(OrderTypes eOrder, int iData1)
{
	switch (eOrder)
	{
	case ORDER_TRAIN: return (iData1 >= 0 && iData1 < GC.getNumUnitInfos() ? getSASGameRecordUnitType((UnitTypes)iData1) : "-");
	case ORDER_CONSTRUCT: return (iData1 >= 0 && iData1 < GC.getNumBuildingInfos() ? getSASGameRecordBuildingType((BuildingTypes)iData1) : "-");
	case ORDER_CREATE: return (iData1 >= 0 && iData1 < GC.getNumProjectInfos() ? getSASGameRecordProjectType((ProjectTypes)iData1) : "-");
	case ORDER_MAINTAIN: return (iData1 >= 0 && iData1 < GC.getNumProcessInfos() ? getSASGameRecordProcessType((ProcessTypes)iData1) : "-");
	default: return "-";
	}
}

static void captureSASGameRecordProductionTarget(CvCity const& kCity, OrderTypes& eOrder, int& iData1, int& iStored, int& iNeeded, int& iTurnsLeft, int& iAccumulatedInactiveTurns)
{
	OrderData const kOrder = kCity.getOrderData(0);
	eOrder = kOrder.eOrderType;
	iData1 = kOrder.iData1;
	iStored = 0;
	iNeeded = 0;
	iTurnsLeft = -1;
	iAccumulatedInactiveTurns = -1;
	switch (eOrder)
	{
	case ORDER_TRAIN:
		iStored = kCity.getUnitProduction((UnitTypes)iData1);
		iNeeded = kCity.getProductionNeeded((UnitTypes)iData1);
		iTurnsLeft = kCity.getProductionTurnsLeft();
		if (iTurnsLeft == MAX_INT)
			iTurnsLeft = -1;
		iAccumulatedInactiveTurns = kCity.getUnitProductionTime((UnitTypes)iData1);
		break;
	case ORDER_CONSTRUCT:
		iStored = kCity.getBuildingProduction((BuildingTypes)iData1);
		iNeeded = kCity.getProductionNeeded((BuildingTypes)iData1);
		iTurnsLeft = kCity.getProductionTurnsLeft();
		if (iTurnsLeft == MAX_INT)
			iTurnsLeft = -1;
		iAccumulatedInactiveTurns = kCity.getBuildingProductionTime((BuildingTypes)iData1);
		break;
	case ORDER_CREATE:
		iStored = kCity.getProjectProduction((ProjectTypes)iData1);
		iNeeded = kCity.getProductionNeeded((ProjectTypes)iData1);
		iTurnsLeft = kCity.getProductionTurnsLeft();
		if (iTurnsLeft == MAX_INT)
			iTurnsLeft = -1;
		break;
	case ORDER_MAINTAIN:
		break;
	default:
		eOrder = NO_ORDER;
		iData1 = -1;
		break;
	}
}

static void noteSASGameRecordAIProductionTargetChangedCity(SASGameRecordPlayerFlow& kFlow, int iCityId)
{
	for (size_t iI = 0; iI < kFlow.aAIProductionTargetChangesByCity.size(); iI++)
	{
		if (kFlow.aAIProductionTargetChangesByCity[iI].first == iCityId)
		{
			kFlow.aAIProductionTargetChangesByCity[iI].second++;
			return;
		}
	}
	kFlow.aAIProductionTargetChangesByCity.push_back(std::make_pair(iCityId, 1));
}

// <!-- custom: Cold enabled path for the header-inline RAII wrapper. Keeping capture/finalization out of the header avoids expanding every CvCityAI includer while the disabled level-0/1 path remains tiny. (ChatGPT-5.6-Sol) -->
void SASGameRecordAIProductionChoiceScope::begin(CvCity const& kCity)
{
	m_pCity = &kCity;
	captureSASGameRecordProductionTarget(kCity, m_eOldOrder, m_iOldData1, m_iOldStored, m_iOldNeeded, m_iOldTurnsLeft, m_iOldAccumulatedInactiveTurns);
}

// <!-- custom: Called only when begin() armed m_pCity; the inline destructor retains the null guard, so this function can assume a valid enabled scope and perform the level-2+ final comparison directly. (ChatGPT-5.6-Sol) -->
void SASGameRecordAIProductionChoiceScope::end()
{
	OrderTypes eNewOrder = NO_ORDER;
	int iNewData1 = -1;
	int iNewStored = 0;
	int iNewNeeded = 0;
	int iNewTurnsLeft = -1;
	int iNewAccumulatedInactiveTurns = -1;
	captureSASGameRecordProductionTarget(*m_pCity, eNewOrder, iNewData1, iNewStored, iNewNeeded, iNewTurnsLeft, iNewAccumulatedInactiveTurns);
	bool const bSameTarget = (m_eOldOrder == eNewOrder && m_iOldData1 == iNewData1);
	if (bSameTarget)
		return;
	bool const bOldTarget = (m_eOldOrder != NO_ORDER);
	bool const bNewTarget = (eNewOrder != NO_ORDER);
	bool const bResume = (bNewTarget && iNewStored > 0);
	// <!-- custom: Normal completion -> fresh next target is not churn. Preserve only a real active-target change or selection that resumes previously stored production. (ChatGPT-5.6-Sol) -->
	if (!bOldTarget && !bResume)
		return;
	PlayerTypes const ePlayer = m_pCity->getOwner();
	if (ePlayer < 0 || ePlayer >= MAX_CIV_PLAYERS)
		return;
	SASGameRecordPlayerFlow& kFlow = g_akSASGameRecordPlayerFlow[ePlayer];
	if (bOldTarget)
	{
		if (bNewTarget)
			kFlow.iAIProductionTargetSwitches++;
		else kFlow.iAIProductionTargetClears++;
		if (m_iOldStored > 0)
		{
			kFlow.iAIProductionInvestedTargetChanges++;
			kFlow.iAIProductionParked += m_iOldStored;
		}
		noteSASGameRecordAIProductionTargetChangedCity(kFlow, m_pCity->getID());
		int const iOldKind = getSASGameRecordProductionKindIndex(m_eOldOrder, m_iOldData1);
		int const iNewKind = getSASGameRecordProductionKindIndex(eNewOrder, iNewData1);
		if (iOldKind >= 0 && iNewKind >= 0)
			kFlow.aiAIProductionTransitions[iOldKind * NUM_SAS_PRODUCTION_KINDS + iNewKind]++;
	}
	if (bResume)
	{
		kFlow.iAIProductionTargetResumes++;
		kFlow.iAIProductionResumed += iNewStored;
	}
	if (gGameRecordLogLevel >= 3)
	{
		char const* szChange = (bOldTarget ? (bNewTarget ? "SWITCH" : "CLEAR") : "RESUME");
		logSASGameRecord("GAME_RECORD_ACTION turn=%d type=AI_PRODUCTION_TARGET_CHANGED player=%d cityId=%d city=%S change=%s oldKind=%s oldTarget=%s oldStored=%d oldNeeded=%d oldTurnsLeft=%d oldAccumulatedInactiveTurns=%d newKind=%s newTarget=%s newStored=%d newNeeded=%d newTurnsLeft=%d newAccumulatedInactiveTurns=%d oldStoredParked=%d newStoredResumed=%d",
			GC.getGame().getGameTurn(), ePlayer, m_pCity->getID(), getSASGameRecordQuotedCityName(m_pCity).GetCString(), szChange,
			getSASGameRecordProductionKind(m_eOldOrder, m_iOldData1), getSASGameRecordProductionType(m_eOldOrder, m_iOldData1), m_iOldStored, m_iOldNeeded, m_iOldTurnsLeft, m_iOldAccumulatedInactiveTurns,
			getSASGameRecordProductionKind(eNewOrder, iNewData1), getSASGameRecordProductionType(eNewOrder, iNewData1), iNewStored, iNewNeeded, iNewTurnsLeft, iNewAccumulatedInactiveTurns, m_iOldStored > 0 ? m_iOldStored : 0, bResume ? iNewStored : 0);
	}
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

// <!-- custom: Settler unit-state helpers for event-based expansion diagnostics. Keep game-record rows descriptive: raw unit counts, visible enemy counts and combat/founding context, while BBAI logs carry the heavier AI-decision reasons. No gameplay behavior change. (ChatGPT-5.5) -->
struct SASGameRecordPlotUnitCounts
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
	CvUnit const* pBestDefender;
	CvUnit const* pFirstSettler;
	SASGameRecordPlotUnitCounts() : iUnits(0), iMilitaryUnits(0), iCivilianUnits(0), iDefenders(0), iHealthyDefenders(0), iWoundedDefenders(0), iSettlers(0), iWorkers(0), iAttackers(0), pBestDefender(NULL), pFirstSettler(NULL) {}
};

static void collectSASGameRecordPlotUnitCounts(CvPlot const& kPlot, PlayerTypes ePlayer, SASGameRecordPlotUnitCounts& kCounts)
{
	for (CLLNode<IDInfo> const* pUnitNode = kPlot.headUnitNode(); pUnitNode != NULL; pUnitNode = kPlot.nextUnitNode(pUnitNode))
	{
		CvUnit const* pLoopUnit = ::getUnit(pUnitNode->m_data);
		if (pLoopUnit == NULL || pLoopUnit->getOwner() != ePlayer)
			continue;
		kCounts.iUnits++;
		if (pLoopUnit->baseCombatStr() > 0 || pLoopUnit->canAttack() || pLoopUnit->canDefend(&kPlot))
			kCounts.iMilitaryUnits++;
		else kCounts.iCivilianUnits++;
		if (isSASGameRecordSettlerUnit(*pLoopUnit))
		{
			kCounts.iSettlers++;
			if (kCounts.pFirstSettler == NULL)
				kCounts.pFirstSettler = pLoopUnit;
		}
		if (isSASGameRecordWorkerUnit(*pLoopUnit))
			kCounts.iWorkers++;
		if (pLoopUnit->canAttack())
			kCounts.iAttackers++;
		if (pLoopUnit->canDefend(&kPlot))
		{
			kCounts.iDefenders++;
			if (pLoopUnit->getDamage() <= 25)
				kCounts.iHealthyDefenders++;
			else kCounts.iWoundedDefenders++;
			if (kCounts.pBestDefender == NULL || pLoopUnit->baseCombatStr() > kCounts.pBestDefender->baseCombatStr() || (pLoopUnit->baseCombatStr() == kCounts.pBestDefender->baseCombatStr() && pLoopUnit->getDamage() < kCounts.pBestDefender->getDamage()))
				kCounts.pBestDefender = pLoopUnit;
		}
	}
}


static void logSASGameRecordWorkedPlots(PlayerTypes ePlayer, int iGameTurn)
{
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	SASGameRecordPlotComposition kComposition;
	int iLoop = 0;
	for (CvCity const* pLoopCity = kPlayer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kPlayer.nextCity(&iLoop))
		addSASGameRecordPlotComposition(kComposition, getSASGameRecordWorkedPlotComposition(*pLoopCity));
	CvString szTerrains;
	CvString szFeatures;
	CvString szBonuses;
	CvString szImprovements;
	CvString szRoutes;
	getSASGameRecordPlotCompositionTypes(kComposition, szTerrains, szFeatures, szBonuses, szImprovements, szRoutes);
	logSASGameRecord("GAME_RECORD_WORKED_PLOTS turn=%d player=%d cities=%d worked=%d improved=%d unimproved=%d land=%d water=%d hills=%d riverSide=%d freshWater=%d bonusImproved=%d bonusUnimproved=%d currentFood=%d currentProd=%d currentCommerce=%d natureFood=%d natureProd=%d natureCommerce=%d terrains=%s features=%s bonuses=%s improvements=%s routes=%s",
			iGameTurn, ePlayer, kPlayer.getNumCities(),
			kComposition.iWorked, kComposition.iWorkedImproved, kComposition.iWorkedUnimproved, kComposition.iLand, kComposition.iWater,
			kComposition.iHills, kComposition.iRiverSide, kComposition.iFreshWater, kComposition.iBonusImproved, kComposition.iBonusUnimproved,
			kComposition.iCurrentFood, kComposition.iCurrentProduction, kComposition.iCurrentCommerce, kComposition.iNatureFood, kComposition.iNatureProduction, kComposition.iNatureCommerce,
			getSASDiagnosticOrDash(szTerrains).GetCString(), getSASDiagnosticOrDash(szFeatures).GetCString(), getSASDiagnosticOrDash(szBonuses).GetCString(), getSASDiagnosticOrDash(szImprovements).GetCString(), getSASDiagnosticOrDash(szRoutes).GetCString());
}

// <!-- custom: City snapshots use the same compact religion/corporation token builders later shared by city-removal provenance.
// Forward declarations keep the builders in their existing lifecycle section without duplicating list logic. (ChatGPT-5.6-Sol) -->
static CvString getSASGameRecordCityReligionList(CvCity const& kCity, bool bHolyOnly);
static CvString getSASGameRecordCityCorporationList(CvCity const& kCity, bool bHeadquartersOnly);

// <!-- custom: Private level-3-only helper; logSASGameRecordCities owns the single detail-level gate so this function does not repeat it for each city/subrow.
// Consequently the detailed trade-partner and large-garrison rows below intentionally have no local `gGameRecordLogLevel >= 3` checks; adding them back would only duplicate the caller gate once per city/subrow. (ChatGPT-5.6-Sol) -->
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
	SASGameRecordPlotUnitCounts kCityUnits;
	collectSASGameRecordPlotUnitCounts(kCity.getPlot(), kCity.getOwner(), kCityUnits);
	// <!-- custom: Keep the periodic city row self-contained enough to explain growth/starvation and current economic/cultural status without creating more per-turn rows.
	// Stored food/granary state, occupation/culture/maintenance and commerce-type output are cheap current-state getters; religion/corporation lists are small loaded-XML scans already used by city-removal provenance. (ChatGPT-5.6-Sol) -->
	CultureLevelTypes const eCultureLevel = kCity.getCultureLevel();
	PlayerTypes const eHighestCulturePlayer = kCity.findHighestCulture();
	// <!-- custom: Keep citizen accounting explicit for post-game city-management analysis.
	// Civ4's inherited specialist API names are unusually misleading: getSpecialistPopulation() is assigned population specialists; getNumGreatPeople() does not count born/present Great People here, but is the cached total changed by setFreeSpecialistCount() for actual typed free-specialist instances; totalFreeSpecialists() is instead the separate generic allowance that lets some assigned specialists be worked without consuming ordinary population.
	// Keep the recorder names explicit so these three mechanics are not conflated by future code/documentation cleanup.
	// Population changes only mark citizen assignment dirty, so a snapshot taken immediately after a severe population loss can temporarily retain more working/assigned citizens than the new population; these fields report observed state rather than promise an immediate arithmetic identity.
	// The by-type lists below use the same explicit naming. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
	// <!-- custom: City-level commerce output/modifiers make each city's contribution to player-level gold/research/culture/espionage measurable; espionage defense remains a separate defensive modifier. (ChatGPT-5.6-Sol) -->
	// <!-- custom: Air-unit occupancy/capacity on the existing city row makes poor basing or saturated airbases visible without adding a separate late-game row. Cargo aircraft are intentionally excluded by CvPlot::countNumAirUnits, matching actual base-capacity use. (GPT-5.6) -->
	// <!-- custom: City defense snapshots expose both the current post-bombard defense modifier and its undamaged ceiling. DefenseDamage/MAX_CITY_DEFENSE_DAMAGE preserves the underlying bombardment state, while bombarded shows whether the city has already been hit this turn. This lets broad game records be paired with the level-3 tactical bombardment actions below. (GPT-5.6) -->
	logSASGameRecord("GAME_RECORD_CITY turn=%d player=%d cityId=%d city=%S x=%d y=%d originalOwner=%d capital=%d foundedTurn=%d acquiredTurn=%d pop=%d highestPop=%d angryPopulation=%d workingPopulation=%d assignedSpecialists=%d freeSpecialistInstances=%d freeSpecialistAllowance=%d foodStored=%d foodKept=%d growthThreshold=%d maxFoodKeptPercent=%d avoidGrowth=%d foodSurplus=%d happySurplus=%d healthSurplus=%d food=%d prod=%d commerce=%d maintenanceTimes100=%d maintenanceModifier=%d occupationTurns=%d disorder=%d ownerCultureTimes100=%d cultureLevel=%s cultureLevelId=%d nextCultureThreshold=%d cultureUpdateTurns=%d ownerCulturePercent=%d highestCulturePlayer=%d highestCulturePercent=%d religions=%s holyReligions=%s corporations=%s headquarters=%s goldRate=%d researchRate=%d cultureRate=%d espionageRate=%d goldRateModifier=%d researchRateModifier=%d cultureRateModifier=%d espionageRateModifier=%d espionageDefenseModifier=%d defenseModifier=%d totalDefense=%d defenseDamage=%d defenseDamageMax=%d bombarded=%d airUnits=%d airCapacity=%d airSpaceAvailable=%d worked=%d workedImproved=%d workedUnimproved=%d workedFood=%d workedProd=%d workedCommerce=%d garrison=%d cityUnits=%d militaryUnits=%d civilianUnits=%d defenders=%d healthyDefenders=%d woundedDefenders=%d settlers=%d workers=%d attackers=%d connectedToCapital=%d plotGroupId=%d tradeRoutes=%d domesticTradeRoutes=%d foreignTradeRoutes=%d tradeFood=%d tradeProd=%d tradeCommerce=%d productionKind=%s production=%s productionUsesFood=%d productionTurns=%d productionStored=%d productionNeeded=%d overflowProduction=%d featureProduction=%d productionConversionX100=%s assignedSpecialistsByType=%s freeSpecialistsByType=%s gpProgress=%d gpThreshold=%d gpRate=%d gpTurnsLeft=%d gpOdds=%s",
			iGameTurn, kCity.getOwner(), kCity.getID(), getSASGameRecordQuotedCityName(&kCity).GetCString(), kCity.getX(), kCity.getY(),
			kCity.getOriginalOwner(), kCity.isCapital(), kCity.getGameTurnFounded(), kCity.getGameTurnAcquired(), kCity.getPopulation(), kCity.getHighestPopulation(),
			kCity.angryPopulation(), kCity.getWorkingPopulation(), kCity.getSpecialistPopulation(), kCity.getNumGreatPeople(), kCity.totalFreeSpecialists(),
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
	// <!-- custom: Large city garrisons in autoplay logs did not reveal whether an army was one parked attack stack or many defensive/miscellaneous groups.
	// This helper is already level-3-only by caller contract, so only the six-military-unit threshold remains here; BBAI UNIT logging remains responsible for the groups' decision reasons. (GPT-5.6-Sol + ChatGPT-5.6-Sol) -->
	if (kCityUnits.iMilitaryUnits >= 6)
	{
		std::vector<int> aiUnitTypes(GC.getNumUnitInfos(), 0);
		std::vector<int> aiUnitAI(NUM_UNITAI_TYPES, 0);
		std::vector<int> aiGroupIds;
		CvSelectionGroup const* pLargestGroup = NULL;
		for (CLLNode<IDInfo> const* pUnitNode = kCity.getPlot().headUnitNode(); pUnitNode != NULL; pUnitNode = kCity.getPlot().nextUnitNode(pUnitNode))
		{
			CvUnit const* pLoopUnit = ::getUnit(pUnitNode->m_data);
			if (pLoopUnit == NULL || pLoopUnit->getOwner() != kCity.getOwner() || !isSASGameRecordMilitaryUnit(*pLoopUnit))
				continue;
			if (pLoopUnit->getUnitType() != NO_UNIT)
				aiUnitTypes[pLoopUnit->getUnitType()]++;
			UnitAITypes const eUnitAI = pLoopUnit->AI_getUnitAIType();
			if (eUnitAI >= 0 && eUnitAI < NUM_UNITAI_TYPES)
				aiUnitAI[eUnitAI]++;
			CvSelectionGroup const* pGroup = pLoopUnit->getGroup();
			if (pGroup == NULL)
				continue;
			bool bGroupAlreadyCounted = false;
			for (size_t iI = 0; iI < aiGroupIds.size(); iI++)
			{
				if (aiGroupIds[iI] == pGroup->getID())
				{
					bGroupAlreadyCounted = true;
					break;
				}
			}
			if (!bGroupAlreadyCounted)
				aiGroupIds.push_back(pGroup->getID());
			if (pLargestGroup == NULL || pGroup->getNumUnits() > pLargestGroup->getNumUnits())
				pLargestGroup = pGroup;
		}
		CvString szUnitTypes;
		CvString szUnitAI;
		for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
			appendSASGameRecordTypeCount(szUnitTypes, getSASGameRecordUnitType((UnitTypes)iI), aiUnitTypes[iI]);
		for (int iI = 0; iI < NUM_UNITAI_TYPES; iI++)
			appendSASGameRecordTypeCount(szUnitAI, getSASGameRecordUnitAIType((UnitAITypes)iI), aiUnitAI[iI]);
		CvSelectionGroupAI const* pLargestGroupAI = (pLargestGroup == NULL ? NULL : &pLargestGroup->AI());
		CvUnitAI const* pLargestGroupHead = (pLargestGroupAI == NULL ? NULL : pLargestGroupAI->AI_getHeadUnit());
		CvPlot const* pLargestGroupMissionPlot = (pLargestGroupAI == NULL ? NULL : pLargestGroupAI->AI_getMissionAIPlot());
		CvUnitAI const* pLargestGroupMissionUnit = (pLargestGroupAI == NULL ? NULL : pLargestGroupAI->AI_getMissionAIUnit());
		int iLargestGroupWounded = 0;
		if (pLargestGroup != NULL)
		{
			FOR_EACH_UNIT_IN(pLoopUnit, *pLargestGroup)
			{
				if (pLoopUnit->getDamage() > 0) iLargestGroupWounded++;
			}
		}
		// <!-- custom: Ordinary city-detail calculations above use CvPlayer, but incoming group-mission queries are available only through CvPlayerAI. Keep the derived reference scoped to this level-3 diagnostic. (GPT-5.6-Sol) -->
		CvPlayerAI const& kOwnerAI = GET_PLAYER(kCity.getOwner());
		int const iLargestGroupIncomingJoiners = (pLargestGroupHead == NULL ? -1 : kOwnerAI.AI_unitTargetMissionAIs(*pLargestGroupHead, MISSIONAI_GROUP));
		// <!-- custom: A peaceful Aztec attack-city group grew to 86 of 137 military units but stopped appearing in ATTACK_CITY_PARKING, so its persistent state or an earlier return path was invisible.
		// At level 3, preserve the largest city group's activity, queued mission, MissionAI target, wounded count, and incoming joiners alongside composition; paired UNIT diagnostics trace AI_attackCityMove when it is actually entered. (GPT-5.6-Sol) -->
		logSASGameRecord("GAME_RECORD_CITY_UNIT_COMPOSITION turn=%d player=%d cityId=%d city=%S militaryUnits=%d groups=%d largestGroupId=%d largestGroupUnits=%d largestGroupHeadAI=%s largestGroupActivity=%d largestGroupMission=%s largestGroupMissionAI=%d largestGroupMissionPlot=(%d,%d) largestGroupMissionUnitOwner=%d largestGroupMissionUnitId=%d largestGroupMissionQueue=%d largestGroupWounded=%d largestGroupIncomingJoiners=%d unitTypes=%s unitAI=%s",
				iGameTurn, kCity.getOwner(), kCity.getID(), getSASGameRecordQuotedCityName(&kCity).GetCString(), kCityUnits.iMilitaryUnits, (int)aiGroupIds.size(), (pLargestGroup == NULL ? -1 : pLargestGroup->getID()), (pLargestGroup == NULL ? 0 : pLargestGroup->getNumUnits()), (pLargestGroupHead == NULL ? "-" : getSASGameRecordUnitAIType(pLargestGroupHead->AI_getUnitAIType())),
				(pLargestGroup == NULL ? NO_ACTIVITY : pLargestGroup->getActivityType()), (pLargestGroup == NULL ? "-" : getSASGameRecordMissionType(pLargestGroup->getMissionType(0))), (pLargestGroupAI == NULL ? NO_MISSIONAI : pLargestGroupAI->AI_getMissionAIType()),
				(pLargestGroupMissionPlot == NULL ? -1 : pLargestGroupMissionPlot->getX()), (pLargestGroupMissionPlot == NULL ? -1 : pLargestGroupMissionPlot->getY()), (pLargestGroupMissionUnit == NULL ? -1 : pLargestGroupMissionUnit->getOwner()), (pLargestGroupMissionUnit == NULL ? -1 : pLargestGroupMissionUnit->getID()), (pLargestGroup == NULL ? 0 : pLargestGroup->getLengthMissionQueue()),
				iLargestGroupWounded, iLargestGroupIncomingJoiners, getSASDiagnosticOrDash(szUnitTypes).GetCString(), getSASDiagnosticOrDash(szUnitAI).GetCString());
	}
}

static void logSASGameRecordCities(PlayerTypes ePlayer, int iGameTurn)
{
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	SASGameRecordPlayerPrevious& kPrevious = g_akSASGameRecordPlayerPrevious[ePlayer];
	bool const bLogCityDetails = (gGameRecordLogLevel >= 3);
	int iCities = 0;
	int iTotalFoodSurplus = 0;
	int iTotalHappySurplus = 0;
	int iTotalHealthSurplus = 0;
	int iTotalFoodYield = 0;
	int iTotalProductionYield = 0;
	int iTotalCommerceYield = 0;
	int iTotalFoodStored = 0;
	int iTotalFoodKept = 0;
	int iTotalMaintenanceTimes100 = 0;
	int iTotalTradeRoutes = 0;
	int iDomesticTradeRoutes = 0;
	int iForeignTradeRoutes = 0;
	int iTradeFood = 0;
	int iTradeProduction = 0;
	int iTradeCommerce = 0;
	int iConnectedToCapital = 0;
	int iUnhappyCities = 0;
	int iUnhealthyCities = 0;
	int iStarvingCities = 0;
	int iOccupiedCities = 0;
	int iDisorderCities = 0;
	int iAvoidGrowthCities = 0;
	int iAngryPopulation = 0;
	int iWorkingPopulation = 0;
	int iCitiesProducingUnits = 0;
	int iCitiesProducingMilitary = 0;
	int iCitiesProducingWorkers = 0;
	int iCitiesProducingSettlers = 0;
	int iCitiesProducingBuildings = 0;
	int iCitiesProducingWonders = 0;
	int iCitiesProducingProjects = 0;
	int iCitiesProducingProcesses = 0;
	int iAssignedSpecialists = 0;
	int iFreeSpecialistInstances = 0;
	int iFreeSpecialistAllowance = 0;
	int iGarrison = 0;
	int iCityUnits = 0;
	int iMilitaryUnitsInCities = 0;
	int iCivilianUnitsInCities = 0;
	int iDefendersInCities = 0;
	int iSettlersInCities = 0;
	int iWorkersInCities = 0;
	int iBestGPTurns = 1000000;
	CvCity const* pNextGPCity = NULL;
	int iLoop = 0;
	CvCity const* pCapital = kPlayer.getCapital();
	for (CvCity const* pLoopCity = kPlayer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kPlayer.nextCity(&iLoop))
	{
		iCities++;
		const int iFoodSurplus = pLoopCity->foodDifference();
		const int iHappySurplus = pLoopCity->happyLevel() - pLoopCity->unhappyLevel();
		const int iHealthSurplus = pLoopCity->goodHealth() - pLoopCity->badHealth();
		iTotalFoodSurplus += iFoodSurplus;
		iTotalHappySurplus += iHappySurplus;
		iTotalHealthSurplus += iHealthSurplus;
		iTotalFoodYield += pLoopCity->getYieldRate(YIELD_FOOD);
		iTotalProductionYield += pLoopCity->getYieldRate(YIELD_PRODUCTION);
		iTotalCommerceYield += pLoopCity->getYieldRate(YIELD_COMMERCE);
		iTotalFoodStored += pLoopCity->getFood();
		iTotalFoodKept += pLoopCity->getFoodKept();
		iTotalMaintenanceTimes100 += pLoopCity->getMaintenanceTimes100();
		int const iCityTradeRoutes = pLoopCity->getTradeRoutes();
		iTotalTradeRoutes += iCityTradeRoutes;
		iTradeFood += pLoopCity->getTradeYield(YIELD_FOOD);
		iTradeProduction += pLoopCity->getTradeYield(YIELD_PRODUCTION);
		iTradeCommerce += pLoopCity->getTradeYield(YIELD_COMMERCE);
		for (int iTrade = 0; iTrade < iCityTradeRoutes; iTrade++)
		{
			CvCity const* pTradeCity = pLoopCity->getTradeCity(iTrade);
			if (pTradeCity == NULL)
				continue;
			if (pTradeCity->getOwner() == ePlayer)
				iDomesticTradeRoutes++;
			else iForeignTradeRoutes++;
		}
		if (pLoopCity->isConnectedToCapital())
			iConnectedToCapital++;
		if (iHappySurplus < 0)
			iUnhappyCities++;
		if (iHealthSurplus < 0)
			iUnhealthyCities++;
		if (iFoodSurplus < 0)
			iStarvingCities++;
		if (pLoopCity->isOccupation())
			iOccupiedCities++;
		if (pLoopCity->isDisorder())
			iDisorderCities++;
		if (pLoopCity->AI().AI_isEmphasizeAvoidGrowth())
			iAvoidGrowthCities++;
		iAngryPopulation += pLoopCity->angryPopulation();
		iWorkingPopulation += pLoopCity->getWorkingPopulation();
		iAssignedSpecialists += pLoopCity->getSpecialistPopulation();
		iFreeSpecialistInstances += pLoopCity->getNumGreatPeople();
		iFreeSpecialistAllowance += pLoopCity->totalFreeSpecialists();
		iGarrison += pLoopCity->plot()->getNumDefenders(ePlayer);
		SASGameRecordPlotUnitCounts kCityUnits;
		collectSASGameRecordPlotUnitCounts(pLoopCity->getPlot(), ePlayer, kCityUnits);
		iCityUnits += kCityUnits.iUnits;
		iMilitaryUnitsInCities += kCityUnits.iMilitaryUnits;
		iCivilianUnitsInCities += kCityUnits.iCivilianUnits;
		iDefendersInCities += kCityUnits.iDefenders;
		iSettlersInCities += kCityUnits.iSettlers;
		iWorkersInCities += kCityUnits.iWorkers;
		const int iGPTurns = pLoopCity->GPTurnsLeft();
		if (iGPTurns >= 0 && iGPTurns < iBestGPTurns)
		{
			iBestGPTurns = iGPTurns;
			pNextGPCity = pLoopCity;
		}
		UnitTypes eProductionUnit = pLoopCity->getProductionUnit();
		BuildingTypes eProductionBuilding = pLoopCity->getProductionBuilding();
		if (eProductionUnit != NO_UNIT)
		{
			iCitiesProducingUnits++;
			UnitAITypes eUnitAI = GC.getInfo(eProductionUnit).getDefaultUnitAIType();
			if (GC.getInfo(eProductionUnit).isMilitaryProduction())
				iCitiesProducingMilitary++;
			if (eUnitAI == UNITAI_WORKER || eUnitAI == UNITAI_WORKER_SEA)
				iCitiesProducingWorkers++;
			if (eUnitAI == UNITAI_SETTLE)
				iCitiesProducingSettlers++;
		}
		else if (eProductionBuilding != NO_BUILDING)
		{
			iCitiesProducingBuildings++;
			if (GC.getInfo(eProductionBuilding).isLimited())
				iCitiesProducingWonders++;
		}
		else if (pLoopCity->getProductionProject() != NO_PROJECT)
			iCitiesProducingProjects++;
		else if (pLoopCity->getProductionProcess() != NO_PROCESS)
			iCitiesProducingProcesses++;
		if (bLogCityDetails) logSASGameRecordCityDetail(*pLoopCity, iGameTurn);
	}
	// <!-- custom: Aggregate citizen-state fields mirror the detailed city semantics: working/angry population, assigned population specialists, actual typed free-specialist instances, and the separate generic free-specialist allowance.
	// Keep these names explicit because the inherited APIs use historically ambiguous terminology. (ChatGPT-5.6-Sol) -->
	logSASGameRecord("GAME_RECORD_CITIES turn=%d player=%d cities=%d capitalId=%d capital=%S connectedToCapital=%d totalFoodSurplus=%d totalHappySurplus=%d totalHealthSurplus=%d totalFood=%d totalProd=%d totalCommerce=%d totalFoodStored=%d totalFoodKept=%d totalMaintenanceTimes100=%d tradeRoutes=%d domesticTradeRoutes=%d foreignTradeRoutes=%d tradeFood=%d tradeProd=%d tradeCommerce=%d unhappyCities=%d unhealthyCities=%d starvingCities=%d occupiedCities=%d disorderCities=%d avoidGrowthCities=%d angryPopulation=%d workingPopulation=%d assignedSpecialists=%d freeSpecialistInstances=%d freeSpecialistAllowance=%d garrison=%d cityUnits=%d militaryUnits=%d civilianUnits=%d defenders=%d settlers=%d workers=%d nextGPCityId=%d nextGPCity=%S nextGPTurns=%d nextGPRate=%d nextGPProgress=%d citiesProducingUnits=%d citiesProducingMilitary=%d citiesProducingWorkers=%d citiesProducingSettlers=%d citiesProducingBuildings=%d citiesProducingWonders=%d citiesProducingProjects=%d citiesProducingProcesses=%d",
			iGameTurn, ePlayer, iCities, pCapital == NULL ? -1 : pCapital->getID(), getSASGameRecordQuotedCityName(pCapital).GetCString(),
			iConnectedToCapital, iTotalFoodSurplus, iTotalHappySurplus, iTotalHealthSurplus, iTotalFoodYield, iTotalProductionYield, iTotalCommerceYield, iTotalFoodStored, iTotalFoodKept, iTotalMaintenanceTimes100,
			iTotalTradeRoutes, iDomesticTradeRoutes, iForeignTradeRoutes, iTradeFood, iTradeProduction, iTradeCommerce,
			iUnhappyCities, iUnhealthyCities, iStarvingCities, iOccupiedCities, iDisorderCities, iAvoidGrowthCities, iAngryPopulation, iWorkingPopulation, iAssignedSpecialists, iFreeSpecialistInstances, iFreeSpecialistAllowance,
			iGarrison, iCityUnits, iMilitaryUnitsInCities, iCivilianUnitsInCities, iDefendersInCities, iSettlersInCities, iWorkersInCities,
			pNextGPCity == NULL ? -1 : pNextGPCity->getID(), getSASGameRecordQuotedCityName(pNextGPCity).GetCString(), pNextGPCity == NULL ? -1 : iBestGPTurns, pNextGPCity == NULL ? 0 : pNextGPCity->getGreatPeopleRate(), pNextGPCity == NULL ? 0 : pNextGPCity->getGreatPeopleProgress(),
			iCitiesProducingUnits, iCitiesProducingMilitary, iCitiesProducingWorkers, iCitiesProducingSettlers, iCitiesProducingBuildings,
			iCitiesProducingWonders, iCitiesProducingProjects, iCitiesProducingProcesses);
	logSASGameRecord("GAME_RECORD_CITIES_DELTAS turn=%d player=%d deltaValid=%d citiesDelta=%+d connectedToCapitalDelta=%+d totalFoodSurplusDelta=%+d totalHappySurplusDelta=%+d totalHealthSurplusDelta=%+d totalFoodDelta=%+d totalProdDelta=%+d totalCommerceDelta=%+d tradeRoutesDelta=%+d tradeCommerceDelta=%+d angryPopulationDelta=%+d workingPopulationDelta=%+d assignedSpecialistsDelta=%+d freeSpecialistInstancesDelta=%+d freeSpecialistAllowanceDelta=%+d garrisonDelta=%+d",
			iGameTurn, ePlayer, kPrevious.bValid,
			getSASGameRecordDelta(kPrevious.bValid, iCities, kPrevious.iCityCount), getSASGameRecordDelta(kPrevious.bValid, iConnectedToCapital, kPrevious.iCityConnectedToCapital), getSASGameRecordDelta(kPrevious.bValid, iTotalFoodSurplus, kPrevious.iCityFoodSurplus),
			getSASGameRecordDelta(kPrevious.bValid, iTotalHappySurplus, kPrevious.iCityHappySurplus), getSASGameRecordDelta(kPrevious.bValid, iTotalHealthSurplus, kPrevious.iCityHealthSurplus), getSASGameRecordDelta(kPrevious.bValid, iTotalFoodYield, kPrevious.iCityFood),
			getSASGameRecordDelta(kPrevious.bValid, iTotalProductionYield, kPrevious.iCityProduction), getSASGameRecordDelta(kPrevious.bValid, iTotalCommerceYield, kPrevious.iCityCommerce), getSASGameRecordDelta(kPrevious.bValid, iTotalTradeRoutes, kPrevious.iCityTradeRoutes),
			getSASGameRecordDelta(kPrevious.bValid, iTradeCommerce, kPrevious.iCityTradeCommerce), getSASGameRecordDelta(kPrevious.bValid, iAngryPopulation, kPrevious.iCityAngryPopulation), getSASGameRecordDelta(kPrevious.bValid, iWorkingPopulation, kPrevious.iCityWorkingPopulation),
			getSASGameRecordDelta(kPrevious.bValid, iAssignedSpecialists, kPrevious.iCityAssignedSpecialists), getSASGameRecordDelta(kPrevious.bValid, iFreeSpecialistInstances, kPrevious.iCityFreeSpecialistInstances), getSASGameRecordDelta(kPrevious.bValid, iFreeSpecialistAllowance, kPrevious.iCityFreeSpecialistAllowance), getSASGameRecordDelta(kPrevious.bValid, iGarrison, kPrevious.iCityGarrison));
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
	kPrevious.iCityAngryPopulation = iAngryPopulation;
	kPrevious.iCityWorkingPopulation = iWorkingPopulation;
	kPrevious.iCityAssignedSpecialists = iAssignedSpecialists;
	kPrevious.iCityFreeSpecialistInstances = iFreeSpecialistInstances;
	kPrevious.iCityFreeSpecialistAllowance = iFreeSpecialistAllowance;
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
		SASGameRecordPlotUnitCounts kCityUnits;
		collectSASGameRecordPlotUnitCounts(pLoopCity->getPlot(), BARBARIAN_PLAYER, kCityUnits);
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
	bool const bLogPlayerDetails = (gGameRecordLogLevel >= 2);
	// <!-- custom: Only query the level-3 threshold when level-2 detail logging is already active; disabled/level-1 snapshots pay one recorder-level read. (ChatGPT-5.6-Sol) -->
	bool const bLogPlayerVerboseDetails = (bLogPlayerDetails && gGameRecordLogLevel >= 3);
	CvTeam const& kTeam = GET_TEAM(kPlayer.getTeam());
	TechTypes eResearch = kPlayer.getCurrentResearch();
	const int iScore = kPlayer.calculateScore();
	const int iCities = kPlayer.getNumCities();
	const int iPopulation = kPlayer.getTotalPopulation();
	const int iLand = kPlayer.getTotalLand();
	const int iUnits = kPlayer.getNumUnits();
	const int iMilitarySupportUnits = kPlayer.getNumMilitaryUnits();
	// <!-- custom: CvPlayer::getNumMilitaryUnits counts XML bMilitarySupport, which can fall sharply when an army upgrades into combat units that intentionally do not pay military support. Count actual combat-capable units with the same predicate used by GAME_RECORD_UNIT_POSTURE, and keep the raw Civ4 counter separately. This scan runs only when a GameRecord player snapshot is already being generated. (ChatGPT-5.6-Sol) -->
	int iCombatUnits = 0;
	int iCombatLoop = 0;
	for (CvUnit const* pLoopUnit = kPlayer.firstUnit(&iCombatLoop); pLoopUnit != NULL; pLoopUnit = kPlayer.nextUnit(&iCombatLoop))
	{
		if (isSASGameRecordMilitaryUnit(*pLoopUnit)) ++iCombatUnits;
	}
	const int iPower = kPlayer.getPower();
	const int iGold = kPlayer.getGold();
	const int iGoldRate = kPlayer.calculateGoldRate();
	// <!-- custom: Keep nominal science visible when no target is selected because that science becomes stored research overflow rather than disappearing. (GPT-5.6-Sol) -->
	const int iResearchRate = kPlayer.calculateResearchRate(eResearch);
	const int iResearchTurns = (eResearch == NO_TECH ? -1 : kPlayer.getResearchTurnsLeft(eResearch, true));
	const int iHistoryScore = kPlayer.getHistorySafe(PLAYER_HISTORY_SCORE, iGameTurn);
	const int iHistoryEconomy = kPlayer.getHistorySafe(PLAYER_HISTORY_ECONOMY, iGameTurn);
	const int iHistoryIndustry = kPlayer.getHistorySafe(PLAYER_HISTORY_INDUSTRY, iGameTurn);
	const int iHistoryAgriculture = kPlayer.getHistorySafe(PLAYER_HISTORY_AGRICULTURE, iGameTurn);
	const int iHistoryPower = kPlayer.getHistorySafe(PLAYER_HISTORY_POWER, iGameTurn);
	const int iHistoryCulture = kPlayer.getHistorySafe(PLAYER_HISTORY_CULTURE, iGameTurn);
	const int iHistoryEspionage = kPlayer.getHistorySafe(PLAYER_HISTORY_ESPIONAGE, iGameTurn);
	SASGameRecordPlayerPrevious& kPrevious = g_akSASGameRecordPlayerPrevious[ePlayer];
	const char* szCiv = (kPlayer.getCivilizationType() == NO_CIVILIZATION ? "-" : GC.getInfo(kPlayer.getCivilizationType()).getType());
	const char* szLeader = (kPlayer.getLeaderType() == NO_LEADER ? "-" : GC.getInfo(kPlayer.getLeaderType()).getType());
	const char* szEra = (kPlayer.getCurrentEra() == NO_ERA ? "-" : GC.getInfo(kPlayer.getCurrentEra()).getType());
	const bool bCurrentlyHumanControlled = kPlayer.isHuman();
	const bool bAutoplayControlled = kPlayer.isHumanDisabled();
	const bool bHumanSlot = (bCurrentlyHumanControlled || bAutoplayControlled);
	// <!-- custom: These observations begin with the current log session rather than the civilization's lifetime; expose that scope in the player-row field names. See KI#379. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
	// <!-- custom: Financial strike is player-wide, not a city disorder state. `getStrikeTurns` is an inherited cumulative counter that is not reset when a strike ends, so expose it as `cumulativeStrikeTurns` rather than implying a current streak.
	// Exact strike turns are logged separately at level 2. (ChatGPT-5.6-Sol) -->
	logSASGameRecord("GAME_RECORD_PLAYER turn=%d player=%d team=%d civ=%s leader=%s isHuman=%d humanSlot=%d currentlyHumanControlled=%d autoplayControlled=%d rank=%d deltaValid=%d score=%d scoreDelta=%+d cities=%d citiesDelta=%+d pop=%d popDelta=%+d land=%d landDelta=%+d units=%d unitsDelta=%+d combatUnits=%d combatUnitsDelta=%+d militarySupportUnits=%d militarySupportUnitsDelta=%+d power=%d powerDelta=%+d gold=%d goldDelta=%+d gpt=%d gptDelta=%+d financialStrike=%d cumulativeStrikeTurns=%d researchRate=%d researchRateDelta=%+d researchPercent=%d currentResearch=%s researchOverflow=%d noResearchAvailable=%d researchTurns=%d era=%s stateReligion=%s techScorePercent=%d combatXP=%d greatPeopleCreated=%d greatGeneralsCreated=%d greatGeneralThreshold=%d goldenAgeTurns=%d loggedGoldenAgeTurns=%d anarchyTurns=%d loggedAnarchyTurns=%d revolutionTimer=%d conversionTimer=%d wars=%s",
			iGameTurn, ePlayer, kPlayer.getTeam(), szCiv, szLeader, bCurrentlyHumanControlled, bHumanSlot, bCurrentlyHumanControlled, bAutoplayControlled, kGame.getPlayerRank(ePlayer) + 1, kPrevious.bValid,
			iScore, getSASGameRecordDelta(kPrevious.bValid, iScore, kPrevious.iScore), iCities, getSASGameRecordDelta(kPrevious.bValid, iCities, kPrevious.iCities),
			iPopulation, getSASGameRecordDelta(kPrevious.bValid, iPopulation, kPrevious.iPopulation), iLand, getSASGameRecordDelta(kPrevious.bValid, iLand, kPrevious.iLand),
			iUnits, getSASGameRecordDelta(kPrevious.bValid, iUnits, kPrevious.iUnits), iCombatUnits, getSASGameRecordDelta(kPrevious.bValid, iCombatUnits, kPrevious.iCombatUnits),
			iMilitarySupportUnits, getSASGameRecordDelta(kPrevious.bValid, iMilitarySupportUnits, kPrevious.iMilitarySupportUnits), iPower, getSASGameRecordDelta(kPrevious.bValid, iPower, kPrevious.iPower), iGold, getSASGameRecordDelta(kPrevious.bValid, iGold, kPrevious.iGold), iGoldRate, getSASGameRecordDelta(kPrevious.bValid, iGoldRate, kPrevious.iGoldRate), kPlayer.isStrike(), kPlayer.getStrikeTurns(),
			iResearchRate, getSASGameRecordDelta(kPrevious.bValid, iResearchRate, kPrevious.iResearchRate), kPlayer.getCommercePercent(COMMERCE_RESEARCH), getSASGameRecordTechType(eResearch), kPlayer.getOverflowResearch(), kPlayer.isNoResearchAvailable(), iResearchTurns,
			szEra, getSASGameRecordReligionType(kPlayer.getStateReligion()), kTeam.getBestKnownTechScorePercent(), kPlayer.getCombatExperience(), kPlayer.getGreatPeopleCreated(), kPlayer.getGreatGeneralsCreated(), kPlayer.greatPeopleThreshold(true),
			kPlayer.getGoldenAgeTurns(), g_aiSASGameRecordLoggedGoldenAgeTurns[ePlayer], kPlayer.getAnarchyTurns(), g_aiSASGameRecordLoggedAnarchyTurns[ePlayer], kPlayer.getRevolutionTimer(), kPlayer.getConversionTimer(), getSASGameRecordWarTeams(kPlayer.getTeam()).GetCString());
	logSASGameRecord("GAME_RECORD_PLAYER_HISTORY turn=%d player=%d deltaValid=%d historyScore=%d historyScoreDelta=%+d historyEconomy=%d historyEconomyDelta=%+d historyIndustry=%d historyIndustryDelta=%+d historyAgriculture=%d historyAgricultureDelta=%+d historyPower=%d historyPowerDelta=%+d historyCulture=%d historyCultureDelta=%+d historyEspionage=%d historyEspionageDelta=%+d",
			iGameTurn, ePlayer, kPrevious.bValid, iHistoryScore, getSASGameRecordDelta(kPrevious.bValid, iHistoryScore, kPrevious.iHistoryScore), iHistoryEconomy, getSASGameRecordDelta(kPrevious.bValid, iHistoryEconomy, kPrevious.iHistoryEconomy), iHistoryIndustry, getSASGameRecordDelta(kPrevious.bValid, iHistoryIndustry, kPrevious.iHistoryIndustry), iHistoryAgriculture, getSASGameRecordDelta(kPrevious.bValid, iHistoryAgriculture, kPrevious.iHistoryAgriculture), iHistoryPower, getSASGameRecordDelta(kPrevious.bValid, iHistoryPower, kPrevious.iHistoryPower), iHistoryCulture, getSASGameRecordDelta(kPrevious.bValid, iHistoryCulture, kPrevious.iHistoryCulture), iHistoryEspionage, getSASGameRecordDelta(kPrevious.bValid, iHistoryEspionage, kPrevious.iHistoryEspionage));
	// <!-- custom: The environment row shows world pollution, but not which player produced it or whether buildings, bonuses, dirty power, or population caused it. Keep these city scans behind record level 2, and derive the total from the four components rather than scanning a fifth time. (GPT-5.6-Sol) -->
	if (bLogPlayerDetails)
	{
		int const iBuildingPollution = kPlayer.calculatePollution(CvPlayer::POLLUTION_BUILDINGS);
		int const iBonusPollution = kPlayer.calculatePollution(CvPlayer::POLLUTION_BONUSES);
		int const iPowerPollution = kPlayer.calculatePollution(CvPlayer::POLLUTION_POWER);
		int const iPopulationPollution = kPlayer.calculatePollution(CvPlayer::POLLUTION_POPULATION);
		logSASGameRecord("GAME_RECORD_POLLUTION turn=%d player=%d total=%d buildings=%d bonuses=%d power=%d population=%d", iGameTurn, ePlayer, iBuildingPollution + iBonusPollution + iPowerPollution + iPopulationPollution, iBuildingPollution, iBonusPollution, iPowerPollution, iPopulationPollution);
	}
	if (bLogPlayerDetails)
	{
		logSASGameRecordPlayerBonuses(ePlayer, iGameTurn, kPrevious);
		logSASGameRecordAIVictoryStages(ePlayer, iGameTurn);
		logSASGameRecordAIMilitaryProduction(ePlayer, iGameTurn);
		logSASGameRecordPolicies(ePlayer, iGameTurn);
		logSASGameRecordEconomy(ePlayer, iGameTurn);
		logSASGameRecordProductionPipeline(ePlayer, iGameTurn);
		logSASGameRecordStatistics(ePlayer, iGameTurn);
		logSASGameRecordEspionage(ePlayer, iGameTurn);
		logSASGameRecordDemographics(ePlayer, iGameTurn);
		logSASGameRecordAttitudes(ePlayer, iGameTurn);
		if (bLogPlayerVerboseDetails) logSASGameRecordDiplomaticMemories(ePlayer, iGameTurn);
		logSASGameRecordDiploStatus(ePlayer, iGameTurn);
		logSASGameRecordUnitPosture(ePlayer, iGameTurn);
		logSASGameRecordWorkers(ePlayer, iGameTurn);
		logSASGameRecordExpansion(ePlayer, iGameTurn);
		logSASGameRecordSettlers(ePlayer, iGameTurn);
		logSASGameRecordWorkedPlots(ePlayer, iGameTurn);
		logSASGameRecordCities(ePlayer, iGameTurn);
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

static void logSASGameRecordSnapshot(int iGameTurn, char const* szReason)
{
	CvGame const& kGame = GC.getGame();
	bool const bLogSnapshotDetails = (gGameRecordLogLevel >= 2);
	bool const bLogSnapshotVerboseDetails = (bLogSnapshotDetails && gGameRecordLogLevel >= 3);
	// <!-- custom: The initial save-load row is written before graphics initialization; the first full snapshot supplies the deferred display context. (GPT-5.6-Sol) -->
	logSASGameRecordDisplayContext();
	uint const uiSnapshotTime = getSASMonotonicMilliseconds();
	uint const uiSnapshotIntervalWallMilliseconds = getSASElapsedMilliseconds(g_uiSASGameRecordPreviousSnapshotTime, uiSnapshotTime);
	CvString const szSnapshotUtc = createSASUtcTimestampMilliseconds();
	// <!-- custom: Focus at this instant cannot prove how long Civ4 was foreground during the interval.
	// Repeated exact samples still help qualify wall-time comparisons without continuous monitoring. (GPT-5.6-Sol) -->
	// <!-- custom: Working set is Civ4's resident RAM, while page-file usage and available 32-bit virtual space help expose memory growth or address-space pressure.
	// Sample these only with the existing full snapshot, and skip all optional system calls when performance metrics are disabled. (GPT-5.6-Sol) -->
	SASGameRecordSystemSnapshot const kSystemSnapshot = getSASGameRecordSystemSnapshot();
	// <!-- custom: Team death and other state transitions can terminate a war without CvTeam::makePeace. Reconcile before snapshots so such wars still receive one final synthetic summary. (GPT-5.6-Sol) -->
	if (bLogSnapshotDetails) reconcileSASGameRecordWars();
	// <!-- custom: This is primarily an autoplay/game-history row, so place its frequently scanned gameplay state first. Keep wall-time and optional operating-system measurements afterward as supporting performance context. (GPT-5.6-Sol) -->
	logSASGameRecord("GAME_RECORD_TURN_BEGIN turn=%d reason=%s elapsed=%d year=%d playersAlive=%d teamsAlive=%d totalCities=%d totalPopulation=%d utc=%s sessionWallMilliseconds=%u snapshotIntervalWallMilliseconds=%u performanceMetricsEnabled=%d processForegroundAtSnapshot=%d processWindowMinimizedAtSnapshot=%d processWorkingSetKB=%d processPeakWorkingSetKB=%d processPagefileUsageKB=%d systemMemoryLoadPercent=%d processAvailableVirtualMB=%d",
			iGameTurn, szReason, kGame.getElapsedGameTurns(), kGame.getGameTurnYear(), kGame.countCivPlayersAlive(), kGame.countCivTeamsAlive(), kGame.getNumCities(), kGame.getTotalPopulation(),
			szSnapshotUtc.GetCString(), getSASGameRecordSessionWallMilliseconds(uiSnapshotTime), uiSnapshotIntervalWallMilliseconds, isSASGameRecordPerformanceMetricsEnabled(), kSystemSnapshot.iProcessForeground, kSystemSnapshot.iProcessWindowMinimized,
			kSystemSnapshot.iProcessWorkingSetKB, kSystemSnapshot.iProcessPeakWorkingSetKB, kSystemSnapshot.iProcessPagefileUsageKB, kSystemSnapshot.iSystemMemoryLoadPercent, kSystemSnapshot.iProcessAvailableVirtualMB);
	g_uiSASGameRecordPreviousSnapshotTime = uiSnapshotTime;
	logSASGameRecordRunStatus(szReason);
	if (bLogSnapshotVerboseDetails) logSASGameRecordMapAscii(false, szReason);
	if (bLogSnapshotDetails)
	{
		logSASGameRecordMapBonusTotals(iGameTurn);
		logSASGameRecordEnvironment(iGameTurn);
		logSASGameRecordVoteSources(iGameTurn);
	}
	for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		PlayerTypes eLoopPlayer = (PlayerTypes)iI;
		if (GET_PLAYER(eLoopPlayer).isAlive() && !GET_PLAYER(eLoopPlayer).isBarbarian())
			logSASGameRecordPlayerSnapshot(eLoopPlayer, iGameTurn);
	}
	// <!-- custom: Reproduce the active player's resolved Foreign Advisor market only at level 3 and only when its independent switch is enabled; lower detail levels and disabled-market runs skip the entire pair/item scan. (ChatGPT-5.6-Sol) -->
	if (bLogSnapshotVerboseDetails && isSASGameRecordTradeMarketEnabled()) logSASGameRecordTradeMarket(iGameTurn);
	for (int iI = 0; iI < MAX_CIV_TEAMS; iI++)
	{
		TeamTypes eLoopTeam = (TeamTypes)iI;
		if (GET_TEAM(eLoopTeam).isAlive() && !GET_TEAM(eLoopTeam).isBarbarian())
			logSASGameRecordTeamSnapshot(eLoopTeam, iGameTurn);
	}
	if (bLogSnapshotDetails)
	{
		logSASGameRecordBarbarians(iGameTurn);
		logSASGameRecordBattleBuckets(iGameTurn);
		logSASGameRecordFlowBuckets(iGameTurn);
	}
	uint const uiSnapshotEndTime = getSASMonotonicMilliseconds();
	logSASGameRecord("GAME_RECORD_TURN_END turn=%d reason=%s sessionWallMilliseconds=%u snapshotWallMilliseconds=%u", iGameTurn, szReason, getSASGameRecordSessionWallMilliseconds(uiSnapshotEndTime), getSASElapsedMilliseconds(uiSnapshotTime, uiSnapshotEndTime));
	g_iSASGameRecordLastFullSnapshotTurn = iGameTurn;
}

void logSASGameRecordTurn(int iGameTurn)
{
	// <!-- custom: Victory now forces a full snapshot immediately. If it occurs on an ordinary snapshot turn, do not repeat the same large snapshot again at end-of-turn. (GPT-5.6-Sol) -->
	if (g_iSASGameRecordLastFullSnapshotTurn == iGameTurn)
		return;
	logSASGameRecordSnapshot(iGameTurn, "interval");
}

// <!-- custom: High-level queue-mutating paths call this only at SASGameRecord level 2+. Keep the latest authoritative cause until the player's next finalized research-target observation.
// If it produces no invested-tech redirection, the observer discards it rather than emitting a standalone/noisy action. (ChatGPT-5.6-Sol) -->
void noteSASGameRecordResearchTargetChangeCause(PlayerTypes ePlayer, ResearchTargetChangeCause eCause)
{
	if (ePlayer < 0 || ePlayer >= MAX_PLAYERS)
		return;
	g_akSASGameRecordResearchPrevious[ePlayer].ePendingCause = eCause;
}

// <!-- custom: Called only from the level-2-gated ordinary research-application path, using values gameplay already computes.
// Store the same-turn split so a completion row can distinguish fresh research from carried overflow without logging every non-completing research turn. (ChatGPT-5.6-Sol) -->
void noteSASGameRecordResearchApplication(PlayerTypes ePlayer, TechTypes eTech, int iModifiedResearchRate, int iIncomingOverflowUnmodified, int iIncomingOverflowModified)
{
	if (ePlayer < 0 || ePlayer >= MAX_PLAYERS)
		return;
	SASGameRecordResearchApplication& kApplication = g_akSASGameRecordResearchApplication[ePlayer];
	kApplication.bValid = true;
	kApplication.iGameTurn = GC.getGame().getGameTurn();
	kApplication.eTech = eTech;
	kApplication.iModifiedResearchRate = iModifiedResearchRate;
	kApplication.iIncomingOverflowUnmodified = iIncomingOverflowUnmodified;
	kApplication.iIncomingOverflowModified = iIncomingOverflowModified;
}

// <!-- custom: Accumulate Golden Age and anarchy turns observed in this GameRecord session only; loaded saves deliberately begin new logs rather than pretending these are persisted lifetime totals. See KI#379.
// This helper intentionally remains callable at every enabled log level because these duration counters feed lower-detail rows; the research branch below separately self-gates at level 2+. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
void updateSASGameRecordPlayerTurnState(PlayerTypes ePlayer)
{
	if (ePlayer < 0 || ePlayer >= MAX_PLAYERS)
		return;
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	if (kPlayer.getGoldenAgeTurns() > 0)
		g_aiSASGameRecordLoggedGoldenAgeTurns[ePlayer]++;
	if (kPlayer.getAnarchyTurns() > 0)
		g_aiSASGameRecordLoggedAnarchyTurns[ePlayer]++;

	// <!-- custom: AI_doResearch has already finalized this turn's target before this hook, while CvPlayer::doResearch has not yet applied this turn's science.
	// Compare that stable boundary with the previous player turn and record only switches away from a still-incomplete technology; routine completed-tech queue progression is deliberately suppressed.
	// Cause comes only from explicit high-level queue-mutating hooks; unknown/uninstrumented paths stay UNKNOWN rather than being inferred from nearby events. (ChatGPT-5.6-Sol) -->
	if (gGameRecordLogLevel >= 2 && kPlayer.isAlive() && !kPlayer.isBarbarian())
	{
		SASGameRecordResearchPrevious& kPrevious = g_akSASGameRecordResearchPrevious[ePlayer];
		TeamTypes const eTeam = kPlayer.getTeam();
		CvTeam const& kTeam = GET_TEAM(eTeam);
		TechTypes const eResearch = kPlayer.getCurrentResearch();
		if (kPrevious.bValid && kPrevious.eTeam == eTeam && kPrevious.eTech != NO_TECH && kPrevious.eTech != eResearch)
		{
			TechTypes const eOldResearch = kPrevious.eTech;
			bool const bOldResearchCompleted = (GC.getInfo(eOldResearch).isRepeat() ?
				kTeam.getTechCount(eOldResearch) > kPrevious.iTechCount : kTeam.isHasTech(eOldResearch));
			if (!bOldResearchCompleted)
			{
				int const iOldProgress = kTeam.getResearchProgress(eOldResearch);
				// <!-- custom: A zero-progress target change wastes/parks no research and is common enough to be low-value noise.
				// Once progress exists, retain team-game context too: another teammate may still be researching the old technology, so this row must not imply that the team's investment was abandoned. (ChatGPT-5.6-Sol) -->
				if (iOldProgress > 0)
				{
					int iOldTeamResearchersAfter = 0;
					for (MemberIter it(eTeam); it.hasNext(); ++it)
					{
						if (it->getCurrentResearch() == eOldResearch)
							iOldTeamResearchersAfter++;
					}
					int const iOldCost = kTeam.getResearchCost(eOldResearch);
					int const iNewProgress = (eResearch == NO_TECH ? 0 : kTeam.getResearchProgress(eResearch));
					int const iNewCost = (eResearch == NO_TECH ? -1 : kTeam.getResearchCost(eResearch));
					logSASGameRecord("GAME_RECORD_ACTION turn=%d type=RESEARCH_TARGET_CHANGED player=%d team=%d reason=%s oldTech=%s oldTeamProgress=%d oldCost=%d oldTeamResearchersAfter=%d newTech=%s newTeamProgress=%d newCost=%d",
						GC.getGame().getGameTurn(), ePlayer, eTeam, getSASResearchTargetChangeCause(kPrevious.ePendingCause), getSASGameRecordTechType(eOldResearch), iOldProgress, iOldCost, iOldTeamResearchersAfter, getSASGameRecordTechType(eResearch), iNewProgress, iNewCost);
				}
			}
		}
		kPrevious.bValid = true;
		kPrevious.eTeam = eTeam;
		kPrevious.eTech = eResearch;
		kPrevious.iTechCount = (eResearch == NO_TECH ? 0 : kTeam.getTechCount(eResearch));
		// <!-- custom: Any tagged cause belongs only to mutations observed since the previous player-turn boundary.
		// If no invested-tech redirection resulted, discard it here so it cannot be misattributed to a later unrelated switch. (ChatGPT-5.6-Sol) -->
		kPrevious.ePendingCause = RESEARCH_TARGET_CHANGE_UNKNOWN;
	}
}


static void countSASGameRecordVisibleEnemiesNearPlot(CvPlot const& kCenter, PlayerTypes ePlayer, int iRange, int& iVisibleEnemies, int& iVisibleCombatEnemies, CvUnit const*& pNearestEnemy, int& iNearestEnemyDistance)
{
	iVisibleEnemies = 0;
	iVisibleCombatEnemies = 0;
	pNearestEnemy = NULL;
	iNearestEnemyDistance = -1;
	if (ePlayer == NO_PLAYER)
		return;
	TeamTypes const eTeam = GET_PLAYER(ePlayer).getTeam();
	for (int iDX = -iRange; iDX <= iRange; iDX++)
	{
		for (int iDY = -iRange; iDY <= iRange; iDY++)
		{
			CvPlot const* pLoopPlot = plotXY(kCenter.getX(), kCenter.getY(), iDX, iDY);
			if (pLoopPlot == NULL || !pLoopPlot->isVisible(eTeam, false))
				continue;
			for (CLLNode<IDInfo> const* pUnitNode = pLoopPlot->headUnitNode(); pUnitNode != NULL; pUnitNode = pLoopPlot->nextUnitNode(pUnitNode))
			{
				CvUnit const* pLoopUnit = ::getUnit(pUnitNode->m_data);
				// <!-- custom: isEnemy resolves plot-sensitive combat ownership and always-hostile rules, so testing every nearby unit against the center plot could misclassify the diagnostic.
				// Use the unit's actual loop plot. See KI#374. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
				if (pLoopUnit == NULL || !pLoopUnit->isEnemy(eTeam, *pLoopPlot) || pLoopUnit->isInvisible(eTeam, false))
					continue;
				iVisibleEnemies++;
				if (pLoopUnit->baseCombatStr() > 0 || pLoopUnit->canAttack())
					iVisibleCombatEnemies++;
				int const iDistance = plotDistance(kCenter.getX(), kCenter.getY(), pLoopPlot->getX(), pLoopPlot->getY());
				if (iNearestEnemyDistance < 0 || iDistance < iNearestEnemyDistance)
				{
					iNearestEnemyDistance = iDistance;
					pNearestEnemy = pLoopUnit;
				}
			}
		}
	}
}

static void logSASGameRecordCityUnits(CvCity const& kCity, char const* szReason)
{
	SASGameRecordPlotUnitCounts kCounts;
	collectSASGameRecordPlotUnitCounts(kCity.getPlot(), kCity.getOwner(), kCounts);
	int iVisibleEnemies = 0;
	int iVisibleCombatEnemies = 0;
	int iNearestEnemyDistance = -1;
	CvUnit const* pNearestEnemy = NULL;
	countSASGameRecordVisibleEnemiesNearPlot(kCity.getPlot(), kCity.getOwner(), 2, iVisibleEnemies, iVisibleCombatEnemies, pNearestEnemy, iNearestEnemyDistance);
	CvCity const* pNearestOtherOwnCity = NULL;
	int iNearestOtherOwnCityDistance = -1;
	int iCityLoop = 0;
	for (CvCity const* pLoopCity = GET_PLAYER(kCity.getOwner()).firstCity(&iCityLoop); pLoopCity != NULL; pLoopCity = GET_PLAYER(kCity.getOwner()).nextCity(&iCityLoop))
	{
		if (pLoopCity->getID() == kCity.getID())
			continue;
		int const iDistance = plotDistance(kCity.getX(), kCity.getY(), pLoopCity->getX(), pLoopCity->getY());
		if (iNearestOtherOwnCityDistance < 0 || iDistance < iNearestOtherOwnCityDistance)
		{
			iNearestOtherOwnCityDistance = iDistance;
			pNearestOtherOwnCity = pLoopCity;
		}
	}
	logSASGameRecord("GAME_RECORD_CITY_UNITS turn=%d reason=%s player=%d cityId=%d city=%S x=%d y=%d pop=%d ownerUnits=%d militaryUnits=%d civilianUnits=%d defenders=%d healthyDefenders=%d woundedDefenders=%d settlers=%d workers=%d attackers=%d bestDefenderId=%d bestDefenderUnit=%s bestDefenderAI=%s bestDefenderDamage=%d visibleEnemiesR2=%d visibleCombatEnemiesR2=%d nearestEnemyPlayer=%d nearestEnemyUnit=%s nearestEnemyDist=%d nearestOtherOwnCityId=%d nearestOtherOwnCity=%S nearestOtherOwnCityDistance=%d",
			GC.getGame().getGameTurn(), szReason, kCity.getOwner(), kCity.getID(), getSASGameRecordQuotedCityName(&kCity).GetCString(), kCity.getX(), kCity.getY(), kCity.getPopulation(),
			kCounts.iUnits, kCounts.iMilitaryUnits, kCounts.iCivilianUnits, kCounts.iDefenders, kCounts.iHealthyDefenders, kCounts.iWoundedDefenders, kCounts.iSettlers, kCounts.iWorkers, kCounts.iAttackers,
			(kCounts.pBestDefender == NULL ? -1 : kCounts.pBestDefender->getID()), (kCounts.pBestDefender == NULL ? "-" : getSASGameRecordUnitType(kCounts.pBestDefender->getUnitType())), (kCounts.pBestDefender == NULL ? "-" : getSASGameRecordUnitAIType(kCounts.pBestDefender->AI_getUnitAIType())), (kCounts.pBestDefender == NULL ? -1 : kCounts.pBestDefender->getDamage()),
			iVisibleEnemies, iVisibleCombatEnemies, (pNearestEnemy == NULL ? -1 : pNearestEnemy->getOwner()), (pNearestEnemy == NULL ? "-" : getSASGameRecordUnitType(pNearestEnemy->getUnitType())), iNearestEnemyDistance,
			(pNearestOtherOwnCity == NULL ? -1 : pNearestOtherOwnCity->getID()), getSASGameRecordQuotedCityName(pNearestOtherOwnCity).GetCString(), iNearestOtherOwnCityDistance);
}

static bool logSASGameRecordSettlerCombatForPlot(CvUnit const* pWinner, CvUnit const* pLoser, CvPlot const* pPlot, PlayerTypes eSettlerOwner, bool bLoserWasSettler, bool bWinnerWasSettler)
{
	if (pWinner == NULL || pLoser == NULL || pPlot == NULL || eSettlerOwner == NO_PLAYER)
		return false;
	SASGameRecordPlotUnitCounts kCounts;
	collectSASGameRecordPlotUnitCounts(*pPlot, eSettlerOwner, kCounts);
	if (kCounts.iSettlers <= 0 && !bLoserWasSettler && !bWinnerWasSettler)
		return false;
	CvUnit const* pSettler = (bLoserWasSettler ? pLoser : (bWinnerWasSettler ? pWinner : kCounts.pFirstSettler));
	CvSelectionGroup const* pSettlerGroup = (pSettler == NULL ? NULL : pSettler->getGroup());
	int iGroupUnits = 0;
	int iGroupDefenders = 0;
	int iGroupSettlers = 0;
	if (pSettlerGroup != NULL)
	{
		FOR_EACH_UNIT_IN(pLoopUnit, *pSettlerGroup)
		{
			iGroupUnits++;
			if (isSASGameRecordSettlerUnit(*pLoopUnit))
				iGroupSettlers++;
			if (pLoopUnit->canDefend(pLoopUnit->plot()))
				iGroupDefenders++;
		}
	}
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=SETTLER_GROUP_ATTACKED settlerOwner=%d settlerId=%d settlerUnit=%s x=%d y=%d cityPlot=%d winnerPlayer=%d winnerUnitId=%d winnerUnit=%s winnerAI=%s winnerBaseStr=%d winnerDamage=%d loserPlayer=%d loserUnitId=%d loserUnit=%s loserAI=%s loserBaseStr=%d loserDamage=%d loserWasSettler=%d winnerWasSettler=%d ownerUnitsOnPlot=%d militaryUnitsOnPlot=%d civilianUnitsOnPlot=%d settlersOnPlot=%d defendersOnPlot=%d healthyDefendersOnPlot=%d workersOnPlot=%d settlerGroupId=%d settlerGroupUnits=%d settlerGroupSettlers=%d settlerGroupDefenders=%d",
			GC.getGame().getGameTurn(), eSettlerOwner, (pSettler == NULL ? -1 : pSettler->getID()), (pSettler == NULL ? "-" : getSASGameRecordUnitType(pSettler->getUnitType())), pPlot->getX(), pPlot->getY(), pPlot->isCity(),
			pWinner->getOwner(), pWinner->getID(), getSASGameRecordUnitType(pWinner->getUnitType()), getSASGameRecordUnitAIType(pWinner->AI_getUnitAIType()), pWinner->baseCombatStr(), pWinner->getDamage(),
			pLoser->getOwner(), pLoser->getID(), getSASGameRecordUnitType(pLoser->getUnitType()), getSASGameRecordUnitAIType(pLoser->AI_getUnitAIType()), pLoser->baseCombatStr(), pLoser->getDamage(),
			bLoserWasSettler, bWinnerWasSettler, kCounts.iUnits, kCounts.iMilitaryUnits, kCounts.iCivilianUnits, kCounts.iSettlers,
			kCounts.iDefenders, kCounts.iHealthyDefenders, kCounts.iWorkers, (pSettlerGroup == NULL ? -1 : pSettlerGroup->getID()), iGroupUnits, iGroupSettlers, iGroupDefenders);
	return true;
}

// <!-- custom: Add the actual battle target for Settler-group context. Using the losing unit's plot falsely treated a failed attack launched from a Settler stack as an attack against that stack. See KI#377. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
static void logSASGameRecordSettlerCombatIfNeeded(CvUnit const* pWinner, CvUnit const* pLoser, CvPlot const* pBattlePlot)
{
	if (pWinner == NULL || pLoser == NULL || pBattlePlot == NULL)
		return;
	bool const bLoserWasSettler = isSASGameRecordSettlerUnit(*pLoser);
	bool const bWinnerWasSettler = isSASGameRecordSettlerUnit(*pWinner);
	if (bLoserWasSettler && logSASGameRecordSettlerCombatForPlot(pWinner, pLoser, pBattlePlot, pLoser->getOwner(), true, bWinnerWasSettler))
		return;
	if (bWinnerWasSettler && logSASGameRecordSettlerCombatForPlot(pWinner, pLoser, pBattlePlot, pWinner->getOwner(), bLoserWasSettler, true))
		return;
	if (logSASGameRecordSettlerCombatForPlot(pWinner, pLoser, pBattlePlot, pLoser->getOwner(), false, false))
		return;
	logSASGameRecordSettlerCombatForPlot(pWinner, pLoser, pBattlePlot, pWinner->getOwner(), false, false);
}

// <!-- custom: GAME_RECORD_ACTION is narrower than a generic row: it records chronological gameplay happenings such as techs, city ownership, war state, Great People, unit upgrades, and victory. Do not rename this to GAME_RECORD_ROW; "row" is too generic because every log line is already a row. This keeps the row type useful without using "event", which can be confused with Civ4 EventInfo/random events. (GPT-5.5) -->
// <!-- custom: Ordinary unit-completion hooks do not see animals and other Barbarian units created directly from fog. Record those explicit spawn sites without instrumenting every unrelated CvPlayer::initUnit caller. (GPT-5.6-Sol) -->
void logSASGameRecordBarbarianSpawn(CvUnit const* pUnit, char const* szCause)
{
	if (pUnit == NULL || pUnit->getOwner() != BARBARIAN_PLAYER)
		return;
	CvPlot const* pPlot = pUnit->plot();
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=BARBARIAN_UNIT_SPAWNED cause=%s unitId=%d unit=%s unitAI=%s x=%d y=%d area=%d cargo=%d transportId=%d",
			GC.getGame().getGameTurn(), szCause, pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), getSASGameRecordUnitAIType(pUnit->AI_getUnitAIType()),
			pUnit->getX(), pUnit->getY(), pPlot == NULL ? -1 : pPlot->getArea().getID(), pUnit->isCargo(), pUnit->getTransportUnit() == NULL ? -1 : pUnit->getTransportUnit()->getID());
}

static CvString getSASGameRecordGoodyUnits(std::vector<CvUnit const*> const& apUnits, bool bIncludePromotions)
{
	CvString szUnits;
	for (size_t iI = 0; iI < apUnits.size(); iI++)
	{
		CvUnit const* pUnit = apUnits[iI];
		if (pUnit == NULL)
			continue;
		CvString szPromotions;
		if (bIncludePromotions)
		{
			FOR_EACH_ENUM(Promotion)
			{
				if (!pUnit->isHasPromotion(eLoopPromotion))
					continue;
				if (!szPromotions.empty())
					szPromotions += "+";
				szPromotions += getSASGameRecordPromotionType(eLoopPromotion);
			}
		}
		CvString szItem;
		if (szPromotions.empty())
			szItem.Format("%s%s:%d@(%d,%d)", szUnits.empty() ? "" : ",", getSASGameRecordUnitType(pUnit->getUnitType()), pUnit->getID(), pUnit->getX(), pUnit->getY());
		else szItem.Format("%s%s:%d@(%d,%d)[%s]", szUnits.empty() ? "" : ",", getSASGameRecordUnitType(pUnit->getUnitType()), pUnit->getID(), pUnit->getX(), pUnit->getY(), szPromotions.GetCString());
		szUnits += szItem;
	}
	return getSASDiagnosticOrDash(szUnits);
}

// <!-- custom: Log the resolved goody result rather than only the XML label. AdvCiv goodies can randomize gold/research, reveal a variable map area, upgrade free units, spawn variable hostile units, or roll a same-sign follow-up outcome.
// Generic TECH_ACQUIRED and map-revelation rows remain complementary chronology; this rare level-2 row ties those downstream effects back to the hut that caused them. (ChatGPT-5.6-Sol) -->
void logSASGameRecordGoodyReceived(PlayerTypes ePlayer, CvPlot const* pPlot, CvUnit const* pTriggerUnit, GoodyTypes eGoody, SASGameRecordGoodyResult const& kResult)
{
	if (pPlot == NULL || ePlayer < 0 || ePlayer >= MAX_PLAYERS || eGoody == NO_GOODY)
		return;
	int const iTechProgressAdded = (kResult.iTechProgressBefore < 0 || kResult.iTechProgressAfter < 0 ? -1 : kResult.iTechProgressAfter - kResult.iTechProgressBefore);
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=GOODY_RECEIVED player=%d team=%d x=%d y=%d area=%d triggerUnitId=%d triggerUnit=%s goody=%s bad=%d followup=%d upgradeRoll=%d upgradeApplied=%d additionalOutcomeAttempted=%d gold=%d newlyRevealedPlots=%d experienceGained=%d damageHealed=%d tech=%s techRewardValue=%d techStoredProgressBefore=%d techStoredProgressAdded=%d techStoredProgressAfter=%d techCost=%d techCompleted=%d freeUnitCount=%d freePromotionsGranted=%d freeUnits=%s barbarianUnitCount=%d barbarianUnits=%s",
			GC.getGame().getGameTurn(), ePlayer, GET_PLAYER(ePlayer).getTeam(), pPlot->getX(), pPlot->getY(), pPlot->getArea().getID(),
			pTriggerUnit == NULL ? -1 : pTriggerUnit->getID(), pTriggerUnit == NULL ? "-" : getSASGameRecordUnitType(pTriggerUnit->getUnitType()), getSASGameRecordGoodyType(eGoody), GC.getInfo(eGoody).isBad(),
			kResult.bFollowupOutcome, kResult.bUpgradeRoll, kResult.bUpgradeApplied, kResult.bAdditionalOutcomeAttempted, kResult.iGold, kResult.iNewlyRevealedPlots, kResult.iExperienceGained, kResult.iDamageHealed,
			getSASGameRecordTechType(kResult.eTech), kResult.iTechRewardValue, kResult.iTechProgressBefore, iTechProgressAdded, kResult.iTechProgressAfter, kResult.iTechCost, kResult.bTechCompleted,
			(int)kResult.apFreeUnits.size(), kResult.iFreePromotionsGranted, getSASGameRecordGoodyUnits(kResult.apFreeUnits, true).GetCString(), (int)kResult.apBarbarianUnits.size(), getSASGameRecordGoodyUnits(kResult.apBarbarianUnits, false).GetCString());
}

// <!-- custom: A hut can exhaust NUM_DO_GOODY_ATTEMPTS without finding an eligible result.
// Preserve that rare factual no-outcome boundary so native DLL-resolved hut removals remain explainable, including failed AdvCiv follow-up rolls. (ChatGPT-5.6-Sol) -->
void logSASGameRecordGoodyNoOutcome(PlayerTypes ePlayer, CvPlot const* pPlot, CvUnit const* pTriggerUnit, GoodyTypes eTaboo, int iAttempts)
{
	if (pPlot == NULL || ePlayer < 0 || ePlayer >= MAX_PLAYERS)
		return;
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=GOODY_NO_OUTCOME player=%d team=%d x=%d y=%d area=%d triggerUnitId=%d triggerUnit=%s followup=%d taboo=%s attempts=%d",
			GC.getGame().getGameTurn(), ePlayer, GET_PLAYER(ePlayer).getTeam(), pPlot->getX(), pPlot->getY(), pPlot->getArea().getID(),
			pTriggerUnit == NULL ? -1 : pTriggerUnit->getID(), pTriggerUnit == NULL ? "-" : getSASGameRecordUnitType(pTriggerUnit->getUnitType()), eTaboo != NO_GOODY, getSASGameRecordGoodyType(eTaboo), iAttempts);
}

// <!-- custom: Keep random-event narration compact: summarize only broad EventInfo effect families and gameplay-relevant Python hooks, not every static XML magnitude or AI candidate value.
// These helpers run only from already level-2-gated lifecycle loggers. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
static CvString getSASGameRecordRandomEventEffects(CvEventInfo const& kEvent)
{
	CvString szEffects;
	if (kEvent.getGold() != 0 || kEvent.getRandomGold() != 0 || kEvent.getTechCostPercent() != 0)
		appendSASGameRecordType(szEffects, "GOLD");
	if (kEvent.getTechCostPercent() != 0)
		appendSASGameRecordType(szEffects, "TECH_COST");
	if (kEvent.getEspionagePoints() != 0)
		appendSASGameRecordType(szEffects, "ESPIONAGE_POINTS");
	if (kEvent.getTechPercent() != 0)
		appendSASGameRecordType(szEffects, "TECH_PROGRESS");
	if (kEvent.isGoldenAge())
		appendSASGameRecordType(szEffects, "GOLDEN_AGE");
	if (kEvent.getFreeUnitSupport() != 0 || kEvent.getInflationModifier() != 0 || kEvent.getSpaceProductionModifier() != 0)
		appendSASGameRecordType(szEffects, "PLAYER_MODIFIER");
	if (kEvent.isDeclareWar())
		appendSASGameRecordType(szEffects, "DECLARE_WAR");
	if (kEvent.getBonusGift() != NO_BONUS)
		appendSASGameRecordType(szEffects, "BONUS_GIFT");
	if (kEvent.getHappy() != 0)
		appendSASGameRecordType(szEffects, "HAPPINESS");
	if (kEvent.getHealth() != 0)
		appendSASGameRecordType(szEffects, "HEALTH");
	if (kEvent.getHurryAnger() != 0 || kEvent.getHappyTurns() != 0)
		appendSASGameRecordType(szEffects, "TEMPORARY_MOOD");
	if (kEvent.getFood() != 0 || kEvent.getFoodPercent() != 0)
		appendSASGameRecordType(szEffects, "FOOD");
	if (kEvent.getPopulationChange() != 0)
		appendSASGameRecordType(szEffects, "POPULATION");
	if (kEvent.getRevoltTurns() > 0)
		appendSASGameRecordType(szEffects, "REVOLT");
	if (kEvent.getCulture() != 0)
		appendSASGameRecordType(szEffects, "CULTURE");
	if (kEvent.getMaxPillage() > 0)
		appendSASGameRecordType(szEffects, "PILLAGE");
	bool bFreeSpecialists = false;
	FOR_EACH_ENUM(Specialist)
	{
		if (kEvent.getFreeSpecialistCount(eLoopSpecialist) != 0)
		{
			bFreeSpecialists = true;
			break;
		}
	}
	if (bFreeSpecialists)
		appendSASGameRecordType(szEffects, "FREE_SPECIALISTS");
	if (kEvent.getUnitClass() != NO_UNITCLASS && kEvent.getNumUnits() > 0)
		appendSASGameRecordType(szEffects, "FREE_UNITS");
	if (kEvent.getBuildingClass() != NO_BUILDINGCLASS && kEvent.getBuildingChange() != 0)
		appendSASGameRecordType(szEffects, "BUILDING_CHANGE");
	if (kEvent.getBuildingYieldChange().isAnyNonDefault() || kEvent.getBuildingCommerceChange().isAnyNonDefault() ||
		kEvent.getBuildingHappyChange().isAnyNonDefault() || kEvent.getBuildingHealthChange().isAnyNonDefault())
	{
		appendSASGameRecordType(szEffects, "BUILDING_MODIFIER");
	}
	bool bPlotYield = false;
	FOR_EACH_ENUM(Yield)
	{
		if (kEvent.getPlotExtraYield(eLoopYield) != 0)
		{
			bPlotYield = true;
			break;
		}
	}
	if (kEvent.getFeatureChange() != 0 || kEvent.getImprovementChange() != 0 || kEvent.getBonusChange() != 0 || kEvent.getRouteChange() != 0 || bPlotYield)
		appendSASGameRecordType(szEffects, "PLOT_CHANGE");
	bool bFreePromotions = false;
	FOR_EACH_ENUM(UnitCombat)
	{
		if (kEvent.getUnitCombatPromotion(eLoopUnitCombat) != NO_PROMOTION)
		{
			bFreePromotions = true;
			break;
		}
	}
	if (!bFreePromotions)
	{
		FOR_EACH_ENUM(UnitClass)
		{
			if (kEvent.getUnitClassPromotion(eLoopUnitClass) != NO_PROMOTION)
			{
				bFreePromotions = true;
				break;
			}
		}
	}
	if (bFreePromotions)
		appendSASGameRecordType(szEffects, "FREE_PROMOTION");
	if (kEvent.getBonusRevealed() != NO_BONUS)
		appendSASGameRecordType(szEffects, "BONUS_REVEAL");
	if (kEvent.getConvertOwnCities() > 0 || kEvent.getConvertOtherCities() > 0)
		appendSASGameRecordType(szEffects, "RELIGION_SPREAD");
	if (kEvent.getOurAttitudeModifier() != 0 || kEvent.getAttitudeModifier() != 0 || kEvent.getTheirEnemyAttitudeModifier() != 0)
		appendSASGameRecordType(szEffects, "DIPLO_ATTITUDE");
	if (kEvent.hasUnitLocalEffect())
		appendSASGameRecordType(szEffects, "UNIT_LOCAL");
	bool bFollowup = false;
	bool bClear = false;
	FOR_EACH_ENUM(Event)
	{
		if (kEvent.getAdditionalEventChance(eLoopEvent) > 0 || kEvent.getAdditionalEventTime(eLoopEvent) != 0)
			bFollowup = true;
		if (kEvent.getClearEventChance(eLoopEvent) > 0)
			bClear = true;
	}
	if (bFollowup)
		appendSASGameRecordType(szEffects, "FOLLOWUP_EVENT");
	if (bClear)
		appendSASGameRecordType(szEffects, "CLEAR_EVENT");
	char const* szPythonCallback = kEvent.getPythonCallback();
	if (szPythonCallback != NULL && szPythonCallback[0] != '\0')
		appendSASGameRecordType(szEffects, "PYTHON_CALLBACK");
	return getSASDiagnosticOrDash(szEffects);
}

static char const* getSASGameRecordRandomEventNormalSelectionMode(CvEventTriggerInfo const& kTrigger)
{
	// <!-- custom: Mirror CvPlayer::doEvents/getEventTriggerWeight semantics exactly. Weight -1 is auto-fired whenever eligible.
	// Values below -1 are excluded from the ordinary weighted/forced loop and are reserved for direct/special firing (e.g. the -2 Partisans trigger), while weight 0 can only reach the delivery boundary through a direct/special caller.
	// The row describes normal engine selection semantics, not an unverifiable claim about this instance's actual caller. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
	int const iWeight = kTrigger.getProbability();
	if (iWeight == -1)
		return "FORCED_WHEN_ELIGIBLE";
	if (iWeight < -1)
		return "DIRECT_OR_SPECIAL_ONLY";
	if (iWeight == 0)
		return "ZERO_WEIGHT_DIRECT_ONLY";
	return "WEIGHTED_RANDOM";
}

static CvString getSASGameRecordRandomEventTriggerPrereqs(CvEventTriggerInfo const& kTrigger)
{
	CvString szPrereqs;
	for (int i = 0; i < kTrigger.getNumPrereqEvents(); i++)
		appendSASGameRecordType(szPrereqs, getSASGameRecordEventType((EventTypes)kTrigger.getPrereqEvent(i)));
	return getSASDiagnosticOrDash(szPrereqs);
}

static CvString getSASGameRecordRandomEventTriggerPythonHooks(CvEventTriggerInfo const& kTrigger)
{
	CvString szHooks;
	if (kTrigger.getPythonCallback() != NULL && kTrigger.getPythonCallback()[0] != '\0') appendSASGameRecordType(szHooks, "CALLBACK");
	if (kTrigger.getPythonCanDo() != NULL && kTrigger.getPythonCanDo()[0] != '\0') appendSASGameRecordType(szHooks, "CAN_DO");
	if (kTrigger.getPythonCanDoCity() != NULL && kTrigger.getPythonCanDoCity()[0] != '\0') appendSASGameRecordType(szHooks, "CAN_DO_CITY");
	if (kTrigger.getPythonCanDoUnit() != NULL && kTrigger.getPythonCanDoUnit()[0] != '\0') appendSASGameRecordType(szHooks, "CAN_DO_UNIT");
	return getSASDiagnosticOrDash(szHooks);
}

static CvString getSASGameRecordRandomEventPythonHooks(CvEventInfo const& kEvent)
{
	CvString szHooks;
	if (kEvent.getPythonCallback() != NULL && kEvent.getPythonCallback()[0] != '\0') appendSASGameRecordType(szHooks, "CALLBACK");
	if (kEvent.getPythonCanDo() != NULL && kEvent.getPythonCanDo()[0] != '\0') appendSASGameRecordType(szHooks, "CAN_DO");
	if (kEvent.getPythonExpireCheck() != NULL && kEvent.getPythonExpireCheck()[0] != '\0') appendSASGameRecordType(szHooks, "EXPIRE_CHECK");
	return getSASDiagnosticOrDash(szHooks);
}

// <!-- custom: Civ4 EventInfo/random-event rows use dedicated RANDOM_EVENT_* row names so they cannot be confused with generic GAME_RECORD_ACTION chronology or Python/CvEventReporter callbacks.
// Target existence is sampled only at actual trigger/reply boundaries. A -1 existence value means that no concrete target of that kind was stored; 0 means an ID/coordinate was stored but no longer resolves, which is important for stale-popup diagnostics such as KI#809/KI#810. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
struct SASGameRecordRandomEventTargets
{
	SASGameRecordRandomEventTargets(CvPlayer const& kPlayer, EventTriggeredData const* pData, EventTypes eEvent)
	: iCityId(-1), iCityExists(-1), eOtherPlayer(NO_PLAYER), iOtherPlayerAlive(-1), iOtherCityId(-1), iOtherCityExists(-1),
	  iUnitId(-1), iUnitExists(-1), iUnitCanApply(-1), szUnit("-"), iPlotX(INVALID_PLOT_COORD), iPlotY(INVALID_PLOT_COORD), iPlotExists(-1), iPlotOwner(NO_PLAYER),
	  eReligion(NO_RELIGION), eCorporation(NO_CORPORATION), eBuilding(NO_BUILDING), iBuildingPresentInCity(-1)
	{
		if (pData == NULL)
			return;
		iCityId = pData->m_iCityId;
		if (iCityId >= 0)
			iCityExists = (kPlayer.getCity(iCityId) != NULL);
		eOtherPlayer = pData->m_eOtherPlayer;
		if (eOtherPlayer != NO_PLAYER && eOtherPlayer >= 0 && eOtherPlayer < MAX_PLAYERS)
		{
			iOtherPlayerAlive = GET_PLAYER(eOtherPlayer).isAlive();
			iOtherCityId = pData->m_iOtherPlayerCityId;
			if (iOtherCityId >= 0)
				iOtherCityExists = (GET_PLAYER(eOtherPlayer).getCity(iOtherCityId) != NULL);
		}
		else iOtherCityId = pData->m_iOtherPlayerCityId;
		iUnitId = pData->m_iUnitId;
		bool const bHasEvent = (eEvent != NO_EVENT);
		bool const bRequiresConcreteUnit = (bHasEvent && GC.getInfo(eEvent).hasUnitLocalEffect());
		if (iUnitId >= 0)
		{
			CvUnit const* pUnit = kPlayer.getUnit(iUnitId);
			iUnitExists = (pUnit != NULL);
			if (pUnit != NULL)
			{
				szUnit = getSASGameRecordUnitType(pUnit->getUnitType());
				if (bHasEvent)
					iUnitCanApply = pUnit->canApplyEvent(eEvent);
			}
			else if (bHasEvent) iUnitCanApply = 0;
		}
		else if (bRequiresConcreteUnit)
		{
			iUnitExists = 0;
			iUnitCanApply = 0;
		}
		iPlotX = pData->m_iPlotX;
		iPlotY = pData->m_iPlotY;
		if (iPlotX != INVALID_PLOT_COORD && iPlotY != INVALID_PLOT_COORD)
		{
			CvPlot const* pPlot = GC.getMap().plot(iPlotX, iPlotY);
			iPlotExists = (pPlot != NULL);
			if (pPlot != NULL)
				iPlotOwner = pPlot->getOwner();
		}
		eReligion = pData->m_eReligion;
		eCorporation = pData->m_eCorporation;
		eBuilding = pData->m_eBuilding;
		if (eBuilding != NO_BUILDING)
		{
			CvCity const* pCity = (iCityId < 0 ? NULL : kPlayer.getCity(iCityId));
			iBuildingPresentInCity = (pCity != NULL && pCity->getNumRealBuilding(eBuilding) > 0);
		}
	}
	int iCityId;
	int iCityExists;
	PlayerTypes eOtherPlayer;
	int iOtherPlayerAlive;
	int iOtherCityId;
	int iOtherCityExists;
	int iUnitId;
	int iUnitExists;
	int iUnitCanApply;
	char const* szUnit;
	int iPlotX;
	int iPlotY;
	int iPlotExists;
	PlayerTypes iPlotOwner;
	ReligionTypes eReligion;
	CorporationTypes eCorporation;
	BuildingTypes eBuilding;
	int iBuildingPresentInCity;
};

static char const* getSASGameRecordRandomEventApplyPath(CvPlayer const& kPlayer, EventTypes eEvent, int iTriggeredId, bool bUpdateTrigger, int& iCountdownDueTurn)
{
	iCountdownDueTurn = -1;
	if (!bUpdateTrigger)
		return "ADDITIONAL_IMMEDIATE";
	EventTriggeredData const* pCountdown = kPlayer.getEventCountdown(eEvent);
	if (pCountdown != NULL && pCountdown->m_iId == iTriggeredId && GC.getGame().getGameTurn() >= pCountdown->m_iTurn)
	{
		iCountdownDueTurn = pCountdown->m_iTurn;
		return "ADDITIONAL_COUNTDOWN";
	}
	return "PRIMARY_REPLY_OR_DIRECT";
}

// <!-- custom: Record only a trigger instance that has actually reached CvPlayer::trigger after target construction/Python trigger mutation and weighted selection.
// Candidate getEventTriggerWeight/initTriggeredData searches remain intentionally absent; the row describes the concrete stored instance that will be delivered to a human popup or immediately resolved by AI. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
void logSASGameRecordRandomEventTriggered(CvPlayer const& kPlayer, EventTriggeredData const& kTriggeredData, char const* szDeliveryPath)
{
	CvEventTriggerInfo const& kTrigger = GC.getInfo(kTriggeredData.m_eTrigger);
	SASGameRecordRandomEventTargets const kTargets(kPlayer, &kTriggeredData, NO_EVENT);
	logSASGameRecord("GAME_RECORD_RANDOM_EVENT_TRIGGERED turn=%d player=%d team=%d triggeredId=%d trigger=%s delivery=%s triggerTurn=%d normalSelectionMode=%s recurring=%d global=%d teamScope=%d singlePlayer=%d plotEvent=%d triggerFiredBeforeDelivery=%d prereqEvents=%s prereqEventCity=%d pythonHooks=%s cityId=%d cityExists=%d otherPlayer=%d otherPlayerAlive=%d otherCityId=%d otherCityExists=%d unitId=%d unitExists=%d unit=%s plot=%d,%d plotExists=%d plotOwner=%d religion=%s corporation=%s building=%s buildingPresentInCity=%d",
			GC.getGame().getGameTurn(), kPlayer.getID(), kPlayer.getTeam(), kTriggeredData.m_iId, getSASGameRecordEventTriggerType(kTriggeredData.m_eTrigger), szDeliveryPath, kTriggeredData.m_iTurn, getSASGameRecordRandomEventNormalSelectionMode(kTrigger),
			kTrigger.isRecurring(), kTrigger.isGlobal(), kTrigger.isTeam(), kTrigger.isSinglePlayer(), kTrigger.isPlotEventTrigger(), kPlayer.isTriggerFired(kTriggeredData.m_eTrigger), getSASGameRecordRandomEventTriggerPrereqs(kTrigger).GetCString(), kTrigger.isPrereqEventCity(), getSASGameRecordRandomEventTriggerPythonHooks(kTrigger).GetCString(),
			kTargets.iCityId, kTargets.iCityExists, kTargets.eOtherPlayer, kTargets.iOtherPlayerAlive, kTargets.iOtherCityId, kTargets.iOtherCityExists,
			kTargets.iUnitId, kTargets.iUnitExists, kTargets.szUnit, kTargets.iPlotX, kTargets.iPlotY, kTargets.iPlotExists, kTargets.iPlotOwner,
			getSASGameRecordReligionType(kTargets.eReligion), getSASGameRecordCorporationType(kTargets.eCorporation), getSASGameRecordBuildingType(kTargets.eBuilding), kTargets.iBuildingPresentInCity);
}

// <!-- custom: An AI can reach the real trigger-delivery boundary yet find no currently legal EventInfo after its normal canDoEvent/AI_eventValue search.
// Record only that final NO_EVENT resolution, never the candidate values/search itself, so a delivered trigger cannot silently disappear from lifecycle history. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
void logSASGameRecordRandomEventNoSelection(CvPlayer const& kPlayer, EventTriggeredData const& kTriggeredData, char const* szResolution)
{
	logSASGameRecord("GAME_RECORD_RANDOM_EVENT_NO_SELECTION turn=%d player=%d team=%d triggeredId=%d trigger=%s resolution=%s triggerTurn=%d ageTurns=%d",
			GC.getGame().getGameTurn(), kPlayer.getID(), kPlayer.getTeam(), kTriggeredData.m_iId, getSASGameRecordEventTriggerType(kTriggeredData.m_eTrigger), szResolution,
			kTriggeredData.m_iTurn, GC.getGame().getGameTurn() - kTriggeredData.m_iTurn);
}

// <!-- custom: This is the synchronized/random-event transaction diagnostic rather than a static EventInfo dump.
// For rejected replies, the row is emitted before triggered-data deletion; ACCEPTED is emitted immediately after setEventOccured and before downstream payload/RNG effects, while specialized result rows preserve realized outcomes.
// That boundary makes KI#809 visible as requiresConcreteUnit=1 + unitExists=0 + canDoEvent=1, and KI#810 as REJECTED_CAN_DO with triggerFiredBefore=0/triggerFiredAfter=1 while eventOccurredAfter remains 0, without repairing either defect here. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
void logSASGameRecordRandomEventApply(CvPlayer const& kPlayer, EventTypes eEvent, int iTriggeredId, EventTriggeredData const* pTriggeredData, bool bUpdateTrigger, char const* szDisposition, int iCanDoEvent, int iTriggerFiredBefore, int iEventOccurredBefore)
{
	CvEventInfo const& kEvent = GC.getInfo(eEvent);
	EventTriggerTypes const eTrigger = (pTriggeredData == NULL ? NO_EVENTTRIGGER : pTriggeredData->m_eTrigger);
	CvEventTriggerInfo const* pTrigger = (eTrigger == NO_EVENTTRIGGER ? NULL : &GC.getInfo(eTrigger));
	SASGameRecordRandomEventTargets const kTargets(kPlayer, pTriggeredData, eEvent);
	int iCountdownDueTurn = -1;
	char const* szApplyPath = getSASGameRecordRandomEventApplyPath(kPlayer, eEvent, iTriggeredId, bUpdateTrigger, iCountdownDueTurn);
	int const iTriggerTurn = (pTriggeredData == NULL ? -1 : pTriggeredData->m_iTurn);
	int const iReplyAgeTurns = (iTriggerTurn < 0 ? -1 : GC.getGame().getGameTurn() - iTriggerTurn);
	int const iTriggerFiredAfter = (eTrigger == NO_EVENTTRIGGER ? -1 : kPlayer.isTriggerFired(eTrigger));
	int const iEventOccurredAfter = (kPlayer.getEventOccured(eEvent) != NULL);
	logSASGameRecord("GAME_RECORD_RANDOM_EVENT_APPLY turn=%d player=%d team=%d triggeredId=%d trigger=%s event=%s applyPath=%s bUpdateTrigger=%d disposition=%s canDoEvent=%d triggerTurn=%d replyAgeTurns=%d countdownDueTurn=%d triggerNormalSelectionMode=%s triggerRecurring=%d triggerGlobal=%d triggerTeam=%d triggerSinglePlayer=%d triggerPlotEvent=%d triggerPrereqEvents=%s triggerPrereqEventCity=%d triggerPythonHooks=%s eventQuest=%d eventGlobal=%d eventTeam=%d eventCityEffect=%d eventOtherCityEffect=%d eventEffects=%s eventPythonHooks=%s triggerFiredBefore=%d triggerFiredAfter=%d eventOccurredBefore=%d eventOccurredAfter=%d requiresConcreteUnit=%d unitId=%d unitExists=%d unit=%s unitCanApply=%d cityId=%d cityExists=%d otherPlayer=%d otherPlayerAlive=%d otherCityId=%d otherCityExists=%d plot=%d,%d plotExists=%d plotOwner=%d religion=%s corporation=%s building=%s buildingPresentInCity=%d",
			GC.getGame().getGameTurn(), kPlayer.getID(), kPlayer.getTeam(), iTriggeredId, getSASGameRecordEventTriggerType(eTrigger), getSASGameRecordEventType(eEvent), szApplyPath, bUpdateTrigger, szDisposition, iCanDoEvent,
			iTriggerTurn, iReplyAgeTurns, iCountdownDueTurn, pTrigger == NULL ? "-" : getSASGameRecordRandomEventNormalSelectionMode(*pTrigger), pTrigger == NULL ? -1 : pTrigger->isRecurring(), pTrigger == NULL ? -1 : pTrigger->isGlobal(), pTrigger == NULL ? -1 : pTrigger->isTeam(), pTrigger == NULL ? -1 : pTrigger->isSinglePlayer(), pTrigger == NULL ? -1 : pTrigger->isPlotEventTrigger(),
			pTrigger == NULL ? "-" : getSASGameRecordRandomEventTriggerPrereqs(*pTrigger).GetCString(), pTrigger == NULL ? -1 : pTrigger->isPrereqEventCity(), pTrigger == NULL ? "-" : getSASGameRecordRandomEventTriggerPythonHooks(*pTrigger).GetCString(),
			kEvent.isQuest(), kEvent.isGlobal(), kEvent.isTeam(), kEvent.isCityEffect(), kEvent.isOtherPlayerCityEffect(), getSASGameRecordRandomEventEffects(kEvent).GetCString(), getSASGameRecordRandomEventPythonHooks(kEvent).GetCString(), iTriggerFiredBefore, iTriggerFiredAfter, iEventOccurredBefore, iEventOccurredAfter, kEvent.hasUnitLocalEffect(),
			kTargets.iUnitId, kTargets.iUnitExists, kTargets.szUnit, kTargets.iUnitCanApply, kTargets.iCityId, kTargets.iCityExists, kTargets.eOtherPlayer, kTargets.iOtherPlayerAlive, kTargets.iOtherCityId, kTargets.iOtherCityExists,
			kTargets.iPlotX, kTargets.iPlotY, kTargets.iPlotExists, kTargets.iPlotOwner, getSASGameRecordReligionType(kTargets.eReligion), getSASGameRecordCorporationType(kTargets.eCorporation), getSASGameRecordBuildingType(kTargets.eBuilding), kTargets.iBuildingPresentInCity);
}

// <!-- custom: EventInfo gold can come from fixed/random gold or a dynamically selected technology-cost percentage.
// Reuse the exact already-computed cost endpoints/result from CvPlayer::applyEvent so logging captures the realized treasury transaction without additional RNG, tech selection, or event-cost calculation. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
void logSASGameRecordRandomEventGoldResult(CvPlayer const& kPlayer, EventTypes eEvent, int iTriggeredId, int iRangeLow, int iRangeHigh, int iPlayerGoldDelta, PlayerTypes eOtherPlayer, bool bGoldToPlayer)
{
	int const iOtherGoldDelta = (bGoldToPlayer && eOtherPlayer != NO_PLAYER ? -iPlayerGoldDelta : 0);
	logSASGameRecord("GAME_RECORD_RANDOM_EVENT_GOLD_RESULT turn=%d player=%d team=%d triggeredId=%d event=%s rangeLow=%d rangeHigh=%d playerGoldDelta=%d otherPlayer=%d otherGoldDelta=%d goldToPlayer=%d",
			GC.getGame().getGameTurn(), kPlayer.getID(), kPlayer.getTeam(), iTriggeredId, getSASGameRecordEventType(eEvent), iRangeLow, iRangeHigh, iPlayerGoldDelta, eOtherPlayer, iOtherGoldDelta, bGoldToPlayer);
}

// <!-- custom: A random EventInfo can dynamically choose a research target and apply only partial progress.
// Record the already-selected tech and actual signed beaker result after gameplay applies it; TECH_ACQUIRED remains canonical if the event completes the technology.
// No extra tech search or RNG is performed for logging. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
void logSASGameRecordRandomEventTechResult(CvPlayer const& kPlayer, EventTypes eEvent, int iTriggeredId, TechTypes eTech, int iTechPercent, int iResearchBefore, int iBeakersApplied, int iResearchAfter, int iTechCost, int iCompleted)
{
	logSASGameRecord("GAME_RECORD_RANDOM_EVENT_TECH_RESULT turn=%d player=%d team=%d triggeredId=%d event=%s tech=%s techPercent=%d researchBefore=%d beakersApplied=%d researchAfter=%d techCost=%d completed=%d",
			GC.getGame().getGameTurn(), kPlayer.getID(), kPlayer.getTeam(), iTriggeredId, getSASGameRecordEventType(eEvent), getSASGameRecordTechType(eTech), iTechPercent, iResearchBefore, iBeakersApplied, iResearchAfter, iTechCost, iCompleted);
}

SASGameRecordRandomEventCityState::SASGameRecordRandomEventCityState() :
		iPopulation(-1), iFood(-1), iFoodYield(-1), iProductionYield(-1), iCommerceYield(-1),
		iGoldRate(-1), iResearchRate(-1), iCultureRate(-1), iEspionageRate(-1), iOwnerCultureTimes100(-1),
		iOccupationTurns(-1), iCultureUpdateTurns(-1), iExtraHappiness(-1), iExtraHealth(-1),
		iHurryAngerTurns(-1), iHappinessTurns(-1), iAngryPopulation(-1), iHappyLevel(-1), iUnhappyLevel(-1),
		iGoodHealth(-1), iBadHealth(-1), iSpaceProductionModifier(-1), iFreeSpecialistInstances(-1),
		eBuilding(NO_BUILDING), iRealBuildingCount(-1)
{}

SASGameRecordRandomEventCityState::SASGameRecordRandomEventCityState(CvCity const& kCity, EventTypes eEvent) :
		iPopulation(kCity.getPopulation()), iFood(kCity.getFood()), iFoodYield(kCity.getYieldRate(YIELD_FOOD)),
		iProductionYield(kCity.getYieldRate(YIELD_PRODUCTION)), iCommerceYield(kCity.getYieldRate(YIELD_COMMERCE)),
		iGoldRate(kCity.getCommerceRate(COMMERCE_GOLD)), iResearchRate(kCity.getCommerceRate(COMMERCE_RESEARCH)),
		iCultureRate(kCity.getCommerceRate(COMMERCE_CULTURE)), iEspionageRate(kCity.getCommerceRate(COMMERCE_ESPIONAGE)),
		iOwnerCultureTimes100(kCity.getCultureTimes100(kCity.getOwner())), iOccupationTurns(kCity.getOccupationTimer()),
		iCultureUpdateTurns(kCity.getCultureUpdateTimer()), iExtraHappiness(kCity.getExtraHappiness()),
		iExtraHealth(kCity.getExtraHealth()), iHurryAngerTurns(kCity.getHurryAngerTimer()),
		iHappinessTurns(kCity.getHappinessTimer()), iAngryPopulation(kCity.angryPopulation()), iHappyLevel(kCity.happyLevel()),
		iUnhappyLevel(kCity.unhappyLevel()), iGoodHealth(kCity.goodHealth()), iBadHealth(kCity.badHealth()),
		iSpaceProductionModifier(kCity.getSpaceProductionModifier()), iFreeSpecialistInstances(kCity.getNumGreatPeople()),
		eBuilding(NO_BUILDING), iRealBuildingCount(-1)
{
	CvEventInfo const& kEvent = GC.getInfo(eEvent);
	BuildingClassTypes const eBuildingClass = (BuildingClassTypes)kEvent.getBuildingClass();
	if (eBuildingClass != NO_BUILDINGCLASS)
	{
		eBuilding = kCity.getCivilization().getBuilding(eBuildingClass);
		if (eBuilding != NO_BUILDING)
			iRealBuildingCount = kCity.getNumRealBuilding(eBuilding);
	}
}

// <!-- custom: Keep deterministic EventInfo city consequences compact and realized: record before/after state only when something actually changed.
// Include immediate yield/commerce output so building modifiers do not disappear until a later periodic snapshot; plot pillage, free units, gold and tech retain their specialized result rows. (ChatGPT-5.6-Sol) -->
void logSASGameRecordRandomEventCityResult(PlayerTypes ePlayer, PlayerTypes eAffectedPlayer, int iTriggeredId, EventTypes eEvent, char const* szScope, CvCity const& kCity, SASGameRecordRandomEventCityState const& kBefore, SASGameRecordRandomEventCityState const& kAfter)
{
	bool const bChanged = (kBefore.iPopulation != kAfter.iPopulation || kBefore.iFood != kAfter.iFood ||
			kBefore.iFoodYield != kAfter.iFoodYield || kBefore.iProductionYield != kAfter.iProductionYield ||
			kBefore.iCommerceYield != kAfter.iCommerceYield || kBefore.iGoldRate != kAfter.iGoldRate ||
			kBefore.iResearchRate != kAfter.iResearchRate || kBefore.iCultureRate != kAfter.iCultureRate ||
			kBefore.iEspionageRate != kAfter.iEspionageRate || kBefore.iOwnerCultureTimes100 != kAfter.iOwnerCultureTimes100 ||
			kBefore.iOccupationTurns != kAfter.iOccupationTurns || kBefore.iCultureUpdateTurns != kAfter.iCultureUpdateTurns ||
			kBefore.iExtraHappiness != kAfter.iExtraHappiness || kBefore.iExtraHealth != kAfter.iExtraHealth ||
			kBefore.iHurryAngerTurns != kAfter.iHurryAngerTurns || kBefore.iHappinessTurns != kAfter.iHappinessTurns ||
			kBefore.iAngryPopulation != kAfter.iAngryPopulation || kBefore.iHappyLevel != kAfter.iHappyLevel ||
			kBefore.iUnhappyLevel != kAfter.iUnhappyLevel || kBefore.iGoodHealth != kAfter.iGoodHealth ||
			kBefore.iBadHealth != kAfter.iBadHealth || kBefore.iSpaceProductionModifier != kAfter.iSpaceProductionModifier ||
			kBefore.iFreeSpecialistInstances != kAfter.iFreeSpecialistInstances || kBefore.eBuilding != kAfter.eBuilding ||
			kBefore.iRealBuildingCount != kAfter.iRealBuildingCount);
	if (!bChanged)
		return;
	logSASGameRecord("GAME_RECORD_RANDOM_EVENT_CITY_RESULT turn=%d player=%d team=%d affectedPlayer=%d affectedTeam=%d triggeredId=%d event=%s scope=%s cityId=%d city=%S x=%d y=%d populationBefore=%d populationAfter=%d populationDelta=%+d foodBefore=%d foodAfter=%d foodDelta=%+d foodYieldBefore=%d foodYieldAfter=%d productionYieldBefore=%d productionYieldAfter=%d commerceYieldBefore=%d commerceYieldAfter=%d goldRateBefore=%d goldRateAfter=%d researchRateBefore=%d researchRateAfter=%d cultureRateBefore=%d cultureRateAfter=%d espionageRateBefore=%d espionageRateAfter=%d ownerCultureTimes100Before=%d ownerCultureTimes100After=%d ownerCultureTimes100Delta=%+d occupationTurnsBefore=%d occupationTurnsAfter=%d cultureUpdateTurnsBefore=%d cultureUpdateTurnsAfter=%d extraHappinessBefore=%d extraHappinessAfter=%d extraHealthBefore=%d extraHealthAfter=%d hurryAngerTurnsBefore=%d hurryAngerTurnsAfter=%d happinessTurnsBefore=%d happinessTurnsAfter=%d angryPopulationBefore=%d angryPopulationAfter=%d happyLevelBefore=%d happyLevelAfter=%d unhappyLevelBefore=%d unhappyLevelAfter=%d goodHealthBefore=%d goodHealthAfter=%d badHealthBefore=%d badHealthAfter=%d spaceProductionModifierBefore=%d spaceProductionModifierAfter=%d freeSpecialistInstancesBefore=%d freeSpecialistInstancesAfter=%d building=%s realBuildingCountBefore=%d realBuildingCountAfter=%d",
			GC.getGame().getGameTurn(), ePlayer, (ePlayer == NO_PLAYER ? NO_TEAM : GET_PLAYER(ePlayer).getTeam()), eAffectedPlayer,
			(eAffectedPlayer == NO_PLAYER ? NO_TEAM : GET_PLAYER(eAffectedPlayer).getTeam()), iTriggeredId, getSASGameRecordEventType(eEvent), szScope,
			kCity.getID(), getSASGameRecordQuotedCityName(&kCity).GetCString(), kCity.getX(), kCity.getY(),
			kBefore.iPopulation, kAfter.iPopulation, kAfter.iPopulation - kBefore.iPopulation, kBefore.iFood, kAfter.iFood, kAfter.iFood - kBefore.iFood,
			kBefore.iFoodYield, kAfter.iFoodYield, kBefore.iProductionYield, kAfter.iProductionYield, kBefore.iCommerceYield, kAfter.iCommerceYield,
			kBefore.iGoldRate, kAfter.iGoldRate, kBefore.iResearchRate, kAfter.iResearchRate, kBefore.iCultureRate, kAfter.iCultureRate, kBefore.iEspionageRate, kAfter.iEspionageRate,
			kBefore.iOwnerCultureTimes100, kAfter.iOwnerCultureTimes100, kAfter.iOwnerCultureTimes100 - kBefore.iOwnerCultureTimes100,
			kBefore.iOccupationTurns, kAfter.iOccupationTurns, kBefore.iCultureUpdateTurns, kAfter.iCultureUpdateTurns,
			kBefore.iExtraHappiness, kAfter.iExtraHappiness, kBefore.iExtraHealth, kAfter.iExtraHealth, kBefore.iHurryAngerTurns, kAfter.iHurryAngerTurns,
			kBefore.iHappinessTurns, kAfter.iHappinessTurns, kBefore.iAngryPopulation, kAfter.iAngryPopulation, kBefore.iHappyLevel, kAfter.iHappyLevel,
			kBefore.iUnhappyLevel, kAfter.iUnhappyLevel, kBefore.iGoodHealth, kAfter.iGoodHealth, kBefore.iBadHealth, kAfter.iBadHealth,
			kBefore.iSpaceProductionModifier, kAfter.iSpaceProductionModifier, kBefore.iFreeSpecialistInstances, kAfter.iFreeSpecialistInstances,
			getSASGameRecordBuildingType(kAfter.eBuilding != NO_BUILDING ? kAfter.eBuilding : kBefore.eBuilding), kBefore.iRealBuildingCount, kAfter.iRealBuildingCount);
}


// <!-- custom: Building-modifier EventInfos can create durable latent state even when no current building/output changes.
// Keep one compact realized operation row per configured modifier: selected-city modifiers retain their stored after value, empire yield/commerce modifiers name the current-city scope, and empire happiness/health modifiers retain the resolved player-level building modifier before/after. (ChatGPT-5.6-Sol) -->
void logSASGameRecordRandomEventBuildingModifierResults(CvPlayer const& kPlayer, EventTypes eEvent, int iTriggeredId, char const* szScope, CvCity const* pCity)
{
	CvEventInfo const& kEvent = GC.getInfo(eEvent);
	bool const bCityScope = (pCity != NULL);
	PlayerTypes const eAffectedPlayer = (bCityScope ? pCity->getOwner() : kPlayer.getID());
	int const iCityId = (bCityScope ? pCity->getID() : -1);
	int const iAffectedCityCount = (bCityScope ? 1 : kPlayer.getNumCities());

	FOR_EACH_NON_DEFAULT_PAIR(kEvent.getBuildingYieldChange(), BuildingClass, YieldChangeMap)
	{
		FOR_EACH_NON_DEFAULT_PAIR(perBuildingClassVal.second, Yield, int)
		{
			BuildingTypes const eBuilding = (bCityScope ? pCity->getCivilization().getBuilding(perBuildingClassVal.first) :
					kPlayer.getCivilization().getBuilding(perBuildingClassVal.first));
			int iBefore = -1;
			int iAfter = -1;
			if (bCityScope)
			{
				iAfter = pCity->getBuildingYieldChange(perBuildingClassVal.first, perYieldVal.first);
				iBefore = iAfter - perYieldVal.second;
			}
			logSASGameRecord("GAME_RECORD_RANDOM_EVENT_BUILDING_MODIFIER_RESULT turn=%d player=%d team=%d triggeredId=%d event=%s scope=%s target=%s affectedPlayer=%d affectedTeam=%d cityId=%d affectedCityCount=%d modifier=YIELD buildingClass=%s building=%s subType=%s operation=ADD configuredValue=%+d valueBefore=%d valueAfter=%d",
					GC.getGame().getGameTurn(), kPlayer.getID(), kPlayer.getTeam(), iTriggeredId, getSASGameRecordEventType(eEvent), szScope,
					bCityScope ? "CITY_STORED_MODIFIER" : "CURRENT_CITIES_STORED_MODIFIER", eAffectedPlayer,
					GET_PLAYER(eAffectedPlayer).getTeam(), iCityId, iAffectedCityCount,
					GC.getInfo(perBuildingClassVal.first).getType(), getSASGameRecordBuildingType(eBuilding), GC.getInfo(perYieldVal.first).getType(),
					perYieldVal.second, iBefore, iAfter);
		}
	}
	FOR_EACH_NON_DEFAULT_PAIR(kEvent.getBuildingCommerceChange(), BuildingClass, CommerceChangeMap)
	{
		FOR_EACH_NON_DEFAULT_PAIR(perBuildingClassVal.second, Commerce, int)
		{
			BuildingTypes const eBuilding = (bCityScope ? pCity->getCivilization().getBuilding(perBuildingClassVal.first) :
					kPlayer.getCivilization().getBuilding(perBuildingClassVal.first));
			int iBefore = -1;
			int iAfter = -1;
			if (bCityScope)
			{
				iAfter = pCity->getBuildingCommerceChange(perBuildingClassVal.first, perCommerceVal.first);
				iBefore = iAfter - perCommerceVal.second;
			}
			logSASGameRecord("GAME_RECORD_RANDOM_EVENT_BUILDING_MODIFIER_RESULT turn=%d player=%d team=%d triggeredId=%d event=%s scope=%s target=%s affectedPlayer=%d affectedTeam=%d cityId=%d affectedCityCount=%d modifier=COMMERCE buildingClass=%s building=%s subType=%s operation=ADD configuredValue=%+d valueBefore=%d valueAfter=%d",
					GC.getGame().getGameTurn(), kPlayer.getID(), kPlayer.getTeam(), iTriggeredId, getSASGameRecordEventType(eEvent), szScope,
					bCityScope ? "CITY_STORED_MODIFIER" : "CURRENT_CITIES_STORED_MODIFIER", eAffectedPlayer,
					GET_PLAYER(eAffectedPlayer).getTeam(), iCityId, iAffectedCityCount,
					GC.getInfo(perBuildingClassVal.first).getType(), getSASGameRecordBuildingType(eBuilding), getSASGameRecordCommerceType(perCommerceVal.first),
					perCommerceVal.second, iBefore, iAfter);
		}
	}
	FOR_EACH_NON_DEFAULT_PAIR(kEvent.getBuildingHappyChange(), BuildingClass, int)
	{
		BuildingTypes const eBuilding = (bCityScope ? pCity->getCivilization().getBuilding(perBuildingClassVal.first) :
				kPlayer.getCivilization().getBuilding(perBuildingClassVal.first));
		int iBefore = -1;
		int iAfter = -1;
		char const* szTarget = "CITY_STORED_MODIFIER";
		char const* szOperation = "SET";
		if (bCityScope)
			iAfter = pCity->getBuildingHappyChange(perBuildingClassVal.first);
		else
		{
			szTarget = "PLAYER_BUILDING_MODIFIER";
			szOperation = "ADD";
			if (eBuilding != NO_BUILDING)
			{
				iAfter = kPlayer.getExtraBuildingHappiness(eBuilding);
				iBefore = iAfter - perBuildingClassVal.second;
			}
		}
		logSASGameRecord("GAME_RECORD_RANDOM_EVENT_BUILDING_MODIFIER_RESULT turn=%d player=%d team=%d triggeredId=%d event=%s scope=%s target=%s affectedPlayer=%d affectedTeam=%d cityId=%d affectedCityCount=%d modifier=HAPPINESS buildingClass=%s building=%s subType=- operation=%s configuredValue=%+d valueBefore=%d valueAfter=%d",
				GC.getGame().getGameTurn(), kPlayer.getID(), kPlayer.getTeam(), iTriggeredId, getSASGameRecordEventType(eEvent), szScope, szTarget,
				eAffectedPlayer, GET_PLAYER(eAffectedPlayer).getTeam(), iCityId, bCityScope ? 1 : -1, GC.getInfo(perBuildingClassVal.first).getType(), getSASGameRecordBuildingType(eBuilding),
				szOperation, perBuildingClassVal.second, iBefore, iAfter);
	}
	FOR_EACH_NON_DEFAULT_PAIR(kEvent.getBuildingHealthChange(), BuildingClass, int)
	{
		BuildingTypes const eBuilding = (bCityScope ? pCity->getCivilization().getBuilding(perBuildingClassVal.first) :
				kPlayer.getCivilization().getBuilding(perBuildingClassVal.first));
		int iBefore = -1;
		int iAfter = -1;
		char const* szTarget = "CITY_STORED_MODIFIER";
		char const* szOperation = "SET";
		if (bCityScope)
			iAfter = pCity->getBuildingHealthChange(perBuildingClassVal.first);
		else
		{
			szTarget = "PLAYER_BUILDING_MODIFIER";
			szOperation = "ADD";
			if (eBuilding != NO_BUILDING)
			{
				iAfter = kPlayer.getExtraBuildingHealth(eBuilding);
				iBefore = iAfter - perBuildingClassVal.second;
			}
		}
		logSASGameRecord("GAME_RECORD_RANDOM_EVENT_BUILDING_MODIFIER_RESULT turn=%d player=%d team=%d triggeredId=%d event=%s scope=%s target=%s affectedPlayer=%d affectedTeam=%d cityId=%d affectedCityCount=%d modifier=HEALTH buildingClass=%s building=%s subType=- operation=%s configuredValue=%+d valueBefore=%d valueAfter=%d",
				GC.getGame().getGameTurn(), kPlayer.getID(), kPlayer.getTeam(), iTriggeredId, getSASGameRecordEventType(eEvent), szScope, szTarget,
				eAffectedPlayer, GET_PLAYER(eAffectedPlayer).getTeam(), iCityId, bCityScope ? 1 : -1, GC.getInfo(perBuildingClassVal.first).getType(), getSASGameRecordBuildingType(eBuilding),
				szOperation, perBuildingClassVal.second, iBefore, iAfter);
	}
}

SASGameRecordRandomEventUnitState::SASGameRecordRandomEventUnitState() :
		iExists(0), iUnitId(-1), eUnit(NO_UNIT), eUnitAI(NO_UNITAI), iX(INVALID_PLOT_COORD), iY(INVALID_PLOT_COORD),
		iDamage(-1), iExperience(-1), iImmobileTurns(-1), ePromotion(NO_PROMOTION), iHasPromotion(-1)
{}

SASGameRecordRandomEventUnitState::SASGameRecordRandomEventUnitState(CvUnit const& kUnit, EventTypes eEvent) :
		iExists(1), iUnitId(kUnit.getID()), eUnit(kUnit.getUnitType()), eUnitAI(kUnit.AI_getUnitAIType()), iX(kUnit.getX()), iY(kUnit.getY()),
		iDamage(kUnit.getDamage()), iExperience(kUnit.getExperience()), iImmobileTurns(kUnit.getImmobileTimer()),
		ePromotion((PromotionTypes)GC.getInfo(eEvent).getUnitPromotion()),
		iHasPromotion(ePromotion == NO_PROMOTION ? -1 : kUnit.isHasPromotion(ePromotion))
{}

// <!-- custom: Preserve the concrete stored-unit result after CvUnit::applyEvent rather than only the EventInfo's UNIT_LOCAL label.
// A disbanded unit is represented by existsAfter=0 and sentinel after-state values; no dead object is dereferenced after the original kill. (ChatGPT-5.6-Sol) -->
void logSASGameRecordRandomEventUnitResult(PlayerTypes ePlayer, int iTriggeredId, EventTypes eEvent, SASGameRecordRandomEventUnitState const& kBefore, SASGameRecordRandomEventUnitState const& kAfter)
{
	CvEventInfo const& kEvent = GC.getInfo(eEvent);
	if (!kBefore.iExists)
		return;
	CvWString const szUnitNameKey(kEvent.getUnitNameKey());
	bool const bRenameApplied = !szUnitNameKey.empty();
	bool const bChanged = (!kAfter.iExists || kBefore.iDamage != kAfter.iDamage || kBefore.iExperience != kAfter.iExperience ||
			kBefore.iImmobileTurns != kAfter.iImmobileTurns || kBefore.iHasPromotion != kAfter.iHasPromotion ||
			kEvent.isDisbandUnit() || bRenameApplied);
	if (!bChanged)
		return;
	logSASGameRecord("GAME_RECORD_RANDOM_EVENT_UNIT_RESULT turn=%d player=%d team=%d triggeredId=%d event=%s unitId=%d unit=%s unitAI=%s x=%d y=%d existsBefore=%d existsAfter=%d damageBefore=%d damageAfter=%d damageDelta=%+d experienceBefore=%d experienceAfter=%d experienceDelta=%+d immobileTurnsBefore=%d immobileTurnsAfter=%d immobileTurnsDelta=%+d promotion=%s hadPromotionBefore=%d hasPromotionAfter=%d unitNameKey=%S renameApplied=%d disbanded=%d",
			GC.getGame().getGameTurn(), ePlayer, ePlayer == NO_PLAYER ? NO_TEAM : GET_PLAYER(ePlayer).getTeam(), iTriggeredId, getSASGameRecordEventType(eEvent),
			kBefore.iUnitId, getSASGameRecordUnitType(kBefore.eUnit), getSASGameRecordUnitAIType(kBefore.eUnitAI), kBefore.iX, kBefore.iY,
			kBefore.iExists, kAfter.iExists, kBefore.iDamage, kAfter.iDamage,
			(kAfter.iDamage < 0 ? -1 : kAfter.iDamage - kBefore.iDamage), kBefore.iExperience, kAfter.iExperience,
			(kAfter.iExperience < 0 ? -1 : kAfter.iExperience - kBefore.iExperience), kBefore.iImmobileTurns, kAfter.iImmobileTurns,
			(kAfter.iImmobileTurns < 0 ? -1 : kAfter.iImmobileTurns - kBefore.iImmobileTurns), getSASGameRecordPromotionType(kBefore.ePromotion),
			kBefore.iHasPromotion, kAfter.iHasPromotion, bRenameApplied ? kEvent.getUnitNameKey() : L"-",
			bRenameApplied, (kEvent.isDisbandUnit() && !kAfter.iExists));
}

SASGameRecordRandomEventPlayerState::SASGameRecordRandomEventPlayerState() :
		iExtraHappiness(-1), iExtraHealth(-1), iBaseFreeUnits(-1), iSpaceProductionModifier(-1), iInflationRate(-1),
		iEspionagePointsAgainstOther(-1), eBonusRevealed(NO_BONUS), iForceRevealedBonus(-1)
{}

SASGameRecordRandomEventPlayerState::SASGameRecordRandomEventPlayerState(CvPlayer const& kPlayer, EventTypes eEvent, PlayerTypes eOtherPlayer) :
		iExtraHappiness(kPlayer.getExtraHappiness()), iExtraHealth(kPlayer.getExtraHealth()), iBaseFreeUnits(kPlayer.getBaseFreeUnits()),
		iSpaceProductionModifier(kPlayer.getSpaceProductionModifier()), iInflationRate(kPlayer.calculateInflationRate()),
		iEspionagePointsAgainstOther(eOtherPlayer == NO_PLAYER ? -1 : GET_TEAM(kPlayer.getTeam()).getEspionagePointsAgainstTeam(GET_PLAYER(eOtherPlayer).getTeam())),
		eBonusRevealed((BonusTypes)GC.getInfo(eEvent).getBonusRevealed()),
		iForceRevealedBonus(eBonusRevealed == NO_BONUS ? -1 : GET_TEAM(kPlayer.getTeam()).isForceRevealedBonus(eBonusRevealed))
{}

// <!-- custom: Keep native player-level EventInfo consequences tied to the selected event without duplicating gold, tech, Golden Age or war rows.
// `inflationRate` is the public realized rate rather than the private raw modifier; configured modifier deltas are included only for the three direct player-modifier XML fields. (ChatGPT-5.6-Sol) -->
void logSASGameRecordRandomEventPlayerResult(CvPlayer const& kPlayer, EventTypes eEvent, int iTriggeredId, PlayerTypes eOtherPlayer, SASGameRecordRandomEventPlayerState const& kBefore, SASGameRecordRandomEventPlayerState const& kAfter)
{
	CvEventInfo const& kEvent = GC.getInfo(eEvent);
	bool const bChanged = (kBefore.iExtraHappiness != kAfter.iExtraHappiness || kBefore.iExtraHealth != kAfter.iExtraHealth ||
			kBefore.iBaseFreeUnits != kAfter.iBaseFreeUnits || kBefore.iSpaceProductionModifier != kAfter.iSpaceProductionModifier ||
			kBefore.iInflationRate != kAfter.iInflationRate || kBefore.iEspionagePointsAgainstOther != kAfter.iEspionagePointsAgainstOther ||
			kBefore.iForceRevealedBonus != kAfter.iForceRevealedBonus || kEvent.getInflationModifier() != 0);
	if (!bChanged)
		return;
	logSASGameRecord("GAME_RECORD_RANDOM_EVENT_PLAYER_RESULT turn=%d player=%d team=%d triggeredId=%d event=%s otherPlayer=%d otherTeam=%d playerExtraHappinessBefore=%d playerExtraHappinessAfter=%d playerExtraHealthBefore=%d playerExtraHealthAfter=%d baseFreeUnitsBefore=%d baseFreeUnitsAfter=%d configuredFreeUnitSupport=%+d spaceProductionModifierBefore=%d spaceProductionModifierAfter=%d configuredSpaceProductionModifier=%+d inflationRateBefore=%d inflationRateAfter=%d configuredInflationModifier=%+d espionagePointsAgainstOtherBefore=%d espionagePointsAgainstOtherAfter=%d espionagePointsDelta=%+d bonusRevealed=%s forceRevealedBefore=%d forceRevealedAfter=%d",
			GC.getGame().getGameTurn(), kPlayer.getID(), kPlayer.getTeam(), iTriggeredId, getSASGameRecordEventType(eEvent), eOtherPlayer,
			eOtherPlayer == NO_PLAYER ? NO_TEAM : GET_PLAYER(eOtherPlayer).getTeam(), kBefore.iExtraHappiness, kAfter.iExtraHappiness,
			kBefore.iExtraHealth, kAfter.iExtraHealth, kBefore.iBaseFreeUnits, kAfter.iBaseFreeUnits, kEvent.getFreeUnitSupport(),
			kBefore.iSpaceProductionModifier, kAfter.iSpaceProductionModifier, kEvent.getSpaceProductionModifier(), kBefore.iInflationRate, kAfter.iInflationRate,
			kEvent.getInflationModifier(), kBefore.iEspionagePointsAgainstOther, kAfter.iEspionagePointsAgainstOther,
			(kBefore.iEspionagePointsAgainstOther < 0 || kAfter.iEspionagePointsAgainstOther < 0 ? -1 : kAfter.iEspionagePointsAgainstOther - kBefore.iEspionagePointsAgainstOther),
			getSASGameRecordBonusType(kAfter.eBonusRevealed != NO_BONUS ? kAfter.eBonusRevealed : kBefore.eBonusRevealed), kBefore.iForceRevealedBonus, kAfter.iForceRevealedBonus);
}

// <!-- custom: EventInfos can grant a persistent free promotion to a whole UnitCombat or UnitClass while also updating existing matching units immediately.
// Record one realized scope row with the newly promoted existing-unit count instead of one row per unit. (ChatGPT-5.6-Sol) -->
void logSASGameRecordRandomEventFreePromotionResult(CvPlayer const& kPlayer, EventTypes eEvent, int iTriggeredId, char const* szScope, int iScopeId, PromotionTypes ePromotion, int iExistingUnitsNewlyPromoted, int iFreePromotionBefore, int iFreePromotionAfter)
{
	char const* szScopeType = "-";
	if (strcmp(szScope, "UNIT_COMBAT") == 0 && iScopeId >= 0 && iScopeId < GC.getNumUnitCombatInfos())
		szScopeType = getSASGameRecordUnitCombatType((UnitCombatTypes)iScopeId);
	else if (strcmp(szScope, "UNIT_CLASS") == 0 && iScopeId >= 0 && iScopeId < GC.getNumUnitClassInfos())
		szScopeType = GC.getInfo((UnitClassTypes)iScopeId).getType();
	logSASGameRecord("GAME_RECORD_RANDOM_EVENT_FREE_PROMOTION_RESULT turn=%d player=%d team=%d triggeredId=%d event=%s scope=%s scopeType=%s promotion=%s existingUnitsNewlyPromoted=%d freePromotionBefore=%d freePromotionAfter=%d",
			GC.getGame().getGameTurn(), kPlayer.getID(), kPlayer.getTeam(), iTriggeredId, getSASGameRecordEventType(eEvent), szScope, szScopeType,
			getSASGameRecordPromotionType(ePromotion), iExistingUnitsNewlyPromoted, iFreePromotionBefore, iFreePromotionAfter);
}

// <!-- custom: EventInfos can create civilization-specific free units directly rather than through city production.
// Record the already resolved unit class/type, requested versus successfully created count, and actual spawn city/location; no extra unit construction or scan is performed. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
void logSASGameRecordRandomEventFreeUnitsResult(PlayerTypes ePlayer, PlayerTypes eAffectedPlayer, EventTypes eEvent, int iTriggeredId, UnitClassTypes eUnitClass, UnitTypes eUnit, int iRequestedCount, int iCreatedCount, CvCity const* pSpawnCity)
{
	logSASGameRecord("GAME_RECORD_RANDOM_EVENT_FREE_UNITS_RESULT turn=%d player=%d team=%d affectedPlayer=%d triggeredId=%d event=%s unitClass=%s unit=%s requested=%d created=%d cityId=%d plot=%d,%d",
			GC.getGame().getGameTurn(), ePlayer, (ePlayer == NO_PLAYER ? NO_TEAM : GET_PLAYER(ePlayer).getTeam()), eAffectedPlayer, iTriggeredId, getSASGameRecordEventType(eEvent),
			eUnitClass == NO_UNITCLASS ? "-" : GC.getInfo(eUnitClass).getType(), getSASGameRecordUnitType(eUnit), iRequestedCount, iCreatedCount,
			pSpawnCity == NULL ? -1 : pSpawnCity->getID(), pSpawnCity == NULL ? INVALID_PLOT_COORD : pSpawnCity->getX(), pSpawnCity == NULL ? INVALID_PLOT_COORD : pSpawnCity->getY());
}

// <!-- custom: Successful ClearEventChance rolls are durable EventInfo lifecycle changes, not candidate diagnostics.
// Record only the realized clear transaction after the existing player/team/global reset loop has run, including how many scoped occurrences actually existed and were cleared. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
void logSASGameRecordRandomEventOccurrenceCleared(CvPlayer const& kPlayer, EventTypes eSourceEvent, EventTypes eClearedEvent, int iTriggeredId, int iClearChance, char const* szScope, TeamTypes eScopeTeam, int iScopePlayerSlots, int iScopeEverAlivePlayers, int iClearedOccurrences)
{
	logSASGameRecord("GAME_RECORD_RANDOM_EVENT_OCCURRENCE_CLEARED turn=%d player=%d team=%d triggeredId=%d sourceEvent=%s clearedEvent=%s clearChance=%d scope=%s scopeTeam=%d scopePlayerSlots=%d scopeEverAlivePlayers=%d clearedOccurrences=%d",
			GC.getGame().getGameTurn(), kPlayer.getID(), kPlayer.getTeam(), iTriggeredId, getSASGameRecordEventType(eSourceEvent), getSASGameRecordEventType(eClearedEvent),
			iClearChance, szScope, eScopeTeam, iScopePlayerSlots, iScopeEverAlivePlayers, iClearedOccurrences);
}

// <!-- custom: checkExpireEvent can end a previously applied quest/event turns after its original trigger.
// Record the exact existing branch that caused meaningful expiry plus the post-removal stored-target validity; routine NON_QUEST_TIMEOUT housekeeping is intentionally suppressed by the caller. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
void logSASGameRecordRandomEventExpired(CvPlayer const& kPlayer, EventTypes eEvent, EventTriggeredData const& kTriggeredData, char const* szReason)
{
	CvEventInfo const& kEvent = GC.getInfo(eEvent);
	CvEventTriggerInfo const& kTrigger = GC.getInfo(kTriggeredData.m_eTrigger);
	SASGameRecordRandomEventTargets const kTargets(kPlayer, &kTriggeredData, eEvent);
	logSASGameRecord("GAME_RECORD_RANDOM_EVENT_EXPIRED turn=%d player=%d team=%d triggeredId=%d trigger=%s event=%s reason=%s quest=%d triggerTurn=%d ageTurns=%d prereqEvents=%s prereqEventCity=%d cityId=%d cityExists=%d otherPlayer=%d otherPlayerAlive=%d otherCityId=%d otherCityExists=%d unitId=%d unitExists=%d unit=%s unitCanApply=%d plot=%d,%d plotExists=%d plotOwner=%d religion=%s corporation=%s building=%s buildingPresentInCity=%d",
			GC.getGame().getGameTurn(), kPlayer.getID(), kPlayer.getTeam(), kTriggeredData.m_iId, getSASGameRecordEventTriggerType(kTriggeredData.m_eTrigger), getSASGameRecordEventType(eEvent), szReason, kEvent.isQuest(), kTriggeredData.m_iTurn, GC.getGame().getGameTurn() - kTriggeredData.m_iTurn,
			getSASGameRecordRandomEventTriggerPrereqs(kTrigger).GetCString(), kTrigger.isPrereqEventCity(), kTargets.iCityId, kTargets.iCityExists, kTargets.eOtherPlayer, kTargets.iOtherPlayerAlive, kTargets.iOtherCityId, kTargets.iOtherCityExists,
			kTargets.iUnitId, kTargets.iUnitExists, kTargets.szUnit, kTargets.iUnitCanApply, kTargets.iPlotX, kTargets.iPlotY, kTargets.iPlotExists, kTargets.iPlotOwner, getSASGameRecordReligionType(kTargets.eReligion), getSASGameRecordCorporationType(kTargets.eCorporation), getSASGameRecordBuildingType(kTargets.eBuilding), kTargets.iBuildingPresentInCity);
}

// <!-- custom: KI#736 is already repaired in gameplay: both pillage scopes roll MinPillage..MaxPillage inclusively.
// Log the realized attempt/destruction counts only after the existing loops, proving endpoint reachability while keeping exact plot destruction in canonical GAME_RECORD_PLOT_CHANGE rows. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
void logSASGameRecordRandomEventPillageResult(char const* szScope, PlayerTypes ePlayer, PlayerTypes eAffectedPlayer, int iCityId, int iTriggeredId, EventTypes eEvent, int iMinPillage, int iMaxPillage, int iAttempts, int iDestroyed)
{
	logSASGameRecord("GAME_RECORD_RANDOM_EVENT_PILLAGE_RESULT turn=%d scope=%s player=%d affectedPlayer=%d cityId=%d triggeredId=%d event=%s minPillage=%d maxPillage=%d attempts=%d destroyed=%d failedAttempts=%d",
			GC.getGame().getGameTurn(), szScope, ePlayer, eAffectedPlayer, iCityId, iTriggeredId, getSASGameRecordEventType(eEvent), iMinPillage, iMaxPillage, iAttempts, iDestroyed, std::max(0, iAttempts - iDestroyed));
}

// <!-- custom: Delayed AdditionalEvent outcomes are actual scheduled lifecycle state, unlike speculative candidate/chance evaluation.
// Record the due turn only after the existing chance roll and earliest-countdown merge have resolved. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
void logSASGameRecordRandomEventCountdownScheduled(CvPlayer const& kPlayer, EventTypes eSourceEvent, EventTypes eFollowupEvent, int iTriggeredId, int iRequestedDueTurn, int iPreviousDueTurn, int iScheduledDueTurn)
{
	logSASGameRecord("GAME_RECORD_RANDOM_EVENT_COUNTDOWN_SCHEDULED turn=%d player=%d team=%d triggeredId=%d sourceEvent=%s followupEvent=%s requestedDueTurn=%d previousDueTurn=%d scheduledDueTurn=%d delayTurns=%d",
			GC.getGame().getGameTurn(), kPlayer.getID(), kPlayer.getTeam(), iTriggeredId, getSASGameRecordEventType(eSourceEvent), getSASGameRecordEventType(eFollowupEvent), iRequestedDueTurn, iPreviousDueTurn, iScheduledDueTurn, iScheduledDueTurn - GC.getGame().getGameTurn());
}

void logSASGameRecordUnitCompleted(CvCity const* pCity, CvUnit const* pUnit, bool bConscripted, int iRawModifiedOverflow, int iUnmodifiedOverflow, int iKeptOverflow, int iLostProduction, int iUnusedOverflowCapacity, int iOverflowGold)
{
	if (pCity == NULL || pUnit == NULL)
		return;
	PlayerTypes const ePlayer = pUnit->getOwner();
	if (ePlayer < 0 || ePlayer >= MAX_PLAYERS)
		return;
	SASGameRecordPlayerFlow& kFlow = g_akSASGameRecordPlayerFlow[ePlayer];
	int const iProductionNeeded = GET_PLAYER(ePlayer).getProductionNeeded(pUnit->getUnitType());
	if (bConscripted)
	{
		kFlow.iUnitsConscripted++;
		kFlow.iConscriptProductionNeeded += iProductionNeeded;
		kFlow.aiConscriptedUnitTypes[pUnit->getUnitType()]++;
	}
	else
	{
		kFlow.iUnitsCompleted++;
		kFlow.iUnitProductionNeeded += iProductionNeeded;
		kFlow.aiUnitTypes[pUnit->getUnitType()]++;
	}
	if (gGameRecordLogLevel >= 3)
	{
		logSASGameRecord("GAME_RECORD_ACTION turn=%d type=UNIT_COMPLETED player=%d cityId=%d city=%S unitId=%d unit=%s unitAI=%s source=%s productionNeeded=%d rawModifiedOverflow=%d unmodifiedOverflow=%d keptOverflow=%d lostProduction=%d unusedOverflowCapacity=%d overflowGold=%d",
			GC.getGame().getGameTurn(), ePlayer, pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), getSASGameRecordUnitAIType(pUnit->AI_getUnitAIType()), bConscripted ? "CONSCRIPT" : "PRODUCTION", iProductionNeeded,
			iRawModifiedOverflow, iUnmodifiedOverflow, iKeptOverflow, iLostProduction, iUnusedOverflowCapacity, iOverflowGold);
	}
}

// <!-- custom: Keep exact research-overflow arithmetic separate from TECH_ACQUIRED because only ordinary research completion has meaningful progress/overflow conversion.
// The threshold caller supplies its exact arithmetic while recorder-local same-turn application context supplies the fresh-research/carried-overflow split without widening generic research APIs. (ChatGPT-5.6-Sol) -->
void logSASGameRecordResearchCompleted(TechTypes eTech, TeamTypes eTeam, PlayerTypes ePlayer, int iProgressBefore, int iProgressBeforeClamp, int iResearchModifier, int iUnmodifiedOverflow)
{
	CvTeam const& kTeam = GET_TEAM(eTeam);
	int const iResearchCost = kTeam.getResearchCost(eTech);
	int const iProgressAdded = iProgressBeforeClamp - iProgressBefore;
	int const iRawModifiedOverflow = std::max(0, iProgressBeforeClamp - iResearchCost);
	SASGameRecordResearchApplication& kApplication = g_akSASGameRecordResearchApplication[ePlayer];
	bool const bApplicationKnown = (kApplication.bValid && kApplication.iGameTurn == GC.getGame().getGameTurn() && kApplication.eTech == eTech);
	int const iModifiedResearchRate = (bApplicationKnown ? kApplication.iModifiedResearchRate : -1);
	int const iIncomingOverflowUnmodified = (bApplicationKnown ? kApplication.iIncomingOverflowUnmodified : -1);
	int const iIncomingOverflowModified = (bApplicationKnown ? kApplication.iIncomingOverflowModified : -1);
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=RESEARCH_COMPLETED player=%d team=%d tech=%s researchCost=%d teamProgressBefore=%d applicationBreakdownKnown=%d modifiedResearchRateApplied=%d incomingOverflowUnmodified=%d incomingOverflowModifiedApplied=%d modifiedProgressAdded=%d teamProgressBeforeClamp=%d researchModifier=%d rawModifiedOverflow=%d outgoingOverflowUnmodified=%d playerOverflowAfter=%d teamStoredProgressAfter=%d",
			GC.getGame().getGameTurn(), ePlayer, eTeam, getSASGameRecordTechType(eTech), iResearchCost, iProgressBefore, bApplicationKnown ? 1 : 0,
			iModifiedResearchRate, iIncomingOverflowUnmodified, iIncomingOverflowModified, iProgressAdded, iProgressBeforeClamp, iResearchModifier, iRawModifiedOverflow, iUnmodifiedOverflow,
			GET_PLAYER(ePlayer).getOverflowResearch(), kTeam.getResearchProgress(eTech));
	kApplication.bValid = false;
}

// <!-- custom: Added eCause to write the acquisition source supplied by gameplay code instead of inferring it from ambiguous announcement/first-discovery flags. (GPT-5.6-Sol + GPT-5.6 Thinking) -->
void logSASGameRecordTechAcquired(TechTypes eType, TeamTypes eTeam, PlayerTypes ePlayer, TechAcquisitionCause eCause)
{
	CvTechInfo const& kTech = GC.getInfo(eType);
	// <!-- custom: The acquisition turn already gives the exact chronology. Mark technologies that enable tech or gold trading, while team snapshots state whether each capability is currently available. (GPT-5.6-Sol) -->
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=TECH_ACQUIRED player=%d team=%d tech=%s source=%s enablesTechTrading=%d enablesGoldTrading=%d", GC.getGame().getGameTurn(), ePlayer, eTeam, getSASGameRecordTechType(eType), getSASTechAcquisitionCause(eCause), kTech.isTechTrading(), kTech.isGoldTrading());
}

void logSASGameRecordCityBuilt(CvCity const* pCity)
{
	if (pCity == NULL)
		return;
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=CITY_BUILT player=%d cityId=%d city=%S x=%d y=%d pop=%d",
			GC.getGame().getGameTurn(), pCity->getOwner(), pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), pCity->getX(), pCity->getY(), pCity->getPopulation());
	if (gGameRecordLogLevel >= 2)
	{
		logSASGameRecordCityBFC(*pCity, "built");
		logSASGameRecordCityUnits(*pCity, "built");
	}
}

// <!-- custom: Land/population victory thresholds can change with world state, so serialize every enabled victory that actually uses either criterion rather than assuming one XML name such as VICTORY_DOMINATION.
// L/P values are current/required percentages multiplied by 100; M is whether all applicable land/population criteria are met at this exact boundary. (ChatGPT-5.6-Sol) -->
static CvString getSASGameRecordLandPopulationVictoryProgress(TeamTypes eTeam)
{
	CvGame const& kGame = GC.getGame();
	CvTeam const& kTeam = GET_TEAM(eTeam);
	int const iLandPlots = std::max(1, GC.getMap().getLandPlots());
	int const iWorldPopulation = std::max(1, kGame.getTotalPopulation());
	int const iLandPctX100 = (10000 * kTeam.getTotalLand()) / iLandPlots;
	int const iPopPctX100 = (10000 * kTeam.getTotalPopulation()) / iWorldPopulation;
	CvString szResult;
	FOR_EACH_ENUM(Victory)
	{
		if (!kGame.isVictoryValid(eLoopVictory))
			continue;
		int const iLandNeed = kGame.getAdjustedLandPercent(eLoopVictory);
		int const iPopNeed = kGame.getAdjustedPopulationPercent(eLoopVictory);
		if (iLandNeed <= 0 && iPopNeed <= 0)
			continue;
		bool const bLandMet = (iLandNeed <= 0 || 100 * kTeam.getTotalLand() >= GC.getMap().getLandPlots() * iLandNeed);
		bool const bPopMet = (iPopNeed <= 0 || 100 * kTeam.getTotalPopulation() >= kGame.getTotalPopulation() * iPopNeed);
		CvString szItem;
		szItem.Format(szResult.empty() ? "%s:L%d/%d:P%d/%d:M%d" : ";%s:L%d/%d:P%d/%d:M%d",
				GC.getInfo(eLoopVictory).getType(), iLandNeed <= 0 ? -1 : iLandPctX100, iLandNeed <= 0 ? -1 : 100 * iLandNeed,
				iPopNeed <= 0 ? -1 : iPopPctX100, iPopNeed <= 0 ? -1 : 100 * iPopNeed, bLandMet && bPopMet);
		szResult += szItem;
	}
	return getSASDiagnosticOrDash(szResult);
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

static void getSASGameRecordRazeCityDistances(CvCity const& kCity, PlayerTypes eRazer, PlayerTypes ePreviousOwner, int& iCapitalDistance, int& iCapitalSameArea, int& iNearestRazerCityDistance, int& iSameAreaRazerCitiesOther, int& iNearestPreviousOwnerCityDistance, int& iSameAreaPreviousOwnerCities)
{
	CvPlayer const& kRazer = GET_PLAYER(eRazer);
	CvCity const* pCapital = kRazer.getCapitalCity();
	iCapitalDistance = (pCapital == NULL ? -1 : plotDistance(kCity.getX(), kCity.getY(), pCapital->getX(), pCapital->getY()));
	iCapitalSameArea = (pCapital == NULL ? -1 : pCapital->getArea().getID() == kCity.getArea().getID());
	iNearestRazerCityDistance = -1;
	iSameAreaRazerCitiesOther = 0;
	int iLoop = 0;
	for (CvCity const* pLoopCity = kRazer.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kRazer.nextCity(&iLoop))
	{
		if (pLoopCity->getID() == kCity.getID())
			continue;
		int const iDistance = plotDistance(kCity.getX(), kCity.getY(), pLoopCity->getX(), pLoopCity->getY());
		if (iNearestRazerCityDistance < 0 || iDistance < iNearestRazerCityDistance)
			iNearestRazerCityDistance = iDistance;
		if (pLoopCity->getArea().getID() == kCity.getArea().getID())
			iSameAreaRazerCitiesOther++;
	}
	iNearestPreviousOwnerCityDistance = -1;
	iSameAreaPreviousOwnerCities = 0;
	if (ePreviousOwner >= 0 && ePreviousOwner < MAX_PLAYERS)
	{
		CvPlayer const& kPreviousOwner = GET_PLAYER(ePreviousOwner);
		iLoop = 0;
		for (CvCity const* pLoopCity = kPreviousOwner.firstCity(&iLoop); pLoopCity != NULL; pLoopCity = kPreviousOwner.nextCity(&iLoop))
		{
			int const iDistance = plotDistance(kCity.getX(), kCity.getY(), pLoopCity->getX(), pLoopCity->getY());
			if (iNearestPreviousOwnerCityDistance < 0 || iDistance < iNearestPreviousOwnerCityDistance)
				iNearestPreviousOwnerCityDistance = iDistance;
			if (pLoopCity->getArea().getID() == kCity.getArea().getID())
				iSameAreaPreviousOwnerCities++;
		}
	}
}

void beginSASGameRecordCityRaze(CvCity const* pCity, PlayerTypes ePlayer)
{
	if (pCity == NULL || ePlayer < 0 || ePlayer >= MAX_PLAYERS)
		return;
	CvPlayerAI const& kRazer = GET_PLAYER(ePlayer);
	SASGameRecordCityRazeContext kContext;
	kContext.eRazer = ePlayer;
	kContext.eRazerTeam = kRazer.getTeam();
	kContext.ePreviousOwner = pCity->getPreviousOwner();
	kContext.ePreviousTeam = (kContext.ePreviousOwner >= 0 && kContext.ePreviousOwner < MAX_PLAYERS ? GET_PLAYER(kContext.ePreviousOwner).getTeam() : NO_TEAM);
	kContext.eOriginalOwner = pCity->getOriginalOwner();
	kContext.eOriginalTeam = (kContext.eOriginalOwner >= 0 && kContext.eOriginalOwner < MAX_PLAYERS ? GET_PLAYER(kContext.eOriginalOwner).getTeam() : NO_TEAM);
	kContext.iGameTurn = GC.getGame().getGameTurn();
	kContext.iCityId = pCity->getID();
	kContext.szCityName = getSASGameRecordQuotedCityName(pCity);
	kContext.iX = pCity->getX();
	kContext.iY = pCity->getY();
	kContext.iArea = pCity->getArea().getID();
	kContext.szRazeMode = (pCity->isAutoRaze() ? "AUTO_RAZE" : (kRazer.isHuman() ? "HUMAN" : "AI"));
	kContext.iPopulation = pCity->getPopulation();
	kContext.iHighestPopulation = pCity->getHighestPopulation();
	kContext.iFoundedTurn = pCity->getGameTurnFounded();
	kContext.iAcquiredTurn = pCity->getGameTurnAcquired();
	kContext.iOccupationTurns = pCity->getOccupationTimer();
	kContext.iRazerCulturePercent = pCity->calculateTeamCulturePercent(kContext.eRazerTeam);
	kContext.iPreviousCulturePercent = (kContext.ePreviousTeam == NO_TEAM ? -1 : pCity->calculateTeamCulturePercent(kContext.ePreviousTeam));
	kContext.eHighestCulturePlayer = pCity->findHighestCulture();
	kContext.iHighestCulturePercent = (kContext.eHighestCulturePlayer == NO_PLAYER ? -1 : pCity->calculateCulturePercent(kContext.eHighestCulturePlayer));
	kContext.iMaintenanceTimes100 = pCity->getMaintenanceTimes100();
	kContext.iConnectedToCapital = pCity->isConnectedToCapital();
	getSASGameRecordRazeCityDistances(*pCity, ePlayer, kContext.ePreviousOwner, kContext.iCapitalDistance, kContext.iCapitalSameArea, kContext.iNearestRazerCityDistance, kContext.iSameAreaRazerCitiesOther, kContext.iNearestPreviousOwnerCityDistance, kContext.iSameAreaPreviousOwnerCities);
	kContext.szBuildings = getSASGameRecordCityBuildings(*pCity, kContext.iBuildings, kContext.iRegularBuildings, kContext.iNationalWonders, kContext.iTeamWonders, kContext.iWorldWonders);
	kContext.szReligions = getSASGameRecordCityReligionList(*pCity, false);
	kContext.szHolyReligions = getSASGameRecordCityReligionList(*pCity, true);
	kContext.szCorporations = getSASGameRecordCityCorporationList(*pCity, false);
	kContext.szHeadquarters = getSASGameRecordCityCorporationList(*pCity, true);
	kContext.iPlayerCitiesBefore = kRazer.getNumCities();
	kContext.iPlayerLandBefore = kRazer.getTotalLand();
	kContext.iPlayerPopulationBefore = kRazer.getTotalPopulation();
	kContext.iTeamCitiesBefore = GET_TEAM(kContext.eRazerTeam).getNumCities();
	kContext.iTeamLandBefore = GET_TEAM(kContext.eRazerTeam).getTotalLand();
	kContext.iTeamPopulationBefore = GET_TEAM(kContext.eRazerTeam).getTotalPopulation();
	kContext.iWorldPopulationBefore = GC.getGame().getTotalPopulation();
	kContext.iLandPctX100Before = (10000 * kContext.iTeamLandBefore) / std::max(1, GC.getMap().getLandPlots());
	kContext.iPopPctX100Before = (10000 * kContext.iTeamPopulationBefore) / std::max(1, GC.getGame().getTotalPopulation());
	if (kRazer.isHuman() && !kRazer.isHumanDisabled())
	{
		kContext.iAIMaxVictoryStage = -1;
		kContext.iAIConquestStage = -1;
		kContext.iAIDominationStage = -1;
	}
	else
	{
		AIVictoryStage const eStages = kRazer.AI_getVictoryStageHash();
		kContext.iAIConquestStage = getSASConquestVictoryStageLevel(eStages);
		kContext.iAIDominationStage = getSASDominationVictoryStageLevel(eStages);
		kContext.iAIMaxVictoryStage = std::max(getSASCultureVictoryStageLevel(eStages), std::max(getSASSpaceVictoryStageLevel(eStages), std::max(kContext.iAIConquestStage, std::max(kContext.iAIDominationStage, getSASDiplomacyVictoryStageLevel(eStages)))));
	}
	kContext.szLandPopVictoryProgressBefore = getSASGameRecordLandPopulationVictoryProgress(kContext.eRazerTeam);
	g_aSASGameRecordCityRazeContexts.push_back(kContext);
}

void endSASGameRecordCityRaze(PlayerTypes ePlayer)
{
	if (g_aSASGameRecordCityRazeContexts.empty())
		return;
	// <!-- custom: LIFO matches nested razes triggered synchronously from Python callbacks; only consume a matching context. (ChatGPT-5.6-Sol) -->
	int iContext = (int)g_aSASGameRecordCityRazeContexts.size() - 1;
	if (g_aSASGameRecordCityRazeContexts[iContext].eRazer != ePlayer)
		return;
	SASGameRecordCityRazeContext const kContext = g_aSASGameRecordCityRazeContexts[iContext];
	g_aSASGameRecordCityRazeContexts.pop_back();
	CvPlayer const& kRazer = GET_PLAYER(ePlayer);
	CvTeam const& kTeam = GET_TEAM(kContext.eRazerTeam);
	int const iPlayerCitiesAfter = kRazer.getNumCities();
	int const iPlayerLandAfter = kRazer.getTotalLand();
	int const iPlayerPopulationAfter = kRazer.getTotalPopulation();
	int const iTeamCitiesAfter = kTeam.getNumCities();
	int const iTeamLandAfter = kTeam.getTotalLand();
	int const iTeamPopulationAfter = kTeam.getTotalPopulation();
	int const iWorldPopulationAfter = GC.getGame().getTotalPopulation();
	int const iLandPctX100After = (10000 * iTeamLandAfter) / std::max(1, GC.getMap().getLandPlots());
	int const iPopPctX100After = (10000 * iTeamPopulationAfter) / std::max(1, iWorldPopulationAfter);
	CvString const szVictoryProgressAfter = getSASGameRecordLandPopulationVictoryProgress(kContext.eRazerTeam);
	CvPlot const& kRazedPlot = GC.getMap().getPlot(kContext.iX, kContext.iY);
	PlayerTypes const eCityPlotOwnerAfter = kRazedPlot.getOwner();
	TeamTypes const eCityPlotTeamAfter = kRazedPlot.getTeam();
	// <!-- custom: One rare level-2 action now preserves both the destroyed city's intrinsic/contextual value and the exact empire/victory effect of deleting it.
	// CITY_ACQUIRED + CITY_BFC/CITY_UNITS already preserve capture/BFC/tactical state, so do not duplicate those large rows here. (ChatGPT-5.6-Sol) -->
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=CITY_RAZED razer=%d razerTeam=%d razeMode=%s previousOwner=%d previousTeam=%d originalOwner=%d originalTeam=%d cityId=%d city=%S x=%d y=%d area=%d pop=%d highestPop=%d foundedTurn=%d cityAge=%d acquiredTurn=%d turnsHeld=%d occupationTurns=%d razerCulturePercent=%d previousCulturePercent=%d highestCulturePlayer=%d highestCulturePercent=%d connectedToCapital=%d capitalDistance=%d capitalSameArea=%d nearestRazerCityDistance=%d sameAreaRazerCitiesOther=%d nearestPreviousOwnerCityDistance=%d sameAreaPreviousOwnerCities=%d maintenanceTimes100=%d buildings=%d regularBuildings=%d nationalWonders=%d teamWonders=%d worldWonders=%d buildingTypes=%s religions=%s holyReligions=%s corporations=%s headquarters=%s cityPlotOwnerAfter=%d cityPlotTeamAfter=%d playerCitiesBefore=%d playerCitiesAfter=%d playerLandBefore=%d playerLandAfter=%d playerLandDelta=%+d playerPopBefore=%d playerPopAfter=%d playerPopDelta=%+d teamCitiesBefore=%d teamCitiesAfter=%d teamLandBefore=%d teamLandAfter=%d teamLandDelta=%+d landPctX100Before=%d landPctX100After=%d landPctX100Delta=%+d teamPopBefore=%d teamPopAfter=%d teamPopDelta=%+d worldPopBefore=%d worldPopAfter=%d popPctX100Before=%d popPctX100After=%d popPctX100Delta=%+d aiMaxVictoryStage=%d aiConquestStage=%d aiDominationStage=%d landPopVictoryProgressBefore=%s landPopVictoryProgressAfter=%s",
			kContext.iGameTurn, kContext.eRazer, kContext.eRazerTeam, kContext.szRazeMode.GetCString(), kContext.ePreviousOwner, kContext.ePreviousTeam, kContext.eOriginalOwner, kContext.eOriginalTeam,
			kContext.iCityId, kContext.szCityName.GetCString(), kContext.iX, kContext.iY, kContext.iArea, kContext.iPopulation, kContext.iHighestPopulation, kContext.iFoundedTurn, kContext.iFoundedTurn < 0 ? -1 : kContext.iGameTurn - kContext.iFoundedTurn,
			kContext.iAcquiredTurn, kContext.iAcquiredTurn < 0 ? -1 : kContext.iGameTurn - kContext.iAcquiredTurn, kContext.iOccupationTurns, kContext.iRazerCulturePercent, kContext.iPreviousCulturePercent, kContext.eHighestCulturePlayer, kContext.iHighestCulturePercent,
			kContext.iConnectedToCapital, kContext.iCapitalDistance, kContext.iCapitalSameArea, kContext.iNearestRazerCityDistance, kContext.iSameAreaRazerCitiesOther, kContext.iNearestPreviousOwnerCityDistance, kContext.iSameAreaPreviousOwnerCities,
			kContext.iMaintenanceTimes100, kContext.iBuildings, kContext.iRegularBuildings, kContext.iNationalWonders, kContext.iTeamWonders, kContext.iWorldWonders, kContext.szBuildings.GetCString(), kContext.szReligions.GetCString(), kContext.szHolyReligions.GetCString(), kContext.szCorporations.GetCString(), kContext.szHeadquarters.GetCString(), eCityPlotOwnerAfter, eCityPlotTeamAfter,
			kContext.iPlayerCitiesBefore, iPlayerCitiesAfter, kContext.iPlayerLandBefore, iPlayerLandAfter, iPlayerLandAfter - kContext.iPlayerLandBefore, kContext.iPlayerPopulationBefore, iPlayerPopulationAfter, iPlayerPopulationAfter - kContext.iPlayerPopulationBefore, kContext.iTeamCitiesBefore, iTeamCitiesAfter, kContext.iTeamLandBefore, iTeamLandAfter, iTeamLandAfter - kContext.iTeamLandBefore, kContext.iLandPctX100Before, iLandPctX100After, iLandPctX100After - kContext.iLandPctX100Before,
			kContext.iTeamPopulationBefore, iTeamPopulationAfter, iTeamPopulationAfter - kContext.iTeamPopulationBefore, kContext.iWorldPopulationBefore, iWorldPopulationAfter, kContext.iPopPctX100Before, iPopPctX100After, iPopPctX100After - kContext.iPopPctX100Before,
			kContext.iAIMaxVictoryStage, kContext.iAIConquestStage, kContext.iAIDominationStage, kContext.szLandPopVictoryProgressBefore.GetCString(), szVictoryProgressAfter.GetCString());
}

void logSASGameRecordCityAcquired(PlayerTypes eOldOwner, PlayerTypes eNewOwner, CvCity const* pCity, bool bConquest, bool bTrade)
{
	if (pCity == NULL)
		return;
	if (eNewOwner >= 0 && eNewOwner < MAX_PLAYERS)
	{
		g_aiSASGameRecordCitiesAcquired[eNewOwner]++;
		if (bConquest)
			g_aiSASGameRecordCitiesConquered[eNewOwner]++;
		if (bTrade)
			g_aiSASGameRecordCitiesTradedIn[eNewOwner]++;
	}
	if (eOldOwner >= 0 && eOldOwner < MAX_PLAYERS)
	{
		g_aiSASGameRecordCitiesLost[eOldOwner]++;
		if (bConquest)
			g_aiSASGameRecordCitiesLostByConquest[eOldOwner]++;
		if (bTrade)
			g_aiSASGameRecordCitiesTradedOut[eOldOwner]++;
	}
	// <!-- custom: Attribute a conquest to its active team-pair war so the final summary distinguishes territorial results from battle losses and abstract war success. (GPT-5.6-Sol) -->
	if (bConquest && eOldOwner >= 0 && eOldOwner < MAX_PLAYERS && eNewOwner >= 0 && eNewOwner < MAX_PLAYERS)
	{
		TeamTypes const eOldTeam = GET_PLAYER(eOldOwner).getTeam();
		TeamTypes const eNewTeam = GET_PLAYER(eNewOwner).getTeam();
		SASGameRecordWarSummary* pWar = findSASGameRecordWar(eOldTeam, eNewTeam);
		if (pWar != NULL)
		{
			if (eNewTeam == pWar->eTeamA)
			{
				pWar->iCitiesCapturedByA++;
				pWar->iPopulationCapturedByA += pCity->getPopulation();
			}
			else
			{
				pWar->iCitiesCapturedByB++;
				pWar->iPopulationCapturedByB += pCity->getPopulation();
			}
			refreshSASGameRecordWarSuccess(*pWar);
		}
	}
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=CITY_ACQUIRED oldOwner=%d newOwner=%d cityId=%d city=%S x=%d y=%d pop=%d conquest=%d trade=%d",
			GC.getGame().getGameTurn(), eOldOwner, eNewOwner, pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), pCity->getX(), pCity->getY(), pCity->getPopulation(), bConquest, bTrade);
	if (gGameRecordLogLevel >= 2)
	{
		logSASGameRecordCityBFC(*pCity, "acquired");
		logSASGameRecordCityUnits(*pCity, "acquired");
	}
}

void logSASGameRecordWarStarted(TeamTypes eDeclarer, TeamTypes eTarget, WarPlanTypes eWarPlan, bool bPrimaryDoW, bool bNewDiplo, PlayerTypes eSponsor, bool bRandomEvent, WarDeclarationCause eCause)
{
	CvTeam const& kDeclarer = GET_TEAM(eDeclarer);
	CvTeam const& kTarget = GET_TEAM(eTarget);
	CvTeamAI const& kTargetAI = GET_TEAM(eTarget);
	char const* szCause = (bRandomEvent ? "RANDOM_EVENT" : (eSponsor != NO_PLAYER ? "SPONSORED_WAR" : getSASWarDeclarationCause(eCause)));
	addSASGameRecordWar(eDeclarer, eTarget, true, eDeclarer, eTarget, eWarPlan, szCause, bPrimaryDoW);
	// <!-- custom: `cause=DIRECT` identifies how war began, not why the AI selected that rival.
	// Preserve the target's exact victory state at declaration time so archived records show whether victory denial was relevant without falsely claiming it was the sole strategic motive. (GPT-5.6-Sol) -->
	int const iTargetMaxVictoryStage = getSASTeamMaxVictoryStage(eTarget);
	int const iTargetSpaceVictoryStage = getSASTeamSpaceVictoryStage(eTarget);
	int const iTargetSpaceshipParts = getSASTeamSpaceshipPartsBuilt(eTarget);
	int const iSpaceshipPartsRequired = getSASSpaceshipPartsRequired();
	int const iTargetSpaceshipPartsPercent = (iSpaceshipPartsRequired <= 0 ? 0 : iTargetSpaceshipParts * 100 / iSpaceshipPartsRequired);
	int const iTargetVictoryCountdown = kTargetAI.AI_getLowestVictoryCountdown();
	bool const bVictoryDenialContext = (iTargetVictoryCountdown >= 0 || iTargetMaxVictoryStage >= 4 || isSASTeamStage3SpaceVictoryThreat(eTarget));
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=WAR_STARTED declarerTeam=%d targetTeam=%d cause=%s primary=%d newDiplo=%d warPlan=%s sponsorPlayer=%d sponsorTeam=%d randomEvent=%d declarerMaster=%d targetMaster=%d declarerWarsAfter=%d targetWarsAfter=%d victoryDenialContext=%d targetMaxVictoryStage=%d targetSpaceVictoryStage=%d targetSpaceshipParts=%d targetSpaceshipPartsPercent=%d targetVictoryCountdown=%d",
			GC.getGame().getGameTurn(), eDeclarer, eTarget, szCause, bPrimaryDoW, bNewDiplo, getSASWarPlanType(eWarPlan),
			eSponsor, eSponsor == NO_PLAYER ? NO_TEAM : GET_PLAYER(eSponsor).getTeam(), bRandomEvent,
			kDeclarer.isAVassal() ? kDeclarer.getMasterTeam() : NO_TEAM, kTarget.isAVassal() ? kTarget.getMasterTeam() : NO_TEAM, kDeclarer.getNumWars(false), kTarget.getNumWars(false),
			bVictoryDenialContext, iTargetMaxVictoryStage, iTargetSpaceVictoryStage, iTargetSpaceshipParts, iTargetSpaceshipPartsPercent, iTargetVictoryCountdown);
}

// <!-- custom: Added final war-success and peace-context parameters because those details are no longer recoverable after CvTeam::makePeace finishes. (GPT-5.6-Sol) -->
void logSASGameRecordWarEnded(TeamTypes eTeam, TeamTypes eOtherTeam, int iTeamAWarSuccess, int iTeamBWarSuccess, bool bCapitulate, TeamTypes eBroker, bool bRandomEvent, bool bReparations)
{
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=WAR_ENDED teamA=%d teamB=%d teamAWarsAfter=%d teamBWarsAfter=%d capitulation=%d brokerTeam=%d randomEvent=%d reparations=%d teamAWarSuccess=%d teamBWarSuccess=%d",
			GC.getGame().getGameTurn(), eTeam, eOtherTeam, GET_TEAM(eTeam).getNumWars(false), GET_TEAM(eOtherTeam).getNumWars(false), bCapitulate, eBroker, bRandomEvent, bReparations, iTeamAWarSuccess, iTeamBWarSuccess);
	SASGameRecordWarSummary* pWar = findSASGameRecordWar(eTeam, eOtherTeam);
	if (pWar == NULL)
		pWar = addSASGameRecordWar(eTeam, eOtherTeam, false, NO_TEAM, NO_TEAM, NO_WARPLAN, "UNOBSERVED_START", false);
	if (pWar == NULL)
		return;
	int const iWarSuccessA = (eTeam == pWar->eTeamA ? iTeamAWarSuccess : iTeamBWarSuccess);
	int const iWarSuccessB = (eTeam == pWar->eTeamA ? iTeamBWarSuccess : iTeamAWarSuccess);
	logSASGameRecordWarSummary(*pWar, "ENDED", "MAKE_PEACE", GC.getGame().getGameTurn(), iWarSuccessA, iWarSuccessB, bCapitulate, eBroker, bRandomEvent, bReparations);
	for (size_t iI = 0; iI < g_aSASGameRecordWars.size(); iI++)
	{
		if (&g_aSASGameRecordWars[iI] == pWar)
		{
			g_aSASGameRecordWars.erase(g_aSASGameRecordWars.begin() + iI);
			break;
		}
	}
}

void logSASGameRecordWarPlanChanged(TeamTypes eTeam, TeamTypes eTarget, WarPlanTypes eOldWarPlan, WarPlanTypes eNewWarPlan, bool bWar, int iOldStateCounter)
{
	if (eTeam < 0 || eTeam >= MAX_TEAMS || eTarget < 0 || eTarget >= MAX_TEAMS)
		return;
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=WAR_PLAN_CHANGED team=%d targetTeam=%d oldWarPlan=%s newWarPlan=%s bWar=%d atWar=%d oldStateCounter=%d ourWars=%d targetWars=%d",
		GC.getGame().getGameTurn(), eTeam, eTarget, getSASWarPlanType(eOldWarPlan), getSASWarPlanType(eNewWarPlan), bWar, GET_TEAM(eTeam).isAtWar(eTarget), iOldStateCounter, GET_TEAM(eTeam).getNumWars(true, true), GET_TEAM(eTarget).getNumWars(true, true));
}

// <!-- custom: CvTeam::addTeam is the authoritative team-merge boundary. Log both pre-merge member lists while the absorbed team still owns its players.
// Periodic team snapshots can then describe the resulting state without forcing a consumer to infer the exact merge turn. (ChatGPT-5.6-Sol) -->
void logSASGameRecordTeamMerged(TeamTypes eSurvivingTeam, TeamTypes eAbsorbedTeam)
{
	if (eSurvivingTeam < 0 || eSurvivingTeam >= MAX_TEAMS || eAbsorbedTeam < 0 || eAbsorbedTeam >= MAX_TEAMS || eSurvivingTeam == eAbsorbedTeam)
		return;
	int iSurvivingPlayerCount = 0;
	int iAbsorbedPlayerCount = 0;
	CvString const szSurvivingPlayers = getSASGameRecordTeamAssignedPlayers(eSurvivingTeam, iSurvivingPlayerCount);
	CvString const szAbsorbedPlayers = getSASGameRecordTeamAssignedPlayers(eAbsorbedTeam, iAbsorbedPlayerCount);
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=TEAM_MERGED survivingTeam=%d absorbedTeam=%d survivingPlayersBefore=%s absorbedPlayers=%s survivingPlayerCountBefore=%d absorbedPlayerCount=%d resultingPlayerCount=%d",
			GC.getGame().getGameTurn(), eSurvivingTeam, eAbsorbedTeam, szSurvivingPlayers.GetCString(), szAbsorbedPlayers.GetCString(), iSurvivingPlayerCount, iAbsorbedPlayerCount, iSurvivingPlayerCount + iAbsorbedPlayerCount);
}

void logSASGameRecordTeamMet(TeamTypes eTeam, TeamTypes eOtherTeam, bool bNewDiplo, int iX1, int iY1, int iX2, int iY2, CvPlot const* pTeamContactPlot, CvPlot const* pOtherContactPlot)
{
	const bool bMeetDataPlot1Valid = (iX1 >= 0 && iY1 >= 0 && iX1 < GC.getMap().getGridWidth() && iY1 < GC.getMap().getGridHeight());
	const bool bMeetDataPlot2Valid = (iX2 >= 0 && iY2 >= 0 && iX2 < GC.getMap().getGridWidth() && iY2 < GC.getMap().getGridHeight());
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=TEAM_MET team=%d otherTeam=%d bNewDiplo=%d teamMembers=%s otherMembers=%s meetDataPlot1=%d,%d meetDataPlot1Valid=%d meetDataPlot2=%d,%d meetDataPlot2Valid=%d teamContactPlot=%d,%d otherTeamContactPlot=%d,%d",
			GC.getGame().getGameTurn(), eTeam, eOtherTeam, bNewDiplo, getSASGameRecordTeamMembers(eTeam).GetCString(), getSASGameRecordTeamMembers(eOtherTeam).GetCString(), iX1, iY1, bMeetDataPlot1Valid, iX2, iY2, bMeetDataPlot2Valid, pTeamContactPlot == NULL ? -1 : pTeamContactPlot->getX(), pTeamContactPlot == NULL ? -1 : pTeamContactPlot->getY(), pOtherContactPlot == NULL ? -1 : pOtherContactPlot->getX(), pOtherContactPlot == NULL ? -1 : pOtherContactPlot->getY());
}

void logSASGameRecordPlayerGoldTrade(PlayerTypes eFromPlayer, PlayerTypes eToPlayer, int iAmount)
{
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=GOLD_TRADE from=%d to=%d amount=%d", GC.getGame().getGameTurn(), eFromPlayer, eToPlayer, iAmount);
}

// <!-- custom: Recorder policy, not diplomacy AI: identify the DiploEvent values whose generic pre-event row is replaced by a richer post-resolution state-delta row. Keep the taxonomy with the recorder schema instead of teaching CvPlayer gameplay code how SASGameRecord groups events. (ChatGPT-5.6-Sol) -->
bool isSASGameRecordResolvedDiploInteraction(DiploEventTypes eDiploEvent)
{
	switch (eDiploEvent)
	{
	case DIPLOEVENT_GIVE_HELP:
	case DIPLOEVENT_REFUSED_HELP:
	case DIPLOEVENT_ACCEPT_DEMAND:
	case DIPLOEVENT_REJECTED_DEMAND:
	case DIPLOEVENT_CONVERT:
	case DIPLOEVENT_NO_CONVERT:
	case DIPLOEVENT_REVOLUTION:
	case DIPLOEVENT_NO_REVOLUTION:
	case DIPLOEVENT_JOIN_WAR:
	case DIPLOEVENT_NO_JOIN_WAR:
	case DIPLOEVENT_STOP_TRADING:
	case DIPLOEVENT_NO_STOP_TRADING:
	case DIPLOEVENT_ASK_HELP:
	case DIPLOEVENT_MADE_DEMAND:
		return true;
	default:
		return false;
	}
}

void captureSASGameRecordDiploRelationState(PlayerTypes eActor, PlayerTypes eOther, SASGameRecordDiploRelationState& kState)
{
	CvPlayerAI const& kActor = GET_PLAYER(eActor);
	CvPlayerAI const& kOther = GET_PLAYER(eOther);
	kState.iActorAttitude = kActor.AI_getAttitudeVal(eOther);
	kState.iOtherAttitude = kOther.AI_getAttitudeVal(eActor);
	for (int iMemory = 0; iMemory < NUM_MEMORY_TYPES; iMemory++)
	{
		MemoryTypes const eMemory = (MemoryTypes)iMemory;
		kState.aiActorMemory[iMemory] = kActor.AI_getMemoryCount(eOther, eMemory);
		kState.aiOtherMemory[iMemory] = kOther.AI_getMemoryCount(eActor, eMemory);
	}
	kState.eActorWarPlan = GET_TEAM(kActor.getTeam()).AI_getWarPlan(kOther.getTeam());
	kState.eOtherWarPlan = GET_TEAM(kOther.getTeam()).AI_getWarPlan(kActor.getTeam());
	kState.bAtWar = GET_TEAM(kActor.getTeam()).isAtWar(kOther.getTeam());
}

// <!-- custom: These semantic labels are SASGameRecord schema, not reusable gameplay enums. Keep the private mapping beside the row formatter; CvGameCoreUtils retains only generic raw enum-token helpers such as getSASDiploEventType. (ChatGPT-5.6-Sol) -->
static char const* getSASGameRecordDiploInteractionType(DiploEventTypes eDiploEvent)
{
	switch (eDiploEvent)
	{
	case DIPLOEVENT_GIVE_HELP:
	case DIPLOEVENT_REFUSED_HELP:
	case DIPLOEVENT_ASK_HELP:
		return "HELP";
	case DIPLOEVENT_ACCEPT_DEMAND:
	case DIPLOEVENT_REJECTED_DEMAND:
	case DIPLOEVENT_MADE_DEMAND:
		return "DEMAND";
	case DIPLOEVENT_CONVERT:
	case DIPLOEVENT_NO_CONVERT:
		return "RELIGION";
	case DIPLOEVENT_REVOLUTION:
	case DIPLOEVENT_NO_REVOLUTION:
		return "CIVIC";
	case DIPLOEVENT_JOIN_WAR:
	case DIPLOEVENT_NO_JOIN_WAR:
		return "JOIN_WAR";
	case DIPLOEVENT_STOP_TRADING:
	case DIPLOEVENT_NO_STOP_TRADING:
		return "STOP_TRADING";
	default:
		return "-";
	}
}

static char const* getSASGameRecordDiploInteractionOutcome(DiploEventTypes eDiploEvent, int iData1)
{
	switch (eDiploEvent)
	{
	case DIPLOEVENT_GIVE_HELP:
	case DIPLOEVENT_ACCEPT_DEMAND:
	case DIPLOEVENT_CONVERT:
	case DIPLOEVENT_REVOLUTION:
	case DIPLOEVENT_JOIN_WAR:
	case DIPLOEVENT_STOP_TRADING:
		return "ACCEPTED";
	case DIPLOEVENT_REFUSED_HELP:
	case DIPLOEVENT_REJECTED_DEMAND:
	case DIPLOEVENT_NO_CONVERT:
	case DIPLOEVENT_NO_REVOLUTION:
	case DIPLOEVENT_NO_JOIN_WAR:
	case DIPLOEVENT_NO_STOP_TRADING:
		return "REFUSED";
	case DIPLOEVENT_ASK_HELP:
	case DIPLOEVENT_MADE_DEMAND:
		return (iData1 > 0 ? "ACCEPTED" : "REFUSED");
	default:
		return "-";
	}
}

static CvString getSASGameRecordDiploInteractionSubject(PlayerTypes eActor, DiploEventTypes eDiploEvent, int iData1)
{
	switch (eDiploEvent)
	{
	case DIPLOEVENT_CONVERT:
	case DIPLOEVENT_NO_CONVERT:
		return GET_PLAYER(eActor).getStateReligion() == NO_RELIGION ? CvString("-") : CvString(GC.getInfo(GET_PLAYER(eActor).getStateReligion()).getType());
	case DIPLOEVENT_REVOLUTION:
	case DIPLOEVENT_NO_REVOLUTION:
		return GET_PLAYER(eActor).getFavoriteCivic() == NO_CIVIC ? CvString("-") : CvString(GC.getInfo(GET_PLAYER(eActor).getFavoriteCivic()).getType());
	case DIPLOEVENT_JOIN_WAR:
	case DIPLOEVENT_NO_JOIN_WAR:
	case DIPLOEVENT_STOP_TRADING:
	case DIPLOEVENT_NO_STOP_TRADING:
		return getSASTeamDiagnosticText((TeamTypes)iData1);
	default:
		return CvString("-");
	}
}

static CvString getSASGameRecordDiploMemoryChanges(PlayerTypes eActor, PlayerTypes eOther, SASGameRecordDiploRelationState const& kBefore, SASGameRecordDiploRelationState const& kAfter)
{
	CvString szChanges;
	for (int iMemory = 0; iMemory < NUM_MEMORY_TYPES; iMemory++)
	{
		MemoryTypes const eMemory = (MemoryTypes)iMemory;
		if (kBefore.aiActorMemory[iMemory] != kAfter.aiActorMemory[iMemory])
		{
			CvString szItem;
			szItem.Format(szChanges.empty() ? "P%d:%s:%d>%d" : ",P%d:%s:%d>%d", eActor, getSASMemoryType(eMemory), kBefore.aiActorMemory[iMemory], kAfter.aiActorMemory[iMemory]);
			szChanges += szItem;
		}
		if (kBefore.aiOtherMemory[iMemory] != kAfter.aiOtherMemory[iMemory])
		{
			CvString szItem;
			szItem.Format(szChanges.empty() ? "P%d:%s:%d>%d" : ",P%d:%s:%d>%d", eOther, getSASMemoryType(eMemory), kBefore.aiOtherMemory[iMemory], kAfter.aiOtherMemory[iMemory]);
			szChanges += szItem;
		}
	}
	return getSASDiagnosticOrDash(szChanges);
}

void logSASGameRecordResolvedDiploInteraction(PlayerTypes eActor, DiploEventTypes eDiploEvent, PlayerTypes eOther, int iData1, SASGameRecordDiploRelationState const& kBefore, SASGameRecordDiploRelationState const& kAfter)
{
	bool const bOtherRequested = (eDiploEvent == DIPLOEVENT_ASK_HELP || eDiploEvent == DIPLOEVENT_MADE_DEMAND);
	PlayerTypes const eRequester = (bOtherRequested ? eOther : eActor);
	PlayerTypes const eResponder = (bOtherRequested ? eActor : eOther);
	int const iRequesterAttitudeBefore = (bOtherRequested ? kBefore.iOtherAttitude : kBefore.iActorAttitude);
	int const iRequesterAttitudeAfter = (bOtherRequested ? kAfter.iOtherAttitude : kAfter.iActorAttitude);
	int const iResponderAttitudeBefore = (bOtherRequested ? kBefore.iActorAttitude : kBefore.iOtherAttitude);
	int const iResponderAttitudeAfter = (bOtherRequested ? kAfter.iActorAttitude : kAfter.iOtherAttitude);
	WarPlanTypes const eRequesterWarPlanBefore = (bOtherRequested ? kBefore.eOtherWarPlan : kBefore.eActorWarPlan);
	WarPlanTypes const eRequesterWarPlanAfter = (bOtherRequested ? kAfter.eOtherWarPlan : kAfter.eActorWarPlan);
	WarPlanTypes const eResponderWarPlanBefore = (bOtherRequested ? kBefore.eActorWarPlan : kBefore.eOtherWarPlan);
	WarPlanTypes const eResponderWarPlanAfter = (bOtherRequested ? kAfter.eActorWarPlan : kAfter.eOtherWarPlan);

	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=DIPLO_INTERACTION requester=%d responder=%d interaction=%s outcome=%s subject=%s event=%s requesterAttitudeValue=%d>%d responderAttitudeValue=%d>%d memoryChanges=%s requesterWarPlan=%s>%s responderWarPlan=%s>%s atWar=%d>%d",
			GC.getGame().getGameTurn(), eRequester, eResponder, getSASGameRecordDiploInteractionType(eDiploEvent), getSASGameRecordDiploInteractionOutcome(eDiploEvent, iData1), getSASGameRecordDiploInteractionSubject(eActor, eDiploEvent, iData1).GetCString(), getSASDiploEventType(eDiploEvent),
			iRequesterAttitudeBefore, iRequesterAttitudeAfter, iResponderAttitudeBefore, iResponderAttitudeAfter, getSASGameRecordDiploMemoryChanges(eActor, eOther, kBefore, kAfter).GetCString(),
			getSASWarPlanType(eRequesterWarPlanBefore), getSASWarPlanType(eRequesterWarPlanAfter), getSASWarPlanType(eResponderWarPlanBefore), getSASWarPlanType(eResponderWarPlanAfter), kBefore.bAtWar ? 1 : 0, kAfter.bAtWar ? 1 : 0);
}

// <!-- custom: CvPlayer::AI_considerOfferExternal is the EXE's submitted human->AI offer boundary rather than a speculative internal valuation call. Preserve rejected packages too; accepted packages will additionally produce the existing DIPLO_DEAL row. (ChatGPT-5.6-Sol) -->
void logSASGameRecordDiploOfferEvaluated(PlayerTypes eProposer, PlayerTypes eResponder, CLinkList<TradeData> const& kProposerGives, CLinkList<TradeData> const& kResponderGives, int iChange, bool bAccepted, SASGameRecordDiploRelationState const& kBefore, SASGameRecordDiploRelationState const& kAfter)
{
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=DIPLO_OFFER_EVALUATED proposer=%d responder=%d outcome=%s change=%d proposerGives=%s responderGives=%s proposerAttitudeValue=%d>%d responderAttitudeValue=%d>%d memoryChanges=%s atWar=%d",
			GC.getGame().getGameTurn(), eProposer, eResponder, bAccepted ? "ACCEPTED" : "REJECTED", iChange, getSASTradeListText(kProposerGives, eProposer).GetCString(), getSASTradeListText(kResponderGives, eResponder).GetCString(),
			kBefore.iOtherAttitude, kAfter.iOtherAttitude, kBefore.iActorAttitude, kAfter.iActorAttitude, getSASGameRecordDiploMemoryChanges(eResponder, eProposer, kBefore, kAfter).GetCString(), kBefore.bAtWar ? 1 : 0);
}

// <!-- custom: The EXE counterproposal wrapper exposes the exact submitted package plus the additions selected by the AI. Log only this resolved boundary, not the many internal candidate/value calculations used to construct it. (ChatGPT-5.6-Sol) -->
void logSASGameRecordDiploCounterProposal(PlayerTypes eProposer, PlayerTypes eResponder, CLinkList<TradeData> const& kOriginalProposerGives, CLinkList<TradeData> const& kOriginalResponderGives, CLinkList<TradeData> const& kProposerAdds, CLinkList<TradeData> const& kResponderAdds, bool bProposed)
{
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=DIPLO_COUNTERPROPOSAL proposer=%d responder=%d outcome=%s originalProposerGives=%s originalResponderGives=%s proposerAdds=%s responderAdds=%s",
			GC.getGame().getGameTurn(), eProposer, eResponder, bProposed ? "PROPOSED" : "NONE", getSASTradeListText(kOriginalProposerGives, eProposer).GetCString(), getSASTradeListText(kOriginalResponderGives, eResponder).GetCString(), getSASTradeListText(kProposerAdds, eProposer).GetCString(), getSASTradeListText(kResponderAdds, eResponder).GetCString());
}

// <!-- custom: The unmoddable EXE does not expose a clean DLL rejection callback carrying AI->human ordinary trade items. BUG's resolved DealRejected UI event does, so log that exact package here without importing Python/localized formatting into the schema. (ChatGPT-5.6-Sol) -->
void logSASGameRecordAIToHumanOfferRejected(PlayerTypes eProposer, PlayerTypes eResponder, CLinkList<TradeData> const& kProposerGives, CLinkList<TradeData> const& kResponderGives)
{
	CvPlayerAI const& kProposer = GET_PLAYER(eProposer);
	CvPlayerAI const& kResponder = GET_PLAYER(eResponder);
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=DIPLO_OFFER_REJECTED proposer=%d responder=%d proposerGives=%s responderGives=%s proposerAttitudeValue=%d responderAttitudeValue=%d atWar=%d",
			GC.getGame().getGameTurn(), eProposer, eResponder, getSASTradeListText(kProposerGives, eProposer).GetCString(), getSASTradeListText(kResponderGives, eResponder).GetCString(),
			kProposer.AI_getAttitudeVal(eResponder), kResponder.AI_getAttitudeVal(eProposer), GET_TEAM(kProposer.getTeam()).isAtWar(kResponder.getTeam()) ? 1 : 0);
}


// <!-- custom: Vote helpers below are recorder schema, not gameplay abstractions: they translate native vote enums/flags into stable factual history tokens while leaving AI valuation in BBAI. (ChatGPT-5.6-Sol) -->
static CvString getSASGameRecordPlayerVoteChoice(PlayerVoteTypes eChoice)
{
	CvString szChoice;
	if (eChoice >= 0 && eChoice < MAX_CIV_TEAMS)
	{
		szChoice.Format("TEAM_%d", eChoice);
		return szChoice;
	}
	switch (eChoice)
	{
	case NO_PLAYER_VOTE_CHECKED: return CvString("UNCHECKED");
	case PLAYER_VOTE_NEVER: return CvString("NEVER");
	case PLAYER_VOTE_ABSTAIN: return CvString("ABSTAIN");
	case PLAYER_VOTE_NO: return CvString("NO");
	case PLAYER_VOTE_YES: return CvString("YES");
	case NO_PLAYER_VOTE: return CvString("NONE");
	default:
		szChoice.Format("VALUE_%d", eChoice);
		return szChoice;
	}
}

static CvString getSASGameRecordVoteEffects(VoteTypes eVote)
{
	if (eVote == NO_VOTE)
		return CvString("-");
	CvVoteInfo const& kVote = GC.getInfo(eVote);
	CvString szEffects;
	CvString szItem;
	if (kVote.isSecretaryGeneral())
		szEffects = "SECRETARY_GENERAL";
	if (kVote.isVictory())
	{
		if (!szEffects.empty()) szEffects += ",";
		szEffects += "DIPLOMATIC_VICTORY";
	}
	if (kVote.getTradeRoutes() != 0)
	{
		szItem.Format(szEffects.empty() ? "TRADE_ROUTES:%+d" : ",TRADE_ROUTES:%+d", kVote.getTradeRoutes());
		szEffects += szItem;
	}
	if (kVote.isFreeTrade())
		szEffects += (szEffects.empty() ? "FREE_TRADE" : ",FREE_TRADE");
	if (kVote.isNoNukes())
		szEffects += (szEffects.empty() ? "NO_NUKES" : ",NO_NUKES");
	FOR_EACH_NON_DEFAULT_KEY(kVote.isForceCivic(), Civic)
	{
		szItem.Format(szEffects.empty() ? "FORCE_CIVIC:%s" : ",FORCE_CIVIC:%s", getSASGameRecordCivicType(eLoopCivic));
		szEffects += szItem;
	}
	if (kVote.isOpenBorders())
		szEffects += (szEffects.empty() ? "OPEN_BORDERS" : ",OPEN_BORDERS");
	if (kVote.isDefensivePact())
		szEffects += (szEffects.empty() ? "DEFENSIVE_PACT" : ",DEFENSIVE_PACT");
	if (kVote.isForcePeace())
		szEffects += (szEffects.empty() ? "FORCE_PEACE" : ",FORCE_PEACE");
	if (kVote.isForceNoTrade())
		szEffects += (szEffects.empty() ? "EMBARGO" : ",EMBARGO");
	if (kVote.isForceWar())
		szEffects += (szEffects.empty() ? "FORCE_WAR" : ",FORCE_WAR");
	if (kVote.isAssignCity())
		szEffects += (szEffects.empty() ? "ASSIGN_CITY" : ",ASSIGN_CITY");
	return getSASDiagnosticOrDash(szEffects);
}

static bool isSASGameRecordPersistentVote(VoteTypes eVote)
{
	if (eVote == NO_VOTE)
		return false;
	CvVoteInfo const& kVote = GC.getInfo(eVote);
	return (kVote.getTradeRoutes() != 0 || kVote.isFreeTrade() || kVote.isNoNukes() || kVote.isForceCivic().isAnyNonDefault());
}

static CvString getSASGameRecordVotePlayerMask(qword uiPlayers)
{
	CvString szPlayers;
	for (int iPlayer = 0; iPlayer < MAX_CIV_PLAYERS; iPlayer++)
	{
		if ((uiPlayers & ((qword)1 << iPlayer)) != 0)
			appendSASDiagnosticIntListValue(szPlayers, iPlayer);
	}
	return getSASDiagnosticOrDash(szPlayers);
}

static CvString getSASGameRecordVoteEligibleTeams(VoteTriggeredData const& kTriggered)
{
	CvGame const& kGame = GC.getGame();
	if (!kGame.isTeamVote(kTriggered.kVoteOption.eVote))
		return CvString("-");
	CvString szTeams;
	for (TeamIter<MAJOR_CIV> itTeam; itTeam.hasNext(); ++itTeam)
	{
		if (kGame.isTeamVoteEligible(itTeam->getID(), kTriggered.eVoteSource))
			appendSASDiagnosticIntListValue(szTeams, itTeam->getID());
	}
	return getSASDiagnosticOrDash(szTeams);
}

static CvString getSASGameRecordVoteBallots(VoteTriggeredData const& kTriggered)
{
	CvGame const& kGame = GC.getGame();
	CvString szBallots;
	for (PlayerIter<MAJOR_CIV> itPlayer; itPlayer.hasNext(); ++itPlayer)
	{
		if (!itPlayer->isVotingMember(kTriggered.eVoteSource))
			continue;
		PlayerVoteTypes const eChoice = kGame.getPlayerVote(itPlayer->getID(), kTriggered.getID());
		CvString szItem;
		szItem.Format(szBallots.empty() ? "P%d@T%d:%s:%d" : ",P%d@T%d:%s:%d",
				itPlayer->getID(), itPlayer->getTeam(), getSASGameRecordPlayerVoteChoice(eChoice).GetCString(),
				itPlayer->getVotes(kTriggered.kVoteOption.eVote, kTriggered.eVoteSource));
		szBallots += szItem;
	}
	return getSASDiagnosticOrDash(szBallots);
}

static void getSASGameRecordVoteTarget(VoteTriggeredData const& kTriggered, PlayerTypes& eTargetPlayer, TeamTypes& eTargetTeam, CvCity const*& pTargetCity, PlayerTypes& eOtherPlayer, TeamTypes& eOtherTeam)
{
	eTargetPlayer = kTriggered.kVoteOption.ePlayer;
	eTargetTeam = (eTargetPlayer == NO_PLAYER ? NO_TEAM : GET_PLAYER(eTargetPlayer).getTeam());
	pTargetCity = NULL;
	if (eTargetPlayer != NO_PLAYER && kTriggered.kVoteOption.iCityId >= 0)
		pTargetCity = GET_PLAYER(eTargetPlayer).getCity(kTriggered.kVoteOption.iCityId);
	eOtherPlayer = kTriggered.kVoteOption.eOtherPlayer;
	eOtherTeam = (eOtherPlayer == NO_PLAYER ? NO_TEAM : GET_PLAYER(eOtherPlayer).getTeam());
}

void logSASGameRecordVoteTriggered(VoteTriggeredData const* pVoteTriggered)
{
	if (pVoteTriggered == NULL || pVoteTriggered->kVoteOption.eVote == NO_VOTE)
		return;
	CvGame const& kGame = GC.getGame();
	VoteTypes const eVote = pVoteTriggered->kVoteOption.eVote;
	CvVoteInfo const& kVote = GC.getInfo(eVote);
	TeamTypes const eSecretaryTeam = kGame.getSecretaryGeneral(pVoteTriggered->eVoteSource);
	PlayerTypes eProposer = NO_PLAYER;
	if (!kVote.isSecretaryGeneral() && eSecretaryTeam != NO_TEAM)
		eProposer = GET_TEAM(eSecretaryTeam).getSecretaryID();
	PlayerTypes eTargetPlayer;
	TeamTypes eTargetTeam;
	CvCity const* pTargetCity;
	PlayerTypes eOtherPlayer;
	TeamTypes eOtherTeam;
	getSASGameRecordVoteTarget(*pVoteTriggered, eTargetPlayer, eTargetTeam, pTargetCity, eOtherPlayer, eOtherTeam);
	bool const bPreviouslyPassed = kGame.isVotePassed(eVote);
	char const* szContext = (kVote.isSecretaryGeneral() ? "AUTOMATIC_SECRETARY_ELECTION" : (bPreviouslyPassed ? "ACTIVE_RESOLUTION_RECONSIDERATION" : "SECRETARY_PROPOSAL"));
	BuildingTypes const eSourceBuilding = kGame.getVoteSourceBuilding(pVoteTriggered->eVoteSource);
	CvCity const* pSourceCity = kGame.getVoteSourceCity(pVoteTriggered->eVoteSource, NO_TEAM);

	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=DIPLO_VOTE_TRIGGERED triggeredId=%d source=%s vote=%s context=%s sourceReligion=%s sourceBuilding=%s sourceOwner=%d sourceCityId=%d sourceCity=%S sourceX=%d sourceY=%d secretaryTeamBefore=%d proposerPlayer=%d teamVote=%d secretaryElection=%d victoryVote=%d previousOutcome=%s previouslyPassed=%d effects=%s eligibleTeams=%s requiredVotes=%d possibleVotes=%d targetPlayer=%d targetTeam=%d targetCityId=%d targetCity=%S targetX=%d targetY=%d otherPlayer=%d otherTeam=%d",
			kGame.getGameTurn(), pVoteTriggered->getID(), getSASGameRecordVoteSourceType(pVoteTriggered->eVoteSource), getSASGameRecordVoteType(eVote), szContext, getSASGameRecordReligionType(kGame.getVoteSourceReligion(pVoteTriggered->eVoteSource)),
			getSASGameRecordBuildingType(eSourceBuilding), pSourceCity == NULL ? -1 : pSourceCity->getOwner(), pSourceCity == NULL ? -1 : pSourceCity->getID(), getSASGameRecordQuotedCityName(pSourceCity).GetCString(), pSourceCity == NULL ? -1 : pSourceCity->getX(), pSourceCity == NULL ? -1 : pSourceCity->getY(),
			eSecretaryTeam, eProposer, kGame.isTeamVote(eVote) ? 1 : 0, kVote.isSecretaryGeneral() ? 1 : 0, kVote.isVictory() ? 1 : 0,
			getSASGameRecordPlayerVoteChoice(kGame.getVoteOutcome(eVote)).GetCString(), bPreviouslyPassed ? 1 : 0, getSASGameRecordVoteEffects(eVote).GetCString(), getSASGameRecordVoteEligibleTeams(*pVoteTriggered).GetCString(),
			kGame.getVoteRequired(eVote, pVoteTriggered->eVoteSource), kGame.countPossibleVote(eVote, pVoteTriggered->eVoteSource),
			eTargetPlayer, eTargetTeam, pVoteTriggered->kVoteOption.iCityId, getSASGameRecordQuotedCityName(pTargetCity).GetCString(), pTargetCity == NULL ? -1 : pTargetCity->getX(), pTargetCity == NULL ? -1 : pTargetCity->getY(), eOtherPlayer, eOtherTeam);
}

void logSASGameRecordVoteResult(VoteTriggeredData const* pVoteTriggered, bool bThresholdPassed, bool bPassed, bool bCancelled, qword uiDefaultedAbstain, qword uiDefiers, qword uiEndorsers)
{
	if (pVoteTriggered == NULL || pVoteTriggered->kVoteOption.eVote == NO_VOTE)
		return;
	CvGame const& kGame = GC.getGame();
	VoteTypes const eVote = pVoteTriggered->kVoteOption.eVote;
	CvVoteInfo const& kVote = GC.getInfo(eVote);
	bool const bTeamVote = kGame.isTeamVote(eVote);
	bool const bPreviouslyPassed = kGame.isVotePassed(eVote);
	TeamTypes eWinningTeam = NO_TEAM;
	int iWinningVotes = -1;
	if (bTeamVote && !bCancelled)
	{
		eWinningTeam = kGame.findHighestVoteTeam(*pVoteTriggered);
		if (eWinningTeam != NO_TEAM)
			iWinningVotes = kGame.countVote(*pVoteTriggered, (PlayerVoteTypes)eWinningTeam);
	}
	int const iYesVotes = (bTeamVote || bCancelled ? -1 : kGame.countVote(*pVoteTriggered, PLAYER_VOTE_YES));
	int const iNoVotes = (bTeamVote || bCancelled ? -1 : kGame.countVote(*pVoteTriggered, PLAYER_VOTE_NO));
	int const iAbstainVotes = (bCancelled ? -1 : kGame.countVote(*pVoteTriggered, PLAYER_VOTE_ABSTAIN));
	int const iNeverVotes = (bTeamVote || bCancelled ? -1 : kGame.countVote(*pVoteTriggered, PLAYER_VOTE_NEVER));
	char const* szResult = (bCancelled ? "CANCELLED" : (bPassed ? "PASSED" : (bThresholdPassed && uiDefiers != 0 ? "DEFIED" : "FAILED")));
	char const* szOperation = "NO_CHANGE";
	if (bCancelled)
		szOperation = "CANCELLED";
	else if (kVote.isSecretaryGeneral())
		szOperation = (bPassed ? "ELECT" : (bPreviouslyPassed ? "CLEAR_SECRETARY" : "NO_ELECTION"));
	else if (kVote.isVictory())
		szOperation = (bPassed ? "DIPLOMATIC_VICTORY" : "NO_VICTORY");
	else if (isSASGameRecordPersistentVote(eVote))
	{
		if (bPreviouslyPassed && !bPassed) szOperation = "DEACTIVATE";
		else if (!bPreviouslyPassed && bPassed) szOperation = "ACTIVATE";
		else if (bPreviouslyPassed && bPassed) szOperation = "KEEP_ACTIVE";
	}
	else if (bPassed)
		szOperation = "APPLY_ONE_SHOT";
	PlayerTypes eTargetPlayer;
	TeamTypes eTargetTeam;
	CvCity const* pTargetCity;
	PlayerTypes eOtherPlayer;
	TeamTypes eOtherTeam;
	getSASGameRecordVoteTarget(*pVoteTriggered, eTargetPlayer, eTargetTeam, pTargetCity, eOtherPlayer, eOtherTeam);
	TeamTypes const eSecretaryTeam = kGame.getSecretaryGeneral(pVoteTriggered->eVoteSource);

	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=DIPLO_VOTE_RESULT triggeredId=%d source=%s vote=%s result=%s operation=%s sourceReligion=%s secretaryTeamBefore=%d teamVote=%d secretaryElection=%d victoryVote=%d previousOutcome=%s previouslyPassed=%d thresholdPassed=%d resolutionPassed=%d winningTeam=%d winningVotes=%d yesVotes=%d noVotes=%d abstainVotes=%d neverVotes=%d requiredVotes=%d possibleVotes=%d defaultedAbstain=%s defiers=%s endorsers=%s effects=%s ballots=%s targetPlayer=%d targetTeam=%d targetCityId=%d targetCity=%S targetX=%d targetY=%d otherPlayer=%d otherTeam=%d",
			kGame.getGameTurn(), pVoteTriggered->getID(), getSASGameRecordVoteSourceType(pVoteTriggered->eVoteSource), getSASGameRecordVoteType(eVote), szResult, szOperation, getSASGameRecordReligionType(kGame.getVoteSourceReligion(pVoteTriggered->eVoteSource)),
			eSecretaryTeam, bTeamVote ? 1 : 0, kVote.isSecretaryGeneral() ? 1 : 0, kVote.isVictory() ? 1 : 0,
			getSASGameRecordPlayerVoteChoice(kGame.getVoteOutcome(eVote)).GetCString(), bPreviouslyPassed ? 1 : 0, bThresholdPassed ? 1 : 0, bPassed ? 1 : 0,
			eWinningTeam, iWinningVotes, iYesVotes, iNoVotes, iAbstainVotes, iNeverVotes,
			kGame.getVoteRequired(eVote, pVoteTriggered->eVoteSource), kGame.countPossibleVote(eVote, pVoteTriggered->eVoteSource),
			getSASGameRecordVotePlayerMask(uiDefaultedAbstain).GetCString(), getSASGameRecordVotePlayerMask(uiDefiers).GetCString(), getSASGameRecordVotePlayerMask(uiEndorsers).GetCString(),
			getSASGameRecordVoteEffects(eVote).GetCString(), getSASGameRecordVoteBallots(*pVoteTriggered).GetCString(),
			eTargetPlayer, eTargetTeam, pVoteTriggered->kVoteOption.iCityId, getSASGameRecordQuotedCityName(pTargetCity).GetCString(), pTargetCity == NULL ? -1 : pTargetCity->getX(), pTargetCity == NULL ? -1 : pTargetCity->getY(), eOtherPlayer, eOtherTeam);
}

void logSASGameRecordReligionFounded(ReligionTypes eReligion, PlayerTypes ePlayer)
{
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=RELIGION_FOUNDED player=%d religion=%s", GC.getGame().getGameTurn(), ePlayer, getSASGameRecordReligionType(eReligion));
}

void logSASGameRecordCorporationFounded(CorporationTypes eCorporation, PlayerTypes ePlayer)
{
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=CORPORATION_FOUNDED player=%d corporation=%s", GC.getGame().getGameTurn(), ePlayer, getSASGameRecordCorporationType(eCorporation));
}

void logSASGameRecordCityGrowthPrevented(CvCity const* pCity, int iFoodDiscarded)
{
	if (pCity == NULL)
		return;
	PlayerTypes const ePlayer = pCity->getOwner();
	if (ePlayer < 0 || ePlayer >= MAX_CIV_PLAYERS)
		return;
	// <!-- custom: Avoid Growth can repeatedly cap a city at its threshold and discard excess food.
	// Keep only interval totals so AI growth suppression is measurable without one action row per capped city turn. (ChatGPT-5.6-Sol) -->
	SASGameRecordPlayerFlow& kFlow = g_akSASGameRecordPlayerFlow[ePlayer];
	kFlow.iCityGrowthPreventedEvents++;
	kFlow.iFoodDiscardedByAvoidGrowth += std::max(0, iFoodDiscarded);
}

void logSASGameRecordCityPopulationChanged(CvCity const* pCity, bool bGrowth, int iPopulationBefore, int iFoodDifference, int iFoodBefore, int iFoodAfterDifference, int iFoodKeptBefore, int iFoodKeptBeforePopulationChange, int iGrowthThresholdBefore)
{
	if (pCity == NULL)
		return;
	PlayerTypes const ePlayer = pCity->getOwner();
	if (ePlayer < 0 || ePlayer >= MAX_CIV_PLAYERS)
		return;
	int const iPopulationAfter = pCity->getPopulation();
	int const iPopulationDelta = iPopulationAfter - iPopulationBefore;
	if ((bGrowth && iPopulationDelta <= 0) || (!bGrowth && iPopulationDelta >= 0))
		return;
	SASGameRecordPlayerFlow& kFlow = g_akSASGameRecordPlayerFlow[ePlayer];
	if (bGrowth)
	{
		kFlow.iCityGrowthEvents++;
		kFlow.iPopulationGainedFromGrowth += iPopulationDelta;
	}
	else
	{
		kFlow.iCityStarvationEvents++;
		kFlow.iPopulationLostToStarvation += -iPopulationDelta;
	}
	// <!-- custom: Level 2 keeps only interval natural-population totals; level 3 preserves the exact city transition and the food/granary states immediately before and after CvCity::doGrowth resolves it.
	// This hook is observation-only and is called after the existing population/food mutations, before Python's successful-growth event can add unrelated side effects. (ChatGPT-5.6-Sol) -->
	if (gGameRecordLogLevel >= 3)
	{
		logSASGameRecord("GAME_RECORD_ACTION turn=%d type=CITY_POPULATION_CHANGED cause=%s player=%d cityId=%d city=%S x=%d y=%d populationBefore=%d populationAfter=%d populationDelta=%+d foodDifference=%+d foodBefore=%d foodAfterDifference=%d foodAfter=%d foodKeptBefore=%d foodKeptBeforePopulationChange=%d foodKeptAfter=%d growthThresholdBefore=%d growthThresholdAfter=%d maxFoodKeptPercent=%d",
			GC.getGame().getGameTurn(), bGrowth ? "GROWTH" : "STARVATION", ePlayer, pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), pCity->getX(), pCity->getY(),
			iPopulationBefore, iPopulationAfter, iPopulationDelta, iFoodDifference, iFoodBefore, iFoodAfterDifference, pCity->getFood(), iFoodKeptBefore, iFoodKeptBeforePopulationChange, pCity->getFoodKept(),
			iGrowthThresholdBefore, pCity->growthThreshold(), pCity->getMaxFoodKeptPercent());
	}
}

void logSASGameRecordCityCultureExpanded(CvCity const* pCity)
{
	if (pCity == NULL || pCity->getCultureLevel() == NO_CULTURELEVEL)
		return;
	CultureLevelTypes const eCultureLevel = pCity->getCultureLevel();
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=CITY_CULTURE_EXPANDED player=%d cityId=%d city=%S x=%d y=%d cultureLevel=%s cultureLevelId=%d ownerCultureTimes100=%d nextCultureThreshold=%d defenseModifier=%d totalDefense=%d",
		GC.getGame().getGameTurn(), pCity->getOwner(), pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), pCity->getX(), pCity->getY(),
		GC.getInfo(eCultureLevel).getType(), eCultureLevel, pCity->getCultureTimes100(pCity->getOwner()), pCity->getCultureThreshold(), pCity->getDefenseModifier(false), pCity->getTotalDefense(false));
}

void logSASGameRecordCityHurry(CvCity const* pCity, HurryTypes eHurry, int iProductionBefore, int iProductionAdded, int iGoldCost, int iPopulationCost, int iHurryAngerAdded, int iGoldBefore, int iPopulationBefore, int iHurryAngerBefore)
{
	if (pCity == NULL || eHurry == NO_HURRY)
		return;
	PlayerTypes const ePlayer = pCity->getOwner();
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=CITY_HURRIED player=%d cityId=%d city=%S x=%d y=%d hurry=%s targetKind=%s target=%s productionBefore=%d productionNeeded=%d productionAdded=%d productionAfter=%d goldCost=%d goldBefore=%d goldAfter=%d populationCost=%d populationBefore=%d populationAfter=%d hurryAngerAdded=%d hurryAngerBefore=%d hurryAngerAfter=%d",
			GC.getGame().getGameTurn(), ePlayer, pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), pCity->getX(), pCity->getY(), GC.getInfo(eHurry).getType(),
			getSASGameRecordCityProductionKind(*pCity), getSASGameRecordCityProductionType(*pCity), iProductionBefore, getSASGameRecordCityProductionNeeded(*pCity), iProductionAdded, pCity->getProduction(),
			iGoldCost, iGoldBefore, kPlayer.getGold(), iPopulationCost, iPopulationBefore, pCity->getPopulation(), iHurryAngerAdded, iHurryAngerBefore, pCity->getHurryAngerTimer());
}

void logSASGameRecordPillage(CvUnit const* pUnit, SASGameRecordPlotState const& kOldPlotState, PlayerTypes eVictimPlayer, int iGoldGained)
{
	if (pUnit == NULL)
		return;
	CvPlot const& kPlot = pUnit->getPlot();
	CvCity const* pWorkingCity = kPlot.getWorkingCity();
	char const* szStructure = (kOldPlotState.eRoute != kPlot.getRouteType() ? "ROUTE" :
			(kOldPlotState.eImprovement != kPlot.getImprovementType() ? "IMPROVEMENT" : "-"));
	TeamTypes const eVictimTeam = (eVictimPlayer == NO_PLAYER ? NO_TEAM : GET_PLAYER(eVictimPlayer).getTeam());
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=UNIT_PILLAGE player=%d team=%d unitId=%d unit=%s unitAI=%s x=%d y=%d victimPlayer=%d victimTeam=%d structure=%s improvementOld=%s improvementNew=%s routeOld=%s routeNew=%s bonus=%s workingCityId=%d workingCity=%S goldGained=%d hiddenNationality=%d alwaysHostile=%d",
			GC.getGame().getGameTurn(), pUnit->getOwner(), pUnit->getTeam(), pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), getSASGameRecordUnitAIType(pUnit->AI_getUnitAIType()),
			kPlot.getX(), kPlot.getY(), eVictimPlayer, eVictimTeam, szStructure,
			getSASGameRecordImprovementType(kOldPlotState.eImprovement), getSASGameRecordImprovementType(kPlot.getImprovementType()),
			getSASGameRecordRouteType(kOldPlotState.eRoute), getSASGameRecordRouteType(kPlot.getRouteType()), getSASGameRecordBonusType(kOldPlotState.eBonus),
			pWorkingCity == NULL ? -1 : pWorkingCity->getID(), getSASGameRecordQuotedCityName(pWorkingCity).GetCString(), iGoldGained,
			pUnit->getUnitInfo().isHiddenNationality() ? 1 : 0, pUnit->isAlwaysHostile(kPlot) ? 1 : 0);
}

static int getSASGameRecordBlockadeContextIndex(PlayerTypes ePlayer, int iUnitId)
{
	for (size_t iI = 0; iI < g_aSASGameRecordBlockades.size(); iI++)
	{
		if (g_aSASGameRecordBlockades[iI].ePlayer == ePlayer && g_aSASGameRecordBlockades[iI].iUnitId == iUnitId)
			return (int)iI;
	}
	return -1;
}

static bool hasSASGameRecordCityReference(std::vector<std::pair<PlayerTypes,int> > const& aCities, PlayerTypes ePlayer, int iCityId)
{
	for (size_t iI = 0; iI < aCities.size(); iI++)
	{
		if (aCities[iI].first == ePlayer && aCities[iI].second == iCityId)
			return true;
	}
	return false;
}

static void captureSASGameRecordBlockadeContext(CvUnit const& kUnit, SASGameRecordBlockadeContext& kContext, bool bCheckCanPlunder)
{
	kContext.ePlayer = kUnit.getOwner();
	kContext.iUnitId = kUnit.getID();
	kContext.iStartTurn = GC.getGame().getGameTurn();
	kContext.iStartElapsedTurn = GC.getGame().getElapsedGameTurns();
	kContext.iStartX = kUnit.getX();
	kContext.iStartY = kUnit.getY();
	kContext.iRangePlots = 0;
	kContext.iAffectedTeams = 0;
	kContext.iAffectedCities = 0;
	kContext.szRangePlots.clear();
	kContext.szAffectedTeams.clear();
	kContext.szAffectedCities.clear();
	kContext.iPlunderEvents = 0;
	kContext.iGoldPlundered = 0;
	kContext.aPlunderedCities.clear();

	std::vector<CvPlot*> apRange;
	// <!-- custom: START mirrors updatePlunder's legal range exactly.
	// A fallback END without recorder start context can request the physical range with legality disabled because the unit may be ending precisely after becoming unable to plunder. (ChatGPT-5.6-Sol) -->
	kUnit.blockadeRange(apRange, 0, bCheckCanPlunder);
	kContext.iRangePlots = (int)apRange.size();
	for (size_t iI = 0; iI < apRange.size(); iI++)
	{
		CvString szItem;
		szItem.Format(kContext.szRangePlots.empty() ? "%d,%d" : ";%d,%d", apRange[iI]->getX(), apRange[iI]->getY());
		kContext.szRangePlots += szItem;
	}

	bool abAffectedTeams[MAX_TEAMS];
	for (int iTeam = 0; iTeam < MAX_TEAMS; iTeam++)
		abAffectedTeams[iTeam] = false;
	// <!-- custom: Mirror CvUnit::updatePlunder's exact team iterator/admission test so the logged team/city set describes teams this unit actually contributes blockade counts against, including hidden-nationality behavior. (ChatGPT-5.6-Sol) -->
	for (TeamIter<ALIVE,KNOWN_POTENTIAL_ENEMY_OF> it(kUnit.getTeam()); it.hasNext(); ++it)
	{
		CvTeam const& kTeam = *it;
		if (!kUnit.isEnemy(kTeam.getID()))
			continue;
		abAffectedTeams[kTeam.getID()] = true;
		CvString szItem;
		szItem.Format(kContext.szAffectedTeams.empty() ? "%d" : ",%d", kTeam.getID());
		kContext.szAffectedTeams += szItem;
		kContext.iAffectedTeams++;
	}

	std::vector<std::pair<PlayerTypes,int> > aCities;
	for (size_t iI = 0; iI < apRange.size(); iI++)
	{
		FOR_EACH_ADJ_PLOT(*apRange[iI])
		{
			CvCity const* pCity = pAdj->getPlotCity();
			if (pCity == NULL || pCity->getTeam() < 0 || pCity->getTeam() >= MAX_TEAMS || !abAffectedTeams[pCity->getTeam()] ||
				hasSASGameRecordCityReference(aCities, pCity->getOwner(), pCity->getID()))
			{
				continue;
			}
			aCities.push_back(std::make_pair(pCity->getOwner(), pCity->getID()));
			CvString szItem;
			szItem.Format(kContext.szAffectedCities.empty() ? "P%d:C%d@%d,%d" : ";P%d:C%d@%d,%d",
					pCity->getOwner(), pCity->getID(), pCity->getX(), pCity->getY());
			kContext.szAffectedCities += szItem;
		}
	}
	kContext.iAffectedCities = (int)aCities.size();
}

void logSASGameRecordBlockadeChanged(CvUnit const* pUnit, bool bStarting)
{
	if (pUnit == NULL)
		return;
	int const iExisting = getSASGameRecordBlockadeContextIndex(pUnit->getOwner(), pUnit->getID());
	if (bStarting)
	{
		if (iExisting >= 0)
			g_aSASGameRecordBlockades.erase(g_aSASGameRecordBlockades.begin() + iExisting);
		SASGameRecordBlockadeContext kContext;
		captureSASGameRecordBlockadeContext(*pUnit, kContext, true);
		g_aSASGameRecordBlockades.push_back(kContext);
		logSASGameRecord("GAME_RECORD_ACTION turn=%d type=NAVAL_BLOCKADE_STARTED player=%d team=%d unitId=%d unit=%s unitAI=%s x=%d y=%d rangePlots=%d plots=%s affectedTeams=%d teams=%s affectedCities=%d cities=%s canPlunder=%d hiddenNationality=%d alwaysHostile=%d",
				GC.getGame().getGameTurn(), pUnit->getOwner(), pUnit->getTeam(), pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), getSASGameRecordUnitAIType(pUnit->AI_getUnitAIType()),
				pUnit->getX(), pUnit->getY(), kContext.iRangePlots, kContext.szRangePlots.empty() ? "-" : kContext.szRangePlots.GetCString(),
				kContext.iAffectedTeams, kContext.szAffectedTeams.empty() ? "-" : kContext.szAffectedTeams.GetCString(),
				kContext.iAffectedCities, kContext.szAffectedCities.empty() ? "-" : kContext.szAffectedCities.GetCString(),
				pUnit->canPlunder(pUnit->getPlot()) ? 1 : 0, pUnit->getUnitInfo().isHiddenNationality() ? 1 : 0, pUnit->isAlwaysHostile(pUnit->getPlot()) ? 1 : 0);
		return;
	}

	SASGameRecordBlockadeContext kContext;
	bool const bStartKnown = (iExisting >= 0);
	if (bStartKnown)
		kContext = g_aSASGameRecordBlockades[iExisting];
	else
	{
		captureSASGameRecordBlockadeContext(*pUnit, kContext, false);
		kContext.iStartTurn = -1;
		kContext.iStartElapsedTurn = -1;
		kContext.iStartX = -1;
		kContext.iStartY = -1;
	}
	int const iDurationTurns = (bStartKnown ? GC.getGame().getGameTurn() - kContext.iStartTurn : -1);
	int const iDurationElapsedTurns = (bStartKnown ? GC.getGame().getElapsedGameTurns() - kContext.iStartElapsedTurn : -1);
	SASGameRecordBlockadeContext kEndContext;
	captureSASGameRecordBlockadeContext(*pUnit, kEndContext, false);
	bool const bScopeChanged = (bStartKnown &&
			(kContext.szAffectedTeams != kEndContext.szAffectedTeams || kContext.szAffectedCities != kEndContext.szAffectedCities));
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=NAVAL_BLOCKADE_ENDED player=%d team=%d unitId=%d unit=%s unitAI=%s startKnown=%d rangeSource=%s startTurn=%d startX=%d startY=%d endX=%d endY=%d durationTurns=%d durationElapsedTurns=%d rangePlots=%d plots=%s affectedTeams=%d teams=%s affectedCities=%d cities=%s endAffectedTeams=%d endTeams=%s endAffectedCities=%d endCities=%s scopeChanged=%d plunderEvents=%d goldPlundered=%d uniquePlunderedCities=%d canPlunderAtEnd=%d",
			GC.getGame().getGameTurn(), pUnit->getOwner(), pUnit->getTeam(), pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), getSASGameRecordUnitAIType(pUnit->AI_getUnitAIType()),
			bStartKnown ? 1 : 0, bStartKnown ? "START" : "END_FALLBACK", kContext.iStartTurn, kContext.iStartX, kContext.iStartY, pUnit->getX(), pUnit->getY(), iDurationTurns, iDurationElapsedTurns,
			kContext.iRangePlots, kContext.szRangePlots.empty() ? "-" : kContext.szRangePlots.GetCString(),
			kContext.iAffectedTeams, kContext.szAffectedTeams.empty() ? "-" : kContext.szAffectedTeams.GetCString(),
			kContext.iAffectedCities, kContext.szAffectedCities.empty() ? "-" : kContext.szAffectedCities.GetCString(),
			kEndContext.iAffectedTeams, kEndContext.szAffectedTeams.empty() ? "-" : kEndContext.szAffectedTeams.GetCString(),
			kEndContext.iAffectedCities, kEndContext.szAffectedCities.empty() ? "-" : kEndContext.szAffectedCities.GetCString(), bScopeChanged ? 1 : 0,
			kContext.iPlunderEvents, kContext.iGoldPlundered, (int)kContext.aPlunderedCities.size(), pUnit->canPlunder(pUnit->getPlot()) ? 1 : 0);
	if (iExisting >= 0)
		g_aSASGameRecordBlockades.erase(g_aSASGameRecordBlockades.begin() + iExisting);
}

void logSASGameRecordBlockadePlunder(CvUnit const* pUnit, CvCity const* pCity, int iGold, int iTradeRoutes, int iProfitPerRoute)
{
	if (pUnit == NULL || pCity == NULL || iGold <= 0)
		return;
	int const iContext = getSASGameRecordBlockadeContextIndex(pUnit->getOwner(), pUnit->getID());
	int iStartTurn = -1;
	int iAgeTurns = -1;
	if (iContext >= 0)
	{
		SASGameRecordBlockadeContext& kContext = g_aSASGameRecordBlockades[iContext];
		kContext.iPlunderEvents++;
		kContext.iGoldPlundered += iGold;
		if (!hasSASGameRecordCityReference(kContext.aPlunderedCities, pCity->getOwner(), pCity->getID()))
			kContext.aPlunderedCities.push_back(std::make_pair(pCity->getOwner(), pCity->getID()));
		iStartTurn = kContext.iStartTurn;
		iAgeTurns = GC.getGame().getGameTurn() - kContext.iStartTurn;
	}
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=NAVAL_BLOCKADE_PLUNDER player=%d team=%d unitId=%d unit=%s unitAI=%s unitX=%d unitY=%d victimPlayer=%d victimTeam=%d cityId=%d city=%S cityX=%d cityY=%d gold=%d tradeRoutes=%d profitPerRoute=%d blockadeStartKnown=%d blockadeStartTurn=%d blockadeAgeTurns=%d",
			GC.getGame().getGameTurn(), pUnit->getOwner(), pUnit->getTeam(), pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), getSASGameRecordUnitAIType(pUnit->AI_getUnitAIType()),
			pUnit->getX(), pUnit->getY(), pCity->getOwner(), pCity->getTeam(), pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), pCity->getX(), pCity->getY(),
			iGold, iTradeRoutes, iProfitPerRoute, iContext >= 0 ? 1 : 0, iStartTurn, iAgeTurns);
}

void logSASGameRecordUnitGifted(CvUnit const* pUnit, PlayerTypes eGiftingPlayer, CvPlot const* pPlotLocation)
{
	if (pUnit == NULL || eGiftingPlayer < 0 || eGiftingPlayer >= MAX_PLAYERS)
		return;
	PlayerTypes const eReceiver = pUnit->getOwner();
	TeamTypes const eGiverTeam = GET_PLAYER(eGiftingPlayer).getTeam();
	TeamTypes const eReceiverTeam = (eReceiver == NO_PLAYER ? NO_TEAM : GET_PLAYER(eReceiver).getTeam());
	CvPlot const* pPlot = (pPlotLocation == NULL ? pUnit->plot() : pPlotLocation);
	int iPromotions = 0;
	FOR_EACH_ENUM(Promotion)
	{
		if (pUnit->isHasPromotion(eLoopPromotion))
			iPromotions++;
	}
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=UNIT_GIFTED giverPlayer=%d giverTeam=%d receiverPlayer=%d receiverTeam=%d unitId=%d unit=%s unitAI=%s x=%d y=%d area=%d experience=%d level=%d promotions=%d damage=%d productionNeeded=%d canCombat=%d cargo=%d transportId=%d",
			GC.getGame().getGameTurn(), eGiftingPlayer, eGiverTeam, eReceiver, eReceiverTeam, pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), getSASGameRecordUnitAIType(pUnit->AI_getUnitAIType()),
			pPlot == NULL ? -1 : pPlot->getX(), pPlot == NULL ? -1 : pPlot->getY(), pPlot == NULL ? -1 : pPlot->getArea().getID(), pUnit->getExperience(), pUnit->getLevel(), iPromotions, pUnit->getDamage(),
			eReceiver == NO_PLAYER ? -1 : GET_PLAYER(eReceiver).getProductionNeeded(pUnit->getUnitType()), pUnit->canCombat() ? 1 : 0, pUnit->isCargo() ? 1 : 0, pUnit->getTransportUnit() == NULL ? -1 : pUnit->getTransportUnit()->getID());
}

void logSASGameRecordReligionChanged(ReligionTypes eReligion, PlayerTypes ePlayer, CvCity const* pCity, bool bAdded)
{
	if (eReligion == NO_RELIGION || pCity == NULL)
		return;
	CvGame const& kGame = GC.getGame();
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=%s player=%d team=%d cityId=%d city=%S x=%d y=%d religion=%s holyCity=%d effectiveStateReligion=%s religionsInCity=%d",
			kGame.getGameTurn(), bAdded ? "RELIGION_SPREAD" : "RELIGION_REMOVED", ePlayer, kPlayer.getTeam(), pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), pCity->getX(), pCity->getY(),
			getSASGameRecordReligionType(eReligion), kGame.getHolyCity(eReligion) == pCity ? 1 : 0, getSASGameRecordReligionType(kPlayer.getStateReligion()), pCity->getReligionCount());
}

void logSASGameRecordReligionSpreadAttempt(CvUnit const* pUnit, ReligionTypes eReligion, CvCity const* pCity, int iDirectSpreadChance, bool bSuccess, ReligionTypes eDisplacedReligion)
{
	if (pUnit == NULL || eReligion == NO_RELIGION || pCity == NULL)
		return;
	char const* szOutcome = (bSuccess ? (eDisplacedReligion == NO_RELIGION ? "SPREAD" : "SPREAD_AND_DISPLACE") : "FAILED");
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=RELIGION_SPREAD_ATTEMPT player=%d team=%d unitId=%d unit=%s unitAI=%s religion=%s targetPlayer=%d targetTeam=%d cityId=%d city=%S x=%d y=%d directSpreadChance=%d outcome=%s displacedReligion=%s religionsBefore=%d",
			GC.getGame().getGameTurn(), pUnit->getOwner(), pUnit->getTeam(), pUnit->getID(),
			getSASGameRecordUnitType(pUnit->getUnitType()), getSASGameRecordUnitAIType(pUnit->AI_getUnitAIType()), getSASGameRecordReligionType(eReligion),
			pCity->getOwner(), pCity->getTeam(), pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), pCity->getX(), pCity->getY(),
			iDirectSpreadChance, szOutcome, getSASGameRecordReligionType(eDisplacedReligion), pCity->getReligionCount());
}

void logSASGameRecordCorporationChanged(CorporationTypes eCorporation, PlayerTypes ePlayer, CvCity const* pCity, bool bAdded)
{
	if (eCorporation == NO_CORPORATION || pCity == NULL)
		return;
	CvGame const& kGame = GC.getGame();
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=%s player=%d team=%d cityId=%d city=%S x=%d y=%d corporation=%s headquarters=%d corporationsInCity=%d",
			kGame.getGameTurn(), bAdded ? "CORPORATION_SPREAD" : "CORPORATION_REMOVED", ePlayer, kPlayer.getTeam(), pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), pCity->getX(), pCity->getY(),
			getSASGameRecordCorporationType(eCorporation), kGame.getHeadquarters(eCorporation) == pCity ? 1 : 0, pCity->getCorporationCount());
}

void logSASGameRecordCorporationSpreadAttempt(CvUnit const* pUnit, CorporationTypes eCorporation, CvCity const* pCity, int iSpreadChance, int iGoldCost, int iGoldBefore, bool bSuccess)
{
	if (pUnit == NULL || eCorporation == NO_CORPORATION || pCity == NULL)
		return;
	CvPlayer const& kPlayer = GET_PLAYER(pUnit->getOwner());
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=CORPORATION_SPREAD_ATTEMPT player=%d team=%d unitId=%d unit=%s unitAI=%s corporation=%s targetPlayer=%d targetTeam=%d cityId=%d city=%S x=%d y=%d spreadChance=%d outcome=%s goldCost=%d goldBefore=%d goldAfter=%d corporationsBefore=%d",
			GC.getGame().getGameTurn(), pUnit->getOwner(), pUnit->getTeam(), pUnit->getID(),
			getSASGameRecordUnitType(pUnit->getUnitType()), getSASGameRecordUnitAIType(pUnit->AI_getUnitAIType()), getSASGameRecordCorporationType(eCorporation),
			pCity->getOwner(), pCity->getTeam(), pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), pCity->getX(), pCity->getY(),
			iSpreadChance, bSuccess ? "SPREAD" : "FAILED", iGoldCost, iGoldBefore, kPlayer.getGold(), pCity->getCorporationCount());
}

void logSASGameRecordCircumnavigated(TeamTypes eTeam, int iFreeSeaMoves, bool bBonusApplied, int iSeaExtraMovesBefore, int iSeaExtraMovesAfter)
{
	if (eTeam == NO_TEAM)
		return;
	CvMap const& kMap = GC.getMap();
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=CIRCUMNAVIGATION_COMPLETED team=%d members=%s wrapX=%d wrapY=%d freeSeaMoves=%d bonusApplied=%d seaExtraMovesBefore=%d seaExtraMovesAfter=%d",
			GC.getGame().getGameTurn(), eTeam, getSASGameRecordTeamMembers(eTeam).GetCString(), kMap.isWrapX() ? 1 : 0, kMap.isWrapY() ? 1 : 0,
			iFreeSeaMoves, bBonusApplied ? 1 : 0, iSeaExtraMovesBefore, iSeaExtraMovesAfter);
}

// <!-- custom: Financial strikes are rare but can begin, force unit disbands and end entirely between periodic snapshots.
// Record each realized strike turn from values already produced by CvPlayer::doGold. (ChatGPT-5.6-Sol) -->
void logSASGameRecordFinancialStrikeTurn(PlayerTypes ePlayer, int iGoldBefore, int iCalculatedGoldRate, int iGoldAfterClamp, int iCumulativeStrikeTurns, int iUnitsBeforeDisband, int iUnitsAfterDisband)
{
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=FINANCIAL_STRIKE_TURN player=%d team=%d goldBefore=%d calculatedGoldRate=%d projectedGoldBeforeClamp=%d goldAfterClamp=%d cumulativeStrikeTurns=%d unitsBeforeDisband=%d unitsAfterDisband=%d unitsDisbanded=%d",
			GC.getGame().getGameTurn(), ePlayer, kPlayer.getTeam(), iGoldBefore, iCalculatedGoldRate, iGoldBefore + iCalculatedGoldRate, iGoldAfterClamp, iCumulativeStrikeTurns,
			iUnitsBeforeDisband, iUnitsAfterDisband, std::max(0, iUnitsBeforeDisband - iUnitsAfterDisband));
}


// <!-- custom: Keep Golden Age/anarchy action rows consistent with the player snapshot by labeling recorder-local observations as logged turns. See KI#379. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
void logSASGameRecordGoldenAge(PlayerTypes ePlayer, bool bStart)
{
	if (ePlayer < 0 || ePlayer >= MAX_PLAYERS)
		return;
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=%s player=%d goldenAgeTurns=%d loggedGoldenAgeTurns=%d anarchyTurns=%d loggedAnarchyTurns=%d",
			GC.getGame().getGameTurn(), bStart ? "GOLDEN_AGE_STARTED" : "GOLDEN_AGE_ENDED", ePlayer, kPlayer.getGoldenAgeTurns(),
			g_aiSASGameRecordLoggedGoldenAgeTurns[ePlayer], kPlayer.getAnarchyTurns(), g_aiSASGameRecordLoggedAnarchyTurns[ePlayer]);
}

void logSASGameRecordGoldenAgeTurnsChanged(PlayerTypes ePlayer, int iChange, int iOldGoldenAgeTurns, int iNewGoldenAgeTurns)
{
	if (ePlayer < 0 || ePlayer >= MAX_PLAYERS)
		return;
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=GOLDEN_AGE_TURNS_CHANGED player=%d change=%+d oldGoldenAgeTurns=%d newGoldenAgeTurns=%d goldenAgeTurns=%d loggedGoldenAgeTurns=%d anarchyTurns=%d loggedAnarchyTurns=%d",
			GC.getGame().getGameTurn(), ePlayer, iChange, iOldGoldenAgeTurns, iNewGoldenAgeTurns, kPlayer.getGoldenAgeTurns(),
			g_aiSASGameRecordLoggedGoldenAgeTurns[ePlayer], kPlayer.getAnarchyTurns(), g_aiSASGameRecordLoggedAnarchyTurns[ePlayer]);
}

void logSASGameRecordAnarchy(PlayerTypes ePlayer, bool bStart)
{
	if (ePlayer < 0 || ePlayer >= MAX_PLAYERS)
		return;
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=%s player=%d anarchyTurns=%d loggedAnarchyTurns=%d goldenAgeTurns=%d loggedGoldenAgeTurns=%d revolutionTimer=%d conversionTimer=%d",
			GC.getGame().getGameTurn(), bStart ? "ANARCHY_STARTED" : "ANARCHY_ENDED", ePlayer, kPlayer.getAnarchyTurns(),
			g_aiSASGameRecordLoggedAnarchyTurns[ePlayer], kPlayer.getGoldenAgeTurns(), g_aiSASGameRecordLoggedGoldenAgeTurns[ePlayer], kPlayer.getRevolutionTimer(), kPlayer.getConversionTimer());
}

void logSASGameRecordCivicChanged(PlayerTypes ePlayer, CivicOptionTypes eCivicOption, CivicTypes eOldCivic, CivicTypes eNewCivic, ReligionTypes eOldEffectiveStateReligion, ReligionTypes eNewEffectiveStateReligion)
{
	if (ePlayer < 0 || ePlayer >= MAX_PLAYERS || eCivicOption == NO_CIVICOPTION)
		return;
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=CIVIC_CHANGED player=%d civicOption=%s oldCivic=%s newCivic=%s oldEffectiveStateReligion=%s newEffectiveStateReligion=%s anarchyTurns=%d",
		GC.getGame().getGameTurn(), ePlayer, GC.getInfo(eCivicOption).getType(), getSASGameRecordCivicType(eOldCivic), getSASGameRecordCivicType(eNewCivic),
		getSASGameRecordReligionType(eOldEffectiveStateReligion), getSASGameRecordReligionType(eNewEffectiveStateReligion), GET_PLAYER(ePlayer).getAnarchyTurns());
}

void logSASGameRecordLastStateReligionChanged(PlayerTypes ePlayer, ReligionTypes eOldReligion, ReligionTypes eNewReligion)
{
	if (ePlayer < 0 || ePlayer >= MAX_PLAYERS)
		return;
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	ReligionTypes const eOldEffectiveReligion = (kPlayer.isStateReligion() ? eOldReligion : NO_RELIGION);
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=LAST_STATE_RELIGION_CHANGED player=%d oldLastStateReligion=%s newLastStateReligion=%s oldEffectiveStateReligion=%s newEffectiveStateReligion=%s anarchyTurns=%d",
		GC.getGame().getGameTurn(), ePlayer, getSASGameRecordReligionType(eOldReligion), getSASGameRecordReligionType(eNewReligion), getSASGameRecordReligionType(eOldEffectiveReligion),
		getSASGameRecordReligionType(kPlayer.getStateReligion()), kPlayer.getAnarchyTurns());
}

void logSASGameRecordBuildingCompletedByProduction(CvCity const* pCity, BuildingTypes eBuilding, int iRawModifiedOverflow, int iUnmodifiedOverflow, int iKeptOverflow, int iLostProduction, int iUnusedOverflowCapacity, int iOverflowGold)
{
	if (pCity == NULL || eBuilding == NO_BUILDING)
		return;
	PlayerTypes const ePlayer = pCity->getOwner();
	SASGameRecordPlayerFlow& kFlow = g_akSASGameRecordPlayerFlow[ePlayer];
	int const iProductionNeeded = GET_PLAYER(ePlayer).getProductionNeeded(eBuilding);
	kFlow.iBuildingsCompleted++;
	kFlow.iBuildingProductionNeeded += iProductionNeeded;
	kFlow.aiBuildingTypes[eBuilding]++;
	// <!-- custom: At level 3, also keep production-completed limited buildings here: WONDER_BUILT remains the rare strategic marker, while BUILDING_COMPLETED now owns the exact production/overflow result like units and projects. (ChatGPT-5.6-Sol) -->
	if (gGameRecordLogLevel >= 3)
		logSASGameRecord("GAME_RECORD_ACTION turn=%d type=BUILDING_COMPLETED player=%d cityId=%d city=%S building=%s productionNeeded=%d rawModifiedOverflow=%d unmodifiedOverflow=%d keptOverflow=%d lostProduction=%d unusedOverflowCapacity=%d overflowGold=%d",
			GC.getGame().getGameTurn(), ePlayer, pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), getSASGameRecordBuildingType(eBuilding), iProductionNeeded,
			iRawModifiedOverflow, iUnmodifiedOverflow, iKeptOverflow, iLostProduction, iUnusedOverflowCapacity, iOverflowGold);
}

void logSASGameRecordBuildingBuilt(CvCity const* pCity, BuildingTypes eBuilding)
{
	if (pCity == NULL || eBuilding == NO_BUILDING || !GC.getInfo(eBuilding).isLimited())
		return;
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=WONDER_BUILT player=%d cityId=%d city=%S building=%s", GC.getGame().getGameTurn(), pCity->getOwner(), pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), getSASGameRecordBuildingType(eBuilding));
}

void logSASGameRecordProjectBuilt(CvCity const* pCity, ProjectTypes eProject, int iRawModifiedOverflow, int iUnmodifiedOverflow, int iKeptOverflow, int iLostProduction, int iUnusedOverflowCapacity, int iOverflowGold)
{
	if (pCity == NULL || eProject == NO_PROJECT)
		return;
	PlayerTypes const ePlayer = pCity->getOwner();
	SASGameRecordPlayerFlow& kFlow = g_akSASGameRecordPlayerFlow[ePlayer];
	int const iProductionNeeded = GET_PLAYER(ePlayer).getProductionNeeded(eProject);
	kFlow.iProjectsCompleted++;
	kFlow.iProjectProductionNeeded += iProductionNeeded;
	kFlow.aiProjectTypes[eProject]++;
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=PROJECT_BUILT player=%d cityId=%d city=%S project=%s productionNeeded=%d rawModifiedOverflow=%d unmodifiedOverflow=%d keptOverflow=%d lostProduction=%d unusedOverflowCapacity=%d overflowGold=%d",
		GC.getGame().getGameTurn(), ePlayer, pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), getSASGameRecordProjectType(eProject), iProductionNeeded,
		iRawModifiedOverflow, iUnmodifiedOverflow, iKeptOverflow, iLostProduction, iUnusedOverflowCapacity, iOverflowGold);
}

void logSASGameRecordProductionOverflow(CvCity const* pCity, int iRawModifiedOverflow, int iUnmodifiedOverflow, int iKeptOverflow, int iLostProduction, int iUnusedCapacity, int iGold)
{
	if (pCity == NULL)
		return;
	SASGameRecordPlayerFlow& kFlow = g_akSASGameRecordPlayerFlow[pCity->getOwner()];
	kFlow.iOverflowActions++;
	kFlow.iRawModifiedOverflow += iRawModifiedOverflow;
	kFlow.iUnmodifiedOverflow += iUnmodifiedOverflow;
	kFlow.iKeptOverflow += iKeptOverflow;
	kFlow.iLostProduction += iLostProduction;
	kFlow.iUnusedOverflowCapacity += iUnusedCapacity;
	kFlow.iOverflowGold += iGold;
	// <!-- custom: Level 3 already carries these exact values on the corresponding production-completion row, so avoid a duplicate action.
	// At level 2, PROJECT_BUILT already owns its exact overflow. Otherwise keep strategically exceptional loss/gold and every Barbarian overflow because ordinary unit/building completion rows and Barbarian production-flow summaries are unavailable there. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
	if (gGameRecordLogLevel == 2 && !pCity->isProductionProject() && (pCity->isBarbarian() || iLostProduction > 0 || iGold > 0))
		logSASGameRecord("GAME_RECORD_ACTION turn=%d type=PRODUCTION_OVERFLOW player=%d cityId=%d city=%S productionKind=%s production=%s rawModifiedOverflow=%d unmodifiedOverflow=%d keptOverflow=%d lostProduction=%d unusedOverflowCapacity=%d gold=%d",
				GC.getGame().getGameTurn(), pCity->getOwner(), pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), getSASGameRecordCityProductionKind(*pCity), getSASGameRecordCityProductionType(*pCity), iRawModifiedOverflow, iUnmodifiedOverflow, iKeptOverflow, iLostProduction, iUnusedCapacity, iGold);
}

void logSASGameRecordProductionFailed(CvCity const* pCity, int iOrderData, bool bProject, int iInvestedProduction, int iGold)
{
	if (pCity == NULL)
		return;
	SASGameRecordPlayerFlow& kFlow = g_akSASGameRecordPlayerFlow[pCity->getOwner()];
	kFlow.iFailedInvestedProduction += iInvestedProduction;
	kFlow.iFailGold += iGold;
	char const* szProduction = (bProject ? GC.getInfo((ProjectTypes)iOrderData).getType() : getSASGameRecordBuildingType((BuildingTypes)iOrderData));
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=PRODUCTION_FAILED_TO_GOLD player=%d cityId=%d city=%S productionKind=%s production=%s investedProduction=%d gold=%d", GC.getGame().getGameTurn(), pCity->getOwner(), pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), bProject ? "PROJECT" : "BUILDING", szProduction, iInvestedProduction, iGold);
}

void logSASGameRecordProductionDecay(CvCity const* pCity, OrderTypes eOrder, int iData1, int iBefore, int iAfter, int iInactiveTurns)
{
	if (pCity == NULL || iAfter >= iBefore)
		return;
	int const iLost = iBefore - iAfter;
	SASGameRecordPlayerFlow& kFlow = g_akSASGameRecordPlayerFlow[pCity->getOwner()];
	kFlow.iProductionDecayActions++;
	kFlow.iProductionDecayLost += iLost;
	if (gGameRecordLogLevel >= 3)
	{
		logSASGameRecord("GAME_RECORD_ACTION turn=%d type=PRODUCTION_DECAY player=%d cityId=%d city=%S productionKind=%s production=%s storedBefore=%d storedAfter=%d lost=%d accumulatedInactiveTurns=%d",
			GC.getGame().getGameTurn(), pCity->getOwner(), pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), getSASGameRecordProductionKind(eOrder, iData1), getSASGameRecordProductionType(eOrder, iData1), iBefore, iAfter, iLost, iInactiveTurns);
	}
}

void logSASGameRecordProductionInvalidated(CvCity const* pCity, OrderTypes eOrder, int iData1, int iStoredLost, bool bActiveTarget, bool bQueued)
{
	if (pCity == NULL || iStoredLost <= 0)
		return;
	SASGameRecordPlayerFlow& kFlow = g_akSASGameRecordPlayerFlow[pCity->getOwner()];
	kFlow.iProductionInvalidatedActions++;
	kFlow.iProductionInvalidatedLost += iStoredLost;
	char const* szReason = (eOrder == ORDER_TRAIN ? "MAXED_UNIT_CLASS" : (eOrder == ORDER_CONSTRUCT ? "MAXED_BUILDING_CLASS" : (eOrder == ORDER_CREATE ? "MAXED_PROJECT" : "UNKNOWN")));
	// <!-- custom: This is actual stored production erased by inherited maxed-class/project cleanup, not strategic target switching.
	// Emit the rare loss at level 2 so parked production cannot disappear between snapshots without provenance. (ChatGPT-5.6-Sol) -->
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=PRODUCTION_INVALIDATED player=%d cityId=%d city=%S productionKind=%s production=%s reason=%s storedLost=%d activeTarget=%d queued=%d",
		GC.getGame().getGameTurn(), pCity->getOwner(), pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), getSASGameRecordProductionKind(eOrder, iData1), getSASGameRecordProductionType(eOrder, iData1), szReason, iStoredLost, bActiveTarget ? 1 : 0, bQueued ? 1 : 0);
}

void logSASGameRecordProductionUpgraded(CvCity const* pCity, UnitTypes eOldUnit, UnitTypes eNewUnit, int iProductionTransferred, int iDestinationProductionBefore)
{
	if (pCity == NULL || (iProductionTransferred <= 0 && iDestinationProductionBefore <= 0))
		return;
	SASGameRecordPlayerFlow& kFlow = g_akSASGameRecordPlayerFlow[pCity->getOwner()];
	if (iProductionTransferred > 0)
	{
		kFlow.iProductionUpgradeTransfers++;
		kFlow.iProductionUpgradeTransferred += iProductionTransferred;
	}
	// <!-- custom: CvCity::upgradeProduction assigns rather than adds at the destination.
	// Any pre-existing destination production is therefore overwritten; preserve that separately as a possible mechanical loss instead of misclassifying it as target churn. (ChatGPT-5.6-Sol) -->
	if (iDestinationProductionBefore > 0)
		kFlow.iProductionUpgradeOverwriteActions++;
	kFlow.iProductionUpgradeOverwritten += std::max(0, iDestinationProductionBefore);
	if (gGameRecordLogLevel >= 3 || iDestinationProductionBefore > 0)
	{
		logSASGameRecord("GAME_RECORD_ACTION turn=%d type=PRODUCTION_UPGRADED player=%d cityId=%d city=%S oldUnit=%s newUnit=%s productionTransferred=%d newProductionBefore=%d newProductionAfter=%d overwrittenDestinationProduction=%d",
			GC.getGame().getGameTurn(), pCity->getOwner(), pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), getSASGameRecordUnitType(eOldUnit), getSASGameRecordUnitType(eNewUnit), iProductionTransferred, iDestinationProductionBefore, iProductionTransferred, std::max(0, iDestinationProductionBefore));
	}
}

void logSASGameRecordVictoryLaunched(PlayerTypes ePlayer, VictoryTypes eVictory)
{
	if (ePlayer < 0 || ePlayer >= MAX_PLAYERS || eVictory == NO_VICTORY)
		return;
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	CvTeam const& kTeam = GET_TEAM(kPlayer.getTeam());
	int iPartsBuilt = 0;
	int iPartsMinimum = 0;
	int iPartsMaximum = 0;
	bool bMinimumComplete = false;
	CvString szProjectParts;
	bool const bProjectVictory = getSASGameRecordVictoryProjectState(kPlayer.getTeam(), eVictory, iPartsBuilt, iPartsMinimum, iPartsMaximum, bMinimumComplete, szProjectParts);
	int const iCountdown = kTeam.getVictoryCountdown(eVictory);
	// <!-- custom: PROJECT_BUILT rows could only imply a spaceship launch. Record the actual launch and its exact arrival state so a Space victory no longer has to be reconstructed from component timing. (GPT-5.6-Sol) -->
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=SPACESHIP_LAUNCHED player=%d team=%d victory=%s countdown=%d arrivalTurn=%d travelTurns=%d launchSuccessPercent=%d partsBuilt=%d partsMinimum=%d partsMaximum=%d projectParts=%s",
			GC.getGame().getGameTurn(), ePlayer, kPlayer.getTeam(), GC.getInfo(eVictory).getType(),
			iCountdown, iCountdown < 0 ? -1 : GC.getGame().getGameTurn() + iCountdown,
			bProjectVictory && bMinimumComplete ? kTeam.getVictoryDelay(eVictory) : -1,
			kTeam.getLaunchSuccessRate(eVictory), iPartsBuilt, iPartsMinimum, iPartsMaximum, bProjectVictory ? szProjectParts.GetCString() : "-");
}

static void logSASGameRecordVictoryProgressRemoved(TeamTypes eTeam, VictoryTypes eVictory, char const* szAction, char const* szCause, int iLaunchSuccessPercent, CvCity const* pCapital)
{
	CvTeam const& kTeam = GET_TEAM(eTeam);
	int iPartsBuilt = 0;
	int iPartsMinimum = 0;
	int iPartsMaximum = 0;
	bool bMinimumComplete = false;
	CvString szProjectParts;
	bool const bProjectVictory = getSASGameRecordVictoryProjectState(eTeam, eVictory, iPartsBuilt, iPartsMinimum, iPartsMaximum, bMinimumComplete, szProjectParts);
	int const iCountdown = kTeam.getVictoryCountdown(eVictory);
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=%s team=%d victory=%s cause=%s countdown=%d arrivalTurn=%d launchSuccessPercent=%d capitalPlayer=%d capitalCityId=%d capital=%S capitalX=%d capitalY=%d projectVictory=%d partsBuilt=%d partsMinimum=%d partsMaximum=%d projectParts=%s",
			GC.getGame().getGameTurn(), szAction, eTeam, getSASGameRecordVictoryType(eVictory), szCause, iCountdown, iCountdown < 0 ? -1 : GC.getGame().getGameTurn() + iCountdown,
			iLaunchSuccessPercent, pCapital == NULL ? NO_PLAYER : pCapital->getOwner(), pCapital == NULL ? -1 : pCapital->getID(),
			getSASGameRecordQuotedCityName(pCapital).GetCString(), pCapital == NULL ? -1 : pCapital->getX(), pCapital == NULL ? -1 : pCapital->getY(),
			bProjectVictory, iPartsBuilt, iPartsMinimum, iPartsMaximum, bProjectVictory ? szProjectParts.GetCString() : "-");
}

void logSASGameRecordVictoryProgressResetForCapital(CvCity const* pCapital)
{
	if (pCapital == NULL || GC.getGame().getGameState() != GAMESTATE_ON)
		return;
	TeamTypes const eTeam = pCapital->getTeam();
	CvTeam const& kTeam = GET_TEAM(eTeam);
	FOR_EACH_ENUM(Victory)
	{
		if (kTeam.getVictoryCountdown(eLoopVictory) >= 0)
			logSASGameRecordVictoryProgressRemoved(eTeam, eLoopVictory, "VICTORY_PROGRESS_RESET", "CAPITAL_LOST", kTeam.getLaunchSuccessRate(eLoopVictory), pCapital);
	}
}

void logSASGameRecordSpaceshipFailed(TeamTypes eTeam, VictoryTypes eVictory, int iLaunchSuccessPercent)
{
	if (eTeam == NO_TEAM || eVictory == NO_VICTORY)
		return;
	// <!-- custom: A failed arrival roll previously erased the countdown and spaceship projects without an explicit event. Preserve the losing launch state immediately before resetVictoryProgress removes it. (GPT-5.6-Sol) -->
	logSASGameRecordVictoryProgressRemoved(eTeam, eVictory, "SPACESHIP_FAILED", "LAUNCH_ROLL_FAILED", iLaunchSuccessPercent, NULL);
}

void logSASGameRecordVassalState(TeamTypes eMaster, TeamTypes eVassal, bool bVassal)
{
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=%s master=%d vassal=%d", GC.getGame().getGameTurn(), bVassal ? "VASSALAGE_STARTED" : "VASSALAGE_ENDED", eMaster, eVassal);
}

void logSASGameRecordVictory(TeamTypes eWinner, VictoryTypes eVictory)
{
	bool const bLogVictoryDetails = (gGameRecordLogLevel >= 2);
	// <!-- custom: Victory can be reported before the ordinary end-turn hook. Flush this turn's buffered map history first so the final snapshot does not precede its last plot changes or map revelation. Reuse the same level-2 gate for final war reconciliation below. (GPT-5.6-Sol + ChatGPT-5.6-Sol) -->
	if (bLogVictoryDetails) flushSASGameRecordTurnChanges(GC.getGame().getGameTurn());
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=VICTORY team=%d victory=%s", GC.getGame().getGameTurn(), eWinner, eVictory == NO_VICTORY ? "-" : GC.getInfo(eVictory).getType());
	// <!-- custom: ReplayInfo preserves only the selected player's final and normalized scores. Record both for every civilization once at victory so benchmark review can compare the complete final field without reconstructing Civ4's final-score formula. (GPT-5.6-Sol) -->
	for (int iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		PlayerTypes const ePlayer = (PlayerTypes)iI;
		CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
		if (!kPlayer.isEverAlive())
			continue;
		bool const bWinner = (kPlayer.getTeam() == eWinner);
		logSASGameRecord("GAME_RECORD_FINAL_SCORE turn=%d player=%d team=%d alive=%d winner=%d score=%d normalizedScore=%d", GC.getGame().getGameTurn(), ePlayer, kPlayer.getTeam(), kPlayer.isAlive(), bWinner, kPlayer.calculateScore(), kPlayer.calculateScore(true, bWinner));
	}
	// <!-- custom: A victory can end the run with wars still active; preserve their observed results without falsely marking them as completed wars. (GPT-5.6-Sol) -->
	if (bLogVictoryDetails)
	{
		reconcileSASGameRecordWars();
		logSASGameRecordOngoingWarSummaries("VICTORY");
	}
	// <!-- custom: Periodic snapshots could stop several turns before victory, leaving every civilization's exact final state unknown. Force one complete marked snapshot now; the ordinary end-turn hook suppresses a duplicate on the same turn. (GPT-5.6-Sol) -->
	logSASGameRecordSnapshot(GC.getGame().getGameTurn(), "victory");
}

void logSASGameRecordPlayerEliminated(PlayerTypes ePlayer)
{
	if (ePlayer < 0 || ePlayer >= MAX_PLAYERS)
		return;
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=PLAYER_ELIMINATED player=%d team=%d civ=%s leader=%s cities=%d units=%d score=%d power=%d playersAlive=%d teamsAlive=%d eliminatedPlayers=%s",
			GC.getGame().getGameTurn(), ePlayer, kPlayer.getTeam(), kPlayer.getCivilizationType() == NO_CIVILIZATION ? "-" : GC.getInfo(kPlayer.getCivilizationType()).getType(), kPlayer.getLeaderType() == NO_LEADER ? "-" : GC.getInfo(kPlayer.getLeaderType()).getType(),
			kPlayer.getNumCities(), kPlayer.getNumUnits(), kPlayer.calculateScore(), kPlayer.getPower(), GC.getGame().countCivPlayersAlive(), GC.getGame().countCivTeamsAlive(), getSASGameRecordEliminatedPlayers().GetCString());
	// <!-- custom: If this eliminated the team's last player, close its active war summaries on the exact elimination turn instead of waiting for the next periodic snapshot. (GPT-5.6-Sol) -->
	reconcileSASGameRecordWars();
	logSASGameRecordRunStatus("playerEliminated");
}

void logSASGameRecordPlayerAliveChanged(PlayerTypes ePlayer, bool bRevived)
{
	if (ePlayer < 0 || ePlayer >= MAX_PLAYERS)
		return;
	CvPlayer const& kPlayer = GET_PLAYER(ePlayer);
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=%s player=%d team=%d civ=%s leader=%s cities=%d units=%d score=%d power=%d playersAlive=%d teamsAlive=%d playersEverAlive=%d",
			GC.getGame().getGameTurn(), bRevived ? "PLAYER_REVIVED" : "PLAYER_APPEARED", ePlayer, kPlayer.getTeam(), kPlayer.getCivilizationType() == NO_CIVILIZATION ? "-" : GC.getInfo(kPlayer.getCivilizationType()).getType(), kPlayer.getLeaderType() == NO_LEADER ? "-" : GC.getInfo(kPlayer.getLeaderType()).getType(),
			kPlayer.getNumCities(), kPlayer.getNumUnits(), kPlayer.calculateScore(), kPlayer.getPower(), GC.getGame().countCivPlayersAlive(), GC.getGame().countCivTeamsAlive(), GC.getGame().countCivPlayersEverAlive());
	logSASGameRecordRunStatus(bRevived ? "playerRevived" : "playerAppeared");
}

// <!-- custom: Debug mode is game-level state; record only successful transitions, not rejected toggle attempts. (ChatGPT-5.6-Sol) -->
void logSASGameRecordDebugModeChanged(bool bOldDebugMode, bool bNewDebugMode)
{
	if (bOldDebugMode == bNewDebugMode)
		return;
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=DEBUG_MODE_CHANGED old=%d new=%d activePlayer=%d",
		GC.getGame().getGameTurn(), bOldDebugMode, bNewDebugMode, GC.getGame().getActivePlayer());
}

void logSASGameRecordAutoPlayChanged(int iOldValue, int iNewValue, bool bChangePlayerStatus, SASAutoPlayEndCause eEndCause)
{
	if (iOldValue == iNewValue)
		return;
	CvGame const& kGame = GC.getGame();
	bool const bStarted = (iOldValue <= 0 && iNewValue > 0);
	bool const bEnded = (iOldValue > 0 && iNewValue <= 0);
	char const* szAction = (bStarted ? "AUTOPLAY_STARTED" : (bEnded ? "AUTOPLAY_ENDED" : "AUTOPLAY_CHANGED"));
	PlayerTypes const eActivePlayer = kGame.getActivePlayer();
	uint const uiAutoPlayTime = getSASMonotonicMilliseconds();
	if (bStarted)
	{
		g_iSASGameRecordAutoPlayRequestId++;
		g_iSASGameRecordAutoPlayRequestedTurns = iNewValue;
		g_iSASGameRecordAutoPlayStartTurn = kGame.getGameTurn();
		g_iSASGameRecordAutoPlayStartElapsedTurn = kGame.getElapsedGameTurns();
		g_eSASGameRecordAutoPlayStartPlayer = eActivePlayer;
		g_uiSASGameRecordAutoPlayStartTime = uiAutoPlayTime;
		g_iSASGameRecordAutoPlayPlayerChanges = 0;
	}
	if (bEnded && eEndCause == SAS_AUTOPLAY_END_UNSPECIFIED)
		eEndCause = (kGame.getWinner() != NO_TEAM ? SAS_AUTOPLAY_END_VICTORY : (eActivePlayer != NO_PLAYER && !GET_PLAYER(eActivePlayer).isAlive() ? SAS_AUTOPLAY_END_ACTIVE_PLAYER_DEFEATED : SAS_AUTOPLAY_END_OTHER));
	int const iCompletedTurns = (!bEnded || g_iSASGameRecordAutoPlayRequestedTurns <= 0 ? 0 : (eEndCause == SAS_AUTOPLAY_END_SCHEDULED ? g_iSASGameRecordAutoPlayRequestedTurns : std::max(0, g_iSASGameRecordAutoPlayRequestedTurns - iOldValue)));
	int const iElapsedGameTurns = (g_iSASGameRecordAutoPlayStartElapsedTurn < 0 ? 0 : std::max(0, kGame.getElapsedGameTurns() - g_iSASGameRecordAutoPlayStartElapsedTurn));
	int const iAutoPlayWallMilliseconds = (g_iSASGameRecordAutoPlayStartTurn < 0 ? -1 : (int)getSASElapsedMilliseconds(g_uiSASGameRecordAutoPlayStartTime, uiAutoPlayTime));
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=%s oldTurnsLeft=%d newTurnsLeft=%d activePlayer=%d changePlayerStatus=%d requestId=%d requestedTurns=%d completedTurns=%d elapsedGameTurns=%d sessionWallMilliseconds=%u autoplayWallMilliseconds=%d startTurn=%d startElapsed=%d startPlayer=%d activePlayerChanges=%d totalActivePlayerChanges=%d endCause=%s",
			kGame.getGameTurn(), szAction, iOldValue, iNewValue, eActivePlayer, bChangePlayerStatus, g_iSASGameRecordAutoPlayRequestId, g_iSASGameRecordAutoPlayRequestedTurns, iCompletedTurns, iElapsedGameTurns, getSASGameRecordSessionWallMilliseconds(uiAutoPlayTime),
			iAutoPlayWallMilliseconds, g_iSASGameRecordAutoPlayStartTurn, g_iSASGameRecordAutoPlayStartElapsedTurn, g_eSASGameRecordAutoPlayStartPlayer, g_iSASGameRecordAutoPlayPlayerChanges, g_iSASGameRecordTotalActivePlayerChanges,
			getSASAutoPlayEndCause(eEndCause));
	// <!-- custom: Manual or scheduled autoplay completion is also a useful record boundary even when the game and its wars continue. (GPT-5.6-Sol) -->
	if (gGameRecordLogLevel >= 2 && bEnded)
	{
		reconcileSASGameRecordWars();
		logSASGameRecordOngoingWarSummaries("AUTOPLAY_ENDED");
	}
	logSASGameRecordRunStatus(szAction);
	if (bEnded)
	{
		g_iSASGameRecordAutoPlayRequestedTurns = 0;
		g_iSASGameRecordAutoPlayStartTurn = -1;
		g_iSASGameRecordAutoPlayStartElapsedTurn = -1;
		g_eSASGameRecordAutoPlayStartPlayer = NO_PLAYER;
		g_uiSASGameRecordAutoPlayStartTime = 0;
		g_iSASGameRecordAutoPlayPlayerChanges = 0;
	}
}

void logSASGameRecordActivePlayerChanged(PlayerTypes eOldPlayer, PlayerTypes eNewPlayer)
{
	g_iSASGameRecordTotalActivePlayerChanges++;
	bool const bDuringAutoPlay = (GC.getGame().getAIAutoPlay() > 0 && g_iSASGameRecordAutoPlayRequestedTurns > 0);
	if (bDuringAutoPlay) g_iSASGameRecordAutoPlayPlayerChanges++;
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=ACTIVE_PLAYER_CHANGED oldPlayer=%d newPlayer=%d autoplayActive=%d autoplayTurnsLeft=%d requestId=%d activePlayerChanges=%d totalActivePlayerChanges=%d",
			GC.getGame().getGameTurn(), eOldPlayer, eNewPlayer, bDuringAutoPlay, GC.getGame().getAIAutoPlay(), bDuringAutoPlay ? g_iSASGameRecordAutoPlayRequestId : -1, bDuringAutoPlay ? g_iSASGameRecordAutoPlayPlayerChanges : 0, g_iSASGameRecordTotalActivePlayerChanges);
}

void logSASGameRecordAutoPlayPopupDismissed(char const* szPopupKind)
{
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=AUTOPLAY_INFORMATIONAL_POPUP_DISMISSED popupKind=%s autoplayTurnsLeft=%d requestId=%d",
			GC.getGame().getGameTurn(), szPopupKind, GC.getGame().getAIAutoPlay(), g_iSASGameRecordAutoPlayRequestedTurns > 0 ? g_iSASGameRecordAutoPlayRequestId : -1);
}

void logSASGameRecordGreatPersonBorn(CvUnit const* pUnit, PlayerTypes ePlayer, CvCity const* pCity)
{
	// <!-- custom: Keep the newborn unit ID and exact spawn plot so one Great Person, especially a Great General, can be followed directly from birth through BBAI decisions to join/construct/attach/death rows instead of correlating only by player and turn. (ChatGPT-5.6-Sol) -->
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=GREAT_PERSON_BORN player=%d cityId=%d city=%S unitId=%d unit=%s x=%d y=%d combatXP=%d greatPeopleCreated=%d greatGeneralsCreated=%d greatGeneralThreshold=%d",
			GC.getGame().getGameTurn(), ePlayer, pCity == NULL ? -1 : pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(),
			pUnit == NULL ? -1 : pUnit->getID(), pUnit == NULL ? "-" : getSASGameRecordUnitType(pUnit->getUnitType()),
			pUnit == NULL ? -1 : pUnit->getX(), pUnit == NULL ? -1 : pUnit->getY(),
			ePlayer == NO_PLAYER ? 0 : GET_PLAYER(ePlayer).getCombatExperience(), ePlayer == NO_PLAYER ? 0 : GET_PLAYER(ePlayer).getGreatPeopleCreated(),
			ePlayer == NO_PLAYER ? 0 : GET_PLAYER(ePlayer).getGreatGeneralsCreated(), ePlayer == NO_PLAYER ? 0 : GET_PLAYER(ePlayer).greatPeopleThreshold(true));
}

void logSASGameRecordGreatPersonJoined(CvUnit const* pUnit, CvCity const* pCity, SpecialistTypes eSpecialist)
{
	if (pUnit == NULL || pCity == NULL)
		return;
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=GREAT_PERSON_JOINED_CITY player=%d unitId=%d unit=%s cityId=%d city=%S specialist=%s freeSpecialists=%d",
			GC.getGame().getGameTurn(), pUnit->getOwner(), pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), eSpecialist == NO_SPECIALIST ? "-" : GC.getInfo(eSpecialist).getType(), pCity->getFreeSpecialistCount(eSpecialist));
}

// <!-- custom: Great Person births and city joining were already recorded, but other completed Great Person missions disappeared from the record when the unit was consumed. Record the rare completed outcome and its concrete gain without logging AI candidate values or reasoning. (GPT-5.6-Sol) -->
void logSASGameRecordGreatPersonConstructed(CvUnit const* pUnit, CvCity const* pCity, BuildingTypes eBuilding)
{
	if (pUnit == NULL || pCity == NULL || eBuilding == NO_BUILDING)
		return;
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=GREAT_PERSON_USED use=CONSTRUCT_BUILDING player=%d unitId=%d unit=%s cityId=%d city=%S building=%s",
			GC.getGame().getGameTurn(), pUnit->getOwner(), pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), getSASGameRecordBuildingType(eBuilding));
}

void logSASGameRecordGreatPersonDiscovered(CvUnit const* pUnit, TechTypes eTech, int iResearch)
{
	if (pUnit == NULL || eTech == NO_TECH)
		return;
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=GREAT_PERSON_USED use=DISCOVER_TECH player=%d unitId=%d unit=%s x=%d y=%d tech=%s research=%d",
			GC.getGame().getGameTurn(), pUnit->getOwner(), pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), pUnit->getX(), pUnit->getY(), getSASGameRecordTechType(eTech), iResearch);
}

void logSASGameRecordGreatPersonHurried(CvUnit const* pUnit, CvCity const* pCity, BuildingTypes eBuilding, int iProduction)
{
	if (pUnit == NULL || pCity == NULL || eBuilding == NO_BUILDING)
		return;
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=GREAT_PERSON_USED use=HURRY_BUILDING player=%d unitId=%d unit=%s cityId=%d city=%S building=%s production=%d",
			GC.getGame().getGameTurn(), pUnit->getOwner(), pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), getSASGameRecordBuildingType(eBuilding), iProduction);
}

void logSASGameRecordGreatPersonTradeMission(CvUnit const* pUnit, CvCity const* pCity, int iGold)
{
	if (pUnit == NULL || pCity == NULL)
		return;
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=GREAT_PERSON_USED use=TRADE_MISSION player=%d unitId=%d unit=%s targetPlayer=%d cityId=%d city=%S gold=%d",
			GC.getGame().getGameTurn(), pUnit->getOwner(), pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), pCity->getOwner(), pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), iGold);
}

void logSASGameRecordGreatPersonGreatWork(CvUnit const* pUnit, CvCity const* pCity, int iCulture)
{
	if (pUnit == NULL || pCity == NULL)
		return;
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=GREAT_PERSON_USED use=GREAT_WORK player=%d unitId=%d unit=%s cityId=%d city=%S culture=%d",
			GC.getGame().getGameTurn(), pUnit->getOwner(), pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), iCulture);
}

void logSASGameRecordGreatPersonInfiltrated(CvUnit const* pUnit, CvCity const* pCity, int iEspionage)
{
	if (pUnit == NULL || pCity == NULL)
		return;
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=GREAT_PERSON_USED use=INFILTRATE player=%d unitId=%d unit=%s targetPlayer=%d targetTeam=%d cityId=%d city=%S espionage=%d",
			GC.getGame().getGameTurn(), pUnit->getOwner(), pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), pCity->getOwner(), pCity->getTeam(), pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), iEspionage);
}

void logSASGameRecordGreatPersonGoldenAgeConsumed(CvUnit const* pUnit)
{
	if (pUnit == NULL)
		return;
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=GREAT_PERSON_USED use=GOLDEN_AGE player=%d unitId=%d unit=%s x=%d y=%d",
			GC.getGame().getGameTurn(), pUnit->getOwner(), pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), pUnit->getX(), pUnit->getY());
}

// <!-- custom: Add an optional explicit death plot because combat supplies its target while a dead attacker still reports its origin; other death paths retain the unit's current location. See KI#377. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
void logSASGameRecordGreatPersonDied(CvUnit const* pUnit, PlayerTypes eResponsiblePlayer, char const* szCause, CvPlot const* pDeathPlot)
{
	if (pUnit == NULL || (!pUnit->isGoldenAge() && pUnit->getUnitInfo().getLeaderExperience() <= 0))
		return;
	CvPlot const* pPlot = (pDeathPlot == NULL ? pUnit->plot() : pDeathPlot);
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=GREAT_PERSON_DIED player=%d unitId=%d unit=%s x=%d y=%d cause=%s responsiblePlayer=%d",
			GC.getGame().getGameTurn(), pUnit->getOwner(), pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), pPlot == NULL ? -1 : pPlot->getX(), pPlot == NULL ? -1 : pPlot->getY(), szCause, eResponsiblePlayer);
}

// <!-- custom: Completed espionage and mission-phase interceptions share one decoder so the same eMission/iExtraData and pre-mission destructible-target context always produce identical readable target tokens. (ChatGPT-5.6-Sol) -->
static void getSASGameRecordEspionageTarget(EspionageMissionTypes eMission, int iExtraData, ImprovementTypes eTargetImprovement, RouteTypes eTargetRoute, UnitTypes eTargetUnit, char const*& szTargetKind, char const*& szTargetType)
{
	szTargetKind = "-";
	szTargetType = "-";
	if (eMission == NO_ESPIONAGEMISSION)
		return;
	CvEspionageMissionInfo const& kMission = GC.getInfo(eMission);
	if (kMission.isDestroyImprovement())
	{
		if (eTargetImprovement != NO_IMPROVEMENT)
		{
			szTargetKind = "improvement";
			szTargetType = getSASGameRecordImprovementType(eTargetImprovement);
		}
		else if (eTargetRoute != NO_ROUTE)
		{
			szTargetKind = "route";
			szTargetType = getSASGameRecordRouteType(eTargetRoute);
		}
	}
	else if (kMission.getDestroyBuildingCostFactor() > 0)
	{
		szTargetKind = "building";
		szTargetType = getSASGameRecordBuildingType((BuildingTypes)iExtraData);
	}
	else if (kMission.getDestroyProjectCostFactor() > 0)
	{
		szTargetKind = "project";
		szTargetType = getSASGameRecordProjectType((ProjectTypes)iExtraData);
	}
	else if (kMission.getDestroyUnitCostFactor() > 0 || kMission.getBuyUnitCostFactor() > 0)
	{
		szTargetKind = "unit";
		szTargetType = getSASGameRecordUnitType(eTargetUnit);
	}
	else if (kMission.getBuyTechCostFactor() > 0)
	{
		szTargetKind = "tech";
		szTargetType = getSASGameRecordTechType((TechTypes)iExtraData);
	}
	else if (kMission.getSwitchCivicCostFactor() > 0)
	{
		szTargetKind = "civic";
		szTargetType = getSASGameRecordCivicType((CivicTypes)iExtraData);
	}
	else if (kMission.getSwitchReligionCostFactor() > 0)
	{
		szTargetKind = "religion";
		szTargetType = getSASGameRecordReligionType((ReligionTypes)iExtraData);
	}
}

// <!-- custom: Periodic espionage totals showed investment against each rival but not what those points accomplished.
// Record only completed missions and actual interceptions at game-record level 2; mission selection and movement reasoning remain BBAI diagnostics.
// Resolve iExtraData to XML types so stolen technologies and sabotaged buildings/projects/units are readable. (GPT-5.6-Sol) -->
void logSASGameRecordEspionageMission(CvUnit const* pUnit, EspionageMissionTypes eMission, PlayerTypes eTargetPlayer, CvPlot const* pPlot, int iExtraData, int iCost, int iEPBefore, int iEPAfter, ImprovementTypes eTargetImprovement, RouteTypes eTargetRoute, UnitTypes eTargetUnit, int iEffectValue, char const* szEffectKind)
{
	if (pUnit == NULL || eMission == NO_ESPIONAGEMISSION)
		return;
	char const* szTargetKind;
	char const* szTargetType;
	getSASGameRecordEspionageTarget(eMission, iExtraData, eTargetImprovement, eTargetRoute, eTargetUnit, szTargetKind, szTargetType);
	CvCity const* pCity = (pPlot == NULL ? NULL : pPlot->getPlotCity());
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=ESPIONAGE_MISSION player=%d spyId=%d spy=%s spyAI=%s targetPlayer=%d targetTeam=%d mission=%s cost=%d epBefore=%d epAfter=%d cityId=%d city=%S x=%d y=%d targetKind=%s target=%s effectKind=%s effectValue=%d extraData=%d fortifyTurns=%d",
			GC.getGame().getGameTurn(), pUnit->getOwner(), pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), getSASGameRecordUnitAIType(pUnit->AI_getUnitAIType()),
			eTargetPlayer, eTargetPlayer == NO_PLAYER ? NO_TEAM : GET_PLAYER(eTargetPlayer).getTeam(), getSASGameRecordEspionageMissionType(eMission), iCost, iEPBefore, iEPAfter,
			pCity == NULL ? -1 : pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), pPlot == NULL ? -1 : pPlot->getX(), pPlot == NULL ? -1 : pPlot->getY(),
			szTargetKind, szTargetType, szEffectKind, iEffectValue, iExtraData, pUnit->getFortifyTurns());
}

void logSASGameRecordSpyIntercepted(CvUnit const* pUnit, PlayerTypes eTargetPlayer, char const* szPhase, int iModifier, int iInterceptChanceX100, EspionageMissionTypes eMission, int iExtraData, ImprovementTypes eTargetImprovement, RouteTypes eTargetRoute, UnitTypes eTargetUnit)
{
	if (pUnit == NULL)
		return;
	char const* szTargetKind;
	char const* szTargetType;
	getSASGameRecordEspionageTarget(eMission, iExtraData, eTargetImprovement, eTargetRoute, eTargetUnit, szTargetKind, szTargetType);
	CvCity const* pCity = pUnit->getPlot().getPlotCity();
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=SPY_INTERCEPTED player=%d spyId=%d spy=%s spyAI=%s targetPlayer=%d targetTeam=%d phase=%s mission=%s targetKind=%s target=%s extraData=%d x=%d y=%d cityId=%d city=%S modifier=%d interceptChanceX100=%d fortifyTurns=%d",
			GC.getGame().getGameTurn(), pUnit->getOwner(), pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), getSASGameRecordUnitAIType(pUnit->AI_getUnitAIType()),
			eTargetPlayer, eTargetPlayer == NO_PLAYER ? NO_TEAM : GET_PLAYER(eTargetPlayer).getTeam(), szPhase, getSASGameRecordEspionageMissionType(eMission),
			szTargetKind, szTargetType, iExtraData, pUnit->getX(), pUnit->getY(), pCity == NULL ? -1 : pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(),
			iModifier, iInterceptChanceX100, pUnit->getFortifyTurns());
}

void logSASGameRecordGreatGeneralAttached(CvUnit const* pGreatGeneral, CvUnit const* pTargetUnit, PromotionTypes ePromotion)
{
	if (pGreatGeneral == NULL || pTargetUnit == NULL)
		return;
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=GREAT_GENERAL_ATTACHED player=%d generalUnitId=%d generalUnit=%s targetUnitId=%d targetUnit=%s targetUnitAI=%s x=%d y=%d promotion=%s targetXP=%d targetLevel=%d",
			GC.getGame().getGameTurn(), pGreatGeneral->getOwner(), pGreatGeneral->getID(), getSASGameRecordUnitType(pGreatGeneral->getUnitType()), pTargetUnit->getID(), getSASGameRecordUnitType(pTargetUnit->getUnitType()), getSASGameRecordUnitAIType(pTargetUnit->AI_getUnitAIType()), pTargetUnit->getX(), pTargetUnit->getY(), ePromotion == NO_PROMOTION ? "-" : GC.getInfo(ePromotion).getType(), pTargetUnit->getExperience(), pTargetUnit->getLevel());
}


void logSASGameRecordUnitScrapped(CvUnit const* pUnit)
{
	if (pUnit == NULL)
		return;
	PlayerTypes const ePlayer = pUnit->getOwner();
	SASGameRecordPlayerFlow& kFlow = g_akSASGameRecordPlayerFlow[ePlayer];
	kFlow.iScrapped++;
	kFlow.iScrappedProductionNeeded += GET_PLAYER(ePlayer).getProductionNeeded(pUnit->getUnitType());
	if (gGameRecordLogLevel >= 3)
	{
		logSASGameRecord("GAME_RECORD_ACTION turn=%d type=UNIT_SCRAPPED player=%d unitId=%d unit=%s unitAI=%s x=%d y=%d damage=%d xp=%d level=%d age=%d cargo=%d cargoSpace=%d",
			GC.getGame().getGameTurn(), ePlayer, pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), getSASGameRecordUnitAIType(pUnit->AI_getUnitAIType()), pUnit->getX(), pUnit->getY(), pUnit->getDamage(), pUnit->getExperience(), pUnit->getLevel(), GC.getGame().getGameTurn() - pUnit->getGameTurnCreated(), pUnit->getCargo(), pUnit->cargoSpace());
	}
}

void logSASGameRecordUnitUpgraded(CvUnit const* pOldUnit, CvUnit const* pNewUnit, int iCost)
{
	if (pOldUnit == NULL || pNewUnit == NULL)
		return;
	PlayerTypes const ePlayer = pNewUnit->getOwner();
	SASGameRecordPlayerFlow& kFlow = g_akSASGameRecordPlayerFlow[ePlayer];
	kFlow.iUpgrades++;
	kFlow.iUpgradeGold += iCost;
	if (gGameRecordLogLevel >= 3)
	{
		logSASGameRecord("GAME_RECORD_ACTION turn=%d type=UNIT_UPGRADED player=%d oldUnitId=%d newUnitId=%d fromUnit=%s toUnit=%s unitAI=%s x=%d y=%d cost=%d oldXP=%d newXP=%d oldLevel=%d newLevel=%d",
			GC.getGame().getGameTurn(), ePlayer, pOldUnit->getID(), pNewUnit->getID(), getSASGameRecordUnitType(pOldUnit->getUnitType()), getSASGameRecordUnitType(pNewUnit->getUnitType()), getSASGameRecordUnitAIType(pNewUnit->AI_getUnitAIType()), pNewUnit->getX(), pNewUnit->getY(), iCost, pOldUnit->getExperience(), pNewUnit->getExperience(), pOldUnit->getLevel(), pNewUnit->getLevel());
	}
}

void logSASGameRecordUnitCaptured(PlayerTypes eOldOwner, UnitTypes eOldUnitType, CvUnit const* pNewUnit)
{
	if (pNewUnit == NULL)
		return;
	PlayerTypes const eNewOwner = pNewUnit->getOwner();
	SASGameRecordPlayerFlow& kFlow = g_akSASGameRecordPlayerFlow[eNewOwner];
	kFlow.iCaptured++;
	kFlow.iCapturedProductionNeeded += GET_PLAYER(eNewOwner).getProductionNeeded(pNewUnit->getUnitType());
	// <!-- custom: Captures are rare and strategically distinct, so retain the exact captured type and location at level 2 in addition to the interval aggregate. (GPT-5.6-Sol + GPT-5.6 Thinking) -->
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=UNIT_CAPTURED oldOwner=%d newOwner=%d oldUnit=%s newUnitId=%d newUnit=%s newUnitAI=%s x=%d y=%d",
		GC.getGame().getGameTurn(), eOldOwner, eNewOwner, getSASGameRecordUnitType(eOldUnitType), pNewUnit->getID(), getSASGameRecordUnitType(pNewUnit->getUnitType()), getSASGameRecordUnitAIType(pNewUnit->AI_getUnitAIType()), pNewUnit->getX(), pNewUnit->getY());
}

void logSASGameRecordCityBombard(CvUnit const* pUnit, CvCity const* pCity, char const* szMode, int iBombardRate, bool bIgnoreBuildingDefense, int iDefenseModifierBefore, int iDefenseDamageBefore)
{
	if (pUnit == NULL || pCity == NULL)
		return;
	prepareSASGameRecordTurnChanges();
	const int iGameTurn = GC.getGame().getGameTurn();
	const int iDefenseModifierAfter = pCity->getDefenseModifier(false);
	const int iDefenseDamageAfter = pCity->getDefenseDamage();
	// <!-- custom: Consecutive bombard actions against the same city are synthetic history, not five nearly identical rows for five Trebuchets. Keep sequences separate when attacker/mode/city changes or defense continuity breaks, and the generic writer flushes a pending sequence before the next unrelated GameRecord row so battle-vs-bombard order remains observable. (GPT-5.6 Thinking) -->
	const bool bContinueSequence = (g_kSASGameRecordPendingCityBombard.bValid && g_kSASGameRecordPendingCityBombard.iTurn == iGameTurn && g_kSASGameRecordPendingCityBombard.szMode == szMode && g_kSASGameRecordPendingCityBombard.ePlayer == pUnit->getOwner() && g_kSASGameRecordPendingCityBombard.eTargetPlayer == pCity->getOwner() && g_kSASGameRecordPendingCityBombard.iCityId == pCity->getID() && g_kSASGameRecordPendingCityBombard.iDefenseModifierAfter == iDefenseModifierBefore && g_kSASGameRecordPendingCityBombard.iDefenseDamageAfter == iDefenseDamageBefore);
	if (!bContinueSequence)
	{
		flushSASGameRecordPendingCityBombard();
		g_kSASGameRecordPendingCityBombard.bValid = true;
		g_kSASGameRecordPendingCityBombard.iTurn = iGameTurn;
		g_kSASGameRecordPendingCityBombard.szMode = szMode;
		g_kSASGameRecordPendingCityBombard.ePlayer = pUnit->getOwner();
		g_kSASGameRecordPendingCityBombard.eTargetPlayer = pCity->getOwner();
		g_kSASGameRecordPendingCityBombard.iCityId = pCity->getID();
		g_kSASGameRecordPendingCityBombard.szCity = getSASGameRecordQuotedCityName(pCity);
		g_kSASGameRecordPendingCityBombard.iX = pCity->getX();
		g_kSASGameRecordPendingCityBombard.iY = pCity->getY();
		g_kSASGameRecordPendingCityBombard.iDefenseModifierBefore = iDefenseModifierBefore;
		g_kSASGameRecordPendingCityBombard.iDefenseDamageBefore = iDefenseDamageBefore;
	}
	g_kSASGameRecordPendingCityBombard.iActions++;
	g_kSASGameRecordPendingCityBombard.iBombardRateTotal += iBombardRate;
	if (bIgnoreBuildingDefense) g_kSASGameRecordPendingCityBombard.iIgnoreBuildingDefenseActions++;
	g_kSASGameRecordPendingCityBombard.iDefenseModifierAfter = iDefenseModifierAfter;
	g_kSASGameRecordPendingCityBombard.iTotalDefense = pCity->getTotalDefense(false);
	g_kSASGameRecordPendingCityBombard.iDefenseDamageAfter = iDefenseDamageAfter;
	g_kSASGameRecordPendingCityBombard.iDefenseDamageMax = GC.getMAX_CITY_DEFENSE_DAMAGE();
	addSASGameRecordCityBombardTypeCount(g_kSASGameRecordPendingCityBombard.aUnitTypes, getSASGameRecordUnitType(pUnit->getUnitType()));
	addSASGameRecordCityBombardTypeCount(g_kSASGameRecordPendingCityBombard.aUnitAIs, getSASGameRecordUnitAIType(pUnit->AI_getUnitAIType()));
}

void logSASGameRecordAirStrike(CvUnit const* pUnit, CvUnit const* pDefender, int iDefenderDamageBefore, int iDefenderDamageAfter)
{
	if (pUnit == NULL || pDefender == NULL)
		return;
	CvPlot const* pTargetPlot = pDefender->plot();
	CvCity const* pCity = (pTargetPlot == NULL ? NULL : pTargetPlot->getPlotCity());
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=AIR_STRIKE player=%d unitId=%d unit=%s unitAI=%s fromX=%d fromY=%d targetPlayer=%d targetUnitId=%d targetUnit=%s targetUnitAI=%s x=%d y=%d cityPlot=%d cityId=%d city=%S attackerAirBaseStr=%d defenderBaseStr=%d defenderDamageBefore=%d defenderDamageAfter=%d damageDealt=%d airCombatLimit=%d",
			GC.getGame().getGameTurn(), pUnit->getOwner(), pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), getSASGameRecordUnitAIType(pUnit->AI_getUnitAIType()), pUnit->getX(), pUnit->getY(), pDefender->getOwner(), pDefender->getID(), getSASGameRecordUnitType(pDefender->getUnitType()), getSASGameRecordUnitAIType(pDefender->AI_getUnitAIType()), pTargetPlot == NULL ? -1 : pTargetPlot->getX(), pTargetPlot == NULL ? -1 : pTargetPlot->getY(), pCity != NULL, pCity == NULL ? -1 : pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), pUnit->airBaseCombatStr(), pDefender->baseCombatStr(), iDefenderDamageBefore, iDefenderDamageAfter, std::max(0, iDefenderDamageAfter - iDefenderDamageBefore), pUnit->airCombatLimit());
}

void logSASGameRecordAirInterception(CvUnit const* pAttacker, CvUnit const* pInterceptor, CvPlot const* pTargetPlot, int iAttackerDamageTaken, int iInterceptorDamageTaken)
{
	if (pAttacker == NULL || pInterceptor == NULL || pTargetPlot == NULL)
		return;
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=AIR_INTERCEPTION attackerPlayer=%d attackerUnitId=%d attackerUnit=%s attackerUnitAI=%s interceptorPlayer=%d interceptorUnitId=%d interceptorUnit=%s interceptorUnitAI=%s x=%d y=%d attackerDamageTaken=%d interceptorDamageTaken=%d attackerDead=%d interceptorDead=%d attackerIsAir=%d",
			GC.getGame().getGameTurn(), pAttacker->getOwner(), pAttacker->getID(), getSASGameRecordUnitType(pAttacker->getUnitType()), getSASGameRecordUnitAIType(pAttacker->AI_getUnitAIType()), pInterceptor->getOwner(), pInterceptor->getID(), getSASGameRecordUnitType(pInterceptor->getUnitType()), getSASGameRecordUnitAIType(pInterceptor->AI_getUnitAIType()), pTargetPlot->getX(), pTargetPlot->getY(), iAttackerDamageTaken, iInterceptorDamageTaken, pAttacker->isDead(), pInterceptor->isDead(), pAttacker->getDomainType() == DOMAIN_AIR);
}

void logSASGameRecordAirBombPlot(CvUnit const* pUnit, CvPlot const* pTargetPlot, char const* szTargetKind, char const* szTarget, bool bSuccess)
{
	if (pUnit == NULL || pTargetPlot == NULL)
		return;
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=AIR_BOMB_PLOT player=%d unitId=%d unit=%s unitAI=%s fromX=%d fromY=%d targetOwner=%d x=%d y=%d targetKind=%s target=%s success=%d",
			GC.getGame().getGameTurn(), pUnit->getOwner(), pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), getSASGameRecordUnitAIType(pUnit->AI_getUnitAIType()), pUnit->getX(), pUnit->getY(), pTargetPlot->getOwner(), pTargetPlot->getX(), pTargetPlot->getY(), szTargetKind, szTarget, bSuccess);
}

// <!-- custom: Record the launch after its interception roll while the nuke unit and pre-detonation target still exist.
// CvUnit::nuke passes its already-computed affected-team flags, so this helper only formats them. (ChatGPT-5.6-Sol) -->
void logSASGameRecordNukeLaunched(CvUnit const* pUnit, CvPlot const* pTargetPlot, bool const* pabAffectedTeams, bool bIntercepted, TeamTypes eBestInterceptorTeam, int iInterceptionChance)
{
	if (pUnit == NULL || pTargetPlot == NULL || pabAffectedTeams == NULL)
		return;
	CvString szAffectedTeams;
	for (int iTeam = 0; iTeam < MAX_TEAMS; iTeam++)
	{
		if (pabAffectedTeams[iTeam])
			appendSASDiagnosticIntListValue(szAffectedTeams, iTeam);
	}
	CvCity const* pTargetCity = pTargetPlot->getPlotCity();
	logSASGameRecord("GAME_RECORD_ACTION turn=%d type=NUKE_LAUNCHED player=%d team=%d unitId=%d unit=%s x=%d y=%d plotOwner=%d plotTeam=%d targetCityId=%d targetCity=%S targetCityOwner=%d targetCityTeam=%d targetCityPopulation=%d affectedTeams=%s intercepted=%d bestInterceptorTeam=%d interceptionChance=%d",
			GC.getGame().getGameTurn(), pUnit->getOwner(), pUnit->getTeam(), pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), pTargetPlot->getX(), pTargetPlot->getY(), pTargetPlot->getOwner(), pTargetPlot->getTeam(),
			pTargetCity == NULL ? -1 : pTargetCity->getID(), getSASGameRecordQuotedCityName(pTargetCity).GetCString(), pTargetCity == NULL ? NO_PLAYER : pTargetCity->getOwner(), pTargetCity == NULL ? NO_TEAM : pTargetCity->getTeam(), pTargetCity == NULL ? -1 : pTargetCity->getPopulation(), getSASDiagnosticOrDash(szAffectedTeams).GetCString(), bIntercepted, eBestInterceptorTeam, iInterceptionChance);
}

// <!-- custom: CvPlot::nukeExplosion already accumulates the real post-random damage effects for player messages. Reuse only those counters here, plus exact fallout/citizen totals gathered in the same loop, so the recorder adds no second map/unit scan. (ChatGPT-5.6-Sol) -->
void logSASGameRecordNukeEffects(CvUnit const* pUnit, CvPlot const* pTargetPlot, int iFalloutPlotsCreated, int iImprovementsDestroyed, int iFeaturesDestroyed, int iUnitsDamaged, int iUnitsKilled, int iBuildingsDestroyed, int iCitiesAffected, int iPopulationKilled)
{
	if (pUnit == NULL || pTargetPlot == NULL)
		return;
	CvCity const* pTargetCity = pTargetPlot->getPlotCity();
	logSASGameRecord("GAME_RECORD_NUKE_EFFECTS turn=%d player=%d team=%d unitId=%d unit=%s x=%d y=%d targetCityId=%d targetCity=%S targetCityOwner=%d targetCityTeam=%d targetCityPopulationAfter=%d falloutPlotsCreated=%d improvementsDestroyed=%d featuresDestroyed=%d unitsDamaged=%d unitsKilled=%d buildingsDestroyed=%d citiesAffected=%d populationKilled=%d nukesExplodedAfter=%d",
			GC.getGame().getGameTurn(), pUnit->getOwner(), pUnit->getTeam(), pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()), pTargetPlot->getX(), pTargetPlot->getY(),
			pTargetCity == NULL ? -1 : pTargetCity->getID(), getSASGameRecordQuotedCityName(pTargetCity).GetCString(), pTargetCity == NULL ? NO_PLAYER : pTargetCity->getOwner(), pTargetCity == NULL ? NO_TEAM : pTargetCity->getTeam(), pTargetCity == NULL ? -1 : pTargetCity->getPopulation(),
			iFalloutPlotsCreated, iImprovementsDestroyed, iFeaturesDestroyed, iUnitsDamaged, iUnitsKilled, iBuildingsDestroyed, iCitiesAffected, iPopulationKilled, GC.getGame().getNukesExploded());
}


// <!-- custom: Preserve each city caught in an actual unit-launched blast, including cases where defenses reduce realized losses to zero.
// The building list is assembled only from buildings the existing destruction loop actually removed; no city/building rescan or RNG is added. (ChatGPT-5.6-Sol) -->
void logSASGameRecordNukeCityEffect(CvUnit const* pNukeUnit, CvCity const* pCity, int iPopulationBefore, int iNukeModifier, std::vector<BuildingTypes> const& aeBuildingsDestroyed)
{
	if (pNukeUnit == NULL || pCity == NULL)
		return;
	CvString szBuildingsDestroyed;
	for (size_t i = 0; i < aeBuildingsDestroyed.size(); i++)
		appendSASGameRecordType(szBuildingsDestroyed, getSASGameRecordBuildingType(aeBuildingsDestroyed[i]));
	int const iPopulationAfter = pCity->getPopulation();
	logSASGameRecord("GAME_RECORD_NUKE_CITY_EFFECT turn=%d player=%d team=%d nukeUnitId=%d nukeUnit=%s affectedPlayer=%d affectedTeam=%d cityId=%d city=%S x=%d y=%d nukeModifier=%d populationBefore=%d populationAfter=%d populationKilled=%d buildingsDestroyedCount=%d buildingsDestroyed=%s",
			GC.getGame().getGameTurn(), pNukeUnit->getOwner(), pNukeUnit->getTeam(), pNukeUnit->getID(), getSASGameRecordUnitType(pNukeUnit->getUnitType()),
			pCity->getOwner(), pCity->getTeam(), pCity->getID(), getSASGameRecordQuotedCityName(pCity).GetCString(), pCity->getX(), pCity->getY(), iNukeModifier,
			iPopulationBefore, iPopulationAfter, std::max(0, iPopulationBefore - iPopulationAfter), (int)aeBuildingsDestroyed.size(),
			getSASDiagnosticOrDash(szBuildingsDestroyed).GetCString());
}

// <!-- custom: Level-3 nuke unit rows retain tactical identity before the existing damage/kill operation can remove the object.
// Direct combat damage records exact before/after damage; indirect cargo and noncombat death use damageAfter=-1 rather than inventing a damage value that gameplay never assigned. (ChatGPT-5.6-Sol) -->
void logSASGameRecordNukeUnitEffect(CvUnit const* pNukeUnit, CvUnit const* pAffectedUnit, CvPlot const* pPlot, int iDamageBefore, int iDamageAfter, bool bKilled, char const* szCause)
{
	if (pNukeUnit == NULL || pAffectedUnit == NULL || pPlot == NULL)
		return;
	CvUnit const* pTransport = pAffectedUnit->getTransportUnit();
	int const iDamageDelta = (iDamageBefore >= 0 && iDamageAfter >= 0 ? iDamageAfter - iDamageBefore : -1);
	logSASGameRecord("GAME_RECORD_NUKE_UNIT_EFFECT turn=%d player=%d team=%d nukeUnitId=%d nukeUnit=%s affectedPlayer=%d affectedTeam=%d unitId=%d unit=%s unitAI=%s x=%d y=%d damageBefore=%d damageAfter=%d damageDelta=%d killed=%d cause=%s cargo=%d transportPlayer=%d transportId=%d",
			GC.getGame().getGameTurn(), pNukeUnit->getOwner(), pNukeUnit->getTeam(), pNukeUnit->getID(), getSASGameRecordUnitType(pNukeUnit->getUnitType()),
			pAffectedUnit->getOwner(), pAffectedUnit->getTeam(), pAffectedUnit->getID(), getSASGameRecordUnitType(pAffectedUnit->getUnitType()), getSASGameRecordUnitAIType(pAffectedUnit->AI_getUnitAIType()),
			pPlot->getX(), pPlot->getY(), iDamageBefore, iDamageAfter, iDamageDelta, bKilled, szCause, pAffectedUnit->isCargo(),
			pTransport == NULL ? NO_PLAYER : pTransport->getOwner(), pTransport == NULL ? -1 : pTransport->getID());
}

// <!-- custom: Only ordinary civilization-vs-civilization battles with no attacker withdrawal chance and a lethal combat limit form a true binary win/loss sample.
// Siege/combat-limit fights, withdrawals and Barbarian free-win rules are recorded separately rather than contaminating expected-vs-observed luck. (ChatGPT-5.6-Sol) -->
static bool isSASGameRecordLuckEligible(CvUnit const& kAttacker, CvUnit const& kDefender)
{
	PlayerTypes const eAttacker = kAttacker.getOwner();
	PlayerTypes const eDefender = kDefender.getOwner();
	return (eAttacker >= 0 && eAttacker < MAX_CIV_PLAYERS && eDefender >= 0 && eDefender < MAX_CIV_PLAYERS && !kAttacker.isBarbarian() && !kDefender.isBarbarian() && kAttacker.withdrawalProbability() <= 0 && kAttacker.combatLimit() >= kDefender.maxHitPoints());
}

static bool popSASGameRecordCombatPending(CvUnit const* pUnitA, CvUnit const* pUnitB, CvPlot const* pBattlePlot, SASGameRecordCombatPending& kResult)
{
	if (pUnitA == NULL || pUnitB == NULL)
		return false;
	for (int iI = (int)g_aSASGameRecordCombatPending.size() - 1; iI >= 0; iI--)
	{
		SASGameRecordCombatPending const& kPending = g_aSASGameRecordCombatPending[iI];
		bool const bAIsAttacker = (pUnitA->getOwner() == kPending.eAttacker && pUnitA->getID() == kPending.iAttackerUnitId && pUnitB->getOwner() == kPending.eDefender && pUnitB->getID() == kPending.iDefenderUnitId);
		bool const bBIsAttacker = (pUnitB->getOwner() == kPending.eAttacker && pUnitB->getID() == kPending.iAttackerUnitId && pUnitA->getOwner() == kPending.eDefender && pUnitA->getID() == kPending.iDefenderUnitId);
		if (!bAIsAttacker && !bBIsAttacker)
			continue;
		if (pBattlePlot != NULL && (pBattlePlot->getX() != kPending.iX || pBattlePlot->getY() != kPending.iY))
			continue;
		kResult = kPending;
		g_aSASGameRecordCombatPending.erase(g_aSASGameRecordCombatPending.begin() + iI);
		return true;
	}
	return false;
}

void noteSASGameRecordCombatStarted(CvUnit const* pAttacker, CvUnit const* pDefender, CvPlot const* pBattlePlot)
{
	if (pAttacker == NULL || pDefender == NULL || pBattlePlot == NULL)
		return;
	bool const bLuckEligible = isSASGameRecordLuckEligible(*pAttacker, *pDefender);
	bool const bLogExactBattle = (gGameRecordLogLevel >= 3);
	bool const bCaptureCombatContext = (bLogExactBattle || bLuckEligible);
	// <!-- custom: Level 2 needs transient context only for the exact-odds statistical sample.
	// Level 3 also keeps attacker identity for every exact battle row, including Barbarian fights whose special free-win semantics intentionally leave odds unknown here.
	// Name the positive requirement explicitly, then early-return to keep the expensive capture path unnested. (ChatGPT-5.6-Sol) -->
	if (!bCaptureCombatContext)
		return;
	SASGameRecordCombatPending kPending;
	kPending.eAttacker = pAttacker->getOwner();
	kPending.eDefender = pDefender->getOwner();
	kPending.iAttackerUnitId = pAttacker->getID();
	kPending.iDefenderUnitId = pDefender->getID();
	kPending.iX = pBattlePlot->getX();
	kPending.iY = pBattlePlot->getY();
	kPending.bLuckEligible = bLuckEligible;
	bool const bCivilizationBattle = (pAttacker->getOwner() >= 0 && pAttacker->getOwner() < MAX_CIV_PLAYERS && pDefender->getOwner() >= 0 && pDefender->getOwner() < MAX_CIV_PLAYERS && !pAttacker->isBarbarian() && !pDefender->isBarbarian());
	kPending.iAttackerCombatOddsPermille = ((bLuckEligible || (bLogExactBattle && bCivilizationBattle)) ? calculateCombatOdds(*pAttacker, *pDefender) : -1);
	g_aSASGameRecordCombatPending.push_back(kPending);
}

static void recordSASGameRecordBattleLuck(PlayerTypes ePlayer, int iOwnOddsPermille, bool bWon)
{
	if (ePlayer < 0 || ePlayer >= MAX_CIV_PLAYERS || iOwnOddsPermille < 0 || iOwnOddsPermille > 1000)
		return;
	SASGameRecordBattleQuality* apQuality[2] = { &g_akSASGameRecordBattleQuality[ePlayer], &g_akSASGameRecordTotalBattleQuality[ePlayer] };
	for (int iI = 0; iI < 2; iI++)
	{
		SASGameRecordBattleQuality& kQuality = *apQuality[iI];
		kQuality.iLuckEligibleBattles++;
		kQuality.iExpectedWinsX1000 += iOwnOddsPermille;
		if (bWon)
		{
			kQuality.iLuckEligibleWins++;
			if (iOwnOddsPermille < 500) kQuality.iUpsetWins++;
			if (kQuality.iLowestOddsWinPermille < 0 || iOwnOddsPermille < kQuality.iLowestOddsWinPermille) kQuality.iLowestOddsWinPermille = iOwnOddsPermille;
		}
		else
		{
			if (iOwnOddsPermille > 500) kQuality.iUpsetLosses++;
			if (iOwnOddsPermille > kQuality.iHighestOddsLossPermille) kQuality.iHighestOddsLossPermille = iOwnOddsPermille;
		}
	}
}

void logSASGameRecordNonlethalCombat(CvUnit const* pAttacker, CvUnit const* pDefender, CvPlot const* pBattlePlot, bool bCombatLimitReached)
{
	if (pAttacker == NULL || pDefender == NULL || pBattlePlot == NULL)
		return;
	PlayerTypes const eAttacker = pAttacker->getOwner();
	PlayerTypes const eDefender = pDefender->getOwner();
	if (eAttacker >= 0 && eAttacker < MAX_CIV_PLAYERS)
	{
		SASGameRecordBattleQuality* apQuality[2] = { &g_akSASGameRecordBattleQuality[eAttacker], &g_akSASGameRecordTotalBattleQuality[eAttacker] };
		for (int iI = 0; iI < 2; iI++)
		{
			if (bCombatLimitReached) apQuality[iI]->iCombatLimitAttacks++;
			else apQuality[iI]->iWithdrawals++;
		}
	}
	if (eDefender >= 0 && eDefender < MAX_CIV_PLAYERS)
	{
		SASGameRecordBattleQuality* apQuality[2] = { &g_akSASGameRecordBattleQuality[eDefender], &g_akSASGameRecordTotalBattleQuality[eDefender] };
		for (int iI = 0; iI < 2; iI++)
		{
			if (bCombatLimitReached) apQuality[iI]->iCombatLimitDefenses++;
			else apQuality[iI]->iEnemyWithdrawals++;
		}
	}
	SASGameRecordCombatPending kPending;
	bool const bPending = popSASGameRecordCombatPending(pAttacker, pDefender, pBattlePlot, kPending);
	if (gGameRecordLogLevel >= 3)
	{
		logSASGameRecord("GAME_RECORD_BATTLE_NONLETHAL turn=%d attacker=%d defender=%d attackerUnit=%s attackerUnitId=%d defenderUnit=%s defenderUnitId=%d reason=%s x=%d y=%d cityPlot=%d attackerBaseStr=%d defenderBaseStr=%d attackerDamage=%d defenderDamage=%d attackerCombatLimit=%d attackerWithdrawal=%d attackerCombatOddsPermille=%d attackerXP=%d attackerLevel=%d defenderXP=%d defenderLevel=%d",
			GC.getGame().getGameTurn(), eAttacker, eDefender, getSASGameRecordUnitType(pAttacker->getUnitType()), pAttacker->getID(),
			getSASGameRecordUnitType(pDefender->getUnitType()), pDefender->getID(), bCombatLimitReached ? "COMBAT_LIMIT" : "WITHDRAWAL",
			pBattlePlot->getX(), pBattlePlot->getY(), pBattlePlot->isCity(),
			pAttacker->baseCombatStr(), pDefender->baseCombatStr(), pAttacker->getDamage(), pDefender->getDamage(), pAttacker->combatLimit(), pAttacker->withdrawalProbability(),
			bPending ? kPending.iAttackerCombatOddsPermille : -1, pAttacker->getExperience(), pAttacker->getLevel(), pDefender->getExperience(), pDefender->getLevel());
	}
}

void logSASGameRecordExperienceChange(CvUnit const* pUnit, int iAdjustedChange, int iActualChange, bool bFromCombat)
{
	if (pUnit == NULL)
		return;
	PlayerTypes const ePlayer = pUnit->getOwner();
	if (ePlayer < 0 || ePlayer >= MAX_PLAYERS)
		return;
	int const iGained = std::max(0, iActualChange);
	int const iLostAdjustment = std::max(0, -iActualChange);
	int const iPreventedByCap = (iAdjustedChange > 0 ? std::max(0, iAdjustedChange - iGained) : 0);
	if (iGained <= 0 && iLostAdjustment <= 0 && iPreventedByCap <= 0)
		return;
	SASGameRecordPlayerFlow& kFlow = g_akSASGameRecordPlayerFlow[ePlayer];
	kFlow.iExperienceGained += iGained;
	if (bFromCombat) kFlow.iCombatExperienceGained += iGained;
	else kFlow.iNonCombatExperienceGained += iGained;
	kFlow.iExperiencePreventedByCap += iPreventedByCap;
	kFlow.iExperienceLostAdjustments += iLostAdjustment;
	SASGameRecordMilitaryQualityTotals& kTotal = g_akSASGameRecordMilitaryQualityTotals[ePlayer];
	kTotal.iExperienceGained += iGained;
	if (bFromCombat) kTotal.iCombatExperienceGained += iGained;
	else kTotal.iNonCombatExperienceGained += iGained;
	kTotal.iExperiencePreventedByCap += iPreventedByCap;
	kTotal.iExperienceLostAdjustments += iLostAdjustment;
}

void logSASGameRecordUnitPromoted(CvUnit const* pUnit, PromotionTypes ePromotion)
{
	if (pUnit == NULL || ePromotion == NO_PROMOTION)
		return;
	PlayerTypes const ePlayer = pUnit->getOwner();
	if (ePlayer < 0 || ePlayer >= MAX_PLAYERS)
		return;
	bool const bLeaderPromotion = GC.getInfo(ePromotion).isLeader();
	SASGameRecordPlayerFlow& kFlow = g_akSASGameRecordPlayerFlow[ePlayer];
	SASGameRecordMilitaryQualityTotals& kTotal = g_akSASGameRecordMilitaryQualityTotals[ePlayer];
	if (bLeaderPromotion)
	{
		kFlow.iLeaderPromotionApplications++;
		kTotal.iLeaderPromotionApplications++;
		return; // <!-- custom: GREAT_GENERAL_ATTACHED already provides the exact level-2 action with both source and target units. (ChatGPT-5.6-Sol) -->
	}
	kFlow.iPromotionsChosen++;
	kTotal.iPromotionsChosen++;
	int const iPromotion = (int)ePromotion;
	if (iPromotion >= 0 && iPromotion < (int)kFlow.aiPromotionChoices.size())
		kFlow.aiPromotionChoices[iPromotion]++;
	if (gGameRecordLogLevel >= 3)
	{
		logSASGameRecord("GAME_RECORD_ACTION turn=%d type=UNIT_PROMOTED player=%d unitId=%d unit=%s unitAI=%s promotion=%s x=%d y=%d xp=%d level=%d",
			GC.getGame().getGameTurn(), ePlayer, pUnit->getID(), getSASGameRecordUnitType(pUnit->getUnitType()),
			getSASGameRecordUnitAIType(pUnit->AI_getUnitAIType()), getSASGameRecordPromotionType(ePromotion),
			pUnit->getX(), pUnit->getY(), pUnit->getExperience(), pUnit->getLevel());
	}
}

// <!-- custom: Add the combat target supplied by CvUnit because a defeated attacker still occupies its origin at this callback.
// Deriving the target from pLoser made failed attacks wrong across coordinates, city-battle counters and dependent action rows. See KI#377.
// CvEventReporter owns the level-2 caller gate, so this helper intentionally does not repeat it before aggregate work. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
void logSASGameRecordCombatResult(CvUnit const* pWinner, CvUnit const* pLoser, CvPlot const* pBattlePlot)
{
	if (pWinner == NULL || pLoser == NULL || pBattlePlot == NULL)
		return;
	bool const bLogExactBattle = (gGameRecordLogLevel >= 3);
	logSASGameRecordSettlerCombatIfNeeded(pWinner, pLoser, pBattlePlot);
	PlayerTypes eWinner = pWinner->getOwner();
	PlayerTypes eLoser = pLoser->getOwner();
	CvPlot const* pPlot = pBattlePlot;
	const bool bCityPlot = (pPlot != NULL && pPlot->isCity());
	SASGameRecordCombatPending kPending;
	bool const bPendingCombat = popSASGameRecordCombatPending(pWinner, pLoser, pPlot, kPending);
	if (bPendingCombat && kPending.bLuckEligible && kPending.iAttackerCombatOddsPermille >= 0)
	{
		bool const bAttackerWon = (pWinner->getOwner() == kPending.eAttacker && pWinner->getID() == kPending.iAttackerUnitId);
		recordSASGameRecordBattleLuck(kPending.eAttacker, kPending.iAttackerCombatOddsPermille, bAttackerWon);
		recordSASGameRecordBattleLuck(kPending.eDefender, 1000 - kPending.iAttackerCombatOddsPermille, !bAttackerWon);
	}
	// <!-- custom: Aggregate battle rows omit the Barbarian player, and Settler-defense rows cover only one special case. At level 3, retain exact ordinary Barbarian/animal combat so spawned pressure can be followed through its actual outcome. (GPT-5.6-Sol) -->
	if (bLogExactBattle && (pWinner->getOwner() == BARBARIAN_PLAYER || pLoser->getOwner() == BARBARIAN_PLAYER))
	{
		logSASGameRecord("GAME_RECORD_ACTION turn=%d type=BARBARIAN_COMBAT winnerPlayer=%d winnerUnitId=%d winnerUnit=%s winnerAI=%s winnerDamage=%d loserPlayer=%d loserUnitId=%d loserUnit=%s loserAI=%s loserDamage=%d x=%d y=%d cityPlot=%d",
				GC.getGame().getGameTurn(), pWinner->getOwner(), pWinner->getID(), getSASGameRecordUnitType(pWinner->getUnitType()), getSASGameRecordUnitAIType(pWinner->AI_getUnitAIType()), pWinner->getDamage(),
				pLoser->getOwner(), pLoser->getID(), getSASGameRecordUnitType(pLoser->getUnitType()), getSASGameRecordUnitAIType(pLoser->AI_getUnitAIType()), pLoser->getDamage(),
				pPlot == NULL ? -1 : pPlot->getX(), pPlot == NULL ? -1 : pPlot->getY(), bCityPlot);
	}
	int const iLoserProductionNeeded = (eLoser >= 0 && eLoser < MAX_PLAYERS ? GET_PLAYER(eLoser).getProductionNeeded(pLoser->getUnitType()) : 0);
	if (eWinner >= 0 && eWinner < MAX_PLAYERS)
	{
		g_aiSASGameRecordBattleWins[eWinner]++;
		g_aiSASGameRecordTotalBattleWins[eWinner]++;
		SASGameRecordPlayerFlow& kWinnerFlow = g_akSASGameRecordPlayerFlow[eWinner];
		kWinnerFlow.iCombatWins++;
		if (eLoser >= 0 && eLoser < MAX_PLAYERS)
		{
			kWinnerFlow.iEnemyProductionNeededDestroyed += iLoserProductionNeeded;
			kWinnerFlow.iEnemyExperienceDestroyed += pLoser->getExperience();
			g_akSASGameRecordMilitaryQualityTotals[eWinner].iEnemyExperienceDestroyed += pLoser->getExperience();
		}
		if (bCityPlot)
		{
			g_aiSASGameRecordCityBattleWins[eWinner]++;
			g_aiSASGameRecordTotalCityBattleWins[eWinner]++;
			kWinnerFlow.iCityPlotWins++;
		}
	}
	if (eLoser >= 0 && eLoser < MAX_PLAYERS)
	{
		g_aiSASGameRecordBattleLosses[eLoser]++;
		g_aiSASGameRecordTotalBattleLosses[eLoser]++;
		SASGameRecordPlayerFlow& kLoserFlow = g_akSASGameRecordPlayerFlow[eLoser];
		kLoserFlow.iCombatLosses++;
		kLoserFlow.iOwnProductionNeededLost += iLoserProductionNeeded;
		kLoserFlow.iOwnExperienceLost += pLoser->getExperience();
		g_akSASGameRecordMilitaryQualityTotals[eLoser].iOwnExperienceLost += pLoser->getExperience();
		if (bCityPlot)
		{
			g_aiSASGameRecordCityBattleLosses[eLoser]++;
			g_aiSASGameRecordTotalCityBattleLosses[eLoser]++;
			kLoserFlow.iCityPlotLosses++;
		}
	}
	// <!-- custom: Keep battle aggregates scoped to the active war between the combatants' teams; this avoids charging third-party or Barbarian losses to another simultaneous war. (GPT-5.6-Sol) -->
	if (eWinner >= 0 && eWinner < MAX_PLAYERS && eLoser >= 0 && eLoser < MAX_PLAYERS)
	{
		TeamTypes const eWinnerTeam = GET_PLAYER(eWinner).getTeam();
		TeamTypes const eLoserTeam = GET_PLAYER(eLoser).getTeam();
		SASGameRecordWarSummary* pWar = findSASGameRecordWar(eWinnerTeam, eLoserTeam);
		if (pWar != NULL)
		{
			if (eWinnerTeam == pWar->eTeamA)
			{
				pWar->iUnitsDestroyedByA++;
				pWar->iProductionDestroyedByA += iLoserProductionNeeded;
				if (bCityPlot) pWar->iCityPlotWinsA++;
			}
			else
			{
				pWar->iUnitsDestroyedByB++;
				pWar->iProductionDestroyedByB += iLoserProductionNeeded;
				if (bCityPlot) pWar->iCityPlotWinsB++;
			}
			refreshSASGameRecordWarSuccess(*pWar);
		}
	}
	if (pLoser->getLeaderUnitType() != NO_UNIT)
	{
		logSASGameRecord("GAME_RECORD_ACTION turn=%d type=GREAT_GENERAL_UNIT_DIED player=%d unitId=%d unit=%s attachedGreatGeneral=%s winnerPlayer=%d winnerUnitId=%d winnerUnit=%s x=%d y=%d",
				GC.getGame().getGameTurn(), eLoser, pLoser->getID(), getSASGameRecordUnitType(pLoser->getUnitType()), getSASGameRecordUnitType(pLoser->getLeaderUnitType()), eWinner, pWinner->getID(), getSASGameRecordUnitType(pWinner->getUnitType()), pPlot->getX(), pPlot->getY());
	}
	logSASGameRecordGreatPersonDied(pLoser, eWinner, "COMBAT", pPlot);
	if (bLogExactBattle)
	{
		// <!-- custom: Include exact unit IDs so WAR_ATTACK_ORDER attacker selections can be joined to the resulting battle even when several units of the same type fight on the same turn.
		// The transient start context additionally preserves true attacker identity and pre-combat odds after visible-combat delay. (ChatGPT-5.6-Sol) -->
		int const iWinnerOddsPermille = (!bPendingCombat || kPending.iAttackerCombatOddsPermille < 0 ? -1 : (pWinner->getOwner() == kPending.eAttacker && pWinner->getID() == kPending.iAttackerUnitId ? kPending.iAttackerCombatOddsPermille : 1000 - kPending.iAttackerCombatOddsPermille));
		logSASGameRecord("GAME_RECORD_BATTLE turn=%d winner=%d loser=%d winnerUnit=%s winnerUnitId=%d loserUnit=%s loserUnitId=%d attacker=%d attackerUnitId=%d attackerCombatOddsPermille=%d winnerCombatOddsPermille=%d luckEligible=%d x=%d y=%d cityPlot=%d winnerBaseStr=%d loserBaseStr=%d winnerDamage=%d loserDamage=%d winnerXP=%d winnerLevel=%d loserXP=%d loserLevel=%d winnerLeaderUnit=%s loserLeaderUnit=%s",
				GC.getGame().getGameTurn(), eWinner, eLoser, getSASGameRecordUnitType(pWinner->getUnitType()), pWinner->getID(), getSASGameRecordUnitType(pLoser->getUnitType()), pLoser->getID(),
				bPendingCombat ? kPending.eAttacker : NO_PLAYER, bPendingCombat ? kPending.iAttackerUnitId : -1, bPendingCombat ? kPending.iAttackerCombatOddsPermille : -1, iWinnerOddsPermille, bPendingCombat && kPending.bLuckEligible,
				pPlot->getX(), pPlot->getY(), bCityPlot, pWinner->baseCombatStr(), pLoser->baseCombatStr(), pWinner->getDamage(), pLoser->getDamage(), 
				pWinner->getExperience(), pWinner->getLevel(), pLoser->getExperience(), pLoser->getLevel(), getSASGameRecordUnitType(pWinner->getLeaderUnitType()), getSASGameRecordUnitType(pLoser->getLeaderUnitType()));
	}
}
