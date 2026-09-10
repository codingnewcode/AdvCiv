#!/usr/bin/env python3
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
"""Third multiline-to-single-line pass for CvMainInterface.py.

Input:  CvMainInterface.singleline2.py
Output: CvMainInterface.singleline3.py

Adds two conservative behaviors on top of the previous passes:
- Collapses remaining real multiline logical statements up to a longer line cap,
  preserving/moving inline COMMENT tokens above the statement.
- Collapses commented-out multiline Python calls/conditions when they are clearly
  bracket-continuation code comments. Normal prose comments are left alone.

Verification:
- significant real-code token sequence is unchanged
- output compiles under Python 3's parser for this file.
"""
from __future__ import print_function
from pathlib import Path
import io
import re
import difflib
import tokenize
import py_compile

SRC = Path('/mnt/data/CvMainInterface.singleline2.py')
DST = Path('/mnt/data/CvMainInterface.singleline3.py')
DIFF = Path('/mnt/data/CvMainInterface.singleline3.diff')
MAX_REAL_CODE_LINE_LEN = 400
MAX_COMMENTED_CODE_LINE_LEN = 500

raw = SRC.read_bytes()
newline = b'\r\n' if b'\r\n' in raw else b'\n'
NL = '\r\n' if newline == b'\r\n' else '\n'
text = raw.decode('utf-8', errors='replace')
lines = text.splitlines(True)

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
        if body.rstrip().endswith(('(', '[', '{')) or part.lstrip().startswith((')', ']', '}', ':', ',')):
            body += part
        else:
            body += ' ' + part
    return body

def significant_code_tokens(path):
    result = []
    with path.open('rb') as f:
        for tok in tokenize.tokenize(f.readline):
            if tok.type in (tokenize.ENCODING, tokenize.ENDMARKER, tokenize.NL, tokenize.NEWLINE, tokenize.COMMENT):
                continue
            result.append((tok.type, tok.string))
    return result

# --- Pass 3A: real Python logical statements ---------------------------------

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
real_changed = 0
real_moved_comments = 0
real_removed_lines = 0
skipped_real = {
    'single_line': 0,
    'multiline_string': 0,
    'backslash': 0,
    'too_long': 0,
    'empty_after_comment_removal': 0,
    'list_or_dict_assignment': 0,
}

for start, end, toks in groups:
    if end <= start:
        skipped_real['single_line'] += 1
        continue
    if any(t.type == tokenize.STRING and t.start[0] != t.end[0] for t in toks):
        skipped_real['multiline_string'] += 1
        continue
    if any(lines[i - 1].rstrip('\r\n').rstrip().endswith('\\') for i in range(start, end)):
        skipped_real['backslash'] += 1
        continue

    comment_toks = [t for t in toks if t.type == tokenize.COMMENT]
    indent = leading_indent(lines[start - 1])

    # Avoid collapsing large data containers; these stay more useful as one entry per line.
    first_meaningful = None
    for t in toks:
        if t.type not in (tokenize.INDENT, tokenize.DEDENT, tokenize.NL, tokenize.NEWLINE, tokenize.COMMENT):
            first_meaningful = t
            break
    stripped_start = lines[start - 1].strip()
    if ('= [' in stripped_start or '= {' in stripped_start) and end - start >= 3:
        skipped_real['list_or_dict_assignment'] += 1
        continue

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
        skipped_real['empty_after_comment_removal'] += 1
        continue

    body = join_parts(code_parts)
    joined_code = indent + body
    comment_lines = [indent + t.string.strip() for t in comment_toks]
    replacement_lines = comment_lines + [joined_code]
    replacement = NL.join(replacement_lines) + NL

    if len(joined_code) > MAX_REAL_CODE_LINE_LEN:
        skipped_real['too_long'] += 1
        continue

    candidate_ranges[start] = (end, replacement)
    real_changed += 1
    real_removed_lines += (end - start + 1) - len(replacement_lines)
    real_moved_comments += len(comment_toks)

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
text_after_real = ''.join(out)

# --- Pass 3B: commented-out bracket continuations -----------------------------

comment_line_re = re.compile(r'^(?P<prefix>[ \t]*#)(?P<body>.*?)(?P<eol>\r?\n?)$')

def split_comment_line(line):
    m = comment_line_re.match(line)
    if not m:
        return None
    return m.group('prefix'), m.group('body'), m.group('eol')

def bracket_delta(s):
    """Return net bracket delta for one code-ish line, ignoring strings and trailing # comments."""
    delta = 0
    stack = []
    i = 0
    n = len(s)
    quote = None
    triple = None
    while i < n:
        c = s[i]
        if quote:
            if c == '\\':
                i += 2
                continue
            if triple:
                if s[i:i+3] == triple:
                    quote = None
                    triple = None
                    i += 3
                    continue
            elif c == quote:
                quote = None
            i += 1
            continue
        if c == '#':
            break
        if c in ('"', "'"):
            if s[i:i+3] in ('"""', "'''"):
                quote = c
                triple = s[i:i+3]
                i += 3
                continue
            quote = c
            i += 1
            continue
        if c in '([{':
            delta += 1
        elif c in ')]}':
            delta -= 1
        i += 1
    return delta

