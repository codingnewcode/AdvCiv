#!/usr/bin/env python3
# AI, UI, logging, or other modifications first developed in AdvCiv-SAS (Simple Advanced Strategy)
# (c) 2026 wonderingabout & AI/LLM helpers (see Authors in AdvCiv-SAS's root README.md)

import argparse
import io
import os
import re
import sys
import zipfile


ROW_PREFIX = "GAME_RECORD_RNG_CHECKPOINT "
FIELD_RE = re.compile(r"([A-Za-z][A-Za-z0-9]*)=([^ ]+)")
STREAMS = ("map", "sync")
DEFAULT_EXAMPLE_OUTPUT = os.path.join(os.path.dirname(__file__), "examples", "sasgamerecord_rng_compared.txt")
COUNTERS = (
    "Calls",
    "NullMessageCalls",
    "ExternalCalls",
    "DeterministicRangeCalls",
    "SeedSets",
)
COMPARE_SUFFIXES = (
    "SessionStartState",
    "IntervalStartState",
    "State",
    "IntervalCalls",
    "SessionCalls",
    "IntervalNullMessageCalls",
    "SessionNullMessageCalls",
    "IntervalExternalCalls",
    "SessionExternalCalls",
    "IntervalDeterministicRangeCalls",
    "SessionDeterministicRangeCalls",
    "IntervalSeedSets",
    "SessionSeedSets",
    "IntervalStreamFingerprint",
    "SessionStreamFingerprint",
    "IntervalCallFingerprint",
    "SessionCallFingerprint",
)


def read_record(path):
    if not zipfile.is_zipfile(path):
        with io.open(path, "r", encoding="utf-8", errors="replace") as handle:
            return handle.read(), os.path.basename(path)

    with zipfile.ZipFile(path, "r") as archive:
        members = [name for name in archive.namelist() if not name.endswith("/") and name.lower().endswith(".log")]
        if len(members) != 1:
            raise ValueError("%s contains %d .log members; provide a ZIP containing exactly one record" % (path, len(members)))
        data = archive.read(members[0]).decode("utf-8", errors="replace")
        return data, "%s:%s" % (os.path.basename(path), members[0])


def parse_checkpoints(text):
    rows = []
    for line_number, raw in enumerate(text.splitlines(), 1):
        if not raw.startswith(ROW_PREFIX):
            continue
        fields = dict(FIELD_RE.findall(raw[len(ROW_PREFIX):]))
        fields["_line"] = line_number
        rows.append(fields)
    return rows


def integer(row, key):
    try:
        return int(row[key])
    except (KeyError, ValueError):
        raise ValueError("line %s has invalid or missing %s" % (row.get("_line", "?"), key))


def advance_lcg(state, calls):
    multiplier = 1103515245
    increment = 12345
    accumulated_multiplier = 1
    accumulated_increment = 0
    while calls > 0:
        if calls & 1:
            accumulated_multiplier = (accumulated_multiplier * multiplier) & 0xFFFFFFFF
            accumulated_increment = (accumulated_increment * multiplier + increment) & 0xFFFFFFFF
        increment = (increment * (multiplier + 1)) & 0xFFFFFFFF
        multiplier = (multiplier * multiplier) & 0xFFFFFFFF
        calls >>= 1
    return (accumulated_multiplier * state + accumulated_increment) & 0xFFFFFFFF


def validate_record(rows, label):
    errors = []
    previous = {}
    for index, row in enumerate(rows):
        for stream in STREAMS:
            prefix = stream
            start_state = integer(row, prefix + "IntervalStartState")
            end_state = integer(row, prefix + "State")
            interval_calls = integer(row, prefix + "IntervalCalls")
            interval_seed_sets = integer(row, prefix + "IntervalSeedSets")
            session_start_state = integer(row, prefix + "SessionStartState")

            if stream not in previous:
                expected_start = session_start_state
                previous[stream] = {"state": session_start_state, "sessionStartState": session_start_state}
                for counter in COUNTERS:
                    previous[stream][counter] = 0
            else:
                expected_start = previous[stream]["state"]

            if session_start_state != previous[stream]["sessionStartState"]:
                errors.append("%s row %d %s session start %u changed from %u" % (label, index + 1, stream, session_start_state, previous[stream]["sessionStartState"]))
            if start_state != expected_start:
                errors.append("%s row %d %s interval start %u != prior state %u" % (label, index + 1, stream, start_state, expected_start))
            if interval_seed_sets == 0:
                reconstructed = advance_lcg(start_state, interval_calls)
                if reconstructed != end_state:
                    errors.append("%s row %d %s LCG reconstruction %u != state %u" % (label, index + 1, stream, reconstructed, end_state))

            for counter in COUNTERS:
                interval_value = integer(row, prefix + "Interval" + counter)
                session_value = integer(row, prefix + "Session" + counter)
                expected_session = previous[stream][counter] + interval_value
                if session_value != expected_session:
                    errors.append("%s row %d %s session%s %d != prior + interval %d" % (label, index + 1, stream, counter, session_value, expected_session))
                previous[stream][counter] = session_value

            for subset in ("NullMessageCalls", "ExternalCalls", "DeterministicRangeCalls"):
                if integer(row, prefix + "Interval" + subset) > interval_calls:
                    errors.append("%s row %d %s interval%s exceeds intervalCalls" % (label, index + 1, stream, subset))
            previous[stream]["state"] = end_state
    return errors


