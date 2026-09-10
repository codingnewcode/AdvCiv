#!/usr/bin/env python3
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
import argparse
import csv
import io
import os
import re
from datetime import datetime, timezone

HEADER_FIELD = "Field"
SUMMARY_PREFIX = "Summary "
INCREMENTS_YEARS_PREFIX = "Increments Years "
INCREMENTS_MONTHS_PREFIX = "Increments Months "
CHART_END_MARKER = "SAS_SEVOPEDIA_GAME_SPEED_CHART_END"
FALLBACK_SPEED_ORDER = ["Nitro", "Turbo", "Quick", "Normal", "Epic", "Marathon", "Slow", "Very Slow",]
CELL_RE = re.compile(r"^T(?P<turn>\d+)=(?P<year>[+-]?\d+)(?:m(?P<month>\d+))?\s*\((?P<inc>\d+)\)$")
SUMMARY_RE = re.compile(r"^Summary\s+(?P<idx>\d+)\s*\((?P<pct>\d+)\)")
INCREMENTS_RE = re.compile(r"^Increments\s+(?P<kind>Years|Months)\s+(?P<idx>\d+)")

def normalize_speed_name(name):
    base = name.split("(", 1)[0]
    base = base.strip().lower()
    return re.sub(r"\s+", "", base)

def parse_csv_line(line):
    try:
        return next(csv.reader(io.StringIO(line), skipinitialspace=True))
    except Exception:
        return None

def parse_summary_cell(cell):
    m = CELL_RE.match(cell.strip())
    if not m:
        return None
    turn = int(m.group("turn"))
    year = int(m.group("year"))
    month = int(m.group("month") or "1")
    inc = int(m.group("inc"))
    total_months = year * 12 + (month - 1)
    return {"turn": turn, "year": year, "month": month, "inc": inc, "total_months": total_months}

def parse_summary_label(label):
    m = SUMMARY_RE.match(label.strip())
    if not m:
        return None
    return int(m.group("idx")), int(m.group("pct"))

def parse_increments_label(label):
    m = INCREMENTS_RE.match(label.strip())
    if not m:
        return None
    return m.group("kind"), int(m.group("idx"))

def _finalize_block(rows):
    if not rows:
        return None

    header = None
    for row in rows:
        second = row[1].strip() if len(row) > 1 else ""
        if second == HEADER_FIELD:
            header = row
            break

    if header is None:
        # Fallback for log formats that omit the explicit CSV header row.
        # Use standard speed ordering and size from first data row.
        width = len(rows[0])
        header = ["", HEADER_FIELD]
        for i in range(2, width):
            j = i - 2
            if j < len(FALLBACK_SPEED_ORDER):
                header.append(FALLBACK_SPEED_ORDER[j])
            else:
                header.append("Speed%d" % (j + 1))

    summaries = []
    increments = []
    for row in rows:
        second = row[1].strip() if len(row) > 1 else ""
        if second.startswith(SUMMARY_PREFIX):
            summaries.append(row)
        if second.startswith(INCREMENTS_YEARS_PREFIX) or second.startswith(INCREMENTS_MONTHS_PREFIX):
            increments.append(row)

    if not summaries:
        return None
    return {"headers": header, "summaries": summaries, "increments": increments}

def extract_latest_summary_block(log_path):
    latest = None
    current_rows = []

    with io.open(log_path, "r", encoding="utf-8", errors="replace") as f:
        for raw in f:
            line = raw.rstrip("\r\n")

            if CHART_END_MARKER in line:
                block = _finalize_block(current_rows)
                if block:
                    latest = block
                current_rows = []
                continue

            row = parse_csv_line(line)
            if not row or len(row) < 3:
                continue
            current_rows.append(row)

    if latest is None:
        block = _finalize_block(current_rows)
        if block:
            latest = block
    return latest

def resolve_speed_column(headers, requested_name):
    requested = normalize_speed_name(requested_name)
    candidates = []
    for i in range(2, len(headers)):
        raw = headers[i].strip()
        norm = normalize_speed_name(raw)
        candidates.append((i, raw, norm))

    exact = [c for c in candidates if c[2] == requested]
    if exact:
        return exact[0][0], exact[0][1]

    starts = [c for c in candidates if c[2].startswith(requested)]
    if len(starts) == 1:
        return starts[0][0], starts[0][1]

    contains = [c for c in candidates if requested in c[2]]
    if len(contains) == 1:
        return contains[0][0], contains[0][1]

    return None, None

