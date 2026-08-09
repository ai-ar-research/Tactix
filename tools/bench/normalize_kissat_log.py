#!/usr/bin/env python3
"""Convert a Kissat `configure -l`, `--log=2` diagnostic log to trace v1.

Kissat's native log is intentionally used instead of carrying a fork.  Fields
not exposed by that log are written as zero/'-'; Reorder anchors and exported
decision literals remain directly comparable.  This adapter is diagnostic,
not a benchmark parser.
"""
import argparse
import re


HEADER = "# solver\tseq\tevent\tconflicts\tdecisions\tpropagations\tlevel\tmode\tactive\tunassigned\ttrail\tqueue\theap\tretired_queue\tretired_heap\tirred\tred\tbinary\tdigest\tsearch_external\ttop32\tpayload"
DECISION = re.compile(r"next .* decision .*literal \d+\((-?\d+)\)")
REORDER = re.compile(r"reorder limit .* after (\d+) conflicts in (stable|focused) mode")
REORDERED = re.compile(r"reordered decisions in (stable|focused) search mode")


def emit(out, seq, event, conflicts, decisions, level, mode, payload="-"):
    row = ["kissat", seq, event, conflicts, decisions, 0, level, mode,
           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "0", 0, "-", payload]
    out.write("\t".join(map(str, row)) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("output")
    args = ap.parse_args()
    seq = decisions = conflicts = 0
    mode = "unknown"
    with open(args.output, "w", encoding="utf-8") as out:
        out.write("# tactix-event-trace\t1\n" + HEADER + "\n")
        with open(args.log, encoding="utf-8", errors="replace") as src:
            for line in src:
                rm = REORDER.search(line)
                if rm:
                    conflicts, mode = int(rm.group(1)), rm.group(2)
                    emit(out, seq, "reorder_begin", conflicts, decisions, 0, mode)
                    seq += 1
                    continue
                if REORDERED.search(line):
                    mode = REORDERED.search(line).group(1)
                    emit(out, seq, "reorder_end", conflicts, decisions, 0, mode)
                    seq += 1
                    continue
                dm = DECISION.search(line)
                if dm:
                    decisions += 1
                    # Native log prefix ends in the current decision level.
                    prefix = re.search(r"\s(\d+)\s+.*next .* decision", line)
                    level = int(prefix.group(1)) if prefix else 0
                    emit(out, seq, "decision", conflicts, decisions, level,
                         mode, dm.group(1))
                    seq += 1


if __name__ == "__main__":
    main()