def row_identity(row):
    return "turn=%s reason=%s" % (row.get("turn", "?"), row.get("reason", "?"))


def compare_rows(rows_a, rows_b):
    count = min(len(rows_a), len(rows_b))
    for index in range(count):
        row_a = rows_a[index]
        row_b = rows_b[index]
        identity_a = row_identity(row_a)
        identity_b = row_identity(row_b)
        if identity_a != identity_b:
            return index, [("checkpoint", identity_a, identity_b)]
        differences = []
        if row_a.get("rngFingerprintSchema") != row_b.get("rngFingerprintSchema"):
            differences.append(("rngFingerprintSchema", row_a.get("rngFingerprintSchema", "<missing>"), row_b.get("rngFingerprintSchema", "<missing>")))
        for stream in STREAMS:
            for suffix in COMPARE_SUFFIXES:
                key = stream + suffix
                if row_a.get(key) != row_b.get(key):
                    differences.append((key, row_a.get(key, "<missing>"), row_b.get(key, "<missing>")))
        if differences:
            return index, differences
    if len(rows_a) != len(rows_b):
        return count, [("checkpointCount", str(len(rows_a)), str(len(rows_b)))]
    return None, []


def explain(differences):
    names = {item[0] for item in differences}
    if "checkpoint" in names or "checkpointCount" in names:
        return "The lifecycle/checkpoint sequence differs before field-level RNG comparison can continue."
    if any(name.endswith("IntervalCalls") or name.endswith("SessionCalls") for name in names):
        return "At least one run consumed a different number of authoritative RNG advances in this interval."
    if any(name.endswith("State") or "StreamFingerprint" in name for name in names):
        return "Authoritative random-stream consumption diverged even if the number of calls happened to match."
    if any("CallFingerprint" in name or "NullMessageCalls" in name or "ExternalCalls" in name for name in names):
        return "Random values may still match, but call provenance, labels/data, NULL-message use, or EXE-wrapper origin differs."
    return "The checkpoint metadata differs."


def emit_report(lines, output_path):
    report = "\n".join(lines) + "\n"
    sys.stdout.write(report)
    if output_path:
        output_dir = os.path.dirname(os.path.abspath(output_path))
        if output_dir and not os.path.isdir(output_dir):
            os.makedirs(output_dir)
        with io.open(output_path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(report)
        print("Wrote: %s" % output_path)


def main(argv=None):
    parser = argparse.ArgumentParser(description="Validate and compare authoritative-RNG checkpoints in two SASGameRecord .log or single-log .zip files.")
    parser.add_argument("record_a")
    parser.add_argument("record_b")
    output_group = parser.add_mutually_exclusive_group()
    output_group.add_argument("--output", help="also write the report to this path")
    output_group.add_argument("--example-output", action="store_true", help="also refresh LLM_Helpers/examples/sasgamerecord_rng_compared.txt")
    args = parser.parse_args(argv)
    output_path = DEFAULT_EXAMPLE_OUTPUT if args.example_output else args.output

    try:
        text_a, label_a = read_record(args.record_a)
        text_b, label_b = read_record(args.record_b)
        rows_a = parse_checkpoints(text_a)
        rows_b = parse_checkpoints(text_b)
        if not rows_a or not rows_b:
            raise ValueError("both records must contain GAME_RECORD_RNG_CHECKPOINT rows")

        errors_a = validate_record(rows_a, "A")
        errors_b = validate_record(rows_b, "B")
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        print("error: %s" % error, file=sys.stderr)
        return 2

    lines = []
    lines.append("A: %s (%d checkpoints, %s)" % (label_a, len(rows_a), "valid" if not errors_a else "%d validation error(s)" % len(errors_a)))
    lines.append("B: %s (%d checkpoints, %s)" % (label_b, len(rows_b), "valid" if not errors_b else "%d validation error(s)" % len(errors_b)))
    for error in errors_a + errors_b:
        lines.append("VALIDATION: %s" % error)
    if errors_a or errors_b:
        emit_report(lines, output_path)
        return 2

    index, differences = compare_rows(rows_a, rows_b)
    if index is None:
        lines.append("RNG checkpoints are identical.")
        emit_report(lines, output_path)
        return 0

    lines.append("First RNG divergence at checkpoint %d:" % (index + 1))
    if index < len(rows_a):
        lines.append("  A %s" % row_identity(rows_a[index]))
    if index < len(rows_b):
        lines.append("  B %s" % row_identity(rows_b[index]))
    for name, value_a, value_b in differences:
        lines.append("  %s: %s vs %s" % (name, value_a, value_b))
    lines.append("Interpretation: %s" % explain(differences))
    emit_report(lines, output_path)
    return 1


if __name__ == "__main__":
    sys.exit(main())
