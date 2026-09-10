#!/usr/bin/env python3
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
# Conservative C++ dead-code candidate finder for LLM/manual review.
# This script does not edit files and does not prove that code is dead.
# It only reports suspicious candidates that deserve review.

import argparse
import datetime
import os
import re
from collections import defaultdict
from pathlib import Path

DEFAULT_SOURCE_DIRS = ("CvGameCoreDLL",)
DEFAULT_OUTPUT_DIR = os.path.join("LLM_Helpers", "outputs")
TEXT_EXTENSIONS = (".cpp", ".h", ".hpp", ".inl")

COMMENTED_DEFINE_RE = re.compile(r"^\s*//\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\b")
ACTIVE_DEFINE_RE = re.compile(r"^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\b")
IFDEF_RE = re.compile(r"^\s*#\s*(?:ifdef|ifndef)\s+([A-Za-z_][A-Za-z0-9_]*)\b")
IF_DEFINED_RE = re.compile(r"defined\s*\(?\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)?")
IF_ZERO_RE = re.compile(r"^\s*#\s*if\s+0\b")
ENDIF_RE = re.compile(r"^\s*#\s*endif\b")

# Good enough for qualified out-of-class C++ method definitions such as:
#   int CvPlayerAI::AI_getSomething(...) const
#   CvFoo::CvFoo(...)
#   template<class T> void EnumMap<T>::reset(...)
QUALIFIED_DEF_RE = re.compile(
    r"^\s*(?:template\s*<[^;{}]+>\s*)?"
    r"(?:(?:[A-Za-z_][\w:<>,~*&\s]+?)\s+)?"
    r"(?P<qual>[A-Za-z_][\w:<>]*)::(?P<name>~?[A-Za-z_][A-Za-z0-9_]*)\s*\("
)

RISKY_NAME_PREFIXES = (
    "AI_", "can", "cannot", "do", "get", "set", "is", "has", "read", "write",
    "update", "init", "uninit", "reset", "copy", "calculate", "process", "change",
    "should", "handle", "report", "mouse", "kbd", "new",
)
RISKY_EXACT_NAMES = set((
    "read", "write", "readPass2", "writeReplay", "reset", "init", "uninit",
    "copyNonDefaults", "getCheckSum", "cacheGlobals", "operator",
))
RISKY_FILES_RE = re.compile(r"(^|[/\\])(Cy|CvDLL|CvGlobals|CvInitCore|CvReplayInfo|CvSavegame|CvGameInterface|CvDllPythonEvents|CvEventReporter|FProfiler|FDataStream)")
RISKY_NAME_SUFFIXES = ("External", "Callback")
RISKY_QUAL_PREFIXES = (
    "CvGameTextMgr", "CvPythonCaller", "Cy", "CvDLL", "CvDll",
    "CvXMLLoadUtility", "CvReplayInfo", "CvSavegame",
)
IGNORED_QUAL_PREFIXES = ("std", "boost")


def relpath(path, root):
    return path.relative_to(root).as_posix()


