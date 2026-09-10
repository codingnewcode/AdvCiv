#!/usr/bin/env python3
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
# Safe-ish broad parenthesis collapse helper.
#
# Goal:
# - Collapse multiline parenthesized expressions to one physical line.
# - Target continuation-alignment noise that produces Ruff/Pylance E101 in old
#   Civ4 Python files.
#
# This is deliberately broader than collapse_multiline_calls.py, but narrower
# and safer than the old collapse_multiline_brackets.py:
# - defaults to parentheses only;
# - skips ranges containing comments;
# - skips ranges containing multiline strings;
# - skips explicit backslash continuations;
# - preserves strings;
# - preserves line endings;
# - compares significant token streams when tokenization works.
#
# If a file is already too indentation-broken for tokenize, the script still
# applies local parenthesis-only rewrites by default and reports that token
# verification was skipped. Use --strict-token-check to refuse those files.
#
# Review the diff before committing. This is not a general formatter.

from __future__ import print_function

import argparse
import io
import subprocess
import sys
import tokenize
from pathlib import Path

OPEN_TO_CLOSE = {"(": ")", "[": "]", "{": "}"}
CLOSE_TO_OPEN = {")": "(", "]": "[", "}": "{"}


def get_tracked_python_files():
    output = subprocess.check_output(["git", "ls-files", "*.py"])
    return [line.decode("utf-8", "replace") for line in output.splitlines()]


def decode_bytes(raw):
    if raw.startswith(b"\xef\xbb\xbf"):
        return "utf-8-sig", raw.decode("utf-8-sig")
    try:
        return "utf-8", raw.decode("utf-8")
    except UnicodeDecodeError:
        return "latin-1", raw.decode("latin-1")


def detect_newline(raw):
    return "\r\n" if b"\r\n" in raw else "\n"


def line_start(text, index):
    pos = text.rfind("\n", 0, index)
    return 0 if pos < 0 else pos + 1


def line_end(text, index):
    pos = text.find("\n", index)
    return len(text) if pos < 0 else pos


def line_number(text, index):
    return text.count("\n", 0, index) + 1


def significant_tokens_from_text(text):
    result = []
    reader = io.BytesIO(text.encode("utf-8", errors="surrogatepass")).readline
    for tok in tokenize.tokenize(reader):
        if tok.type in (tokenize.ENCODING, tokenize.ENDMARKER, tokenize.NL, tokenize.NEWLINE,):
            continue
        result.append((tok.type, tok.string))
    return result


def try_significant_tokens(text):
    try:
        return significant_tokens_from_text(text), None
    except Exception as err:
        return None, "%s: %s" % (err.__class__.__name__, err)


def skip_string(text, i):
    quote = text[i]
    triple = text.startswith(quote * 3, i)
    start = i
    has_newline = False

    if triple:
        i += 3
        while i < len(text):
            if text.startswith(quote * 3, i):
                return i + 3, has_newline
            if text[i] == "\n":
                has_newline = True
            i += 1
        return len(text), True

    i += 1
    while i < len(text):
        c = text[i]
        if c == "\\":
            i += 2
            continue
        if c == "\n":
            # A real newline inside a non-triple string means the file is already
            # malformed or uses odd escaping. Treat as multiline/unsafe.
            return i + 1, True
        if c == quote:
            return i + 1, has_newline
        i += 1

    return len(text), True


def find_matching_bracket(text, start):
    opener = text[start]
    stack = [opener]
    i = start + 1
    has_comment = False
    has_multiline_string = False
    has_backslash_continuation = False

    while i < len(text):
        c = text[i]

        if c in ("'", '"'):
            new_i, string_has_newline = skip_string(text, i)
            if string_has_newline:
                has_multiline_string = True
            i = new_i
            continue

        if c == "#":
            has_comment = True
            j = text.find("\n", i)
            if j < 0:
                return -1, has_comment, has_multiline_string, has_backslash_continuation
            i = j + 1
            continue

        if c == "\\":
            j = i + 1
            while j < len(text) and text[j] in " \t":
                j += 1
            if j < len(text) and text[j] in "\r\n":
                has_backslash_continuation = True
            i += 1
            continue

        if c in OPEN_TO_CLOSE:
            stack.append(c)
            i += 1
            continue

        if c in CLOSE_TO_OPEN:
            if not stack or stack[-1] != CLOSE_TO_OPEN[c]:
                return -1, has_comment, has_multiline_string, has_backslash_continuation
            stack.pop()
            if not stack:
                return i, has_comment, has_multiline_string, has_backslash_continuation
            i += 1
            continue

        i += 1

    return -1, has_comment, has_multiline_string, has_backslash_continuation


def collapse_inner(inner):
    out = []
    i = 0
    pending_space = False

    while i < len(inner):
        c = inner[i]

        if c in ("'", '"'):
            if pending_space and out and out[-1] not in " \t([{":
                out.append(" ")
            pending_space = False
            start = i
            i, _string_has_newline = skip_string(inner, i)
            out.append(inner[start:i])
            continue

        if c == "\n":
            pending_space = True
            i += 1
            while i < len(inner) and inner[i] in " \t\r\n":
                if inner[i] == "\n":
                    pending_space = True
                i += 1
            continue

        if c == "\r":
            i += 1
            continue

        if c in " \t":
            pending_space = True
            i += 1
            continue

        if pending_space:
            if out and out[-1] not in " \t([{":
                if c not in ")]},:":
                    out.append(" ")
            pending_space = False

        out.append(c)
        i += 1

    return "".join(out).strip()


def allowed_openers(args):
    allowed = {"("}
    if args.include_square:
        allowed.add("[")
    if args.include_curly:
        allowed.add("{")
    if args.include_all_brackets:
        allowed.update(["[", "{"])
    return allowed


