#!/usr/bin/env python3
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
# Conservative C++ header cleanup helper.
#
# Collapses simple inline functions whose body is exactly one non-return
# statement, optionally preceded by whole-line comments:
#   T f()
#   {
#       // rationale
#       statement;
#   }
# becomes:
#   // rationale
#   T f() { statement; }
#
# This intentionally complements collapse_cpp_inline_returns.py. Review the diff
# before committing.

from __future__ import annotations

from pathlib import Path
import argparse
from contextlib import nullcontext
from datetime import datetime, timezone
import difflib
import re
import sys

from collapse_cpp_inline_returns import (
	HEADER_SUFFIXES,
	append_line_comment,
	block_comment_start_flags,
	brace_line_comment,
	leading_ws,
	line_comment_index,
	signature_start_is_safe,
	split_tail_comment,
)
from collapse_cpp_signatures import (
	decode_bytes,
	display_path,
	get_default_repo_root,
	iter_candidate_files,
	split_line_ending,
	stripped_for_length,
)


class Rewrite:
	def __init__(self, start: int, end: int, lines: list[str]):
		self.start = start
		self.end = end
		self.lines = lines


class Stats:
	def __init__(self):
		self.files_scanned = 0
		self.files_changed = 0
		self.functions = 0
		self.template_prefixes = 0


def default_diff_file_path(repo_root: Path, timestamp: str) -> Path:
	return repo_root / "LLM_Helpers" / "outputs" / f"collapse_cpp_inline_statements_{timestamp}.diff.txt"


def resolve_output_file_path(repo_root: Path, value: str | None, default_path: Path) -> Path | None:
	if value is None:
		return None
	if value == "auto":
		return default_path
	path = Path(value)
	return path if path.is_absolute() else repo_root / path


def is_whole_line_comment(text: str) -> bool:
	stripped = text.strip()
	return stripped.startswith("//") and "/*" not in stripped and "*/" not in stripped


def is_simple_statement(text: str) -> bool:
	stripped = text.strip()
	if not stripped:
		return False
	code, comment = split_tail_comment(stripped)
	if comment is not None:
		stripped = code.strip()
	if not stripped.endswith(";"):
		return False
	if stripped.startswith(("return ", "if ", "if(", "for ", "for(", "while ", "while(", "switch ", "switch(", "else", "do ", "try", "catch", "case ", "default:")):
		return False
	if stripped.startswith(("delete ", "delete[]", "throw ", "SAFE_DELETE", "SAFE_DELETE_ARRAY")):
		return False
	if line_comment_index(stripped) >= 0 or "/*" in stripped or "*/" in stripped:
		return False
	if "{" in stripped or "}" in stripped:
		return False
	return True


def signature_is_safe_statement_function(text: str) -> bool:
	if not signature_start_is_safe(text):
		return False
	stripped = text.strip()
	if stripped.startswith(":"):
		return False
	if "~" in stripped:
		return False
	if re.search(r"\)\s*:", stripped):
		return False
	return True


def collect_comments_and_statement(bodies: list[str], start_index: int) -> tuple[list[str], str, int] | None:
	comments: list[str] = []
	index = start_index
	while index < len(bodies) and is_whole_line_comment(bodies[index]):
		comments.append(bodies[index].strip())
		index += 1
	if index >= len(bodies) or not is_simple_statement(bodies[index]):
		return None
	code, comment = split_tail_comment(bodies[index].strip())
	if comment is not None:
		comments.append("//" + comment)
	return comments, code.strip(), index


