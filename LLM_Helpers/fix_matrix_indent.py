#!/usr/bin/env python3
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
# Normalize mixed tabs/spaces inside multiline matrix/data assignments.
#
# This helper is for old Civ4 map-script data blocks such as:
#   templates = {
#       3: {0: [...]}
#   }
#
# It does not collapse lines. It only converts leading mixed tab/space
# indentation to spaces-only on continuation lines inside multiline
# list/dict/tuple assignment blocks. This targets Ruff E101 noise while
# preserving matrix readability.
#
# Safety:
# - skips single-line assignments;
# - skips non-assignment continuations;
# - does not touch the assignment's first line indentation;
# - preserves visual indentation width with default tabsize;
# - strips trailing whitespace only on lines whose leading indent changed;
# - refuses to write if significant token sequence changes.
#
# Review git diff after use. This is not a general formatter.

from __future__ import print_function

import argparse
import io
import sys
import tokenize
from pathlib import Path

OPENERS = "([{"
CLOSERS = ")]}"
MATCH_OPEN = {")": "(", "]": "[", "}": "{"}

def decode_bytes(raw):
    if raw.startswith(b"\xef\xbb\xbf"):
        return "utf-8-sig", raw.decode("utf-8-sig")
    try:
        return "utf-8", raw.decode("utf-8")
    except UnicodeDecodeError:
        return "latin-1", raw.decode("latin-1")

def leading_indent(line):
    return line[:len(line) - len(line.lstrip(" \t"))]

def has_mixed_indent(prefix):
    return " " in prefix and "\t" in prefix

def expanded_width(prefix, tabsize):
    col = 0
    for ch in prefix:
        if ch == "\t":
            col += tabsize - (col % tabsize)
        elif ch == " ":
            col += 1
    return col

def strip_trailing_ws_keep_eol(line):
    eol = ""
    body = line
    if line.endswith("\r\n"):
        body = line[:-2]
        eol = "\r\n"
    elif line.endswith("\n"):
        body = line[:-1]
        eol = "\n"
    return body.rstrip(" \t") + eol

def significant_tokens_from_text(text):
    result = []
    reader = io.BytesIO(text.encode("utf-8", errors="surrogatepass")).readline
    for tok in tokenize.tokenize(reader):
        if tok.type in (tokenize.ENCODING, tokenize.ENDMARKER, tokenize.NL, tokenize.NEWLINE,):
            continue
        result.append((tok.type, tok.string))
    return result

def find_assignment_opener(stripped):
    lhs = []
    i = 0
    n = len(stripped)
    while i < n and stripped[i] not in "=#":
        lhs.append(stripped[i])
        i += 1
    if i >= n or stripped[i] != "=":
        return -1
    if i + 1 < n and stripped[i + 1] == "=":
        return -1
    lhs_text = "".join(lhs).strip()
    if not lhs_text:
        return -1
    for ch in lhs_text:
        if not (ch.isalnum() or ch in "_."):
            return -1
    i += 1
    while i < n and stripped[i] in " \t":
        i += 1
    if i < n and stripped[i] in OPENERS:
        return i
    return -1

def update_bracket_state(segment, stack, state):
    quote = state.get("quote")
    triple = state.get("triple")
    i = 0
    n = len(segment)
    single3 = "'" * 3
    double3 = '"' * 3

    while i < n:
        c = segment[i]

        if quote:
            if c == "\\":
                i += 2
                continue
            if triple:
                if segment.startswith(triple, i):
                    quote = None
                    triple = None
                    i += 3
                    continue
            elif c == quote:
                quote = None
            i += 1
            continue

        if c == "#":
            break

        if c in ("'", '"'):
            if segment.startswith(single3, i) or segment.startswith(double3, i):
                quote = c
                triple = segment[i:i + 3]
                i += 3
                continue
            quote = c
            triple = None
            i += 1
            continue

        if c in OPENERS:
            stack.append(c)
        elif c in CLOSERS:
            if stack and stack[-1] == MATCH_OPEN[c]:
                stack.pop()

        i += 1

    state["quote"] = quote
    state["triple"] = triple