def looks_like_commented_code_start(body):
    s = body.lstrip()
    # Allow one extra comment marker for blocks written like '# #foo(...)'.
    if s.startswith('#'):
        s2 = s[1:].lstrip()
    else:
        s2 = s
    if not s2:
        return False
    if s2.startswith(('<', '-', '*')):
        return False
    # Most commented-out continuation code here is assignment/call/condition style.
    code_keywords = ('if ', 'elif ', 'while ', 'for ', 'return ', 'assert ')
    if s2.startswith(code_keywords):
        return True
    if re.match(r'[A-Za-z_][A-Za-z0-9_\.]*\s*(=|\(|\[|\{|\.)', s2):
        return True
    if s2.startswith(('self.', 'gc.', 'Cy', 'screen.', 'BugUtil.', 'MainOpt.', 'gRect(', 'gSet', 'localText.')):
        return True
    return False

def collapse_commented_body(first_prefix, first_body, bodies):
    # Preserve the leading whitespace after '#' from the first line.
    leading_after_hash = first_body[:len(first_body) - len(first_body.lstrip(' \t'))]
    parts = []
    for b in bodies:
        part = b.strip()
        if part:
            parts.append(part)
    if not parts:
        return None
    body = join_parts(parts)
    return first_prefix + leading_after_hash + body + NL

comment_changed = 0
comment_removed_lines = 0
comment_skipped = {'too_long': 0, 'unbalanced': 0, 'not_code_like': 0}

work_lines = text_after_real.splitlines(True)
out2 = []
i = 0
while i < len(work_lines):
    split = split_comment_line(work_lines[i])
    if not split:
        out2.append(work_lines[i])
        i += 1
        continue
    prefix, body, eol = split
    if bracket_delta(body) <= 0 or not looks_like_commented_code_start(body):
        out2.append(work_lines[i])
        i += 1
        continue

    depth = bracket_delta(body)
    bodies = [body]
    j = i + 1
    has_internal_commented_prose = False
    while j < len(work_lines) and depth > 0:
        split_j = split_comment_line(work_lines[j])
        if not split_j:
            break
        _, body_j, _ = split_j
        # A second '#' after uncommenting the line usually means an explanatory
        # comment inside commented-out code. Keep such blocks multiline so the
        # explanation remains attached to the specific subexpression.
        if body_j.lstrip().startswith('#'):
            has_internal_commented_prose = True
        bodies.append(body_j)
        depth += bracket_delta(body_j)
        j += 1

    if has_internal_commented_prose:
        out2.append(work_lines[i])
        i += 1
        comment_skipped['internal_comment'] = comment_skipped.get('internal_comment', 0) + 1
        continue

    if depth != 0 or j == i + 1:
        out2.append(work_lines[i])
        i += 1
        if depth != 0:
            comment_skipped['unbalanced'] += 1
        continue

    collapsed = collapse_commented_body(prefix, body, bodies)
    if collapsed is None:
        out2.append(work_lines[i])
        i += 1
        continue
    if len(strip_line_ending(collapsed)) > MAX_COMMENTED_CODE_LINE_LEN:
        out2.append(work_lines[i])
        i += 1
        comment_skipped['too_long'] += 1
        continue

    out2.append(collapsed)
    comment_changed += 1
    comment_removed_lines += (j - i - 1)
    i = j

new_text = ''.join(out2)
DST.write_bytes(new_text.encode('utf-8'))

orig_tokens = significant_code_tokens(SRC)
new_tokens = significant_code_tokens(DST)
code_verified = orig_tokens == new_tokens

# Also verify token sequence vs the original uploaded baseline for extra confidence? No: this pass input is second pass.
try:
    py_compile.compile(str(DST), doraise=True)
    py3_compile_ok = True
except Exception as e:
    py3_compile_ok = False
    py3_compile_error = repr(e)

# Diff from pass2 to pass3.
diff = difflib.unified_diff(SRC.read_text(encoding='utf-8', errors='replace').splitlines(True), DST.read_text(encoding='utf-8', errors='replace').splitlines(True), fromfile='CvMainInterface.singleline2.py', tofile='CvMainInterface.singleline3.py',)
DIFF.write_text(''.join(diff), encoding='utf-8')

print('real_code_changed_logical_statements=%d' % real_changed)
print('real_code_moved_comments=%d' % real_moved_comments)
print('real_code_removed_physical_lines=%d' % real_removed_lines)
print('real_code_max_line_len=%d' % MAX_REAL_CODE_LINE_LEN)
print('real_code_skipped=%s' % skipped_real)
print('commented_code_changed_groups=%d' % comment_changed)
print('commented_code_removed_physical_lines=%d' % comment_removed_lines)
print('commented_code_max_line_len=%d' % MAX_COMMENTED_CODE_LINE_LEN)
print('commented_code_skipped=%s' % comment_skipped)
print('code_token_sequence_verified=%s' % code_verified)
print('py3_compile_ok=%s' % py3_compile_ok)
if not code_verified:
    for idx, (a, b) in enumerate(zip(orig_tokens, new_tokens)):
        if a != b:
            print('first_code_token_difference_at=%d orig=%r new=%r' % (idx, a, b))
            break
    raise SystemExit(1)
if not py3_compile_ok:
    print('py3_compile_error=%s' % py3_compile_error)
    raise SystemExit(1)