def try_rewrite(bodies: list[str], eols: list[str], block_flags: list[bool], index: int, max_line_len: int) -> Rewrite | None:
	if index + 2 >= len(bodies) or block_flags[index]:
		return None
	signature = bodies[index]
	signature_code, signature_comment = split_tail_comment(signature)
	signature_body = signature_code.rstrip()
	if signature_body.endswith("{"):
		result = collect_comments_and_statement(bodies, index + 1)
		if result is None:
			return None
		comments, statement, statement_index = result
		if statement_index + 1 >= len(bodies) or any(block_flags[index : statement_index + 2]):
			return None
		close_brace = bodies[statement_index + 1]
		close_ok, close_comment = brace_line_comment(close_brace, "}")
		if not close_ok or leading_ws(close_brace) != leading_ws(signature):
			return None
		signature_before_brace = signature_body[:-1].rstrip()
		if not signature_is_safe_statement_function(signature_before_brace):
			return None
		collapsed = signature_before_brace.strip() + " { " + statement + " }"
		collapsed = append_line_comment(collapsed, signature_comment)
		collapsed = append_line_comment(collapsed, close_comment)
		if len(stripped_for_length(collapsed)) > max_line_len:
			return None
		replacement = [leading_ws(signature) + comment + eols[index] for comment in comments]
		replacement.append(leading_ws(signature) + collapsed + eols[index])
		return Rewrite(index, statement_index + 1, replacement)
	if index + 3 >= len(bodies):
		return None
	open_brace = bodies[index + 1]
	open_ok, open_comment = brace_line_comment(open_brace, "{")
	if leading_ws(open_brace) != leading_ws(signature) or not open_ok:
		return None
	result = collect_comments_and_statement(bodies, index + 2)
	if result is None:
		return None
	comments, statement, statement_index = result
	if statement_index + 1 >= len(bodies) or any(block_flags[index : statement_index + 2]):
		return None
	close_brace = bodies[statement_index + 1]
	close_ok, close_comment = brace_line_comment(close_brace, "}")
	if not close_ok or leading_ws(close_brace) != leading_ws(signature):
		return None
	if not signature_is_safe_statement_function(signature_code):
		return None
	collapsed = signature_code.strip() + " { " + statement + " }"
	collapsed = append_line_comment(collapsed, signature_comment)
	collapsed = append_line_comment(collapsed, close_comment)
	if len(stripped_for_length(collapsed)) > max_line_len:
		return None
	replacement = []
	if open_comment is not None:
		replacement.append(leading_ws(signature) + "//" + open_comment + eols[index])
	replacement.extend(leading_ws(signature) + comment + eols[index] for comment in comments)
	replacement.append(leading_ws(signature) + collapsed + eols[index])
	return Rewrite(index, statement_index + 1, replacement)


def is_template_prefix_line(text: str) -> bool:
	stripped = text.strip()
	return stripped.startswith("template<") and stripped.endswith(">") and line_comment_index(stripped) < 0 and "/*" not in stripped and "*/" not in stripped


def is_collapsed_inline_statement_line(text: str) -> bool:
	code, _comment = split_tail_comment(text)
	stripped = code.strip()
	return " { " in stripped and stripped.endswith("}") and " { return " not in stripped


def collapse_template_prefixes(lines: list[str], max_line_len: int) -> tuple[list[str], int]:
	bodies: list[str] = []
	eols: list[str] = []
	for line in lines:
		body, eol = split_line_ending(line)
		bodies.append(body)
		eols.append(eol)
	block_flags = block_comment_start_flags(bodies)
	new_lines: list[str] = []
	count = 0
	index = 0
	while index < len(lines):
		if index + 1 >= len(lines):
			new_lines.append(lines[index])
			index += 1
			continue
		if block_flags[index] or block_flags[index + 1]:
			new_lines.append(lines[index])
			index += 1
			continue
		template_line = bodies[index]
		if not is_template_prefix_line(template_line):
			new_lines.append(lines[index])
			index += 1
			continue
		comment_index = index + 1
		comments: list[str] = []
		while comment_index < len(lines) and not block_flags[comment_index] and is_whole_line_comment(bodies[comment_index]):
			if leading_ws(template_line) != leading_ws(bodies[comment_index]):
				break
			comments.append(bodies[comment_index].strip())
			comment_index += 1
		if comment_index >= len(lines) or block_flags[comment_index]:
			new_lines.append(lines[index])
			index += 1
			continue
		inline_line = bodies[comment_index]
		if not is_collapsed_inline_statement_line(inline_line):
			new_lines.append(lines[index])
			index += 1
			continue
		if leading_ws(template_line) != leading_ws(inline_line):
			new_lines.append(lines[index])
			index += 1
			continue
		collapsed = leading_ws(template_line) + template_line.strip() + " " + inline_line.strip()
		if len(stripped_for_length(collapsed)) > max_line_len:
			new_lines.append(lines[index])
			index += 1
			continue
		new_lines.extend(leading_ws(template_line) + comment + eols[index] for comment in comments)
		new_lines.append(collapsed + eols[index])
		count += 1
		index = comment_index + 1
	return new_lines, count


