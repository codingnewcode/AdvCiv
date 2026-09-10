#!/usr/bin/env python3
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
# Targeted second-pass multiline collapse helper.
#
# This is a separate follow-up to collapse_multiline_calls.py.
# Keep collapse_multiline_calls.py as the committed known-good pass 1.
#
# This pass 2 helper keeps the same basic safety idea:
# - collapse only selected multiline logical statements;
# - preserve line endings;
# - refuse to write if significant token sequence changes.
#
# Extra pass 2 targets:
# - multiline def headers, e.g. def __init__(..., ...):
# - selected long map-generation calls, e.g. generatePlotsInRegion(...)
#
# Deliberate skips:
# - if/elif/while/for/with/try/except headers;
# - comments inside the collapsed statement;
# - multiline strings;
# - explicit backslash continuations;
# - list/dict/table assignments;
# - calls whose first argument is a list/dict literal, e.g. foo([...]) or foo({...});
# - long boolean return cascades beyond the normal conservative line cap.
#
# Review git diff after use. This is not a general formatter.

from __future__ import print_function

import argparse
import difflib
import io
import re
import sys
import tokenize
from pathlib import Path

DEFAULT_MAX_LINE_LEN = 380
DEFAULT_DEF_HEADER_MAX_LINE_LEN = 900
DEFAULT_LONG_CALL_MAX_LINE_LEN = 1200
DEFAULT_LONG_CALL_NAMES = "generatePlotsInRegion,generateCenter,generateCenterPlot"

HEADER_KEYWORDS = ("def ", "class ", "if ", "elif ", "while ", "for ", "with ", "try:", "except", "finally:", "else:")

CALL_PREFIX_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_\.]*$")
ASSIGN_CALL_PREFIX_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_\.]*\s*=\s*[A-Za-z_][A-Za-z0-9_\.]*$")
RETURN_CALL_PREFIX_RE = re.compile(r"^return\s+[A-Za-z_][A-Za-z0-9_\.]*$")
DEF_HEADER_PREFIX_RE = re.compile(r"^def\s+[A-Za-z_][A-Za-z0-9_]*$")

def decode_bytes(raw):
    if raw.startswith(b"\xef\xbb\xbf"):
        return "utf-8-sig", raw.decode("utf-8-sig")
    try:
        return "utf-8", raw.decode("utf-8")
    except UnicodeDecodeError:
        return "latin-1", raw.decode("latin-1")

def line_ending_from_bytes(raw):
    return "\r\n" if b"\r\n" in raw else "\n"

def leading_indent(s):
    return s[:len(s) - len(s.lstrip(" \t"))]

def strip_line_ending(s):
    return s.rstrip("\r\n")

def parse_name_set(value):
    if not value:
        return set()
    return set(part.strip() for part in value.split(",") if part.strip())

def join_parts(parts):
    body = ""
    for part in parts:
        if not body:
            body = part
            continue
        left = body.rstrip()
        right = part.lstrip()
        if left.endswith(("(", "[", "{")):
            body += right
        elif right.startswith((")", "]", "}", ":", ",")):
            body += right
        else:
            body += " " + right
    return body

def significant_tokens_from_text(text):
    # Tokenize does not require Python 3 parsing. It is usually enough for
    # Python 2.4-era code where ast.parse/py_compile would fail on old syntax.
    result = []
    reader = io.BytesIO(text.encode("utf-8", errors="surrogatepass")).readline
    for tok in tokenize.tokenize(reader):
        if tok.type in (tokenize.ENCODING, tokenize.ENDMARKER, tokenize.NL, tokenize.NEWLINE,):
            continue
        result.append((tok.type, tok.string))
    return result

def first_opener(text):
    in_str = None
    triple = None
    i = 0
    while i < len(text):
        c = text[i]
        if in_str:
            if c == "\\":
                i += 2
                continue
            if triple:
                if text[i:i + 3] == triple:
                    in_str = None
                    triple = None
                    i += 3
                    continue
            elif c == in_str:
                in_str = None
            i += 1
            continue
        if c in ("'", '"'):
            if text[i:i + 3] in ("'''", "\"\"\""):
                in_str = c
                triple = text[i:i + 3]
                i += 3
                continue
            in_str = c
            i += 1
            continue
        if c in "([{":
            return c, i
        i += 1
    return None, -1

