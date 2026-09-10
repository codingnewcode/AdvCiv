#!/usr/bin/env python3
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
"""Conservative multiline-to-single-line pass for CvMainInterface.py.

Collapses balanced logical statements spanning multiple physical lines when:
- no comments are inside the logical statement,
- no multiline string token is inside,
- no explicit backslash continuation is used,
- the resulting physical line stays within MAX_LINE_LEN.

This preserves token sequence except whitespace/newline positions for changed statements.
"""
from pathlib import Path
import io
import tokenize

SRC = Path('/mnt/data/CvMainInterface.py')
DST = Path('/mnt/data/CvMainInterface.singleline.py')
MAX_LINE_LEN = 300

raw = SRC.read_bytes()
newline = b'\r\n' if b'\r\n' in raw else b'\n'
text = raw.decode('utf-8', errors='replace')
lines = text.splitlines(True)

# Build logical statement groups from tokenize. We don't parse AST because this is old Civ4/Python 2 code.
groups = []
cur = []
with SRC.open('rb') as f:
    for tok in tokenize.tokenize(f.readline):
        if tok.type in (tokenize.ENCODING, tokenize.ENDMARKER):
            continue
        if not cur and tok.type in (tokenize.NL, tokenize.COMMENT, tokenize.INDENT, tokenize.DEDENT):
            continue
        cur.append(tok)
        if tok.type == tokenize.NEWLINE:
            if cur:
                groups.append((cur[0].start[0], cur[-1].end[0], cur[:]))
            cur = []

candidate_ranges = {}
skipped = {
    'single_line': 0,
    'comment': 0,
    'multiline_string': 0,
    'backslash': 0,
    'too_long': 0,
}
changed = 0
removed_lines = 0

def leading_indent(s):
    return s[:len(s) - len(s.lstrip(' \t'))]

for start, end, toks in groups:
    if end <= start:
        skipped['single_line'] += 1
        continue
    if any(t.type == tokenize.COMMENT for t in toks):
        skipped['comment'] += 1
        continue
    if any(t.type == tokenize.STRING and t.start[0] != t.end[0] for t in toks):
        skipped['multiline_string'] += 1
        continue
    # Explicit backslash continuations are more fragile; leave them untouched.
    if any(lines[i - 1].rstrip('\r\n').rstrip().endswith('\\') for i in range(start, end)):
        skipped['backslash'] += 1
        continue

    indent = leading_indent(lines[start - 1])
    stripped_parts = [ln.strip() for ln in lines[start - 1:end]]
    # Join physical lines into one logical line. Avoid adding spaces immediately after opening brackets
    # or immediately before closing brackets, but otherwise keep a separator so tokens do not merge.
    parts = [part for part in stripped_parts if part]
    body = ''
    for part in parts:
        if not body:
            body = part
            continue
        if body.rstrip().endswith(('(', '[', '{')) or part.lstrip().startswith((')', ']', '}')):
            body += part
        else:
            body += ' ' + part
    joined = indent + body
    joined += '\r\n' if newline == b'\r\n' else '\n'
    if len(joined.rstrip('\r\n')) > MAX_LINE_LEN:
        skipped['too_long'] += 1
        continue
    candidate_ranges[start] = (end, joined)
    changed += 1
    removed_lines += end - start

out = []
i = 1
n = len(lines)
while i <= n:
    if i in candidate_ranges:
        end, joined = candidate_ranges[i]
        out.append(joined)
        i = end + 1
    else:
        out.append(lines[i - 1])
        i += 1

new_text = ''.join(out)
DST.write_bytes(new_text.encode('utf-8'))

# Token-sequence verification: ignore whitespace-only layout tokens and positions.
def significant_tokens(path):
    result = []
    with path.open('rb') as f:
        for tok in tokenize.tokenize(f.readline):
            if tok.type in (tokenize.ENCODING, tokenize.ENDMARKER, tokenize.NL, tokenize.NEWLINE):
                continue
            # INDENT/DEDENT are semantic; keep them.
            result.append((tok.type, tok.string))
    return result

orig_tokens = significant_tokens(SRC)
new_tokens = significant_tokens(DST)
verified = orig_tokens == new_tokens
print('changed_logical_statements=%d' % changed)
print('removed_physical_lines=%d' % removed_lines)
print('max_line_len=%d' % MAX_LINE_LEN)
print('skipped=%s' % skipped)
print('token_sequence_verified=%s' % verified)
if not verified:
    for idx, (a, b) in enumerate(zip(orig_tokens, new_tokens)):
        if a != b:
            print('first_token_difference_at=%d orig=%r new=%r' % (idx, a, b))
            break
    raise SystemExit(1)
