#!/usr/bin/env python3
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
# Static drift check for getDefineBOOL/FLOAT/INT/STRING keys used anywhere
# in the mod (Python + DLL) against every place defines can be declared
# (mod XML + base BTS XML + vanilla Civ4 XML + any extras).
#
# Variants covered: getDefineBOOL, getDefineFLOAT, getDefineINT, getDefineSTRING.
# All use <DefineName> in XML regardless of value type. (DLL also has
# getDefineINTExternal, which is a method definition, not a call site -- the
# regex requires an immediate '(' so it does not match those.)
#
# Civ4 getDefine* return type-default (0 / 0.0 / false / "") for both missing
# AND legitimately zero/empty keys, so a runtime fail-loud wrapper has no
# clean sentinel (unlike getInfoTypeForString which has -1).
# SASDefineGuard.verify_or_raise covers the wholesale "defines didn't load"
# case at launch; this script covers the remaining "specific key got
# renamed/removed" drift case statically.
#
# Behavior:
#   - Greps literal getDefine{BOOL,FLOAT,INT,STRING}("X") in
#     Assets/Python/**/*.py AND CvGameCoreDLL/**/*.{cpp,h}.
#   - Checks each "X" against <DefineName> tags from:
#       mod      Assets/XML/*.xml
#       BTS      <bts-xml-dir>/*.xml   (auto: ../../Assets/XML/ from mod root)
#       vanilla  <vanilla-xml-dir>/*.xml (auto: ../../../Assets/XML/)
#       extras   --extra-xml <path> (repeatable; file or dir)
#   - Variable-arg calls (getDefineINT(szVar)) are counted but not audited.
#   - Exit 1 if drift found, 0 otherwise.
#   - Writes a timestamped report to LLM_Helpers/outputs/ unless --no-output-file.

import argparse
import os
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

DEFINE_BLOCK_RE = re.compile(r"<Define>(.*?)</Define>", re.DOTALL)
DEFINE_NAME_RE = re.compile(r"<DefineName>([A-Z0-9_]+)</DefineName>")
LITERAL_CALL_RE = re.compile(r'getDefine(?:BOOL|FLOAT|INT|STRING)\(\s*"([A-Z0-9_]+)"\s*\)')
VARIABLE_CALL_RE = re.compile(r'getDefine(?:BOOL|FLOAT|INT|STRING)\(\s*[^"\s]')

def extract_defined_keys(xml_paths):
    """Return (set of declared keys, list of files that contributed at least one)."""
    keys = set()
    contributing = []
    for path in xml_paths:
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            print(f"warn: cannot read {path}: {exc}", file=sys.stderr)
            continue
        # <!-- custom: require <DefineName> to be inside a <Define>...</Define> block so a stray <DefineName>FOO</DefineName> dropped into an unrelated XML (e.g. CIV4UnitInfos.xml) is not falsely treated as declared. The Civ4 engine only loads defines from <Define> blocks anyway. Caveat: <DefineName> inside an XML comment <!-- ... --> is still counted; zero such cases exist across current mod+BTS+vanilla XML. (Claude code Opus 4.7) -->
        found = []
        for block in DEFINE_BLOCK_RE.findall(text):
            found.extend(DEFINE_NAME_RE.findall(block))
        if found:
            keys.update(found)
            contributing.append(path)
    return keys, contributing

def collect_xml_files(path):
    """Accept a file or a dir; return list of *.xml files found."""
    if path is None:
        return []
    p = Path(path)
    if not p.exists():
        return []
    if p.is_file():
        return [p]
    return sorted(p.rglob("*.xml"))

def scan_sources(roots_and_patterns):
    """roots_and_patterns: list of (root_path, glob_pattern) tuples.
    Returns (usage dict[key -> list of source files], variable_call_count)."""
    usage = {}
    variable_calls = 0
    for root, pattern in roots_and_patterns:
        if not root.is_dir():
            continue
        for src_path in root.rglob(pattern):
            try:
                text = src_path.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            for key in LITERAL_CALL_RE.findall(text):
                usage.setdefault(key, []).append(src_path)
            variable_calls += len(VARIABLE_CALL_RE.findall(text))
    return usage, variable_calls

def _rel(path, anchor):
    try:
        return str(Path(path).relative_to(anchor))
    except ValueError:
        return str(path)

def build_report(stats, xml_groups, missing_by_source, usage, mod_root, show_callsites):
    """xml_groups: list of (label, count, [Path, ...]) tuples."""
    lines = []
    for label, count, files in xml_groups:
        lines.append(f"{label:30s} {count}")
        for f in files:
            lines.append(f"  - {_rel(f, mod_root)}")
    for label, value in stats:
        lines.append(f"{label:30s} {value}")
    lines.append("")
    if not missing_by_source:
        lines.append("OK: every literal getDefine{BOOL,FLOAT,INT,STRING} key is declared somewhere.")
        return "\n".join(lines) + "\n"
    total = sum(len(keys) for keys in missing_by_source.values())
    lines.append(f"DRIFT: {total} key(s) used but not declared in any scanned XML:")
    for source_label, keys in missing_by_source.items():
        if not keys:
            continue
        lines.append(f"  [{source_label}]")
        for key in keys:
            lines.append(f"    {key}")
            if show_callsites:
                for p in usage[key]:
                    try:
                        rel = p.relative_to(mod_root)
                    except ValueError:
                        rel = p
                    lines.append(f"      - {rel}")
    return "\n".join(lines) + "\n"

