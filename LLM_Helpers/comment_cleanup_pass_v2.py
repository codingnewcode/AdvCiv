#!/usr/bin/env python3
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
from pathlib import Path
import re
import difflib
import tokenize
import io

SRC = Path('/mnt/data/CvMainInterface.singleline3.py')
DST = Path('/mnt/data/CvMainInterface.commentcleanup.py')
DIFF = Path('/mnt/data/CvMainInterface.commentcleanup.diff')

TYPO_REPLACEMENTS = [
    ('throught', 'thought'),
    ('swappend', 'swapped'),
    ('uneeded', 'unneeded'),
    ('everytime', 'every time'),
    ('b/c', 'because'),
    ('prob the column', 'probably the column'),
    ('move up the Bonuses up', 'move the Bonuses up'),
]
CODEISH_RE = re.compile(r"^(?:if|elif|else|for|while|try|except|return|continue|break|global|import|from|def|class)\b|^[A-Za-z_][A-Za-z0-9_]*(?:\s*=|\s*\(|\.)")
SENTENCE_END_RE = re.compile(r"(?:[.!?]|\.\.\.|[\)\]'\"])[\s]*$")

def significant_tokens(text):
    toks=[]
    for tok in tokenize.generate_tokens(io.StringIO(text).readline):
        if tok.type in (tokenize.COMMENT, tokenize.NL, tokenize.NEWLINE, tokenize.INDENT, tokenize.DEDENT, tokenize.ENDMARKER):
            continue
        toks.append((tok.type, tok.string))
    return toks

def is_plain_comment_line(line):
    stripped = line.lstrip(' \t')
    return stripped.startswith('# ')

def split_comment(line):
    indent = line[:len(line) - len(line.lstrip(' \t'))]
    content = line.lstrip(' \t')[2:]
    return indent, content

def fix_typos(line):
    if not line.lstrip(' \t').startswith('#'):
        return line,0
    n=line
    c=0
    for old,new in TYPO_REPLACEMENTS:
        if old in n:
            n=n.replace(old,new)
            c+=1
    return n,c

def skip_content(content):
    s=content.strip()
    if not s:
        return True
    if '<!-- custom' in s or '-->' in s:
        return True
    if s.startswith(('BUG -','<','</','SF CHANGE','Ex:','Format:','Step ', 'TODO', 'NB:')):
        return True
    if re.match(r'^[0-9]+\)', s) or s.startswith(('-', '*')):
        return True
    if CODEISH_RE.match(s):
        return True
    if ' #' in s:
        return True
    return False

def smart_join(parts):
    out=''
    prev=''
    for part in parts:
        part=part.strip()
        if not out:
            out=part; prev=part; continue
        # Preserve wrapped-sentence behavior; insert a period only when previous line looks complete and the next is a new sentence.
        if prev and not SENTENCE_END_RE.search(prev) and part[:1].isupper() and not prev.endswith(('and','or','the','a','an','to','of','in','for','with','that','than','as','is','are','be','adding')):
            out += '. ' + part
        else:
            out += ' ' + part
        prev=part
    return out

def maybe_flush_group(out, indent, group):
    if not group:
        return 0
    # Do not collapse lowercase-start groups; they are often continuation after an intentionally separate line.
    first=group[0].strip()
    if len(group) > 1 and (first[:1].isupper() or first.startswith(('(', 'advc.'))) and len(indent + '# ' + smart_join(group)) <= 260:
        out.append(indent + '# ' + smart_join(group))
        return 1
    else:
        for c in group:
            out.append(indent + '# ' + c)
        return 0

def transform(text):
    src_lines=text.split('\n')
    ends=text.endswith('\n')
    if ends:
    	src_lines=src_lines[:-1]
    lines=[]
    typos=0
    for l in src_lines:
        n,c=fix_typos(l); lines.append(n)
        typos+=c
    out=[]
    collapsed=0
    i=0
    while i < len(lines):
        if not is_plain_comment_line(lines[i]):
            out.append(lines[i]); i+=1; continue
        # top copyright/license-ish header: leave structured
        if i < 20:
            out.append(lines[i]); i+=1; continue
        indent, _ = split_comment(lines[i])
        # collect same-indent plain comments only
        j=i
        contents=[]
        raw=[]
        while j < len(lines) and is_plain_comment_line(lines[j]) and split_comment(lines[j])[0] == indent:
            _ind, cont=split_comment(lines[j]); contents.append(cont); raw.append(lines[j])
            j+=1
        # Process only short-ish runs; long prose/doc blocks are intentionally multiline.
        if len(contents) < 2 or len(contents) > 6:
            out.extend(raw); i=j; continue
        group=[]
        any_collapse=0
        for cont in contents:
            s=cont.strip()
            if skip_content(cont):
                any_collapse += maybe_flush_group(out, indent, group)
                group=[]
                out.append(indent + '# ' + cont)
                continue
            group.append(cont)
            # Flush at natural sentence boundary if group has at least 2 lines; avoids giant multi-sentence comments.
            if SENTENCE_END_RE.search(s):
                if len(group) == 1:
                    # Single complete sentence/label; keep it separate so following lowercase continuation lines do not get merged into it.
                    out.append(indent + '# ' + group[0]); group=[]
                elif len(group) >= 2:
                    any_collapse += maybe_flush_group(out, indent, group)
                    group=[]
        any_collapse += maybe_flush_group(out, indent, group)
        collapsed += any_collapse
        i=j
    result='\n'.join(out)
    if ends:
    	result+='\n'
    return result,typos,collapsed

raw=SRC.read_bytes(); use_crlf=b'\r\n' in raw
text=raw.decode('utf-8').replace('\r\n','\n').replace('\r','\n')
new,typos,collapsed=transform(text)
if significant_tokens(text)!=significant_tokens(new):
    raise SystemExit('ERROR code tokens changed')
DST.write_bytes((new.replace('\n','\r\n') if use_crlf else new).encode('utf-8'))
DIFF.write_text(''.join(difflib.unified_diff(text.splitlines(True), new.splitlines(True), fromfile='CvMainInterface.singleline3.py', tofile='CvMainInterface.commentcleanup.py')), encoding='utf-8')
print('typo replacements',typos)
print('comment groups collapsed',collapsed)
