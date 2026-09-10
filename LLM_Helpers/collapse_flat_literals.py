#!/usr/bin/env python3
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
# Conservative flat-literal collapse helper.
#
# Goal:
# - Collapse simple flat multiline Python list/tuple literals to one physical
#   line for grepability and to remove continuation-indent lint noise.
# - Leave matrix-like/data-table structures alone.
#
# Intended examples:
#   names = ["A",
#            "B",
#            "C"]
# becomes:
#   names = ["A", "B", "C"]
#
#   region_data[thisRegion] = [iWestX, iEastX,
#                              iSouthY, iNorthY]
# becomes:
#   region_data[thisRegion] = [iWestX, iEastX, iSouthY, iNorthY]
#
# Not intended:
# - nested lists/dicts/tuples, e.g. matrices or map templates;
# - blocks containing comments;
# - blocks with nested calls/containers;
# - dict literals;
# - long collapses beyond --max-line-len.
#
# Default max line length is intentionally fairly high because this pass
# targets grep-friendly flat data arrays, not normal prose/code wrapping. Use a
# lower --max-line-len for stricter diffs.
#
# This is not a formatter. Review the diff before committing.

from __future__ import print_function

import argparse
import difflib
import io
import os
import re
import sys
import tokenize
from pathlib import Path

DEFAULT_MAX_LINE_LEN = 500

ASSIGN_OPEN_RE = re.compile(
    r"^(?P<prefix>\s*.+(?<![<>=!])=(?![=])\s*)(?P<open>[\[\(])(?P<rest>.*)$"
)
OPEN_TO_CLOSE = {"[": "]", "(": ")"}
TOKEN_SKIP_TYPES = set([
    tokenize.NL,
    tokenize.NEWLINE,
    tokenize.INDENT,
    tokenize.DEDENT,
    tokenize.ENDMARKER,
])


def decode_bytes(raw):
    if raw.startswith(b"\xef\xbb\xbf"):
        return "utf-8-sig", raw.decode("utf-8-sig")
    try:
        return "utf-8", raw.decode("utf-8")
    except UnicodeDecodeError:
        return "latin-1", raw.decode("latin-1")


def split_line_ending(line):
    if line.endswith("\r\n"):
        return line[:-2], "\r\n"
    if line.endswith("\n"):
        return line[:-1], "\n"
    if line.endswith("\r"):
        return line[:-1], "\r"
    return line, ""


def significant_tokens(text):
    try:
        tokens = tokenize.generate_tokens(io.StringIO(text).readline)
        return [
            (tok.type, tok.string)
            for tok in tokens
            if tok.type not in TOKEN_SKIP_TYPES and tok.type != tokenize.COMMENT
        ]
    except Exception:
        return None


def has_unquoted_comment(text):
    quote = None
    triple = False
    i = 0

    while i < len(text):
        ch = text[i]

        if quote is not None:
            if triple:
                if text.startswith(quote * 3, i):
                    i += 3
                    quote = None
                    triple = False
                    continue
            elif ch == "\\":
                i += 2
                continue
            elif ch == quote:
                quote = None
            i += 1
            continue

        if ch == "#":
            return True

        if ch in ("'", '"'):
            quote = ch
            if text.startswith(ch * 3, i):
                triple = True
                i += 3
                continue
            i += 1
            continue

        i += 1

    return False


def scan_flat_literal(lines, start_index, prefix, open_char, rest_after_open, max_line_len):
    close_char = OPEN_TO_CLOSE[open_char]
    pieces = []
    quote = None
    triple = False
    nested = False
    closed = False
    close_index = None

    line_index = start_index
    while line_index < len(lines):
        body, _eol = split_line_ending(lines[line_index])
        scan_text = rest_after_open if line_index == start_index else body
        piece_chars = []
        i = 0

        while i < len(scan_text):
            ch = scan_text[i]

            if quote is not None:
                piece_chars.append(ch)
                if triple:
                    if scan_text.startswith(quote * 3, i):
                        piece_chars.extend([quote, quote])
                        i += 3
                        quote = None
                        triple = False
                        continue
                elif ch == "\\":
                    if i + 1 < len(scan_text):
                        piece_chars.append(scan_text[i + 1])
                    i += 2
                    continue
                elif ch == quote:
                    quote = None
                i += 1
                continue

            if ch == "#":
                return None

            if ch in ("'", '"'):
                quote = ch
                piece_chars.append(ch)
                if scan_text.startswith(ch * 3, i):
                    triple = True
                    piece_chars.extend([ch, ch])
                    i += 3
                    continue
                i += 1
                continue

            if ch in "[({":
                nested = True
                return None

            if ch in "])}":
                if ch != close_char:
                    return None

                after_close = scan_text[i + 1:]
                if has_unquoted_comment(after_close) or after_close.strip():
                    return None

                closed = True
                close_index = line_index
                break

            piece_chars.append(ch)
            i += 1

        pieces.append("".join(piece_chars).strip())

        if closed:
            break

        if quote is not None and not triple:
            # A single-quoted string cannot safely continue across lines here.
            return None

        line_index += 1

    if not closed or nested or close_index is None:
        return None

    span = lines[start_index:close_index + 1]
    if any(not split_line_ending(line)[0].strip() for line in span[1:-1]):
        return None

    body = " ".join(piece for piece in pieces if piece)
    collapsed = prefix + open_char + body + close_char

    if len(collapsed) > max_line_len:
        return None

    return close_index, collapsed