def prefix_call_name(prefix):
    # For assignment-call prefixes such as "x = Foo", use "Foo".
    if "=" in prefix:
        prefix = prefix.rsplit("=", 1)[1].strip()
    if prefix.startswith("return "):
        prefix = prefix[len("return "):].strip()
    return prefix

def call_name_matches(call_name, allowed_names):
    if not call_name or not allowed_names:
        return False
    final_part = call_name.rsplit(".", 1)[-1]
    return call_name in allowed_names or final_part in allowed_names

def is_def_header_statement(stripped):
    opener, idx = first_opener(stripped)
    if opener != "(" or idx < 1:
        return False
    prefix = stripped[:idx].strip()
    return DEF_HEADER_PREFIX_RE.match(prefix) is not None and stripped.rstrip().endswith(":")

def starts_with_container_argument(stripped):
    opener, idx = first_opener(stripped)
    if opener != "(" or idx < 1:
        return False
    i = idx + 1
    while i < len(stripped) and stripped[i] in " \t":
        i += 1
    return i < len(stripped) and stripped[i] in "[{"

def classify_statement(joined_body, long_call_names):
    stripped = joined_body.strip()
    if not stripped:
        return None

    if is_def_header_statement(stripped):
        return "def_header"

    if stripped.startswith(HEADER_KEYWORDS):
        return None
    if stripped.startswith(("import ", "from ", "global ", "assert ")):
        return None

    opener, idx = first_opener(stripped)
    if opener != "(" or idx < 1:
        return None

    prefix = stripped[:idx].strip()
    call_name = prefix_call_name(prefix)

    if CALL_PREFIX_RE.match(prefix):
        if call_name_matches(call_name, long_call_names):
            return "long_named_call"
        return "normal_call"

    if RETURN_CALL_PREFIX_RE.match(prefix):
        return "return_expr"

    if ASSIGN_CALL_PREFIX_RE.match(prefix):
        if call_name_matches(call_name, long_call_names):
            return "long_named_call"
        return "normal_call"

    return None

def max_len_for_kind(kind, max_line_len, def_header_max_line_len, long_call_max_line_len):
    if kind == "def_header":
        return def_header_max_line_len
    if kind == "long_named_call":
        return long_call_max_line_len
    return max_line_len

def should_skip_group(lines, start, end, toks, joined_body, max_line_len, def_header_max_line_len, long_call_names, long_call_max_line_len):
    if end <= start:
        return "single_line"
    if any(t.type == tokenize.COMMENT for t in toks):
        return "comment"
    if any(t.type == tokenize.STRING and t.start[0] != t.end[0] for t in toks):
        return "multiline_string"
    if any(lines[i - 1].rstrip("\r\n").rstrip().endswith("\\") for i in range(start, end)):
        return "backslash"

    stripped_start = lines[start - 1].strip()

    # Large data containers are usually clearer and safer one item per line.
    if ("= [" in stripped_start or "= {" in stripped_start or stripped_start.endswith(("= [", "= {"))):
        return "list_or_dict_assignment"

    kind = classify_statement(joined_body, long_call_names)
    if kind is None:
        return "not_call_like"

    # Avoid collapsing structured row/list blocks such as aSupport.extend([...]).
    if kind != "def_header" and starts_with_container_argument(joined_body.strip()):
        return "container_argument"

    limit = max_len_for_kind(kind, max_line_len, def_header_max_line_len, long_call_max_line_len)
    if len(leading_indent(lines[start - 1]) + joined_body) > limit:
        return "too_long"

    return None

def collect_logical_groups(text):
    groups = []
    cur = []
    reader = io.BytesIO(text.encode("utf-8", errors="surrogatepass")).readline
    for tok in tokenize.tokenize(reader):
        if tok.type in (tokenize.ENCODING, tokenize.ENDMARKER):
            continue
        if not cur and tok.type in (tokenize.NL, tokenize.COMMENT, tokenize.INDENT, tokenize.DEDENT):
            continue
        cur.append(tok)
        if tok.type == tokenize.NEWLINE:
            if cur:
                groups.append((cur[0].start[0], cur[-1].end[0], cur[:]))
            cur = []
    return groups

