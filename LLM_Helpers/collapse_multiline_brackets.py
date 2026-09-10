#!/usr/bin/env python3
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
# Collapse multi-line bracketed expressions ((...), [...], {...}) to one-liners
# in a single Python source file. Useful for grepping boilerplate Civ4 API calls
# in an LLM/AI workflow.
#
# Preserves: original line endings (CRLF/LF), string literals, comments outside
# collapsed ranges. Strips line comments + indentation INSIDE a collapsed range.
#
# Run on ONE file at a time and eyeball the diff before committing — a directory
# sweep was tried once and reverted.
#
# Usage (PowerShell from mod root):
#   python LLM_Helpers\collapse_multiline_brackets.py <path-to-file.py>
#
# Verify after each run:
#   python -c "import ast; ast.parse(open(r'<path>', 'r', encoding='utf-8').read())"
import sys

def collapse_inner(inner):
    # Strip line comments, replace newlines + leading whitespace with single space.
    # String literals (incl. triple-quoted) are preserved verbatim.
    out = []
    i = 0
    n = len(inner)
    in_str = None
    while i < n:
        c = inner[i]
        if in_str:
            out.append(c)
            if c == "\\" and i + 1 < n:
                out.append(inner[i+1])
                i += 2
                continue
            if c == in_str:
                in_str = None
            i += 1
            continue
        if c == '"' or c == "'":
            if i + 2 < n and inner[i:i+3] in ('"""', "'''"):
                triple = inner[i:i+3]
                out.append(triple)
                i += 3
                while i + 2 < n and inner[i:i+3] != triple:
                    out.append(inner[i])
                    i += 1
                if i + 2 < n and inner[i:i+3] == triple:
                    out.append(triple)
                    i += 3
                continue
            in_str = c
            out.append(c)
            i += 1
            continue
        if c == "#":
            while i < n and inner[i] != "\n":
                i += 1
            continue
        if c == "\n":
            i += 1
            while i < n and inner[i] in " \t":
                i += 1
            if out and out[-1] not in " \t" and i < n:
                out.append(" ")
            continue
        out.append(c)
        i += 1
    while out and out[-1] in " \t":
        out.pop()
    return "".join(out)

def collapse_brackets(src):
    out = []
    i = 0
    n = len(src)
    in_str = None
    while i < n:
        ch = src[i]
        if in_str:
            out.append(ch)
            if ch == "\\" and i + 1 < n:
                out.append(src[i+1])
                i += 2
                continue
            if ch == in_str:
                in_str = None
            i += 1
            continue
        if ch == '"' or ch == "'":
            if i + 2 < n and src[i:i+3] in ('"""', "'''"):
                triple = src[i:i+3]
                out.append(triple)
                i += 3
                while i + 2 < n and src[i:i+3] != triple:
                    out.append(src[i])
                    i += 1
                if i + 2 < n and src[i:i+3] == triple:
                    out.append(triple)
                    i += 3
                continue
            in_str = ch
            out.append(ch)
            i += 1
            continue
        if ch == "#":
            while i < n and src[i] != "\n":
                out.append(src[i])
                i += 1
            continue
        if ch in "([{":
            open_ch = ch
            close_ch = {"(": ")", "[": "]", "{": "}"}[ch]
            j = i + 1
            depth = 1
            inner_str = None
            while j < n and depth > 0:
                c = src[j]
                if inner_str:
                    if c == "\\" and j + 1 < n:
                        j += 2
                        continue
                    if c == inner_str:
                        inner_str = None
                    j += 1
                    continue
                if c == "#":
                    while j < n and src[j] != "\n":
                        j += 1
                    continue
                if c == '"' or c == "'":
                    if j + 2 < n and src[j:j+3] in ('"""', "'''"):
                        triple = src[j:j+3]
                        j += 3
                        while j + 2 < n and src[j:j+3] != triple:
                            j += 1
                        if j + 2 < n and src[j:j+3] == triple:
                            j += 3
                        continue
                    inner_str = c
                    j += 1
                    continue
                if c == open_ch:
                    depth += 1
                elif c == close_ch:
                    depth -= 1
                j += 1
            if j > n:
                out.append(src[i:])
                return "".join(out)
            inner = src[i+1:j-1]
            if "\n" in inner:
                collapsed = collapse_inner(inner).lstrip().rstrip()
                out.append(open_ch)
                out.append(collapsed)
                out.append(close_ch)
            else:
                out.append(src[i:j])
            i = j
            continue
        out.append(ch)
        i += 1
    return "".join(out)

def main():
    if len(sys.argv) < 2:
        print("usage: _collapse_one_tmp.py <file>")
        sys.exit(1)
    f = sys.argv[1]
    with open(f, "rb") as fh:
        raw = fh.read()
    use_crlf = b"\r\n" in raw
    s = raw.decode("utf-8")
    s_norm = s.replace("\r\n", "\n").replace("\r", "\n")
    new_s = collapse_brackets(s_norm)
    if new_s == s_norm:
        print("no changes")
        return
    if use_crlf:
        new_s = new_s.replace("\n", "\r\n")
    with open(f, "wb") as fh:
        fh.write(new_s.encode("utf-8"))
    print("collapsed", f)

if __name__ == "__main__":
    main()