def format_delta_months(delta_months):
    sign = "+" if delta_months >= 0 else "-"
    v = abs(delta_months)
    return "%s%dy%dm" % (sign, v // 12, v % 12)

def format_delta_vs_increment(delta_months, increment_months):
    if increment_months <= 0:
        return "n/a", "n/a"
    ratio = float(delta_months) / float(increment_months)
    return "%+.2f" % ratio, "%+.1f%%" % (ratio * 100.0)

def split_tokens(cell):
    text = cell.strip()
    if not text:
        return []
    parts = [p.strip() for p in text.split(",")]
    return [p for p in parts if p]

def flatten_increments(rows, col, kind):
    picked = []
    for row in rows:
        label = row[1].strip() if len(row) > 1 else ""
        info = parse_increments_label(label)
        if not info:
            continue
        row_kind, idx = info
        if row_kind != kind:
            continue
        if len(row) <= col:
            continue
        picked.append((idx, split_tokens(row[col])))
    picked.sort(key=lambda x: x[0])
    out = []
    for _idx, tokens in picked:
        out.extend(tokens)
    return out

def make_slug(name):
    s = normalize_speed_name(name)
    s = re.sub(r"[^a-z0-9_]+", "_", s)
    return s.strip("_") or "speed"

def run(args):
    if not os.path.isfile(args.log):
        raise SystemExit("Log file not found: %s" % args.log)

    block = extract_latest_summary_block(args.log)
    if not block:
        raise SystemExit("No Sevopedia game speed summary block found in log.")

    headers = block["headers"]
    rows = block["summaries"]
    increments_rows = block.get("increments", [])

    normal_col, normal_name = resolve_speed_column(headers, args.normal_speed)
    target_col, target_name = resolve_speed_column(headers, args.speed)

    if normal_col is None:
        raise SystemExit("Could not resolve normal speed column: %s" % args.normal_speed)
    if target_col is None:
        raise SystemExit("Could not resolve target speed column: %s" % args.speed)

    parsed = []
    for row in rows:
        if len(row) <= max(normal_col, target_col):
            continue

        info = parse_summary_label(row[1].strip())
        if not info:
            continue
        idx, pct = info
        if args.summary is not None and idx != args.summary:
            continue

        norm = parse_summary_cell(row[normal_col])
        targ = parse_summary_cell(row[target_col])
        if not norm or not targ:
            continue

        parsed.append((idx, pct, norm, targ, targ["total_months"] - norm["total_months"]))

    if not parsed:
        raise SystemExit("No matching summary rows found.")

    lines = []
    lines.append("Log: %s" % args.log)
    lines.append("Compare: %s vs %s" % (target_name, normal_name))
    lines.append("")
    lines.append("Idx  Pct  Normal                Target                Delta     Delta/Inc    d(Inc)  Delta%Inc")
    lines.append("---  ---  --------------------  --------------------  --------  ---------  --------  ---------")

    prev_ratio = None
    for idx, pct, norm, targ, d in parsed:
        n = "T%d=%+dm%d (%d)" % (norm["turn"], norm["year"], norm["month"], norm["inc"])
        t = "T%d=%+dm%d (%d)" % (targ["turn"], targ["year"], targ["month"], targ["inc"])
        ratio_text, pct_text = format_delta_vs_increment(d, targ["inc"])
        ratio_val = None
        if targ["inc"] > 0:
            ratio_val = float(d) / float(targ["inc"])
        d_ratio_text = "n/a"
        if ratio_val is not None and prev_ratio is not None:
            d_ratio_text = "%+.2f" % (ratio_val - prev_ratio)
        lines.append("%3d  %3d  %-20s  %-20s  %-8s  %9s  %8s  %9s" % (idx, pct, n, t, format_delta_months(d), ratio_text, d_ratio_text, pct_text))
        if ratio_val is not None:
            prev_ratio = ratio_val

    # Increment comparison section
    n_years = flatten_increments(increments_rows, normal_col, "Years")
    t_years = flatten_increments(increments_rows, target_col, "Years")
    n_months = flatten_increments(increments_rows, normal_col, "Months")
    t_months = flatten_increments(increments_rows, target_col, "Months")

    def append_increment_section(title, left, right):
        lines.append("")
        lines.append("%s" % title)
        lines.append("Idx  Normal    Target    Eq")
        lines.append("---  --------  --------  --")
        max_len = max(len(left), len(right))
        for i in range(max_len):
            lv = left[i] if i < len(left) else "-"
            rv = right[i] if i < len(right) else "-"
            eq = "==" if lv == rv else "!="
            lines.append("%3d  %-8s  %-8s  %s" % (i + 1, lv, rv, eq))

    append_increment_section("Increments Years (flattened)", n_years, t_years)
    append_increment_section("Increments Months (flattened)", n_months, t_months)

    output_text = "\n".join(lines)
    print(output_text)

    out_dir = args.output_dir
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    summary_suffix = ""
    if args.summary is not None:
        summary_suffix = "_s%02d" % args.summary
    fname = "%s_%s%s.txt" % (timestamp, make_slug(target_name), summary_suffix)
    out_path = os.path.join(out_dir, fname)
    with io.open(out_path, "w", encoding="utf-8") as f:
        f.write(output_text)
        f.write("\n")
    print("")
    print("Saved: %s" % out_path)

def main():
    default_log = os.path.join(os.path.expanduser("~"), "Documents", "My Games", "beyond the sword", "Logs", "PythonDbg.log",)

    parser = argparse.ArgumentParser(description="Compare Sevopedia summary rows between Normal and a selected speed")
    parser.add_argument("--log", default=default_log, help="Path to PythonDbg.log")
    parser.add_argument("--speed", default="slow", help="Target speed name (e.g. slow, marathon, veryslow)")
    parser.add_argument("--normal-speed", default="normal", help="Reference speed name")
    parser.add_argument("--summary", type=int, help="Optional summary index (1-20)")
    parser.add_argument("--output-dir", default=os.path.join("LLM_Helpers", "outputs"), help="Output directory for timestamped .txt output",)

    run(parser.parse_args())

if __name__ == "__main__":
    main()