def transform_text(text, newline, max_line_len, def_header_max_line_len, long_call_names, long_call_max_line_len):
    lines = text.splitlines(True)
    groups = collect_logical_groups(text)
    candidate_ranges = {}
    skipped = {
        "single_line": 0,
        "comment": 0,
        "multiline_string": 0,
        "backslash": 0,
        "list_or_dict_assignment": 0,
        "container_argument": 0,
        "not_call_like": 0,
        "too_long": 0,
    }
    changed = 0
    removed_lines = 0

    for start, end, toks in groups:
        parts = []
        for line_no in range(start, end + 1):
            part = strip_line_ending(lines[line_no - 1]).strip()
            if part:
                parts.append(part)
        joined_body = join_parts(parts)

        skip_reason = should_skip_group(lines, start, end, toks, joined_body, max_line_len, def_header_max_line_len, long_call_names, long_call_max_line_len,)
        if skip_reason is not None:
            skipped[skip_reason] = skipped.get(skip_reason, 0) + 1
            continue

        indent = leading_indent(lines[start - 1])
        replacement = indent + joined_body + newline
        candidate_ranges[start] = (end, replacement)
        changed += 1
        removed_lines += end - start

    out = []
    i = 1
    n = len(lines)
    while i <= n:
        if i in candidate_ranges:
            end, replacement = candidate_ranges[i]
            out.append(replacement)
            i = end + 1
        else:
            out.append(lines[i - 1])
            i += 1

    return "".join(out), changed, removed_lines, skipped

def main(argv):
    parser = argparse.ArgumentParser(description="Targeted second-pass multiline collapse helper.")
    parser.add_argument("path", help="Python source file to process")
    parser.add_argument("--in-place", action="store_true", help="rewrite the input file")
    parser.add_argument("--output", help="write transformed text to this path")
    parser.add_argument("--diff", action="store_true", help="print unified diff")
    parser.add_argument("--max-line-len", type=int, default=DEFAULT_MAX_LINE_LEN)
    parser.add_argument("--def-header-max-line-len", type=int, default=DEFAULT_DEF_HEADER_MAX_LINE_LEN)
    parser.add_argument("--long-call-names", default=DEFAULT_LONG_CALL_NAMES)
    parser.add_argument("--long-call-max-line-len", type=int, default=DEFAULT_LONG_CALL_MAX_LINE_LEN)
    args = parser.parse_args(argv)

    src_path = Path(args.path)
    raw = src_path.read_bytes()
    encoding, text = decode_bytes(raw)
    newline = line_ending_from_bytes(raw)

    long_call_names = parse_name_set(args.long_call_names)

    new_text, changed, removed_lines, skipped = transform_text(text, newline, args.max_line_len, args.def_header_max_line_len, long_call_names, args.long_call_max_line_len,)

    if changed:
        orig_tokens = significant_tokens_from_text(text)
        new_tokens = significant_tokens_from_text(new_text)
        if orig_tokens != new_tokens:
            print("ERROR: significant token sequence changed; refusing to write.", file=sys.stderr)
            for idx, pair in enumerate(zip(orig_tokens, new_tokens)):
                if pair[0] != pair[1]:
                    print("first_token_difference_at=%d orig=%r new=%r" % (idx, pair[0], pair[1]), file=sys.stderr)
                    break
            return 1

    if args.diff:
        diff = difflib.unified_diff(text.splitlines(True), new_text.splitlines(True), fromfile=str(src_path), tofile=str(src_path) + ".collapsed2",)
        sys.stdout.write("".join(diff))

    write_encoding = encoding if encoding != "utf-8-sig" else "utf-8"
    if args.output:
        Path(args.output).write_bytes(new_text.encode(write_encoding))
    if args.in_place:
        src_path.write_bytes(new_text.encode(write_encoding))

    print("changed_logical_statements=%d" % changed)
    print("removed_physical_lines=%d" % removed_lines)
    print("max_line_len=%d" % args.max_line_len)
    print("def_header_max_line_len=%d" % args.def_header_max_line_len)
    print("long_call_names=%s" % ",".join(sorted(long_call_names)))
    print("long_call_max_line_len=%d" % args.long_call_max_line_len)
    print("skipped=%s" % skipped)
    if not args.in_place and not args.output:
        print("dry_run_only=true")
    return 0

if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
