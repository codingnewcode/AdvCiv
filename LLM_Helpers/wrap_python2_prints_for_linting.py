#!/usr/bin/env python3
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
"""Conservatively wrap easy Python 2 print statements for Python 3 linters.

Purpose
-------
Convert only safe one-line Python 2 prints like:

    print "hello"
    print 'hello'
    print "value = %s" % value
    print "-" * 40
    print szDebugText

into:

    print("hello")
    print('hello')
    print("value = %s" % value)
    print("-" * 40)
    print(szDebugText)

This keeps the statement valid in Civ4's Python 2.4 runtime for the
single-expression cases and lets Python 3 linters parse more files.

Deliberately skipped
--------------------
- print "a", b              # Python 2 multi-argument print
- print "a",                # Python 2 trailing-comma print
- print >> sys.stderr, "a"  # redirected print
- print ("a", b)            # already parenthesized / ambiguous tuple
- print "a"; print "b"      # multiple statements on one line
- commented-out prints      # comments do not block linters such as Ruff parsing

Path handling
-------------
This is a portable helper, not a sandbox-only staged script:

- Pass local Windows/PowerShell paths directly.
- Pass ChatGPT sandbox paths such as /mnt/data/CvDiplomacy.py directly.
- Pass directories with --recursive to scan *.py files.
- Use --sandbox-samples in ChatGPT when the common uploaded sample files exist.

Default mode is dry-run. Use --diff to inspect changes, then --in-place only
after the diff looks boring. Always review generated source diffs before commit.
"""

from __future__ import print_function

import argparse
import difflib
import io
from pathlib import Path
import sys
import tokenize

TEXT_ENCODINGS = ("utf-8-sig", "utf-8", "cp1252", "latin-1")
DEFAULT_INCLUDES = ("*.py",)
SANDBOX_SAMPLE_PATHS = ("/mnt/data/CvMapGeneratorUtil.py", "/mnt/data/CvDiplomacy.py", "/mnt/data/CvMainInterface.py", "/mnt/data/BTG_Cross.py", "/mnt/data/PerfectMongoose.py", "/mnt/data/Planet_Generator_0_68.py",)

class FileResult(object):
    def __init__(self, path, encoding, original, rewritten, changed_lines, skipped_reasons):
        self.path = path
        self.encoding = encoding
        self.original = original
        self.rewritten = rewritten
        self.changed_lines = changed_lines
        self.skipped_reasons = skipped_reasons

    @property
    def changed(self):
        return self.original != self.rewritten

    @property
    def skipped_lines(self):
        return sum(self.skipped_reasons.values())

def decode_bytes(data):
    for encoding in TEXT_ENCODINGS:
        try:
            text = data.decode(encoding)
        except UnicodeDecodeError:
            continue
        if encoding == "utf-8-sig" and not data.startswith(b"\xef\xbb\xbf"):
            return text, "utf-8"
        return text, encoding
    return data.decode("latin-1"), "latin-1"

def split_line_ending(line):
    if line.endswith("\r\n"):
        return line[:-2], "\r\n"
    if line.endswith("\n"):
        return line[:-1], "\n"
    if line.endswith("\r"):
        return line[:-1], "\r"
    return line, ""

def leading_print_statement(body):
    stripped = body.lstrip(" \t")
    indent = body[:len(body) - len(stripped)]

    if not stripped.startswith("print"):
        return None

    after = stripped[5:]
    if after and (after[0].isalnum() or after[0] == "_"):
        return None
    if after.startswith("(") or after.startswith(" ("):
        return None

    return indent, after

def find_comment_column(body):
    try:
        tokens = tokenize.generate_tokens(io.StringIO(body + "\n").readline)
        for token in tokens:
            if token.type == tokenize.COMMENT:
                return token.start[1]
    except (tokenize.TokenError, IndentationError):
        return None
    return None

