#!/usr/bin/env python3
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
# Convert the tracked base-AdvCiv ODT manual into grep/LLM-friendly plain text.

from __future__ import annotations

import argparse
from pathlib import Path
import zipfile
import xml.etree.ElementTree as ET


# <!-- custom: Keep this helper dependency-free so refreshing manual.txt is reproducible from the repository with ordinary Python 3.
# Visual fidelity is intentionally not a goal; the output is for grep/VS Code/LLM search and review. (ChatGPT-5.6-Sol) -->

NS = {
    "office": "urn:oasis:names:tc:opendocument:xmlns:office:1.0",
    "text": "urn:oasis:names:tc:opendocument:xmlns:text:1.0",
    "table": "urn:oasis:names:tc:opendocument:xmlns:table:1.0",
}
TEXT_S = "{%s}s" % NS["text"]
TEXT_TAB = "{%s}tab" % NS["text"]
TEXT_LINE_BREAK = "{%s}line-break" % NS["text"]
TEXT_P = "{%s}p" % NS["text"]
TEXT_H = "{%s}h" % NS["text"]
TEXT_LIST = "{%s}list" % NS["text"]
TEXT_LIST_ITEM = "{%s}list-item" % NS["text"]
TEXT_LIST_HEADER = "{%s}list-header" % NS["text"]
TABLE_TABLE = "{%s}table" % NS["table"]
TABLE_ROW = "{%s}table-row" % NS["table"]
TABLE_CELL = "{%s}table-cell" % NS["table"]
OFFICE_TEXT = "{%s}text" % NS["office"]


def inline_text(elem: ET.Element) -> str:
    """Extract visible inline text, honoring ODT explicit spaces/tabs/line breaks."""
    parts = []
    if elem.text:
        parts.append(elem.text)
    for child in elem:
        if child.tag == TEXT_S:
            count = int(child.attrib.get("{%s}c" % NS["text"], "1"))
            parts.append(" " * count)
        elif child.tag == TEXT_TAB:
            parts.append("\t")
        elif child.tag == TEXT_LINE_BREAK:
            parts.append("\n")
        else:
            parts.append(inline_text(child))
        if child.tail:
            parts.append(child.tail)
    return "".join(parts)


def clean_block(text: str) -> str:
    lines = [line.rstrip() for line in text.replace("\r\n", "\n").replace("\r", "\n").split("\n")]
    return "\n".join(lines).strip()


def emit_blocks(elem: ET.Element, out: list[str], list_depth: int = 0) -> None:
    """Walk document block structure without duplicating paragraphs inside lists/tables."""
    for child in elem:
        if child.tag == TEXT_H:
            value = clean_block(inline_text(child))
            if value:
                out.extend(["", value, ""])
        elif child.tag == TEXT_P:
            value = clean_block(inline_text(child))
            if value:
                out.append(value)
        elif child.tag == TEXT_LIST:
            emit_list(child, out, list_depth)
        elif child.tag == TABLE_TABLE:
            emit_table(child, out)
        else:
            emit_blocks(child, out, list_depth)


def emit_list(list_elem: ET.Element, out: list[str], depth: int) -> None:
    for item in list_elem:
        if item.tag == TEXT_LIST_HEADER:
            # ODT list headers are introductory content, not bulleted list items.
            emit_blocks(item, out, depth)
            continue
        if item.tag != TEXT_LIST_ITEM:
            continue
        first_text = True
        for child in item:
            if child.tag in (TEXT_P, TEXT_H):
                value = clean_block(inline_text(child))
                if value:
                    prefix = ("    " * depth) + ("• " if first_text else "  ")
                    out.append(prefix + value)
                    first_text = False
            elif child.tag == TEXT_LIST:
                emit_list(child, out, depth + 1)
            elif child.tag == TABLE_TABLE:
                emit_table(child, out)
            else:
                emit_blocks(child, out, depth)


def iter_table_rows(elem: ET.Element):
    """Yield rows belonging to this table while leaving nested tables to emit_blocks."""
    for child in elem:
        if child.tag == TABLE_TABLE:
            continue
        if child.tag == TABLE_ROW:
            yield child
        else:
            yield from iter_table_rows(child)


def emit_table(table_elem: ET.Element, out: list[str]) -> None:
    for row in iter_table_rows(table_elem):
        cells = []
        for cell in row:
            if cell.tag != TABLE_CELL:
                continue
            values = []
            emit_blocks(cell, values)
            cells.append([value for value in values if value.strip()])
        if not any(cells):
            continue

        # <!-- custom: Preserve logical paragraph/list boundaries inside ODT table cells instead of flattening them into very long " / "-joined lines.
		# Pair corresponding cell blocks with tabs where possible; continuation blocks remain separate grep/LLM-friendly lines. (ChatGPT-5.6-Sol) -->
        for i in range(max(len(values) for values in cells)):
            columns = [values[i] if i < len(values) else "" for values in cells]
            out.append("\t".join(columns).rstrip())


def normalize_blocks(blocks: list[str]) -> str:
    """Keep logical spacing compact and deterministic for text search."""
    normalized = []
    blank = False
    for block in blocks:
        block = block.rstrip()
        if not block:
            if normalized and not blank:
                normalized.append("")
            blank = True
            continue
        normalized.append(block)
        blank = False
    while normalized and not normalized[-1]:
        normalized.pop()
    return "\n".join(normalized) + "\n"


def convert(input_path: Path, output_path: Path) -> tuple[int, int]:
    with zipfile.ZipFile(input_path, "r") as odt:
        try:
            content_xml = odt.read("content.xml")
        except KeyError as exc:
            raise RuntimeError("%s does not contain ODT content.xml" % input_path) from exc

    root = ET.fromstring(content_xml)
    office_text = root.find(".//office:text", NS)
    if office_text is None:
        raise RuntimeError("Could not find office:text in %s" % input_path)

    blocks: list[str] = []
    emit_blocks(office_text, blocks)
    text = normalize_blocks(blocks)

    # Guard against silently replacing the manual with a broken/tiny extraction.
    if len(text) < 10000 or text.count("\n") < 100:
        raise RuntimeError(
            "Extraction looks unexpectedly small (%d characters, %d lines); refusing to overwrite."
            % (len(text), text.count("\n") + 1)
        )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    # Match the tracked manual.txt's Windows-friendly CRLF convention deterministically.
    with output_path.open("w", encoding="utf-8", newline="\r\n") as f:
        f.write(text)

    return len(text), text.count("\n") + 1


def parse_args() -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parent.parent
    default_dir = repo_root / "_0_Common_Docs" / "AdvCiv_Base_Doc"
    parser = argparse.ArgumentParser(
        description="Convert AdvCiv manual.odt to grep/LLM-friendly manual.txt without external dependencies."
    )
    parser.add_argument("input", nargs="?", type=Path, default=default_dir / "manual.odt")
    parser.add_argument("output", nargs="?", type=Path, default=default_dir / "manual.txt")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not args.input.is_file():
        raise SystemExit("Input ODT not found: %s" % args.input)
    chars, lines = convert(args.input, args.output)
    print("Input:  %s" % args.input)
    print("Output: %s" % args.output)
    print("Wrote:  %d characters, %d lines" % (chars, lines))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
