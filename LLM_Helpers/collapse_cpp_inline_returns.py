#!/usr/bin/env python3
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
# Conservative C++ header cleanup helper.
#
# Collapses simple inline functions whose body is exactly one return statement,
# including return expressions split across multiple physical lines:
#   T f(...) const
#   {
#       return expr;
#   }
# or:
#   T f(...) const {
#       return expr;
#   }
# becomes:
#   T f(...) const { return expr; }
# Opening-brace comments are hoisted above the collapsed function; closing-brace
# tail comments are preserved after the collapsed body.
# and, when safe:
#   template<class T>
#   T f() { return value; }
# becomes:
#   template<class T> T f() { return value; }
#
# This is intentionally separate from collapse_cpp_signatures.py; it rewrites
# function bodies, not just signatures. Review the diff before committing.

from __future__ import annotations

from pathlib import Path
import argparse
from contextlib import nullcontext
from datetime import datetime, timezone
import difflib
import re
import sys

from collapse_cpp_signatures import (
	decode_bytes,
	display_path,
	get_default_repo_root,
	iter_candidate_files,
	split_line_ending,
	stripped_for_length,
)


HEADER_SUFFIXES = {".h", ".hpp", ".inl"}
CONTROL_START_WORDS = {"if", "else", "for", "while", "switch", "catch", "return", "sizeof"}


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


def leading_ws(text: str) -> str:
	return text[: len(text) - len(text.lstrip(" \t"))]


def default_diff_file_path(repo_root: Path, timestamp: str) -> Path:
	return repo_root / "LLM_Helpers" / "outputs" / f"collapse_cpp_inline_returns_{timestamp}.diff.txt"


def resolve_output_file_path(repo_root: Path, value: str | None, default_path: Path) -> Path | None:
	if value is None:
		return None
	if value == "auto":
		return default_path
	path = Path(value)
	return path if path.is_absolute() else repo_root / path


def line_comment_index(text: str) -> int:
	in_string = None
	escaped = False
	for index, ch in enumerate(text):
		if in_string is not None:
			if escaped:
				escaped = False
			elif ch == "\\":
				escaped = True
			elif ch == in_string:
				in_string = None
			continue
		if ch in ('"', "'"):
			in_string = ch
			continue
		if ch == "/" and index + 1 < len(text) and text[index + 1] == "/":
			return index
	return -1


def split_tail_comment(text: str) -> tuple[str, str | None]:
	index = line_comment_index(text)
	if index < 0:
		return text.rstrip(), None
	return text[:index].rstrip(), text[index + 2 :].rstrip()


def append_line_comment(text: str, comment: str | None) -> str:
	if comment is None:
		return text
	return text + " //" + comment


def brace_line_comment(text: str, brace: str) -> tuple[bool, str | None]:
	code, comment = split_tail_comment(text)
	return code.strip() == brace, comment


def one_line_block_comment(text: str) -> str:
	body = text.strip()
	if body.startswith("/*"):
		body = body[2:]
	if body.endswith("*/"):
		body = body[:-2]
	return "/* " + " ".join(body.split()) + " */"


def collect_leading_body_comments(bodies: list[str], start_index: int) -> tuple[list[str], int] | None:
	comments: list[str] = []
	index = start_index
	while index < len(bodies):
		stripped = bodies[index].strip()
		if stripped.startswith("//"):
			comments.append(stripped)
			index += 1
			continue
		if stripped.startswith("/*"):
			block_lines = [stripped]
			while not block_lines[-1].endswith("*/"):
				index += 1
				if index >= len(bodies):
					return None
				next_line = bodies[index].strip()
				if line_comment_index(next_line) >= 0:
					return None
				block_lines.append(next_line)
			comments.append(one_line_block_comment(" ".join(block_lines)))
			index += 1
			continue
		break
	return comments, index


def collect_leading_asserts(bodies: list[str], start_index: int) -> tuple[list[str], int]:
	asserts: list[str] = []
	index = start_index
	while index < len(bodies):
		stripped = bodies[index].strip()
		if not stripped.startswith(("FAssert(", "FAssertMsg(", "FAssertBounds(", "FAssertEnumBounds(")):
			break
		code, comment = split_tail_comment(stripped)
		if comment is not None or "/*" in code or "*/" in code or not code.strip().endswith(";"):
			break
		asserts.append(code.strip())
		index += 1
	return asserts, index