def is_probably_huge_table(text, start, end, max_lines):
    return text.count("\n", start, end + 1) + 1 > max_lines


def build_candidates(text, args):
    allowed = allowed_openers(args)
    candidates = []
    skipped = {
        "not_allowed_bracket": 0,
        "no_match": 0,
        "single_line": 0,
        "comment": 0,
        "multiline_string": 0,
        "backslash": 0,
        "too_many_lines": 0,
        "too_long": 0,
        "empty": 0,
    }

    i = 0
    while i < len(text):
        c = text[i]

        if c in ("'", '"'):
            i, _string_has_newline = skip_string(text, i)
            continue

        if c == "#":
            j = text.find("\n", i)
            if j < 0:
                break
            i = j + 1
            continue

        if c in OPEN_TO_CLOSE:
            if c not in allowed:
                skipped["not_allowed_bracket"] += 1
                i += 1
                continue

            end, has_comment, has_multiline_string, has_backslash = find_matching_bracket(text, i)
            if end < 0:
                skipped["no_match"] += 1
                i += 1
                continue

            if "\n" not in text[i:end + 1]:
                skipped["single_line"] += 1
                i = end + 1
                continue

            if has_comment:
                skipped["comment"] += 1
                i = end + 1
                continue

            if has_multiline_string:
                skipped["multiline_string"] += 1
                i = end + 1
                continue

            if has_backslash:
                skipped["backslash"] += 1
                i = end + 1
                continue

            if is_probably_huge_table(text, i, end, args.max_physical_lines):
                skipped["too_many_lines"] += 1
                i = end + 1
                continue

            inner = text[i + 1:end]
            collapsed_inner = collapse_inner(inner)
            if not collapsed_inner and inner.strip():
                skipped["empty"] += 1
                i = end + 1
                continue

            replacement = c + collapsed_inner + OPEN_TO_CLOSE[c]

            start_of_line = line_start(text, i)
            end_of_line = line_end(text, end)
            new_line = text[start_of_line:i] + replacement + text[end + 1:end_of_line]
            if len(new_line) > args.max_line_len:
                skipped["too_long"] += 1
                i = end + 1
                continue

            candidates.append((i, end + 1, replacement))
            i = end + 1
            continue

        i += 1

    return candidates, skipped


def apply_candidates(text, candidates):
    if not candidates:
        return text

    out = []
    last = 0
    for start, end, replacement in candidates:
        if start < last:
            continue
        out.append(text[last:start])
        out.append(replacement)
        last = end
    out.append(text[last:])
    return "".join(out)


def process_path(path, args):
    raw = path.read_bytes()
    encoding, text = decode_bytes(raw)

    before_tokens, before_error = try_significant_tokens(text) if not args.no_token_check else (None, None)

    candidates, skipped = build_candidates(text, args)
    new_text = apply_candidates(text, candidates)

    changed = new_text != text
    token_status = "not_checked"

    if changed and not args.no_token_check:
        after_tokens, after_error = try_significant_tokens(new_text)

        if before_error is None and after_error is None:
            if before_tokens != after_tokens:
                print("ERROR %s: significant token sequence changed; refusing to write." % path, file=sys.stderr)
                return 1, 0, 0, "changed", skipped
            token_status = "verified"
        elif args.strict_token_check:
            print("ERROR %s: token check unavailable; before=%r after=%r" % (path, before_error, after_error), file=sys.stderr)
            return 1, 0, 0, "strict_token_failed", skipped
        else:
            token_status = "skipped_before=%r_after=%r" % (before_error, after_error)

    if changed and args.in_place:
        write_encoding = encoding if encoding != "utf-8-sig" else "utf-8"
        path.write_bytes(new_text.encode(write_encoding))

    removed_lines = text.count("\n") - new_text.count("\n")
    if changed or args.verbose:
        print("%s %s: changed_ranges=%d removed_physical_lines=%d token_status=%s skipped=%s" % ("WRITE" if args.in_place else "WOULD", path, len(candidates), removed_lines, token_status, skipped,))

    return 0, 1 if changed else 0, removed_lines, token_status, skipped


def main(argv):
    parser = argparse.ArgumentParser(description="Collapse safe multiline parenthesized expressions.")
    parser.add_argument("paths", nargs="*", help="Python files. Defaults to git-tracked *.py files.")
    parser.add_argument("--in-place", action="store_true")
    parser.add_argument("--max-line-len", type=int, default=1800)
    parser.add_argument("--max-physical-lines", type=int, default=40)
    parser.add_argument("--include-square", action="store_true")
    parser.add_argument("--include-curly", action="store_true")
    parser.add_argument("--include-all-brackets", action="store_true")
    parser.add_argument("--no-token-check", action="store_true")
    parser.add_argument("--strict-token-check", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args(argv)

    paths = args.paths or get_tracked_python_files()

    changed_files = 0
    removed_lines_total = 0
    failed = 0
    skipped_total = {}

    for name in paths:
        path = Path(name)
        if not path.is_file():
            continue

        rc, changed, removed_lines, _token_status, skipped = process_path(path, args)
        if rc:
            failed += 1
        changed_files += changed
        removed_lines_total += removed_lines

        for reason, count in skipped.items():
            skipped_total[reason] = skipped_total.get(reason, 0) + count

    print("changed_files=%d removed_physical_lines=%d failed=%d in_place=%s" % (changed_files, removed_lines_total, failed, args.in_place,))
    print("max_line_len=%d max_physical_lines=%d include_square=%s include_curly=%s include_all_brackets=%s" % (args.max_line_len, args.max_physical_lines, args.include_square, args.include_curly, args.include_all_brackets,))
    print("skipped_total=%s" % skipped_total)

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
