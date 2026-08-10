#!/usr/bin/env python3
"""Compare one Tactix/Kissat reduction at the first causal mismatch.

Raw traces use solver-local refs only to join clause and rank records.  Cross-
solver pairing uses the complete sorted permanent-external literal tuple and
retains duplicate multiplicity.
"""
import argparse
import collections
import json
import sys


COLUMNS = ("solver kind reduction conflict mode tier1 tier2 live_red "
           "candidates victims ref size glue used reason eligibility rank "
           "selected literals").split()
INTS = {"reduction", "conflict", "tier1", "tier2", "live_red",
        "candidates", "victims", "ref", "size", "glue", "used", "reason",
        "rank", "selected"}


def value(name, text):
    if text == "-":
        return None
    if name == "literals":
        return tuple(int(x) for x in text.split(",") if x)
    if name in INTS:
        return int(text)
    return text


def load(path):
    summaries, clauses, ranks = {}, [], {}
    with open(path) as src:
        for lineno, line in enumerate(src, 1):
            if not line.strip() or line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) != len(COLUMNS):
                raise ValueError(f"{path}:{lineno}: expected {len(COLUMNS)} "
                                 f"columns, got {len(fields)}")
            row = {k: value(k, v) for k, v in zip(COLUMNS, fields)}
            if row["kind"] == "summary":
                summaries[row["reduction"]] = row
            elif row["kind"] == "clause":
                clauses.append(row)
            elif row["kind"] == "rank":
                ranks[(row["reduction"], row["ref"])] = row
            else:
                raise ValueError(f"{path}:{lineno}: unknown kind "
                                 f"{row['kind']!r}")
    for row in clauses:
        rank = ranks.get((row["reduction"], row["ref"]))
        if rank:
            row["rank"], row["selected"] = rank["rank"], rank["selected"]
    return summaries, clauses


def public(row):
    return {k: row.get(k) for k in
            ("size", "glue", "used", "reason", "eligibility", "rank",
             "selected", "literals")}


def compare(left_path, right_path, reduction=None):
    ls, lc = load(left_path)
    rs, rc = load(right_path)
    common = sorted(set(ls) & set(rs))
    if reduction is None:
        if not common:
            raise ValueError("traces have no common reduction")
        reduction = common[0]
    if reduction not in ls or reduction not in rs:
        raise ValueError(f"reduction {reduction} lacks a summary in one trace")
    left_summary, right_summary = ls[reduction], rs[reduction]
    result = {"schema": "tactix-kissat-reduce-compare-v1",
              "status": "match", "category": "exact-match",
              "reduction": reduction,
              "tactix_summary": left_summary,
              "kissat_summary": right_summary,
              "witness": None}

    for field in ("conflict", "mode", "tier1", "tier2"):
        if left_summary[field] != right_summary[field]:
            result.update(status="diverged", category="tier-threshold-mismatch",
                          witness={"field": field,
                                   "tactix": left_summary[field],
                                   "kissat": right_summary[field]})
            return result

    def grouped(rows):
        out = collections.defaultdict(list)
        for row in rows:
            if row["reduction"] == reduction:
                out[row["literals"]].append(row)
        for values in out.values():
            values.sort(key=lambda r: (r["glue"], r["used"], r["reason"],
                                       r["eligibility"], r["rank"] is None,
                                       r["rank"] or 0, r["ref"]))
        return out

    lg, rg = grouped(lc), grouped(rc)
    for key in sorted(set(lg) | set(rg)):
        if len(lg.get(key, ())) != len(rg.get(key, ())):
            result.update(status="diverged", category="clause-population-mismatch",
                          witness={"literals": key,
                                   "tactix_multiplicity": len(lg.get(key, ())),
                                   "kissat_multiplicity": len(rg.get(key, ()))})
            return result
    fields = (("size", "clause-metadata-mismatch"),
              ("glue", "glue-mismatch"),
              ("used", "used-history-mismatch"),
              ("reason", "reason-lock-mismatch"),
              ("eligibility", "eligibility-mismatch"),
              ("rank", "ranking-mismatch"),
              ("selected", "cutoff-mismatch"))
    for key in sorted(lg):
        for duplicate, (left, right) in enumerate(zip(lg[key], rg[key])):
            for field, category in fields:
                if left[field] != right[field]:
                    result.update(
                        status="diverged", category=category,
                        witness={"literals": key, "duplicate": duplicate,
                                 "field": field, "tactix": public(left),
                                 "kissat": public(right)})
                    return result
    for field in ("live_red", "candidates", "victims"):
        if left_summary[field] != right_summary[field]:
            result.update(status="diverged", category="cutoff-mismatch",
                          witness={"field": field,
                                   "tactix": left_summary[field],
                                   "kissat": right_summary[field]})
            return result
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("tactix")
    ap.add_argument("kissat")
    ap.add_argument("--reduction", type=int)
    ap.add_argument("--json")
    ap.add_argument("--require-match", action="store_true")
    args = ap.parse_args()
    try:
        result = compare(args.tactix, args.kissat, args.reduction)
    except (OSError, ValueError) as error:
        print(f"compare-reduce-trace: {error}", file=sys.stderr)
        return 2
    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.json:
        with open(args.json, "w") as out:
            out.write(rendered)
    sys.stdout.write(rendered)
    return int(args.require_match and result["status"] != "match")


if __name__ == "__main__":
    raise SystemExit(main())
