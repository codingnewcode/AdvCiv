# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
# <!-- custom: Creates ordinary SAVEGAME_NORMAL fast saves in the normal single-player save directory at selected game lifecycle/autoplay events.
# These are permanent normal saves, not Civ4's rotating Autosaves-folder files and not BUG AutoSave UI/options.
# The behavior is controlled only by SAS GlobalDefines and is useful for both ordinary play convenience and repetitive testing. (ChatGPT-5.6-Sol) -->

from CvPythonExtensions import *
import os
import time
import BugPath
import BugUtil
import CvUtil

gc = CyGlobalContext()

_SAVE_EXTENSION = ".CivBeyondSwordSave"
# <!-- custom: Lazy-cache SAS GlobalDefines because this helper can be imported during Python startup before XML define loading is safely complete. (ChatGPT-5.6-Sol) -->
_SAS_FAST_SAVE_GAME_START_ENABLE = None
_SAS_FAST_SAVE_GAME_END_ENABLE = None
_SAS_FAST_SAVE_AUTOPLAY_STOP_ENABLE = None

def _isGameStartSaveEnabled():
	global _SAS_FAST_SAVE_GAME_START_ENABLE
	if _SAS_FAST_SAVE_GAME_START_ENABLE is None:
		_SAS_FAST_SAVE_GAME_START_ENABLE = (gc.getDefineINT("SAS_FAST_SAVE_GAME_START_ENABLE") > 0)
	return _SAS_FAST_SAVE_GAME_START_ENABLE

def _isGameEndSaveEnabled():
	global _SAS_FAST_SAVE_GAME_END_ENABLE
	if _SAS_FAST_SAVE_GAME_END_ENABLE is None:
		_SAS_FAST_SAVE_GAME_END_ENABLE = (gc.getDefineINT("SAS_FAST_SAVE_GAME_END_ENABLE") > 0)
	return _SAS_FAST_SAVE_GAME_END_ENABLE

def _isAutoPlayStopSaveEnabled():
	global _SAS_FAST_SAVE_AUTOPLAY_STOP_ENABLE
	if _SAS_FAST_SAVE_AUTOPLAY_STOP_ENABLE is None:
		_SAS_FAST_SAVE_AUTOPLAY_STOP_ENABLE = (gc.getDefineINT("SAS_FAST_SAVE_AUTOPLAY_STOP_ENABLE") > 0)
	return _SAS_FAST_SAVE_AUTOPLAY_STOP_ENABLE

def _safeToken(szText, szPrefix="", iMaxLen=24):
	# Return a short ASCII token suitable for a Windows save filename.
	try:
		szText = CvUtil.convertToStr(szText)
	except:
		szText = str(szText)
	if szPrefix and szText.startswith(szPrefix):
		szText = szText[len(szPrefix):]

	chars = []
	bPreviousUnderscore = False
	for ch in szText.upper():
		if ('A' <= ch <= 'Z') or ('0' <= ch <= '9'):
			chars.append(ch)
			bPreviousUnderscore = False
		elif not bPreviousUnderscore:
			chars.append('_')
			bPreviousUnderscore = True

	szToken = ''.join(chars).strip('_')
	if not szToken:
		szToken = "UNKNOWN"
	if iMaxLen > 0 and len(szToken) > iMaxLen:
		szToken = szToken[:iMaxLen].rstrip('_')
	return szToken

def _getNormalSinglePlayerSaveDir():
	szRoot = BugPath.getRootDir()
	if not szRoot:
		BugUtil.error("SASFastSave - could not find Beyond the Sword user directory")
		return None
	szDir = BugPath.join(szRoot, "Saves", "single")
	if not os.path.isdir(szDir):
		try:
			os.makedirs(szDir)
		except:
			BugUtil.error("SASFastSave - could not create save directory %s", szDir)
			return None
	return szDir

