#!/usr/bin/env python3
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
# Static "unused text key" check: every TXT_KEY_* defined in the mod's
# GameText XML (Assets/XML/Text/*.xml by default) that is never referenced
# anywhere in the mod (Python + DLL C++/H + all other XML) NOR in the
# inherited base BTS / vanilla Civ4 Python+XML is flagged, grouped by the
# file that defines it.
#
# A tag is a DEFINITION when it appears as <Tag>TXT_KEY_FOO</Tag> inside a
# <TEXT>...</TEXT> block (mirrors audit_define_keys.py requiring <DefineName>
# inside <Define>).
#
# A tag is REFERENCED if its exact token appears anywhere in:
#   mod      Assets/Python/**/*.py, PrivateMaps/**/*.py,
#            CvGameCoreDLL/**/*.{cpp,h}, Assets/Config/**/*.xml,
#            Assets/XML/**/*.xml   (<Tag>...</Tag> spans scrubbed so a
#            definition never counts as its own reference)
#   base     <bts>/Assets/Python/**/*.py + <bts>/Assets/XML/**/*.xml
#            (auto: ../../Assets from mod root, like audit_define_keys.py)
#   vanilla  <civ4>/Assets/Python/**/*.py + <civ4>/Assets/XML/**/*.xml
#            (auto: ../../../Assets from mod root)
# Scanning base+vanilla is the CLEAN authoritative source for engine/
# front-end keys (main menu, setup screens, Civilopedia, sealevel/worldsize
# Info XML) that the mod inherits but does not override -- so they are
# confirmed used by real evidence instead of guess-whitelisted.
#
# Remaining false-positive sources (still a STATIC check; output is
# CANDIDATES, verify before deleting):
#   1. Dynamic construction: code builds keys by string concat, e.g.
#      "TXT_KEY_BUG_OPT_" + id.upper(). Tags under a known dynamic-family
#      prefix are routed to a separate REVIEW bucket, not LIKELY-UNUSED.
#   2. Engine-derived suffixes: _PEDIA/_STRATEGY/_HELP/... are appended to
#      a base key at runtime; if the base key is referenced the derived
#      tag is treated as used.
#   3. Pure compiled-EXE-internal keys referenced in NO Python/XML even in
#      base+vanilla. Rare after the external scan; such residue lands in
#      LIKELY-UNUSED and is the one class still needing a human eyeball.
#
# Behavior: exit 1 if any LIKELY-UNUSED tag found, 0 otherwise. Writes a
# timestamped report to LLM_Helpers/outputs/ unless --no-output-file.

import argparse
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

MOD_ROOT = Path(__file__).resolve().parent.parent
BASE_ASSETS = (MOD_ROOT.parent.parent / "Assets")          # BTS base
VANILLA_ASSETS = (MOD_ROOT.parent.parent.parent / "Assets")  # vanilla Civ4

TEXT_BLOCK_RE = re.compile(r"<TEXT>(.*?)</TEXT>", re.DOTALL | re.IGNORECASE)
TAG_DEF_RE = re.compile(r"<Tag>\s*(TXT_KEY_[A-Z0-9_]+)\s*</Tag>", re.IGNORECASE)
TAG_SPAN_RE = re.compile(r"<Tag>.*?</Tag>", re.DOTALL | re.IGNORECASE)
XML_COMMENT_RE = re.compile(r"<!--.*?-->", re.DOTALL)
CPP_BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)
LINE_COMMENT_RE = re.compile(r"(?m)^\s*(?://|#).*$")
TOKEN_RE = re.compile(r"TXT_KEY_[A-Z0-9_]+")

DERIVED_SUFFIXES = ("_PEDIA", "_STRATEGY", "_HELP", "_QUOTE", "_HEADING", "_DESC", "_ADJECTIVE", "_TEXT", "_HOVER",)
DYNAMIC_PREFIXES = ("TXT_KEY_BUG_OPT_", "TXT_KEY_BUG_OPTTAB_", "TXT_KEY_BUG_OPTBUTTON_", "TXT_KEY_BUG_OPTLABEL_", "TXT_KEY_BUG_OPTLIST_",)
MOD_GLOBS = ("Assets/Python/**/*.py", "PrivateMaps/**/*.py", "CvGameCoreDLL/**/*.cpp", "CvGameCoreDLL/**/*.h", "Assets/Config/**/*.xml", "Assets/XML/**/*.xml",)
EXTERNAL_GLOBS = ("Python/**/*.py", "XML/**/*.xml")

def read(path):
	try:
		return path.read_text(encoding="utf-8", errors="replace")
	except OSError as exc:
		print("warn: cannot read %s: %s" % (path, exc), file=sys.stderr)
		return ""

def iter_glob(root, globs):
	seen = set()
	for g in globs:
		for p in root.glob(g):
			if p.is_file():
				rp = p.resolve()
				if rp not in seen:
					seen.add(rp)
					yield p

def collect_definitions(text_paths):
	"""tag -> sorted list of relative files that define it (in <TEXT> blocks)."""
	defined = {}
	for path in text_paths:
		rel = str(path.relative_to(MOD_ROOT)).replace("\\", "/")
		for block in TEXT_BLOCK_RE.findall(XML_COMMENT_RE.sub("", read(path))):
			for tag in TAG_DEF_RE.findall(block):
				defined.setdefault(tag, set()).add(rel)
	return dict((t, sorted(f)) for t, f in defined.items())

