#!/usr/bin/env python3
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
# Shared XML define helpers for GitHub workflow checks.
#
# This module runs outside Civ4, through GitHub Actions or a local Python 3 command.
# It does not need Civ4 Python 2.4 compatibility.

from pathlib import Path
import argparse
import sys
import xml.etree.ElementTree as ET


def get_default_repo_root() -> Path:
	return Path(__file__).resolve().parents[3]


def local_name(tag: str) -> str:
	return tag.rsplit("}", 1)[-1]


def child_text_by_local_name(node: ET.Element, child_name: str) -> str | None:
	for child in list(node):
		if local_name(child.tag) == child_name:
			return (child.text or "").strip()
	return None


def read_global_define_ints(repo_root: Path) -> dict[str, int]:
	path = repo_root / "Assets" / "XML" / "GlobalDefines_advciv_sas.xml"
	if not path.exists():
		raise RuntimeError(f"missing file: {path}")

	tree = ET.parse(path)
	defines: dict[str, int] = {}

	for define in tree.getroot().iter():
		if local_name(define.tag) != "Define":
			continue

		name = child_text_by_local_name(define, "DefineName")
		value_text = child_text_by_local_name(define, "iDefineIntVal")
		if name is None or value_text is None:
			continue

		try:
			defines[name] = int(value_text)
		except ValueError as exc:
			raise RuntimeError(f"{name} has non-int iDefineIntVal: {value_text!r}") from exc

	return defines


def read_global_define_texts(repo_root: Path) -> dict[str, str]:
	path = repo_root / "Assets" / "XML" / "GlobalDefines_advciv_sas.xml"
	if not path.exists():
		raise RuntimeError(f"missing file: {path}")

	tree = ET.parse(path)
	defines: dict[str, str] = {}

	for define in tree.getroot().iter():
		if local_name(define.tag) != "Define":
			continue

		name = child_text_by_local_name(define, "DefineName")
		value_text = child_text_by_local_name(define, "DefineTextVal")
		if name is None or value_text is None:
			continue

		defines[name] = value_text

	return defines


def require_int_values(defines: dict[str, int], expected_by_name: dict[str, int]) -> list[str]:
	failures: list[str] = []

	for name, expected in sorted(expected_by_name.items()):
		if name not in defines:
			failures.append(f"{name}: missing define")
			continue

		actual = defines[name]
		if actual != expected:
			failures.append(f"{name}: expected {expected}, found {actual}")

	return failures


def run_global_define_check(title: str, expected_by_name: dict[str, int], description: str) -> int:
	parser = argparse.ArgumentParser(description=description)
	parser.add_argument("--repo-root", type=Path, default=get_default_repo_root(), help="repository root; defaults to the root containing .github/")
	args = parser.parse_args()

	defines = read_global_define_ints(args.repo_root)
	failures = require_int_values(defines, expected_by_name)

	if failures:
		print(f"FAIL {title}")
		for failure in failures:
			print(f"  - {failure}")
		return 1

	print(f"PASS {title}")
	return 0


def main_from_expected_values(title: str, expected_by_name: dict[str, int], description: str) -> None:
	sys.exit(run_global_define_check(title, expected_by_name, description))