def find_matrix_assignment_blocks(lines):
    blocks = []
    i = 0
    while i < len(lines):
        raw = lines[i].rstrip("\r\n")
        stripped = raw.lstrip(" \t")
        opener_idx_in_stripped = find_assignment_opener(stripped)
        if opener_idx_in_stripped < 0:
            i += 1
            continue

        stack = []
        state = {"quote": None, "triple": None}
        update_bracket_state(stripped[opener_idx_in_stripped:], stack, state)
        if not stack:
            i += 1
            continue

        start = i
        j = i + 1
        while j < len(lines) and stack:
            update_bracket_state(lines[j].rstrip("\r\n"), stack, state)
            j += 1

        if not stack and j - 1 > start:
            blocks.append((start, j - 1))
            i = j
        else:
            i = start + 1

    return blocks

def transform_text(text, tabsize):
    lines = text.splitlines(True)
    blocks = find_matrix_assignment_blocks(lines)

    changed_lines = []
    changed_blocks = 0
    out = list(lines)

    for start, end in blocks:
        block_changed = False
        for idx in range(start + 1, end + 1):
            line = out[idx]
            prefix = leading_indent(line)
            if not has_mixed_indent(prefix):
                continue
            stripped = line[len(prefix):]
            new_prefix = " " * expanded_width(prefix, tabsize)
            new_line = strip_trailing_ws_keep_eol(new_prefix + stripped)
            if new_line != line:
                out[idx] = new_line
                changed_lines.append(idx + 1)
                block_changed = True
        if block_changed:
            changed_blocks += 1

    return "".join(out), changed_blocks, changed_lines, blocks

def main(argv):
    parser = argparse.ArgumentParser(description="Normalize mixed indent inside multiline matrix/data assignments.")
    parser.add_argument("path", help="Python source file to process")
    parser.add_argument("--in-place", action="store_true", help="rewrite the input file")
    parser.add_argument("--output", help="write transformed text to this path")
    parser.add_argument("--tabsize", type=int, default=4)
    args = parser.parse_args(argv)

    src_path = Path(args.path)
    raw = src_path.read_bytes()
    encoding, text = decode_bytes(raw)

    try:
        orig_tokens = significant_tokens_from_text(text)
    except Exception as exc:
        print("skipped_tokenize_error_before=%s" % (exc,))
        print("changed_matrix_indent_lines=0")
        if not args.in_place and not args.output:
            print("dry_run_only=true")
        return 0

    new_text, changed_blocks, changed_lines, blocks = transform_text(text, args.tabsize)

    if changed_lines:
        try:
            new_tokens = significant_tokens_from_text(new_text)
        except Exception as exc:
            print("ERROR: tokenize failed after transform; refusing to write: %s" % (exc,), file=sys.stderr)
            return 1
        if orig_tokens != new_tokens:
            print("ERROR: significant token sequence changed; refusing to write.", file=sys.stderr)
            for idx, pair in enumerate(zip(orig_tokens, new_tokens)):
                if pair[0] != pair[1]:
                    print("first_token_difference_at=%d orig=%r new=%r" % (idx, pair[0], pair[1]), file=sys.stderr)
                    break
            return 1

    write_encoding = encoding if encoding != "utf-8-sig" else "utf-8"
    if args.output:
        Path(args.output).write_bytes(new_text.encode(write_encoding))
    if args.in_place:
        src_path.write_bytes(new_text.encode(write_encoding))

    print("matrix_assignment_blocks=%d" % len(blocks))
    print("changed_matrix_blocks=%d" % changed_blocks)
    print("changed_matrix_indent_lines=%d" % len(changed_lines))
    if changed_lines:
        preview = ",".join(str(x) for x in changed_lines[:60])
        if len(changed_lines) > 60:
            preview += ",..."
        print("changed_lines=%s" % preview)
    if not args.in_place and not args.output:
        print("dry_run_only=true")
    return 0

if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