def find_candidates(text, max_line_len):
    lines = text.splitlines(True)
    candidates = []
    index = 0

    while index < len(lines):
        body, _eol = split_line_ending(lines[index])
        match = ASSIGN_OPEN_RE.match(body)
        if match is None:
            index += 1
            continue

        if has_unquoted_comment(body[:match.start("open")]):
            index += 1
            continue

        prefix = match.group("prefix")
        open_char = match.group("open")
        rest_after_open = match.group("rest")

        result = scan_flat_literal(
            lines,
            index,
            prefix,
            open_char,
            rest_after_open,
            max_line_len,
        )

        if result is None:
            index += 1
            continue

        close_index, collapsed = result
        if close_index == index:
            index += 1
            continue

        candidates.append((index, close_index, collapsed))
        index = close_index + 1

    return candidates


def apply_candidates(text, candidates):
    if not candidates:
        return text

    lines = text.splitlines(True)
    for start, end, collapsed in reversed(candidates):
        _body, eol = split_line_ending(lines[start])
        lines[start:end + 1] = [collapsed + eol]

    return "".join(lines)


def transform_text(text, max_line_len):
    candidates = find_candidates(text, max_line_len)
    return apply_candidates(text, candidates), candidates


def expand_paths(paths, recursive):
    expanded = []
    for name in paths:
        path = Path(name)
        if path.is_dir():
            pattern = "**/*.py" if recursive else "*.py"
            expanded.extend(sorted(path.glob(pattern)))
        else:
            expanded.append(path)
    return expanded


def process_path(path, args):
    raw = path.read_bytes()
    encoding, text = decode_bytes(raw)

    before_sig = significant_tokens(text)
    new_text, candidates = transform_text(text, args.max_line_len)

    if not candidates:
        if args.verbose:
            print("NOCHANGE %s" % path)
        return 0, 0

    after_sig = significant_tokens(new_text)
    if before_sig is not None and after_sig is not None and before_sig != after_sig:
        print("REFUSE %s: significant token sequence changed" % path)
        return 0, 1

    print("%s %s: collapsed_flat_literals=%d" % (
        "WRITE" if args.in_place else "WOULD",
        path,
        len(candidates),
    ))

    for start, end, collapsed in candidates:
        preview = collapsed
        if len(preview) > 180:
            preview = preview[:177] + "..."
        print("  lines %d-%d -> %r" % (start + 1, end + 1, preview))

    if args.diff:
        diff = difflib.unified_diff(
            text.splitlines(True),
            new_text.splitlines(True),
            fromfile=str(path),
            tofile=str(path) + " (flat literals)",
            lineterm="",
        )
        for line in diff:
            sys.stdout.write(line)

    if args.in_place:
        write_encoding = encoding if encoding != "utf-8-sig" else "utf-8"
        path.write_bytes(new_text.encode(write_encoding))

    return len(candidates), 0


def main(argv):
    parser = argparse.ArgumentParser(
        description="Collapse simple flat multiline Python list/tuple assignment literals to one line."
    )
    parser.add_argument("paths", nargs="+", help="Files or directories to process.")
    parser.add_argument("--recursive", action="store_true", help="Recurse into directories.")
    parser.add_argument("--in-place", action="store_true", help="Write changes.")
    parser.add_argument("--diff", action="store_true", help="Print unified diff.")
    parser.add_argument("--max-line-len", type=int, default=DEFAULT_MAX_LINE_LEN)
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args(argv)

    changed_files = 0
    collapsed_total = 0
    refused = 0

    for path in expand_paths(args.paths, args.recursive):
        if not path.is_file():
            continue

        collapsed, errors = process_path(path, args)
        refused += errors
        if collapsed:
            changed_files += 1
            collapsed_total += collapsed

    if changed_files or refused or args.verbose:
        print("changed_files=%d collapsed_flat_literals=%d refused=%d in_place=%s max_line_len=%d" % (
            changed_files,
            collapsed_total,
            refused,
            args.in_place,
            args.max_line_len,
        ))

    return 1 if refused else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
