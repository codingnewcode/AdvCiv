#!/usr/bin/env python3
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)
#
import argparse
import io
import os
import random
import re
from datetime import datetime, timezone
import xml.etree.ElementTree as ET

START_YEAR = -50000
SUMMARY_STEPS = 20
PREFERRED_LADDER = [3000, 2400, 1800, 1500, 1200, 900, 720, 600, 480, 360, 300, 240, 180, 144, 120, 108, 96, 84, 72, 60, 48, 36, 24, 12]
INVALID_SCORE = 10 ** 12

def _local_name(tag):
    return tag.split("}", 1)[-1]

def _normalize_speed_name(name):
    s = name.strip().lower()
    s = re.sub(r"^gamespeed_", "", s)
    s = s.replace("_", "")
    s = s.replace(" ", "")
    return s

def _display_speed_name(speed_type):
    raw = re.sub(r"^GAMESPEED_", "", speed_type)
    raw = raw.replace("_", " ").strip().title()
    return raw

def _make_slug(name):
    s = _normalize_speed_name(name)
    s = re.sub(r"[^a-z0-9_]+", "_", s)
    return s.strip("_") or "speed"

def _year_month_to_total_months(year, month):
    return year * 12 + (month - 1)

def _format_delta_months(delta_months):
    sign = "+" if delta_months >= 0 else "-"
    v = abs(delta_months)
    return "%s%dy%dm" % (sign, v // 12, v % 12)

def _format_delta_vs_increment(delta_months, increment_months):
    if increment_months <= 0:
        return "n/a", "n/a"
    ratio = float(delta_months) / float(increment_months)
    return "%+.2f" % ratio, "%+.1f%%" % (ratio * 100.0)

def parse_xml_speeds(xml_path):
    root = ET.parse(xml_path).getroot()
    speeds = {}

    for node in root.iter():
        if _local_name(node.tag) != "GameSpeedInfo":
            continue

        speed_type = None
        num_turns = None
        rows = []

        for ch in node:
            n = _local_name(ch.tag)
            if n == "Type":
                speed_type = (ch.text or "").strip()
            elif n == "iNumTurns":
                num_turns = int((ch.text or "0").strip())
            elif n == "GameTurnInfos":
                for r in ch:
                    if _local_name(r.tag) != "GameTurnInfo":
                        continue
                    inc = None
                    turns = None
                    for x in r:
                        xn = _local_name(x.tag)
                        if xn == "iMonthIncrement":
                            inc = int((x.text or "0").strip())
                        elif xn == "iTurnsPerIncrement":
                            turns = int((x.text or "0").strip())
                    if inc is not None and turns is not None:
                        rows.append((inc, turns))

        if speed_type and rows:
            if not num_turns:
                num_turns = sum(n for (_m, n) in rows)
            speeds[speed_type] = {"type": speed_type, "num_turns": num_turns, "rows": rows}

    return speeds

def resolve_speed_type(speeds, requested_name):
    want = _normalize_speed_name(requested_name)
    items = []
    for speed_type in speeds:
        norm = _normalize_speed_name(speed_type)
        disp = _normalize_speed_name(_display_speed_name(speed_type))
        items.append((speed_type, norm, disp))

    exact = [it[0] for it in items if it[1] == want or it[2] == want]
    if exact:
        return exact[0]

    starts = [it[0] for it in items if it[1].startswith(want) or it[2].startswith(want)]
    if len(starts) == 1:
        return starts[0]

    contains = [it[0] for it in items if want in it[1] or want in it[2]]
    if len(contains) == 1:
        return contains[0]

    return None

def get_date_at_turn(rows, turn, start_year=START_YEAR):
    elapsed_turns = 0
    elapsed_months = 0
    current_increment = 0

    for inc, span in rows:
        if elapsed_turns >= turn:
            break
        take = min(span, turn - elapsed_turns)
        elapsed_months += take * inc
        elapsed_turns += take
        current_increment = inc

    total_months = start_year * 12 + elapsed_months
    year = total_months // 12
    month = (total_months % 12) + 1
    return {"turn": turn, "year": year, "month": month, "inc": current_increment, "total_months": total_months}

def build_summary_rows(speed_data, summary_steps=SUMMARY_STEPS):
    out = []
    num_turns = speed_data["num_turns"]
    rows = speed_data["rows"]
    step_turn = num_turns // summary_steps
    for i in range(1, summary_steps + 1):
        turn = step_turn * i
        pct = int(round((100.0 * i) / float(summary_steps)))
        out.append((i, pct, get_date_at_turn(rows, turn)))
    return out

def compare_from_xml(xml_path, normal_speed_name, target_speed_name, summary_steps=SUMMARY_STEPS):
    speeds = parse_xml_speeds(xml_path)
    normal_type = resolve_speed_type(speeds, normal_speed_name)
    target_type = resolve_speed_type(speeds, target_speed_name)
    if normal_type is None:
        raise SystemExit("Could not resolve normal speed: %s" % normal_speed_name)
    if target_type is None:
        raise SystemExit("Could not resolve target speed: %s" % target_speed_name)

    normal_data = speeds[normal_type]
    target_data = speeds[target_type]
    normal_summary = build_summary_rows(normal_data, summary_steps)
    target_summary = build_summary_rows(target_data, summary_steps)

    parsed = []
    for (idx, pct, n), (_idx2, _pct2, t) in zip(normal_summary, target_summary):
        parsed.append((idx, pct, n, t, t["total_months"] - n["total_months"]))

    return {
        "normal_type": normal_type,
        "target_type": target_type,
        "normal_name": _display_speed_name(normal_type),
        "target_name": _display_speed_name(target_type),
        "normal_data": normal_data,
        "target_data": target_data,
        "parsed": parsed,
    }

def score_alignment(parsed):
    # Weight the difficult early-mid zone where we tune the most.
    weights = {1: 1.0, 2: 1.5, 3: 2.5, 4: 5.0, 5: 5.0, 6: 5.0, 7: 4.0, 8: 4.5, 9: 3.0, 10: 2.0, 11: 2.0, 12: 2.0}
    score = 0.0
    for idx, _pct, _n, _t, delta_months in parsed:
        w = weights.get(idx, 1.0)
        years = delta_months / 12.0
        score += w * (years * years)
    return score

def score_alignment_slow(parsed):
    # Style-first: keep close enough to Normal, but prioritize smooth shape over exactness.
    score = 0.0

    delta_years = {}
    delta_ratio = {}
    for idx, _pct, _n, _t, delta_months in parsed:
        delta_years[idx] = delta_months / 12.0
        inc = _t.get("inc", 0)
        if inc <= 0:
            return INVALID_SCORE
        delta_ratio[idx] = float(delta_months) / float(inc)

    # Hard acceptance bands (ratio in "increments", i.e. Delta/Inc).
    # Tight early, medium mid, looser later because increments become tiny.
    for idx in range(1, SUMMARY_STEPS + 1):
        r = abs(delta_ratio.get(idx, 0.0))
        if idx <= 3 and r > 2.5:
            return INVALID_SCORE
        if 4 <= idx <= 8 and r > 3.5:
            return INVALID_SCORE
        if 9 <= idx <= 12 and r > 40.0:
            return INVALID_SCORE

    # Light alignment pressure.
    for idx in range(1, SUMMARY_STEPS + 1):
        y = abs(delta_years.get(idx, 0.0))
        w = 0.6
        if 3 <= idx <= 12:
            w = 1.0
        score += w * y

    # Strongly avoid cliffs/overshoot in the key zone.
    for idx in [4, 5, 6]:
        y = delta_years.get(idx, 0.0)
        if y < -30:
            score += (abs(y + 30) * 80.0)
    y7 = delta_years.get(7, 0.0)
    if y7 > 8:
        score += (y7 - 8) * 140.0
    if y7 < -12:
        score += (abs(y7 + 12) * 80.0)

    # Smoothness in 3..10.
    for idx in range(3, 10):
        jump = abs(delta_years.get(idx + 1, 0.0) - delta_years.get(idx, 0.0))
        if jump > 18:
            score += (jump - 18) * 25.0

    return score

def _style_penalty_rows(rows):
    penalty = 0.0
    prev = None
    for inc, _turns in rows:
        if prev is not None:
            if inc > prev:
                penalty += 1000000.0
            if prev >= 120 and inc >= 120:
                ratio = float(prev) / float(inc)
                if ratio > 2.0:
                    penalty += (ratio - 2.0) * 2000.0
        if inc >= 120 and inc not in PREFERRED_LADDER:
            penalty += 800.0
        prev = inc
    return penalty

def _nearest_ladder_value(value, max_allowed):
    candidates = [v for v in PREFERRED_LADDER if v <= max_allowed]
    if not candidates:
        return min(max_allowed, value)
    return min(candidates, key=lambda v: abs(v - value))

def _has_non_increasing_increments(rows):
    prev = None
    for inc, _turns in rows:
        if prev is not None and inc > prev:
            return False
        prev = inc
    return True

def _emit_report(compare_result, title, meta_lines=None):
    parsed = compare_result["parsed"]
    focus_start = compare_result.get("focus_start_pct")
    focus_end = compare_result.get("focus_end_pct")
    if focus_start is not None and focus_end is not None:
        parsed = [row for row in parsed if focus_start <= row[1] <= focus_end]
        if not parsed:
            raise SystemExit("No summary rows left after focus filter: %d-%d%%" % (focus_start, focus_end))
    lines = []
    lines.append(title)
    lines.append("Compare: %s vs %s" % (compare_result["target_name"], compare_result["normal_name"]))
    if meta_lines:
        lines.extend(meta_lines)
    lines.append("")
    lines.append("Idx  Pct  Normal                Target                Delta     Delta/Inc    d(Inc)  Delta%Inc")
    lines.append("---  ---  --------------------  --------------------  --------  ---------  --------  ---------")
    prev_ratio = None
    for idx, pct, n, t, d in parsed:
        ns = "T%d=%+dm%d (%d)" % (n["turn"], n["year"], n["month"], n["inc"])
        ts = "T%d=%+dm%d (%d)" % (t["turn"], t["year"], t["month"], t["inc"])
        ratio_text, pct_text = _format_delta_vs_increment(d, t["inc"])
        ratio_val = None
        if t["inc"] > 0:
            ratio_val = float(d) / float(t["inc"])
        d_ratio_text = "n/a"
        if ratio_val is not None and prev_ratio is not None:
            d_ratio_text = "%+.2f" % (ratio_val - prev_ratio)
        lines.append("%3d  %3d  %-20s  %-20s  %-8s  %9s  %8s  %9s" % (idx, pct, ns, ts, _format_delta_months(d), ratio_text, d_ratio_text, pct_text))
        if ratio_val is not None:
            prev_ratio = ratio_val
    lines.append("")
    lines.append("Score: %f" % score_alignment(parsed))
    return "\n".join(lines)

def _slow_autoloop_candidate_rows(base_rows, rng):
    # Row indices are 0-based in GameTurnInfos list for GAMESPEED_SLOW.
    # Tune only a narrow section to keep behavior interpretable.
    rows = [list(r) for r in base_rows]

    # Build a strictly non-increasing, human-readable sequence for rows 5..16 (indices 4..15).
    prev = rows[3][0]
    for idx in range(4, 16):
        base_inc = rows[idx][0]
        target = _nearest_ladder_value(base_inc, prev)
        options = sorted([v for v in PREFERRED_LADDER if v <= prev], reverse=True)
        if not options:
            return None
        pos = options.index(target) if target in options else 0
        delta = rng.choice([-1, 0, 1])
        pos2 = min(max(0, pos + delta), len(options) - 1)
        rows[idx][0] = options[pos2]
        prev = rows[idx][0]

    # Move turns around early-mid boundaries to smooth summary 4-8.
    d0 = rng.randint(-3, 3)
    if rows[4][1] + d0 < 1 or rows[5][1] - d0 < 1:
        return None
    rows[4][1] += d0
    rows[5][1] -= d0

    d1 = rng.randint(-4, 4)
    if rows[11][1] + d1 < 1 or rows[12][1] - d1 < 1:
        return None
    rows[11][1] += d1
    rows[12][1] -= d1

    d2 = rng.randint(-6, 6)
    if rows[13][1] + d2 < 1 or rows[14][1] - d2 < 1:
        return None
    rows[13][1] += d2
    rows[14][1] -= d2

    # Hard global rule: no yoyo, increments must be non-increasing.
    if not _has_non_increasing_increments([(m, n) for (m, n) in rows]):
        return None

    # Preserve total months by compensating on late rows.
    base_total = sum(m * n for m, n in base_rows)
    new_total = sum(m * n for m, n in rows)
    delta = new_total - base_total
    if delta % 2 != 0:
        return None
    shift = delta // 2
    if abs(shift) > 180:
        return None
    # +shift means move turns 6m -> 4m; -shift means reverse.
    if shift > 0:
        # donors: rows 24,23,22 (indices 23,22,21), recipient row 26.
        for donor in [23, 22, 21]:
            if shift <= 0:
                break
            take = min(shift, rows[donor][1] - 1)
            rows[donor][1] -= take
            rows[25][1] += take
            shift -= take
    elif shift < 0:
        need = -shift
        # donor row 26 -> recipient row24 first.
        take = min(need, rows[25][1] - 1)
        rows[25][1] -= take
        rows[23][1] += take
        need -= take
        if need > 0:
            return None
    if shift > 0:
        return None

    if sum(n for _m, n in rows) != sum(n for _m, n in base_rows):
        return None
    # Keep late-zone turn counts within sane range to avoid pathological tails.
    if rows[22][1] < 100 or rows[22][1] > 320:
        return None
    if rows[23][1] < 100 or rows[23][1] > 320:
        return None
    if rows[25][1] < 60 or rows[25][1] > 260:
        return None
    return [(m, n) for (m, n) in rows]

def run_autoloop(xml_path, target_speed, normal_speed, iterations, seed):
    speeds = parse_xml_speeds(xml_path)
    target_type = resolve_speed_type(speeds, target_speed)
    normal_type = resolve_speed_type(speeds, normal_speed)
    if target_type is None or normal_type is None:
        raise SystemExit("Could not resolve speed types for autoloop.")
    if target_type != "GAMESPEED_SLOW":
        raise SystemExit("Autoloop draft currently targets only GAMESPEED_SLOW.")

    base = speeds[target_type]
    base_rows = base["rows"]
    rng = random.Random(seed)

    # Baseline compare
    baseline = compare_from_xml(xml_path, normal_speed, target_speed, SUMMARY_STEPS)
    best_rows = base_rows
    best_score = score_alignment_slow(baseline["parsed"])
    if not _has_non_increasing_increments(best_rows) or best_score >= INVALID_SCORE:
        best_score = 10 ** 18

    for _ in range(iterations):
        candidate_rows = _slow_autoloop_candidate_rows(base_rows, rng)
        if candidate_rows is None:
            continue

        # Evaluate candidate quickly by replacing target rows in-memory.
        speeds[target_type]["rows"] = candidate_rows
        target_summary = build_summary_rows(speeds[target_type], SUMMARY_STEPS)
        normal_summary = build_summary_rows(speeds[normal_type], SUMMARY_STEPS)
        parsed = []
        for (idx, pct, n), (_i2, _p2, t) in zip(normal_summary, target_summary):
            parsed.append((idx, pct, n, t, t["total_months"] - n["total_months"]))
        sc = score_alignment_slow(parsed)
        sc += _style_penalty_rows(candidate_rows)

        # Human-readable preference in tuned section:
        # favor year steps like 10/9/8/7/6/5/4/3/2/1 years (x12 months).
        preferred = set([12, 24, 36, 48, 60, 72, 84, 96, 108, 120, 144, 180, 240, 300, 360, 480, 600, 720, 900, 1200, 1500, 1800, 2400, 3000])
        for idx in range(4, 16):
            inc = candidate_rows[idx][0]
            if inc >= 120 and inc not in preferred:
                sc += 800.0

        # Extra guard: reject any candidate that violates global monotonic increments.
        if not _has_non_increasing_increments(candidate_rows):
            continue

        # Keep late compensation close to baseline profile.
        baseline_late = [base_rows[22][1], base_rows[23][1], base_rows[25][1]]
        cand_late = [candidate_rows[22][1], candidate_rows[23][1], candidate_rows[25][1]]
        for b, c in zip(baseline_late, cand_late):
            sc += abs(c - b) * 3.0

        if sc >= INVALID_SCORE:
            continue
        if sc < best_score:
            best_score = sc
            best_rows = candidate_rows

    # Restore and build final report data
    speeds[target_type]["rows"] = best_rows
    final_target_summary = build_summary_rows(speeds[target_type], SUMMARY_STEPS)
    final_normal_summary = build_summary_rows(speeds[normal_type], SUMMARY_STEPS)
    final_parsed = []
    for (idx, pct, n), (_i2, _p2, t) in zip(final_normal_summary, final_target_summary):
        final_parsed.append((idx, pct, n, t, t["total_months"] - n["total_months"]))

    return {
        "normal_type": normal_type,
        "target_type": target_type,
        "normal_name": _display_speed_name(normal_type),
        "target_name": _display_speed_name(target_type),
        "parsed": final_parsed,
        "score": best_score,
        "rows": best_rows,
        "iterations": iterations,
        "seed": seed,
        "found_valid": best_score < INVALID_SCORE,
    }

def main():
    parser = argparse.ArgumentParser(description="XML-based summary compare and draft autoloop tuner.")
    parser.add_argument("--xml", default=os.path.join("Assets", "XML", "GameInfo", "CIV4GameSpeedInfo.xml"))
    parser.add_argument("--speed", default="slow")
    parser.add_argument("--normal-speed", default="normal")
    parser.add_argument("--output-dir", default=os.path.join("LLM_Helpers", "outputs"))
    parser.add_argument("--autoloop", action="store_true", help="Run draft autoloop tuner (currently for slow).")
    parser.add_argument("--iterations", type=int, default=4000)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--summary-steps", type=int, default=SUMMARY_STEPS, help="Number of summary checkpoints (20=5%%, 50=2%%).")
    parser.add_argument("--focus-start-pct", type=int, default=0, help="Only print/check rows with pct >= this value.")
    parser.add_argument("--focus-end-pct", type=int, default=100, help="Only print/check rows with pct <= this value.")
    args = parser.parse_args()
    if args.focus_start_pct < 0 or args.focus_end_pct > 100 or args.focus_start_pct > args.focus_end_pct:
        raise SystemExit("Invalid focus range: %d-%d%%" % (args.focus_start_pct, args.focus_end_pct))

    if args.autoloop:
        if args.summary_steps != SUMMARY_STEPS:
            raise SystemExit("--autoloop currently requires --summary-steps=%d" % SUMMARY_STEPS)
        result = run_autoloop(args.xml, args.speed, args.normal_speed, args.iterations, args.seed)
        meta = [
            "Mode: XML autoloop draft",
            "Iterations: %d" % result["iterations"],
            "Seed: %d" % result["seed"],
            "Best score: %f" % result["score"],
        ]
        if not result["found_valid"]:
            meta.append("Status: no valid decremental candidate found under current local move constraints")
        compare_result = {
            "normal_name": result["normal_name"],
            "target_name": result["target_name"],
            "parsed": result["parsed"],
            "focus_start_pct": args.focus_start_pct,
            "focus_end_pct": args.focus_end_pct,
        }
        text = _emit_report(compare_result, "Source: %s" % args.xml, meta)
        if result["found_valid"]:
            text += "\n\nSuggested %s rows (iMonthIncrement x iTurnsPerIncrement):\n" % result["target_type"]
            for i, (m, n) in enumerate(result["rows"], 1):
                text += "%2d: %d x %d\n" % (i, m, n)
    else:
        compare_result = compare_from_xml(args.xml, args.normal_speed, args.speed, args.summary_steps)
        compare_result["focus_start_pct"] = args.focus_start_pct
        compare_result["focus_end_pct"] = args.focus_end_pct
        text = _emit_report(compare_result, "Source: %s" % args.xml)

    print(text)

    if not os.path.isdir(args.output_dir):
        os.makedirs(args.output_dir)
    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    mode = "autoloop" if args.autoloop else "xml"
    name = "%s_%s_%s.txt" % (ts, _make_slug(args.speed), mode)
    out_path = os.path.join(args.output_dir, name)
    with io.open(out_path, "w", encoding="utf-8") as f:
        f.write(text)
        f.write("\n")
    print("")
    print("Saved: %s" % out_path)

if __name__ == "__main__":
    main()
