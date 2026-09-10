#!/usr/bin/env python3
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
"""Second conservative multiline-to-single-line pass for CvMainInterface.py.

Collapses balanced logical statements spanning multiple physical lines even when
comments appear inside the logical statement. Those comments are preserved by
moving them directly above the collapsed statement at the statement indentation.

Still skips:
- multiline string tokens,
- explicit backslash continuations,
- statements that would exceed MAX_LINE_LEN.

Verification compares significant code tokens before/after, and also checks
that the ordered list of comment strings is unchanged.
"""
from pathlib import Path
import tokenize

SRC = Path('/mnt/data/CvMainInterface.singleline.py')
DST = Path('/mnt/data/CvMainInterface.singleline2.py')
MAX_LINE_LEN = 300

raw = SRC.read_bytes()
newline = b'\r\n' if b'\r\n' in raw else b'\n'
NL = '\r\n' if newline == b'\r\n' else '\n'
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
    'no_comment': 0,
    'multiline_string': 0,
    'backslash': 0,
    'too_long': 0,
    'empty_after_comment_removal': 0,
}
changed = 0
removed_lines = 0
moved_comments = 0

def leading_indent(s):
    return s[:len(s) - len(s.lstrip(' \t'))]

def strip_line_ending(s):
    return s.rstrip('\r\n')

def join_parts(parts):
    body = ''
    for part in parts:
        if not body:
            body = part
            continue
        # Avoid awkward spaces just inside brackets or before colon/comma-ish closers.
        if body.rstrip().endswith(('(', '[', '{')) or part.lstrip().startswith((')', ']', '}', ':', ',')):
            body += part
        else:
            body += ' ' + part
    return body

for start, end, toks in groups:
    if end <= start:
        skipped['single_line'] += 1
        continue
    comment_toks = [t for t in toks if t.type == tokenize.COMMENT]
    if not comment_toks:
        skipped['no_comment'] += 1
        continue
    if any(t.type == tokenize.STRING and t.start[0] != t.end[0] for t in toks):
        skipped['multiline_string'] += 1
        continue
    # Explicit backslash continuations are more fragile; leave them untouched.
    if any(lines[i - 1].rstrip('\r\n').rstrip().endswith('\\') for i in range(start, end)):
        skipped['backslash'] += 1
        continue

    indent = leading_indent(lines[start - 1])

    # Remove comment text from the physical lines belonging to this statement.
    # For each line, a COMMENT token means everything from its start column onward
    # is ignored by Python, so trimming there preserves the code tokens.
    cut_cols_by_line = {}
    for t in comment_toks:
        line_no, col = t.start
        cut_cols_by_line[line_no] = min(col, cut_cols_by_line.get(line_no, col))

    code_parts = []
    for line_no in range(start, end + 1):
        raw_line = strip_line_ending(lines[line_no - 1])
        if line_no in cut_cols_by_line:
            raw_line = raw_line[:cut_cols_by_line[line_no]]
        part = raw_line.strip()
        if part:
            code_parts.append(part)

    if not code_parts:
        skipped['empty_after_comment_removal'] += 1
        continue

    body = join_parts(code_parts)
    joined_code = indent + body
    comment_lines = [indent + t.string.strip() for t in comment_toks]
    replacement_lines = comment_lines + [joined_code]
    replacement = NL.join(replacement_lines) + NL

    if len(joined_code) > MAX_LINE_LEN:
        skipped['too_long'] += 1
        continue

    candidate_ranges[start] = (end, replacement)
    changed += 1
    removed_lines += (end - start + 1) - len(replacement_lines)
    moved_comments += len(comment_toks)

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

new_text = ''.join(out)
DST.write_bytes(new_text.encode('utf-8'))

# Token-sequence verification: ignore whitespace-only layout tokens and positions.
def significant_code_tokens(path):
    result = []
    with path.open('rb') as f:
        for tok in tokenize.tokenize(f.readline):
            if tok.type in (tokenize.ENCODING, tokenize.ENDMARKER, tokenize.NL, tokenize.NEWLINE, tokenize.COMMENT):
                continue
            # INDENT/DEDENT are semantic; keep them.
            result.append((tok.type, tok.string))
    return result

def comment_tokens(path):
    result = []
    with path.open('rb') as f:
        for tok in tokenize.tokenize(f.readline):
            if tok.type == tokenize.COMMENT:
                result.append(tok.string)
    return result

orig_tokens = significant_code_tokens(SRC)
new_tokens = significant_code_tokens(DST)
code_verified = orig_tokens == new_tokens
orig_comments = comment_tokens(SRC)
new_comments = comment_tokens(DST)
comments_verified = orig_comments == new_comments

print('changed_logical_statements=%d' % changed)
print('moved_comments=%d' % moved_comments)
print('removed_physical_lines=%d' % removed_lines)
print('max_line_len=%d' % MAX_LINE_LEN)
print('skipped=%s' % skipped)
print('code_token_sequence_verified=%s' % code_verified)
print('comment_sequence_verified=%s' % comments_verified)
if not code_verified:
    for idx, (a, b) in enumerate(zip(orig_tokens, new_tokens)):
        if a != b:
            print('first_code_token_difference_at=%d orig=%r new=%r' % (idx, a, b))
            break
    raise SystemExit(1)
if not comments_verified:
    for idx, (a, b) in enumerate(zip(orig_comments, new_comments)):
        if a != b:
            print('first_comment_difference_at=%d orig=%r new=%r' % (idx, a, b))
            break
    raise SystemExit(1)