def open_brace_comments(text: str) -> tuple[bool, list[str] | None]:
	code, line_comment = split_tail_comment(text)
	if code.strip() == "{":
		return True, ["//" + line_comment] if line_comment is not None else []
	stripped = text.strip()
	if stripped.startswith("{"):
		after_brace = stripped[1:].strip()
		if after_brace.startswith("/*"):
			return True, [after_brace]
	return False, None


def block_comment_start_flags(bodies: list[str]) -> list[bool]:
	flags: list[bool] = []
	in_block = False
	for body in bodies:
		flags.append(in_block)
		index = 0
		line_comment = line_comment_index(body)
		scan_end = line_comment if line_comment >= 0 else len(body)
		while index < scan_end:
			if in_block:
				end = body.find("*/", index, scan_end)
				if end < 0:
					break
				in_block = False
				index = end + 2
				continue
			start = body.find("/*", index, scan_end)
			if start < 0:
				break
			end = body.find("*/", start + 2, scan_end)
			if end < 0:
				in_block = True
				break
			index = end + 2
	return flags


def signature_start_is_safe(text: str) -> bool:
	stripped = text.strip()
	if not stripped or stripped.startswith(("//", "/*", "*", "#")):
		return False
	if stripped.count("/*") != stripped.count("*/"):
		return False
	if any(stripped.startswith(word + " ") or stripped.startswith(word + "(") for word in CONTROL_START_WORDS):
		return False
	if "(" not in stripped or ")" not in stripped:
		return False
	if stripped.endswith(("{", ";", ":")):
		return False
	prefix = stripped.split("(", 1)[0].strip()
	if not prefix or "=" in prefix or "." in prefix or "->" in prefix:
		return False
	if re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", prefix):
		# Constructor/destructor bodies need a separate review pass.
		return False
	return True


def join_cpp_fragments(parts: list[str]) -> str:
	if not parts:
		return ""
	result = parts[0]
	for part in parts[1:]:
		if result.endswith(("::", "->", ".", "(", "[", "{")) or part.startswith(("::", "->", ".", ")", "]", "}", ",", ";")):
			result += part
		else:
			result += " " + part
	return result


def return_payload_from_lines(bodies: list[str], start_index: int) -> tuple[list[str], list[str], str, int] | None:
	comment_result = collect_leading_body_comments(bodies, start_index)
	if comment_result is None:
		return None
	comments, start_index = comment_result
	asserts, start_index = collect_leading_asserts(bodies, start_index)
	parts: list[str] = []
	index = start_index
	while index < len(bodies):
		stripped = bodies[index].strip()
		if not stripped:
			return None
		if index == start_index:
			if not stripped.startswith("return "):
				return None
		elif stripped.startswith(("return ", "{", "}")):
			return None
		code, comment = split_tail_comment(stripped)
		if "/*" in code or "*/" in code:
			return None
		if comment is not None and not code.endswith(";"):
			return None
		if comment is not None:
			comments.append("//" + comment)
		parts.append(code.strip())
		if code.strip().endswith(";"):
			return comments, asserts, join_cpp_fragments(parts), index
		index += 1
	return None


def collapsed_body(asserts: list[str], return_payload: str) -> str:
	body_parts = asserts + [return_payload]
	return " ".join(body_parts)