def _getInitialSeedTokens():
	game = gc.getGame()
	# <!-- custom: Keep the map and sync seeds near the filename tail because they are mainly useful for exact map/gameplay-RNG lineage and advanced filename filtering rather than immediate identification in Civ4's narrow save UI.
	# A map seed alone is not a complete map fingerprint when reused with another map script/options, so readable world-size and map-script tokens remain in the filename too. Reading either serialized initial seed does not advance RNG state. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
	# <!-- custom: An earlier draft defensively used long(seed) & 0xFFFFFFFFL. '&' is bitwise AND, and that mask would keep the low 32 bits if Python had received a signed or wider value.
	# The mask was removed because these C++ getters already return unsigned int values, so it was redundant. Python 2's trailing 'L' long-literal syntax also prevents modern Ruff/Pylance Python-3 parsers from parsing the file. (ChatGPT-5.6-Sol) -->
	uiMapSeed = game.getInitialMapRandSeed()
	uiSyncSeed = game.getInitialSyncRandSeed()
	return ("M%08X" % uiMapSeed, "S%08X" % uiSyncSeed)

def _getWorldSizeToken():
	map = gc.getMap()
	iWorldSize = map.getWorldSize()
	if iWorldSize < 0:
		return "NOWORLD"
	# <!-- custom: Use the stable XML type rather than localized display text so a Fast Save remains concise and documentation-friendly across UI languages. (ChatGPT-5.6-Sol) -->
	return _safeToken(gc.getWorldInfo(iWorldSize).getType(), "WORLDSIZE_", 16)

def _getMapScriptToken():
	map = gc.getMap()
	try:
		szMapScript = CvUtil.convertToStr(map.getMapScriptName())
	except:
		szMapScript = str(map.getMapScriptName())
	# <!-- custom: Keep the map script greppable and able to disambiguate reuse of the same map seed, but place it after turn/reason information so those more important fields are less likely to be clipped in Civ4's narrow save UI. Strip path/extension noise, then sanitize/cap unusual custom script names for Windows path safety. (ChatGPT-5.6-Sol) -->
	szMapScript = os.path.splitext(os.path.basename(szMapScript))[0]
	return _safeToken(szMapScript, "", 24)

def _getInitialLeaderToken():
	game = gc.getGame()
	iPlayer = game.getInitialActivePlayer()
	if iPlayer < 0:
		iPlayer = game.getActivePlayer()
	if iPlayer < 0:
		return "NOPLAYER"
	iLeader = gc.getPlayer(iPlayer).getLeaderType()
	if iLeader < 0:
		return "PLAYER%d" % iPlayer
	# <!-- custom: Use the original player's stable XML leader type (e.g. LEADER_GANDHI -> GANDHI), never a user-entered player name such as "PC".
	# AdvCiv serializes the initial active player, so this token continues to identify the original test/game even if unattended autoplay later hands control to another player after defeat. (ChatGPT-5.6-Sol) -->
	return _safeToken(gc.getLeaderHeadInfo(iLeader).getType(), "LEADER_", 24)

def _getGameSpeedToken():
	game = gc.getGame()
	iSpeed = game.getGameSpeedType()
	if iSpeed < 0:
		return "NOSPEED"
	# <!-- custom: Use the stable, language-independent XML type. Standard names are already short; cap only unusually long custom names as a filename safety measure. (ChatGPT-5.6-Sol) -->
	return _safeToken(gc.getGameSpeedInfo(iSpeed).getType(), "GAMESPEED_", 20)