def expression_skip_reason(expr):
    if not expr:
        return None

    if expr.startswith(">>"):
        return "redirect"

    if expr.startswith("("):
        return "already/ambiguous parens"

    try:
        tokens = tokenize.generate_tokens(io.StringIO(expr + "\n").readline)
        depth = 0
        previous_significant = None
        for token in tokens:
            token_type = token.type
            token_text = token.string

            if token_type in (tokenize.NL, tokenize.NEWLINE, tokenize.ENDMARKER, tokenize.INDENT, tokenize.DEDENT):
                continue

            if token_type == tokenize.OP:
                if token_text in ("(", "[", "{"):
                    depth += 1
                elif token_text in (")", "]", "}"):
                    depth -= 1
                    if depth < 0:
                        return "unbalanced brackets"
                elif depth == 0 and token_text == ",":
                    return "top-level comma"
                elif depth == 0 and token_text == ">>":
                    return "redirect"
                elif depth == 0 and token_text == ";":
                    return "top-level semicolon"

            previous_significant = token_text

        if previous_significant == ",":
            return "trailing comma"
        if depth != 0:
            return "unbalanced brackets"

    except (tokenize.TokenError, IndentationError) as exc:
        return "tokenize error: %s" % (exc,)

    return None

def rewrite_line(line):
    body, eol = split_line_ending(line)
    parsed = leading_print_statement(body)
    if parsed is None:
        return line, False, None

    if body.lstrip(" \t").startswith("#"):
        return line, False, "commented"

    comment_column = find_comment_column(body)
    if comment_column is None:
        code = body.rstrip()
        comment = ""
    else:
        code = body[:comment_column].rstrip()
        comment = body[comment_column:]

    parsed_code = leading_print_statement(code)
    if parsed_code is None:
        return line, False, None

    indent, after_print = parsed_code
    expr = after_print.strip()

    reason = expression_skip_reason(expr)
    if reason is not None:
        return line, False, reason

    if expr:
        new_code = "%sprint(%s)" % (indent, expr)
    else:
        new_code = "%sprint(\"\")" % indent

    if comment:
        new_code += " " + comment.lstrip()

    return new_code + eol, True, None

def rewrite_text(text):
    rewritten_lines = []
    changed_lines = 0
    skipped_reasons = {}

    for line in text.splitlines(True):
        new_line, changed, skip_reason = rewrite_line(line)
        rewritten_lines.append(new_line)
        if changed:
            changed_lines += 1
        elif skip_reason is not None:
            skipped_reasons[skip_reason] = skipped_reasons.get(skip_reason, 0) + 1

    rewritten = "".join(rewritten_lines)
    return rewritten, changed_lines, skipped_reasons

def process_file(path):
    data = path.read_bytes()
    original, encoding = decode_bytes(data)
    rewritten, changed_lines, skipped_reasons = rewrite_text(original)
    return FileResult(path, encoding, original, rewritten, changed_lines, skipped_reasons)

def unified_diff(result):
    old_lines = result.original.splitlines(True)
    new_lines = result.rewritten.splitlines(True)
    return "".join(difflib.unified_diff(old_lines, new_lines, fromfile=str(result.path), tofile=str(result.path) + " (wrapped prints)",))

def write_text_preserving_encoding(path, text, encoding):
    path.write_bytes(text.encode(encoding))

def write_result(result, output, in_place):
    if output is not None and in_place:
        raise ValueError("--output and --in-place cannot be used together")
    target = result.path if in_place else output
    if target is None:
        return
    write_text_preserving_encoding(target, result.rewritten, result.encoding)

def result_summary_lines(results):
    total_changed = sum(result.changed_lines for result in results)
    total_skipped = sum(result.skipped_lines for result in results)
    changed_files = sum(1 for result in results if result.changed)

    lines = []
    lines.append("Files examined: %d" % len(results))
    lines.append("Files with changes: %d" % changed_files)
    lines.append("Print lines wrapped: %d" % total_changed)
    lines.append("Print lines skipped: %d" % total_skipped)

    for result in results:
        if result.changed_lines or result.skipped_lines:
            lines.append("")
            lines.append(str(result.path))
            lines.append("  encoding: %s" % result.encoding)
            lines.append("  wrapped: %d" % result.changed_lines)
            if result.skipped_reasons:
                for reason in sorted(result.skipped_reasons):
                    lines.append("  skipped %-24s %d" % (reason + ":", result.skipped_reasons[reason]))
    return lines