def try_rewrite(bodies: list[str], eols: list[str], block_flags: list[bool], index: int, max_line_len: int) -> Rewrite | None:
	if index + 2 >= len(bodies):
		return None
	if block_flags[index]:
		return None
	signature = bodies[index]
	signature_code, signature_comment = split_tail_comment(signature)
	signature_body = signature_code.rstrip()
	if signature_body.endswith("{"):
		return_result = return_payload_from_lines(bodies, index + 1)
		if return_result is None:
			return None
		return_comments, return_asserts, return_payload, return_end = return_result
		if return_end + 1 >= len(bodies):
			return None
		if any(block_flags[index : return_end + 2]):
			return None
		close_brace = bodies[return_end + 1]
		close_ok, close_comment = brace_line_comment(close_brace, "}")
		if not close_ok:
			return None
		if leading_ws(close_brace) != leading_ws(signature):
			return None
		signature_before_brace = signature_body[:-1].rstrip()
		if not signature_start_is_safe(signature_before_brace):
			return None
		collapsed = signature_before_brace.strip() + " { " + collapsed_body(return_asserts, return_payload) + " }"
		collapsed = append_line_comment(collapsed, signature_comment)
		collapsed = append_line_comment(collapsed, close_comment)
		if len(stripped_for_length(collapsed)) > max_line_len:
			return None
		replacement = [leading_ws(signature) + comment + eols[index] for comment in return_comments]
		replacement.append(leading_ws(signature) + collapsed + eols[index])
		return Rewrite(index, return_end + 1, replacement)
	if index + 3 >= len(bodies):
		return None
	open_brace = bodies[index + 1]
	open_ok, open_comments = open_brace_comments(open_brace)
	if leading_ws(open_brace) != leading_ws(signature) or not open_ok or open_comments is None:
		return None
	if open_comments and open_comments[0].startswith("/*") and not open_comments[0].endswith("*/"):
		comment_lines = [open_comments[0]]
		comment_index = index + 2
		while not comment_lines[-1].strip().endswith("*/"):
			if comment_index >= len(bodies):
				return None
			comment_lines.append(bodies[comment_index].strip())
			comment_index += 1
		open_comments = [one_line_block_comment(" ".join(comment_lines))]
		return_result = return_payload_from_lines(bodies, comment_index)
		if return_result is None:
			return None
		return_comments, return_asserts, return_payload, return_end = return_result
	else:
		return_result = return_payload_from_lines(bodies, index + 2)
		if return_result is None:
			return None
		return_comments, return_asserts, return_payload, return_end = return_result
	if return_end + 1 >= len(bodies):
		return None
	if block_flags[index] or block_flags[return_end] or block_flags[return_end + 1]:
		return None
	close_brace = bodies[return_end + 1]
	close_ok, close_comment = brace_line_comment(close_brace, "}")
	if not close_ok:
		return None
	if leading_ws(close_brace) != leading_ws(signature):
		return None
	if not signature_start_is_safe(signature_code):
		return None
	collapsed = signature_code.strip() + " { " + collapsed_body(return_asserts, return_payload) + " }"
	collapsed = append_line_comment(collapsed, signature_comment)
	collapsed = append_line_comment(collapsed, close_comment)
	if len(stripped_for_length(collapsed)) > max_line_len:
		return None
	replacement = []
	for comment in open_comments + return_comments:
		replacement.append(leading_ws(signature) + comment + eols[index])
	replacement.append(leading_ws(signature) + collapsed + eols[index])
	return Rewrite(index, return_end + 1, replacement)


def is_template_prefix_line(text: str) -> bool:
	stripped = text.strip()
	if not stripped.startswith("template<") or not stripped.endswith(">"):
		return False
	if line_comment_index(stripped) >= 0 or "/*" in stripped or "*/" in stripped:
		return False
	return True


def is_collapsed_inline_return_line(text: str) -> bool:
	code, _comment = split_tail_comment(text)
	stripped = code.strip()
	return " { return " in stripped and stripped.endswith("}")


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
		template_line = bodies[index]
		inline_line = bodies[index + 1]
		if block_flags[index] or block_flags[index + 1]:
			new_lines.append(lines[index])
			index += 1
			continue
		if not is_template_prefix_line(template_line) or not is_collapsed_inline_return_line(inline_line):
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
		new_lines.append(collapsed + eols[index])
		count += 1
		index += 2
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
	parser = argparse.ArgumentParser(description="Collapse simple one-return inline C++ header functions.")
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
			parts.append(f"{stats.functions} inline return collapse(s)")
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
		print("No simple inline return collapses found.")
		if diff_file_path is not None and diff_file_path.exists() and diff_file_path.stat().st_size == 0:
			diff_file_path.unlink()
	return 0


if __name__ == "__main__":
	raise SystemExit(main(sys.argv[1:]))