def _getVictoryTokens():
	game = gc.getGame()
	iWinner = game.getWinner()
	iVictory = game.getVictory()
	if iWinner < 0 or iVictory < 0:
		return None

	szVictory = _safeToken(gc.getVictoryInfo(iVictory).getType(), "VICTORY_", 20)
	# <!-- custom: The original player leader is already shown before the turn in every Fast Save. If that player's team won, the victory type alone is enough and avoids duplicating the same leader in the narrow save UI.
	# If another team won, append that winning team's XML leader so a filename such as GANDHI_..._CULTURAL_BOUDICA shows both the original player and the actual winner at a glance. (ChatGPT-5.6-Sol) -->
	iInitialPlayer = game.getInitialActivePlayer()
	if iInitialPlayer >= 0 and gc.getPlayer(iInitialPlayer).getTeam() == iWinner:
		return (szVictory,)

	iLeaderPlayer = gc.getTeam(iWinner).getLeaderID()
	if iLeaderPlayer < 0:
		szLeader = "TEAM%d" % iWinner
	else:
		iLeader = gc.getPlayer(iLeaderPlayer).getLeaderType()
		szLeader = _safeToken(gc.getLeaderHeadInfo(iLeader).getType(), "LEADER_", 24)
	return (szVictory, szLeader)

def _buildBaseName(szReason):
	game = gc.getGame()
	szMapSeed, szSyncSeed = _getInitialSeedTokens()
	szUTC = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
	# <!-- custom: Optimize the narrow save UI for human scanning: original XML leader -> zero-padded turn -> game speed -> reason/victory -> world size -> map script -> map seed -> sync seed -> UTC timestamp.
	# Put immediately meaningful information before opaque seed metadata so it remains visible when Civ4 clips long filenames.
	# This gives up alphabetical grouping by map seed, but saves are normally sorted by file time and advanced tools can still filter the complete filename by either seed.
	# The UTC timestamp stays last as uniqueness metadata because Civ4's save dialog and file browsers already expose and sort by file date separately. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
	return "%s_T%04d_%s_%s_%s_%s_%s_%s_%s" % (_getInitialLeaderToken(), game.getGameTurn(), _getGameSpeedToken(), szReason, _getWorldSizeToken(), _getMapScriptToken(), szMapSeed, szSyncSeed, szUTC)

def _save(szReason):
	game = gc.getGame()
	if game.isGameMultiPlayer():
		return None

	szDir = _getNormalSinglePlayerSaveDir()
	if not szDir:
		return None

	szBase = _buildBaseName(szReason)
	szPath = BugPath.join(szDir, szBase + _SAVE_EXTENSION)
	# <!-- custom: Never overwrite a useful fast save if two save callbacks happen within the same UTC second. (ChatGPT-5.6-Sol) -->
	iDuplicate = 2
	while os.path.exists(szPath):
		szPath = BugPath.join(szDir, "%s_%d%s" % (szBase, iDuplicate, _SAVE_EXTENSION))
		iDuplicate += 1

	szPath = CvUtil.convertToStr(szPath)
	BugUtil.debug("SASFastSave - saving normal game to %s", szPath)
	game.saveGame(szPath)
	return szPath

# <!-- custom: START fast saves intentionally use BUG GameStart, which runs after BUG's earlier PreGameStart initialization.
# The first implementation called from CvGame::autoSave(true), but that path can run on turn slice 0; runtime testing created no START file there.
# Moving the hook here was runtime-tested successfully with a turn-0 START save. argsList is unused and optional so the helper can still be called directly. (ChatGPT-5.6-Sol) -->
def saveGameStart(argsList=None):
	if not _isGameStartSaveEnabled():
		return None
	return _save("START")

# <!-- custom: The victory event runs after CvGame::setWinner assigns the winner/victory needed for the filename, but before the later OVER/EXTENDED end-game transition. Earlier C++ hooks during or after that transition produced no victory file in autoplay testing. argsList is unused and optional so genuine non-victory GAMESTATE_OVER endings can still call this helper directly. (ChatGPT-5.6-Sol + GPT-5.6-Sol) -->
def saveGameEnd(argsList=None):
	if not _isGameEndSaveEnabled():
		return None
	victoryTokens = _getVictoryTokens()
	if victoryTokens:
		# <!-- custom: No WIN_ prefix is needed because a victory type is already unambiguous and filename space is scarce in Civ4's save UI. -->
		return _save("_".join(victoryTokens))
	return _save("END")

def saveAutoPlayStop():
	if not _isAutoPlayStopSaveEnabled():
		return None
	return _save("STOP")