def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--mod-root", type=Path, default=Path.cwd(), help="mod root (default: CWD)")
    parser.add_argument("--bts-xml-dir", type=Path, default=None, help="dir holding base BTS *.xml (default: <mod-root>/../../Assets/XML/)")
    parser.add_argument("--vanilla-xml-dir", type=Path, default=None, help="dir holding vanilla Civ4 *.xml (default: <mod-root>/../../../Assets/XML/)")
    parser.add_argument("--dll-root", type=Path, default=None, help="dir to scan for .cpp/.h literal calls (default: <mod-root>/CvGameCoreDLL/)")
    parser.add_argument("--extra-xml", action="append", default=[], help="additional XML file or dir to treat as authoritative; repeatable")
    parser.add_argument("--no-dll", action="store_true", help="skip DLL scan")
    parser.add_argument("--show-callsites", action="store_true", help="list files that use each missing key")
    parser.add_argument("--output-dir", default=os.path.join("LLM_Helpers", "outputs"), help="dir to write timestamped report (default: LLM_Helpers/outputs)")
    parser.add_argument("--no-output-file", action="store_true", help="print to stdout only; do not write a report file")
    args = parser.parse_args()

    mod_root = args.mod_root.resolve()
    mod_xml_dir = mod_root / "Assets" / "XML"
    py_root = mod_root / "Assets" / "Python"
    dll_root = (args.dll_root or (mod_root / "CvGameCoreDLL")).resolve()
    bts_xml_dir = (args.bts_xml_dir or (mod_root / ".." / ".." / "Assets" / "XML")).resolve()
    vanilla_xml_dir = (args.vanilla_xml_dir or (mod_root / ".." / ".." / ".." / "Assets" / "XML")).resolve()

    if not mod_xml_dir.is_dir() or not py_root.is_dir():
        print(f"error: expected {mod_xml_dir} and {py_root} to exist", file=sys.stderr)
        return 2

    mod_xml_files = sorted(mod_xml_dir.rglob("*.xml"))
    bts_xml_files = collect_xml_files(bts_xml_dir)
    vanilla_xml_files = collect_xml_files(vanilla_xml_dir)
    extra_xml_files = []
    for entry in args.extra_xml:
        extra_xml_files.extend(collect_xml_files(entry))

    defined_mod, mod_contrib = extract_defined_keys(mod_xml_files)
    defined_bts, bts_contrib = extract_defined_keys(bts_xml_files)
    defined_vanilla, vanilla_contrib = extract_defined_keys(vanilla_xml_files)
    defined_extra, extra_contrib = extract_defined_keys(extra_xml_files)
    defined_all = defined_mod | defined_bts | defined_vanilla | defined_extra

    if not defined_bts:
        print(f"warn: no BTS *.xml found at {bts_xml_dir}", file=sys.stderr)
    if not defined_vanilla:
        print(f"warn: no vanilla *.xml found at {vanilla_xml_dir}", file=sys.stderr)

    source_specs = [("python", py_root, "*.py")]
    if not args.no_dll:
        source_specs.append(("dll_cpp", dll_root, "*.cpp"))
        source_specs.append(("dll_h", dll_root, "*.h"))

    usage_total = {}
    variable_total = 0
    usage_by_source = {label: ({}, 0) for label, _, _ in source_specs}
    for label, root, pattern in source_specs:
        usage, var_count = scan_sources([(root, pattern)])
        usage_by_source[label] = (usage, var_count)
        for key, paths in usage.items():
            usage_total.setdefault(key, []).extend(paths)
        variable_total += var_count

    missing_by_source = {}
    for label, (usage, _) in usage_by_source.items():
        keys = sorted(k for k in usage if k not in defined_all)
        if keys:
            missing_by_source[label] = keys

    xml_groups = [
        (f"mod XML files (scanned {len(mod_xml_files)}, contributed):", len(mod_contrib), mod_contrib),
        (f"BTS XML files (scanned {len(bts_xml_files)}, contributed):", len(bts_contrib), bts_contrib),
        (f"vanilla XML files (scanned {len(vanilla_xml_files)}, contributed):", len(vanilla_contrib), vanilla_contrib),
        (f"extra XML files (scanned {len(extra_xml_files)}, contributed):", len(extra_contrib), extra_contrib),
    ]
    stats = [
        ("mod XML defines:", len(defined_mod)),
        ("BTS XML defines:", len(defined_bts)),
        ("vanilla XML defines:", len(defined_vanilla)),
        ("extra XML defines:", len(defined_extra)),
        ("distinct literal keys (py):", len(usage_by_source.get("python", ({}, 0))[0])),
        ("distinct literal keys (dll):", len(usage_by_source.get("dll_cpp", ({}, 0))[0]) + len(usage_by_source.get("dll_h", ({}, 0))[0])),
        ("variable-arg calls (skipped):", variable_total),
    ]

    report = build_report(stats, xml_groups, missing_by_source, usage_total, mod_root, args.show_callsites)
    sys.stdout.write(report)

    if not args.no_output_file:
        out_dir = (mod_root / args.output_dir).resolve()
        out_dir.mkdir(parents=True, exist_ok=True)
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        status = "drift" if missing_by_source else "ok"
        out_path = out_dir / f"{stamp}_audit_define_keys_{status}.txt"
        out_path.write_text(report, encoding="utf-8")
        try:
            rel = out_path.relative_to(mod_root)
        except ValueError:
            rel = out_path
        print(f"wrote {rel}")

    return 1 if missing_by_source else 0

if __name__ == "__main__":
    sys.exit(main())