def collect_mod_references():
	"""Tokens used in the mod, with <Tag> definition spans scrubbed in XML."""
	referenced = set()
	for path in iter_glob(MOD_ROOT, MOD_GLOBS):
		text = read(path)
		if path.suffix.lower() == ".xml":
			text = XML_COMMENT_RE.sub("", TAG_SPAN_RE.sub("", text))
		else:
			text = LINE_COMMENT_RE.sub("", CPP_BLOCK_COMMENT_RE.sub("", text))
		referenced.update(TOKEN_RE.findall(text))
	return referenced

def collect_external_tokens(root):
	"""Any TXT_KEY_* token in base/vanilla Python+XML (no scrub: a vanilla
	GameText <Tag> means the base game/EXE uses that key)."""
	if not root.is_dir():
		print("warn: external assets root not found, skipped: %s" % root, file=sys.stderr)
		return set()
	tokens = set()
	for path in iter_glob(root, EXTERNAL_GLOBS):
		tokens.update(TOKEN_RE.findall(read(path)))
	return tokens

def main():
	ap = argparse.ArgumentParser(description="Flag unused mod GameText TXT_KEY_* entries.")
	ap.add_argument("--text-glob", default="Assets/XML/Text/*.xml", help="glob (relative to mod root) for GameText files whose <Tag> entries are audited")
	ap.add_argument("--prefix", default="", help="only audit defined tags starting with this prefix (e.g. TXT_KEY_SAS)")
	ap.add_argument("--no-external", action="store_true", help="do NOT scan base BTS / vanilla Civ4 (mod-only references; more false positives)")
	ap.add_argument("--base-assets", default="", help="override base BTS Assets dir")
	ap.add_argument("--vanilla-assets", default="", help="override vanilla Civ4 Assets dir")
	ap.add_argument("--no-output-file", action="store_true", help="print only; do not write a report into LLM_Helpers/outputs/")
	args = ap.parse_args()

	text_paths = sorted(p for p in MOD_ROOT.glob(args.text_glob) if p.is_file())
	if not text_paths:
		print("error: no text files matched %s under %s" % (args.text_glob, MOD_ROOT), file=sys.stderr)
		return 2

	defined = collect_definitions(text_paths)
	if args.prefix:
		defined = dict((t, f) for t, f in defined.items() if t.startswith(args.prefix))

	referenced = collect_mod_references()
	mod_only = set(referenced)
	ext_roots = []
	if not args.no_external:
		base = Path(args.base_assets).resolve() if args.base_assets else BASE_ASSETS
		vanilla = Path(args.vanilla_assets).resolve() if args.vanilla_assets else VANILLA_ASSETS
		for r in (base, vanilla):
			ext_roots.append(r)
			referenced |= collect_external_tokens(r)

	likely = {}   # rel file -> [tags]
	review = {}   # rel file -> [tags]  (dynamic-prefix families)
	rescued = 0   # tags that mod-only would flag but base/vanilla confirm used
	for tag in sorted(defined):
		if tag in referenced:
			if tag not in mod_only:
				rescued += 1
			continue
		base_used = False
		for suf in DERIVED_SUFFIXES:
			if tag.endswith(suf) and tag[:-len(suf)] in referenced:
				base_used = True
				break
		if base_used:
			continue
		bucket = review if tag.startswith(DYNAMIC_PREFIXES) else likely
		for f in defined[tag]:
			bucket.setdefault(f, []).append(tag)

	dup = dict((t, f) for t, f in defined.items() if len(f) > 1)
	n_likely = sum(len(v) for v in likely.values())
	n_review = sum(len(v) for v in review.values())

	L = []
	L.append("AdvCiv-SAS unused GameText key audit")
	L.append("mod root : %s" % MOD_ROOT)
	L.append("text glob: %s%s" % (args.text_glob, (" (prefix=%s)" % args.prefix) if args.prefix else ""))
	if args.no_external:
		L.append("external : SKIPPED (mod-only; expect engine/front-end false positives)")
	else:
		L.append("external : %s" % " | ".join("%s%s" % (r, "" if r.is_dir() else " (MISSING)") for r in ext_roots))
	L.append("defined  : %d tags in %d files | referenced tokens: %d | rescued by base/vanilla: %d" % (len(defined), len(text_paths), len(referenced), rescued))
	L.append("LIKELY-UNUSED: %d | REVIEW (maybe dynamic): %d | duplicate-defined: %d" % (n_likely, n_review, len(dup)))
	L.append("")
	L.append("=== LIKELY UNUSED (no reference in mod, base BTS, or vanilla; verify before deleting) ===")
	if n_likely == 0:
		L.append("(none)")
	for f in sorted(likely):
		L.append("\n[%s]" % f)
		for t in sorted(likely[f]):
			L.append("  %s" % t)
	L.append("")
	L.append("=== REVIEW: matches a known dynamic-construction prefix (likely used via string concat) ===")
	if n_review == 0:
		L.append("(none)")
	for f in sorted(review):
		L.append("\n[%s]" % f)
		for t in sorted(review[f]):
			L.append("  %s" % t)
	if dup:
		L.append("")
		L.append("=== NOTE: tags defined in more than one file ===")
		for t in sorted(dup):
			L.append("  %s -> %s" % (t, ", ".join(dup[t])))

	report = "\n".join(L)
	print(report)

	if not args.no_output_file:
		out_dir = MOD_ROOT / "LLM_Helpers" / "outputs"
		out_dir.mkdir(parents=True, exist_ok=True)
		stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
		out_path = out_dir / ("unused_text_keys_%s.txt" % stamp)
		out_path.write_text(report + "\n", encoding="utf-8")
		print("\nreport written: %s" % out_path)

	return 1 if n_likely else 0

if __name__ == "__main__":
	sys.exit(main())