def print_summary(results):
    for line in result_summary_lines(results):
        print(line)

def normalize_include_patterns(patterns):
    if not patterns:
        return list(DEFAULT_INCLUDES)
    normalized = []
    for pattern in patterns:
        for part in pattern.split(";"):
            part = part.strip()
            if part:
                normalized.append(part)
    return normalized or list(DEFAULT_INCLUDES)

def iter_directory_files(directory, include_patterns, recursive):
    seen = set()
    for pattern in include_patterns:
        iterator = directory.rglob(pattern) if recursive else directory.glob(pattern)
        for path in iterator:
            if path.is_file():
                resolved = path.resolve()
                if resolved not in seen:
                    seen.add(resolved)
                    yield path

def expand_input_paths(raw_paths, include_patterns, recursive, sandbox_samples):
    paths = []

    if sandbox_samples:
        for sample in SANDBOX_SAMPLE_PATHS:
            path = Path(sample)
            if path.exists():
                paths.append(path)

    for raw_path in raw_paths:
        path = Path(raw_path)
        if path.is_dir():
            paths.extend(iter_directory_files(path, include_patterns, recursive))
        else:
            paths.append(path)

    # Preserve order while removing duplicates.
    seen = set()
    unique = []
    for path in paths:
        key = str(path.resolve()) if path.exists() else str(path)
        if key not in seen:
            seen.add(key)
            unique.append(path)
    return unique

def validate_paths(paths):
    missing = [path for path in paths if not path.exists()]
    directories = [path for path in paths if path.exists() and path.is_dir()]
    errors = []
    for path in missing:
        errors.append("missing path: %s" % path)
    for path in directories:
        errors.append("directory was not expanded: %s" % path)
    return errors

def parse_args(argv):
    parser = argparse.ArgumentParser(description="Conservatively wrap easy Python 2 print statements as print(...) for Python 3 linters.")
    parser.add_argument("paths", nargs="*", help="Python source files or directories to scan/rewrite.")
    parser.add_argument("--recursive", action="store_true", help="When a directory is passed, scan it recursively.")
    parser.add_argument("--include", action="append", help="Glob pattern for directory scanning. Default: *.py. Can be repeated or separated with semicolons.")
    parser.add_argument("--sandbox-samples", action="store_true", help="Also scan common /mnt/data sample files when they exist. Useful inside ChatGPT.")
    parser.add_argument("--in-place", action="store_true", help="Rewrite each file in place. Default is dry-run.")
    parser.add_argument("--output", help="Write rewritten output to this path. Only valid with exactly one input file.")
    parser.add_argument("--diff", action="store_true", help="Print a unified diff for files that would change.")
    parser.add_argument("--report", help="Write the summary report to this path as well as stdout.")
    parser.add_argument("--check", action="store_true", help="Exit with status 1 if any file would change.")
    return parser.parse_args(argv)

def main(argv):
    args = parse_args(argv)
    include_patterns = normalize_include_patterns(args.include)
    input_paths = expand_input_paths(args.paths, include_patterns, args.recursive, args.sandbox_samples)

    if not input_paths:
        print("No input paths. Pass files/directories, or use --sandbox-samples inside ChatGPT.", file=sys.stderr)
        return 2

    if args.output and len(input_paths) != 1:
        print("--output requires exactly one input file", file=sys.stderr)
        return 2

    if args.output and args.in_place:
        print("--output and --in-place cannot be used together", file=sys.stderr)
        return 2

    path_errors = validate_paths(input_paths)
    if path_errors:
        for error in path_errors:
            print(error, file=sys.stderr)
        return 2

    results = [process_file(path) for path in input_paths]

    if args.diff:
        for result in results:
            if result.changed:
                diff_text = unified_diff(result)
                sys.stdout.write(diff_text)
                if diff_text and not diff_text.endswith("\n"):
                    print("")

    output_path = Path(args.output) if args.output else None
    for result in results:
        write_result(result, output_path, args.in_place)

    print_summary(results)

    if args.report:
        report_path = Path(args.report)
        report_path.write_text("\n".join(result_summary_lines(results)) + "\n", encoding="utf-8")

    if args.check and any(result.changed for result in results):
        return 1
    return 0

if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
