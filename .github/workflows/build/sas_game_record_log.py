#!/usr/bin/env python3
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
# Build check: SASGameRecord report logging must be disabled by default.

from pathlib import Path
import argparse
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "lib"))

from xml_defines import get_default_repo_root, read_global_define_ints, require_int_values


EXPECTED_GAME_RECORD_DEFAULTS = {
	"SAS_GAME_RECORD_LOG_LEVEL": 0,
	# These configure enabled record logging but do not enable it themselves.
	"SAS_GAME_RECORD_INTERVAL_TURNS_UNSCALED_GAMESPEED": 10,
	"SAS_GAME_RECORD_LOG_USE_TIMESTAMPED_FILENAME": 1,
	"SAS_GAME_RECORD_PERFORMANCE_METRICS_ENABLE": 1,
	"SAS_GAME_RECORD_SYSTEM_CONTEXT_LEVEL": 2,
}


def main() -> int:
	parser = argparse.ArgumentParser(description="Check that SASGameRecord report logging is disabled by default unless explicitly listed as non-enabling configuration.")
	parser.add_argument("--repo-root", type=Path, default=get_default_repo_root(), help="repository root; defaults to the root containing .github/")
	args = parser.parse_args()

	defines = read_global_define_ints(args.repo_root)
	failures = require_int_values(defines, EXPECTED_GAME_RECORD_DEFAULTS)
	if failures:
		print("FAIL SASGameRecord report logging disabled by default")
		for failure in failures:
			print(f"  - {failure}")
		return 1
	print(f"PASS SASGameRecord report logging disabled by default: checked {len(EXPECTED_GAME_RECORD_DEFAULTS)} define(s)")
	return 0


if __name__ == "__main__":
	sys.exit(main())