def read_text(path):
    data = path.read_bytes()
    for encoding in ("utf-8-sig", "utf-8", "latin-1"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            pass
    return data.decode("latin-1", errors="replace")


def iter_source_files(root, source_dirs):
    seen = set()
    for source_dir in source_dirs:
        base = root / source_dir
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if path.is_file() and path.suffix.lower() in TEXT_EXTENSIONS:
                real = path.resolve()
                if real in seen:
                    continue
                seen.add(real)
                yield path


def strip_string_literals(text):
    # Keep line structure mostly intact; this only reduces obvious false positives in strings.
    text = re.sub(r'"(?:\\.|[^"\\])*"', '""', text)
    text = re.sub(r"'(?:\\.|[^'\\])*'", "''", text)
    return text


def collect_files(root, source_dirs):
    files = []
    for path in iter_source_files(root, source_dirs):
        text = read_text(path)
        files.append({"path": path, "rel": relpath(path, root), "text": text, "scan": strip_string_literals(text)})
    return files


def collect_macro_info(files):
    active_defines = defaultdict(list)
    commented_defines = defaultdict(list)
    conditional_uses = defaultdict(list)
    if0_blocks = []

    for f in files:
        lines = f["text"].splitlines()
        if0_stack = []
        for idx, line in enumerate(lines, 1):
            m = COMMENTED_DEFINE_RE.match(line)
            if m:
                commented_defines[m.group(1)].append((f["rel"], idx, line.strip()))
            m = ACTIVE_DEFINE_RE.match(line)
            if m and not line.lstrip().startswith("//#"):
                active_defines[m.group(1)].append((f["rel"], idx, line.strip()))
            m = IFDEF_RE.match(line)
            if m:
                conditional_uses[m.group(1)].append((f["rel"], idx, line.strip()))
            if line.lstrip().startswith("#") and "defined" in line:
                for macro in IF_DEFINED_RE.findall(line):
                    conditional_uses[macro].append((f["rel"], idx, line.strip()))
            if IF_ZERO_RE.match(line):
                if0_stack.append((idx, line.strip()))
            elif ENDIF_RE.match(line) and if0_stack:
                start_idx, start_line = if0_stack.pop()
                if0_blocks.append((f["rel"], start_idx, idx, idx - start_idx + 1))
        for start_idx, start_line in if0_stack:
            if0_blocks.append((f["rel"], start_idx, len(lines), len(lines) - start_idx + 1))

    return active_defines, commented_defines, conditional_uses, if0_blocks


def looks_like_definition_line(line):
    stripped = line.strip()
    if not stripped:
        return False
    if stripped.endswith(";"):
        return False
    if " = " in stripped and "{" not in stripped:
        return False
    if stripped.startswith(("return ", "if ", "else ", "for ", "while ", "switch ")):
        return False
    return True


def collect_definitions(files):
    defs = []
    for f in files:
        lines = f["scan"].splitlines()
        for idx, line in enumerate(lines, 1):
            if line.lstrip().startswith("//") or line.lstrip().startswith("*"):
                continue
            if "::" not in line or "(" not in line:
                continue
            if not looks_like_definition_line(line):
                continue
            m = QUALIFIED_DEF_RE.match(line)
            if not m:
                continue
            qual = m.group("qual")
            name = m.group("name")
            if qual.split("::", 1)[0] in IGNORED_QUAL_PREFIXES:
                continue
            if name.startswith("~") or name == "operator":
                continue
            short_qual = qual.split("::")[-1].split("<", 1)[0]
            if name == short_qual:  # constructor
                continue
            defs.append({"rel": f["rel"], "line": idx, "qual": qual, "name": name, "text": line.strip()})
    return defs

def build_reference_index(files):
    call_counts = defaultdict(int)
    samples = defaultdict(list)
    call_re = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(")
    for f in files:
        for idx, line in enumerate(f["scan"].splitlines(), 1):
            if "(" not in line:
                continue
            for match in call_re.finditer(line):
                name = match.group(1)
                call_counts[name] += 1
                if len(samples[name]) < 5:
                    samples[name].append((f["rel"], idx, line.strip()))
    return call_counts, samples

def classify_definition(defn):
    reasons = []
    name = defn["name"]
    rel = defn["rel"]
    qual = defn["qual"].split("::", 1)[0]
    if name in RISKY_EXACT_NAMES:
        reasons.append("serialization/init-style name")
    if name.startswith(RISKY_NAME_PREFIXES):
        reasons.append("common callback/virtual-style prefix")
    if name.endswith(RISKY_NAME_SUFFIXES):
        reasons.append("external/callback-style suffix")
    if qual.startswith(RISKY_QUAL_PREFIXES):
        reasons.append("UI/Python/save/replay-facing class")
    if RISKY_FILES_RE.search(rel):
        reasons.append("DLL/Python/save/replay-facing file")
    if "AI_" in name or "/AI" in rel or "AI" in rel:
        reasons.append("AI method/file; may be virtual or indirectly dispatched")
    return reasons

def analyze_definitions(files, max_items, include_risky):
    defs = collect_definitions(files)
    if max_items <= 0:
        return [], len(defs), 0
    call_counts, sample_index = build_reference_index(files)
    candidates = []
    suppressed_risky = 0
    for defn in defs:
        name_refs = call_counts.get(defn["name"], 0)
        samples = sample_index.get(defn["name"], [])
        risk = classify_definition(defn)
        # Usually definition + declaration gives 2 name refs. More than that means likely used.
        if name_refs <= 2:
            confidence = "medium" if not risk else "low/risky"
            if risk and not include_risky:
                suppressed_risky += 1
                continue
            candidates.append({
                "confidence": confidence,
                "defn": defn,
                "name_refs": name_refs,
                "risk": risk,
                "samples": samples,
            })
    candidates.sort(key=lambda c: (0 if c["confidence"] == "medium" else 1, c["name_refs"], c["defn"]["rel"], c["defn"]["line"]))
    return candidates[:max_items], len(defs), suppressed_risky

def find_disabled_macro_candidates(active_defines, commented_defines, conditional_uses):
    rows = []
    for macro in sorted(commented_defines):
        if macro in active_defines:
            continue
        uses = conditional_uses.get(macro, [])
        rows.append({
            "macro": macro,
            "commented": commented_defines[macro],
            "uses": uses,
        })
    rows.sort(key=lambda r: (-len(r["uses"]), r["macro"]))
    return rows


def normalize_token(text):
    return re.sub(r"[^a-z0-9]", "", text.lower())


def macro_tokens(macro):
    ignore = set(("enable", "test", "debug", "extra", "asserts", "log", "logging"))
    return [part.lower() for part in re.split(r"_+", macro.strip("_")) if part and part.lower() not in ignore]


def markdown_list_item(lines, text):
    # <!-- custom: Markdownlint MD032 wants standalone lists surrounded by blank lines; keep generated reports VS Code/markdownlint-friendly instead of relying on readers to ignore style warnings. (ChatGPT-5.5) -->
    if lines and lines[-1] != "" and not lines[-1].lstrip().startswith("- "):
        lines.append("")
    lines.append(text)


def normalize_markdown_lines(lines):
    # <!-- custom: Markdownlint MD012 wants at most one consecutive blank line; normalize generated reports once at write time. (ChatGPT-5.5) -->
    normalized = []
    blank_seen = False
    for line in lines:
        if line.strip() == "":
            if not blank_seen:
                normalized.append("")
            blank_seen = True
        else:
            normalized.append(line.rstrip())
            blank_seen = False
    while normalized and normalized[-1] == "":
        normalized.pop()
    return normalized


def find_same_name_unguarded_sources(files, disabled_macros):
    rows = []
    rel_to_text = {f["rel"]: f["text"] for f in files}
    for row in disabled_macros:
        macro = row["macro"]
        if "EXTRA_ASSERTS" in macro:
            continue
        tokens = macro_tokens(macro)
        if not tokens:
            continue
        for f in files:
            if not f["rel"].endswith(".cpp"):
                continue
            basename = normalize_token(Path(f["rel"]).stem)
            # Avoid broad false positives like AGENT_ITERATOR_TEST matching AgentIterator.cpp;
            # prefer explicit test/repro implementation filenames.
            if "test" not in basename and "repro" not in basename:
                continue
            if not all(token in basename for token in tokens):
                continue
            text = rel_to_text[f["rel"]]
            if macro not in text:
                rows.append({"macro": macro, "rel": f["rel"], "reason": "source filename matches disabled macro tokens but file does not mention the macro"})
    return rows


def write_report(root, files, output_path, args):
    active_defines, commented_defines, conditional_uses, if0_blocks = collect_macro_info(files)
    disabled_macros = find_disabled_macro_candidates(active_defines, commented_defines, conditional_uses)
    unguarded_sources = find_same_name_unguarded_sources(files, disabled_macros)
    max_unreferenced = 0 if args.focus == "high" else args.max_unreferenced
    candidates, definition_count, suppressed_risky = analyze_definitions(files, max_unreferenced, args.include_risky_low_reference)

    lines = []
    now = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M:%SZ")
    lines.append("# C++ dead-code candidate report")
    lines.append("")
    lines.append("Generated: %s" % now)
    lines.append("")
    lines.append("Root: `%s`" % root)
    lines.append("")
    lines.append("Source dirs: `%s`" % "`, `".join(args.source_dir))
    lines.append("")
    lines.append("## Important warning")
    lines.append("")
    lines.append("This is a conservative candidate finder for LLM/manual review, not a proof of dead code. Civ4 DLL code can be reached by virtual dispatch, EXE callbacks, Python/Cy exports, serialization, macros, and debug-only paths. Do not delete or compile out anything based only on this report.")
    lines.append("")
    lines.append("## Summary")
    lines.append("")
    markdown_list_item(lines, "- Source files scanned: %d" % len(files))
    markdown_list_item(lines, "- Qualified C++ definitions scanned: %d" % definition_count)
    markdown_list_item(lines, "- Report focus: `%s`" % args.focus)
    markdown_list_item(lines, "- Disabled/commented macro candidates: %d" % len(disabled_macros))
    markdown_list_item(lines, "- Same-name unguarded source candidates: %d" % len(unguarded_sources))
    markdown_list_item(lines, "- `#if 0` blocks found: %d" % len(if0_blocks))
    markdown_list_item(lines, "- Low-reference function candidates shown: %d" % len(candidates))
    markdown_list_item(lines, "- Risky low-reference candidates suppressed: %d" % suppressed_risky)
    lines.append("")

    lines.append("## Highest-value macro/test candidates")
    lines.append("")
    if disabled_macros:
        lines.append("These macros have commented-out `#define` lines and no active `#define` found in the scanned source. They are good review targets for code that should perhaps be fully compile-gated when disabled.")
        lines.append("")
        for row in disabled_macros[:40]:
            lines.append("### `%s`" % row["macro"])
            lines.append("")
            lines.append("Commented define:")
            for rel, line, text in row["commented"][:5]:
                markdown_list_item(lines, "- `%s:%d`: `%s`" % (rel, line, text.replace("`", "'")))
            if row["uses"]:
                lines.append("")
                lines.append("Conditional uses:")
                for rel, line, text in row["uses"][:8]:
                    markdown_list_item(lines, "- `%s:%d`: `%s`" % (rel, line, text.replace("`", "'")))
            else:
                lines.append("")
                lines.append("No conditional uses found; the commented define may be stale.")
            lines.append("")
    else:
        lines.append("No disabled/commented macro candidates found.")
        lines.append("")

    lines.append("## Same-name unguarded source candidates")
    lines.append("")
    lines.append("These are heuristic hits where a disabled macro looks related to a `.cpp` filename, but that `.cpp` file does not mention the macro. This can identify test/debug implementations that still compile even when their feature macro is disabled.")
    lines.append("")
    if unguarded_sources:
        for row in unguarded_sources[:40]:
            markdown_list_item(lines, "- `%s`: related disabled macro `%s`; %s" % (row["rel"], row["macro"], row["reason"]))
        lines.append("")
    else:
        lines.append("No same-name unguarded source candidates found.")
        lines.append("")

    lines.append("## Existing `#if 0` blocks")
    lines.append("")
    if if0_blocks:
        lines.append("These are already excluded from compilation. Large blocks are listed first for context; they are not new action items by themselves.")
        lines.append("")
        for rel, start, end, count in sorted(if0_blocks, key=lambda x: (-x[3], x[0], x[1]))[:80]:
            markdown_list_item(lines, "- `%s:%d-%d` (%d lines)" % (rel, start, end, count))
        lines.append("")
    else:
        lines.append("No `#if 0` blocks found.")
        lines.append("")

    lines.append("## Low-reference qualified function candidates")
    lines.append("")
    lines.append("These definitions have at most two broad `name(` references across scanned C++ files, usually meaning definition plus declaration only. False positives are common, especially for virtual methods, callbacks, and serialization. By default, risky/noisy low-reference hits are suppressed; pass `--include-risky-low-reference` to show them, or `--focus high` to skip this section entirely.")
    lines.append("")
    if args.focus == "high":
        lines.append("Skipped because `--focus high` was used.")
        lines.append("")
    elif candidates:
        for c in candidates:
            d = c["defn"]
            lines.append("### `%s::%s` (`%s:%d`)" % (d["qual"], d["name"], d["rel"], d["line"]))
            lines.append("")
            markdown_list_item(lines, "- Definition: `%s:%d`" % (d["rel"], d["line"]))
            markdown_list_item(lines, "- Broad call-like references: %d" % c["name_refs"])
            markdown_list_item(lines, "- Review confidence: %s" % c["confidence"])
            if c["risk"]:
                markdown_list_item(lines, "- Risk flags: %s" % "; ".join(c["risk"]))
            markdown_list_item(lines, "- Definition line: `%s`" % d["text"].replace("`", "'"))
            if c["samples"]:
                markdown_list_item(lines, "- Sample references:")
                for rel, line, text in c["samples"]:
                    lines.append("  - `%s:%d`: `%s`" % (rel, line, text.replace("`", "'")))
            lines.append("")
    else:
        lines.append("No low-reference qualified function candidates found.")
        lines.append("")

    lines.append("## Suggested LLM review prompt")
    lines.append("")
    lines.append("> Review this report as a candidate list only. For each candidate, decide whether it is truly unreachable, merely test/debug-only, virtual/callback/serialization-risky, or worth leaving alone. Prefer compile-gating disabled test infrastructure over deleting inherited code. Do not remove functions that may be called by the EXE, Python/Cy exports, virtual dispatch, or save/replay serialization.")
    lines.append("")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    lines = normalize_markdown_lines(lines)
    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return len(candidates), len(disabled_macros), len(if0_blocks)


def default_output_path(root):
    ts = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    return root / DEFAULT_OUTPUT_DIR / ("cpp_dead_code_candidates_%s.md" % ts)


def parse_args():
    parser = argparse.ArgumentParser(description="Find conservative C++ dead-code candidates for LLM/manual review. Does not edit files.")
    parser.add_argument("--root", default=".", help="Repository root. Default: current directory.")
    parser.add_argument("--source-dir", action="append", default=None, help="Source directory to scan. Can be repeated. Default: CvGameCoreDLL.")
    parser.add_argument("--output", default=None, help="Markdown output path. Default: LLM_Helpers/outputs/cpp_dead_code_candidates_<timestamp>.md")
    parser.add_argument("--max-unreferenced", type=int, default=80, help="Maximum low-reference function candidates to show. Default: 80.")
    parser.add_argument("--include-risky-low-reference", action="store_true", help="Also show low-reference candidates flagged as risky/noisy. Default: suppress them.")
    parser.add_argument("--focus", choices=("all", "high"), default="all", help="Report focus. Use high for only macro/test and #if 0 sections; default: all.")
    return parser.parse_args()


def main():
    args = parse_args()
    root = Path(args.root).resolve()
    if args.source_dir is None:
        args.source_dir = list(DEFAULT_SOURCE_DIRS)
    output_path = Path(args.output).resolve() if args.output else default_output_path(root)
    files = collect_files(root, args.source_dir)
    if not files:
        raise SystemExit("No C++ source files found under: %s" % ", ".join(args.source_dir))
    func_count, macro_count, if0_count = write_report(root, files, output_path, args)
    print("Wrote %s" % output_path)
    print("Reported %d low-reference function candidates, %d disabled macro candidates, %d #if 0 blocks." % (func_count, macro_count, if0_count))


if __name__ == "__main__":
    main()