def rewrite_lines(lines: list[str], max_line_len: int) -> tuple[list[str], Stats]:
	bodies: list[str] = []
	eols: list[str] = []
	for line in lines:
		body, eol = split_line_ending(line)
		bodies.append(body)
		eols.append(eol)
	block_flags = block_comment_start_flags(bodies)
	stats = Stats()
	new_lines: list[str] = []
	index = 0
	while index < len(lines):
		rewrite = try_rewrite(bodies, eols, block_flags, index, max_line_len)
		if rewrite is None:
			new_lines.append(lines[index])
			index += 1
			continue
		new_lines.extend(rewrite.lines)
		stats.functions += 1
		index = rewrite.end + 1
	new_lines, template_prefixes = collapse_template_prefixes(new_lines, max_line_len)
	stats.template_prefixes += template_prefixes
	return new_lines, stats


def process_file(repo_root: Path, path: Path, args: argparse.Namespace, stats: Stats, diff_file) -> bool:
	relative_path = path.relative_to(repo_root)
	if path.suffix.lower() not in HEADER_SUFFIXES:
		return False
	raw = path.read_bytes()
	encoding, text = decode_bytes(raw)
	lines = text.splitlines(keepends=True)
	new_lines, file_stats = rewrite_lines(lines, args.max_line_len)
	stats.files_scanned += 1
	if file_stats.functions == 0 and file_stats.template_prefixes == 0:
		return False
	old_text = "".join(lines)
	new_text = "".join(new_lines)
	if old_text == new_text:
		return False
	stats.files_changed += 1
	stats.functions += file_stats.functions
	stats.template_prefixes += file_stats.template_prefixes
	if diff_file is not None:
		diff_file.writelines(
			difflib.unified_diff(
				lines,
				new_lines,
				fromfile=f"{relative_path.as_posix()} (before)",
				tofile=f"{relative_path.as_posix()} (after)",
				lineterm="\n",
			)
		)
	if args.in_place:
		path.write_text(new_text, encoding=encoding, newline="")
	return True


def parse_args(argv: list[str]) -> argparse.Namespace:
	parser = argparse.ArgumentParser(description="Collapse simple one-statement inline C++ header functions.")
	parser.add_argument("paths", nargs="*", type=Path, help="files or directories to scan; defaults to tracked C/C++ files under CvGameCoreDLL")
	parser.add_argument("--repo-root", type=Path, default=get_default_repo_root())
	parser.add_argument("--in-place", action="store_true", help="write changes instead of only reporting")
	parser.add_argument("--diff-file", nargs="?", const="auto", default=None, help="write review diff to a file; without a path, writes a timestamped diff under LLM_Helpers/outputs/")
	parser.add_argument("--max-line-len", type=int, default=600)
	return parser.parse_args(argv)


def main(argv: list[str]) -> int:
	args = parse_args(argv)
	repo_root = args.repo_root.resolve()
	timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
	diff_file_path = resolve_output_file_path(repo_root, args.diff_file, default_diff_file_path(repo_root, timestamp))
	if diff_file_path is not None:
		diff_file_path.parent.mkdir(parents=True, exist_ok=True)
	stats = Stats()
	changed: list[Path] = []
	diff_context = diff_file_path.open("w", encoding="utf-8", newline="") if diff_file_path is not None else nullcontext(None)
	with diff_context as diff_file:
		for path in iter_candidate_files(repo_root, args.paths):
			if process_file(repo_root, path, args, stats, diff_file):
				changed.append(path.relative_to(repo_root))
	print(f"Scanned {stats.files_scanned} C/C++ header-like file(s).")
	if changed:
		verb = "Updated" if args.in_place else "Would update"
		parts = []
		if stats.functions:
			parts.append(f"{stats.functions} inline statement collapse(s)")
		if stats.template_prefixes:
			parts.append(f"{stats.template_prefixes} template prefix join(s)")
		print(f"{verb} {len(changed)} file(s): {', '.join(parts)}.")
		for path in changed:
			print(f"  - {display_path(repo_root, repo_root / path)}")
		if diff_file_path is not None:
			print(f"Diff file written: {display_path(repo_root, diff_file_path)}")
		if not args.in_place:
			print("Run again with --in-place after reviewing the target list/diff.")
			return 1
	else:
		print("No simple inline statement collapses found.")
		if diff_file_path is not None and diff_file_path.exists() and diff_file_path.stat().st_size == 0:
			diff_file_path.unlink()
	return 0


if __name__ == "__main__":
	raise SystemExit(main(sys.argv[1:]))
